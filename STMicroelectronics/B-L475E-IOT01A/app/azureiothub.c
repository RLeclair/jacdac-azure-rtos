#include "jdstm.h"

#include "jacdac/dist/c/azureiothubhealth.h"
#include "jacs_internal.h"

#define LOG(msg, ...) DMESG("aziot: " msg, ##__VA_ARGS__)

struct srv_state {
    SRV_COMMON;

    // regs
    uint16_t conn_status;
    uint32_t push_period_ms;
    uint32_t push_watchdog_period_ms;

    // non-regs
    bool waiting_for_net;
    uint32_t reconnect_timer;
    uint32_t flush_timer;
    uint32_t watchdog_timer_ms;

    char *hub_name;
    char *device_id;
    char *sas_token;
    char *pub_topic;

    nvs_handle_t nvs_handle;
    esp_mqtt_client_handle_t client;
};

REG_DEFINITION(                                                //
    azureiothub_regs,                                          //
    REG_SRV_COMMON,                                            //
    REG_U16(JD_AZURE_IOT_HUB_HEALTH_REG_CONNECTION_STATUS),    //
    REG_U32(JD_AZURE_IOT_HUB_HEALTH_REG_PUSH_PERIOD),          //
    REG_U32(JD_AZURE_IOT_HUB_HEALTH_REG_PUSH_WATCHDOG_PERIOD), //
)


char *extract_property(const char *property_bag, int plen, const char *key) {
    int klen = strlen(key);
    for (int ptr = 0; ptr + klen < plen;) {
        int nextp = ptr;
        while (nextp < plen && property_bag[nextp] != ';')
            nextp++;
        if (property_bag[ptr + klen] == '=' && memcmp(property_bag + ptr, key, klen) == 0) {
            int sidx = ptr + klen + 1;
            int rlen = nextp - sidx;
            char *r = jd_alloc(rlen + 1);
            memcpy(r, property_bag + sidx, rlen);
            r[rlen] = 0;
            return r;
        }
        if (nextp < plen)
            nextp++;
        ptr = nextp;
    }
    return NULL;
}

char *double_array_to_json(int numvals, const double *vals) {
    if (numvals == 0)
        return jd_strdup("[]");

    char **parts2 = jd_alloc(sizeof(char *) * (numvals + 2));
    parts2[0] = "[";
    for (int i = 0; i < numvals; ++i)
        parts2[i + 1] = jd_sprintf_a("%f%s", vals[i], i == numvals - 1 ? "]" : ",");
    parts2[numvals + 1] = NULL;
    char *msg = jd_concat_many((const char **)parts2);
    for (int i = 1; parts2[i]; ++i)
        jd_free(parts2[i]);
    return msg;
}

static int parse_json_array(unsigned len, const char *data, double *dst) {
    char buf[32];
    int dp = 0;
    for (unsigned i = 0; i < len; ++i) {
        char c = data[i];
        if (c == 0)
            break;
        if (strchr("[],\t\n\r ", c))
            continue;
        if (isdigit(c) || c == '.' || c == '-' || c == '+') {
            int j = i + 1;
            while (j - i < 30 && j < len && data[j] && (isdigit(data[j]) || strchr(".eE+-", data[j]))) {
                j++;
            }
            memcpy(buf, data + i, j - i);
            buf[j - i] = 0;
            if (dst)
                dst[dp] = atof(buf);
            dp++;
            i = j - 1;
        } else {
            DMESG("invalid num array char '%c'", c);
            return dp;
        }
    }
    return dp;
}

static const char *status_name(int st) {
    switch (st) {
    case JD_AZURE_IOT_HUB_HEALTH_CONNECTION_STATUS_CONNECTED:
        return "CONNECTED";
    case JD_AZURE_IOT_HUB_HEALTH_CONNECTION_STATUS_DISCONNECTED:
        return "DISCONNECTED";
    case JD_AZURE_IOT_HUB_HEALTH_CONNECTION_STATUS_CONNECTING:
        return "CONNECTING";
    case JD_AZURE_IOT_HUB_HEALTH_CONNECTION_STATUS_DISCONNECTING:
        return "DISCONNECTING";
    default:
        return "???";
    }
}

static void feed_watchdog(srv_t *state) {
    if (state->push_watchdog_period_ms)
        state->watchdog_timer_ms = now_ms + state->push_watchdog_period_ms;
}

static void set_status(srv_t *state, uint16_t status) {
    if (state->conn_status == status)
        return;
    LOG("status %d (%s)", status, status_name(status));
    state->conn_status = status;
    jd_send_event_ext(state, JD_AZURE_IOT_HUB_HEALTH_EV_CONNECTION_STATUS_CHANGE,
                      &state->conn_status, sizeof(state->conn_status));
}

static void clear_conn_string(srv_t *state) {
    jd_free(state->hub_name);
    jd_free(state->device_id);
    jd_free(state->sas_token);
    jd_free(state->pub_topic);
    state->hub_name = NULL;
    state->device_id = NULL;
    state->sas_token = NULL;
    state->pub_topic = NULL;
}

static void on_command(srv_t *state, esp_mqtt_event_handle_t event) {
    LOG("azureiot method: '%s' rid=%d", label, ridval);

    int numvals = parse_json_array(event->data_len, event->data, NULL);
    double *d = jd_alloc(numvals * 8 + 1);
    parse_json_array(event->data_len, event->data, d);

    DMESG("args=%-s", double_array_to_json(numvals, d));

    jacscloud_on_method(label, ridval, numvals, d);
}

static esp_err_t mqtt_event_handler_cb(srv_t *state, esp_mqtt_event_handle_t event) {
    switch (event->event_id) {
    case MQTT_EVENT_DISCONNECTED:
        set_status(state, JD_AZURE_IOT_HUB_HEALTH_CONNECTION_STATUS_DISCONNECTED);
        break;
    }

    return ESP_OK;
}

static void azureiothub_disconnect(srv_t *state) {
    if (!state->client)
        return;

    set_status(state, JD_AZURE_IOT_HUB_HEALTH_CONNECTION_STATUS_DISCONNECTING);
    esp_mqtt_client_disconnect(state->client);
}

static void azureiothub_reconnect(srv_t *state) {
    if (!state->hub_name || !wifi_is_connected()) {
        azureiothub_disconnect(state);
        return;
    }

    char *uri = jd_sprintf_a("mqtts://%s", state->hub_name);
    char *username = jd_sprintf_a("%s/%-s/?api-version=2018-06-30", state->hub_name,
                                  jd_urlencode(state->device_id));

    LOG("connecting to %s/%s", uri, state->device_id);

    const esp_mqtt_client_config_t mqtt_cfg = {
        .uri = uri, //
        .client_id = state->device_id,
        .username = username,
        .password = state->sas_token,
        .crt_bundle_attach = esp_crt_bundle_attach,
        // forward to default event loop, which will run on main task:
        .event_handle = mqtt_event_handler_outer,
        // .disable_auto_reconnect=true,
        // .path = "/$iothub/websocket?iothub-no-client-cert=true" // for wss:// proto
    };

    if (!state->client) {
        state->client = esp_mqtt_client_init(&mqtt_cfg);
        JD_ASSERT(state->client != NULL);
        CHK(esp_event_handler_instance_register(MY_MQTT_EVENTS, MQTT_EVENT_ANY, mqtt_event_handler,
                                                state, NULL));
        esp_mqtt_client_start(state->client);
    } else {
        CHK(esp_mqtt_set_config(state->client, &mqtt_cfg));
        CHK(esp_mqtt_client_reconnect(state->client));
    }

    set_status(state, JD_AZURE_IOT_HUB_HEALTH_CONNECTION_STATUS_CONNECTING);

    jd_free(uri);
    jd_free(username);
}

// TODO check if IoT SDK already has conn-string parsing
static int set_conn_string(srv_t *state, const char *conn_str, int conn_len, int save) {
    if (conn_len == 0) {
        LOG("clear connection string");
        clear_conn_string(state);
        if (save) {
            nvs_erase_key(state->nvs_handle, "conn_str");
            nvs_commit(state->nvs_handle);
        }
        azureiothub_reconnect(state);
        return 0;
    }

    char *hub_name_enc = NULL;
    char *device_id_enc = NULL;

    char *hub_name = extract_property(conn_str, conn_len, "HostName");
    char *device_id = extract_property(conn_str, conn_len, "DeviceId");
    char *sas_key = extract_property(conn_str, conn_len, "SharedAccessKey");

    if (!hub_name || !device_id || !sas_key) {
        LOG("failed parsing conn string: %s", conn_str);
        goto fail;
    }

    if (save) {
        // store conn string in flash
        nvs_set_blob(state->nvs_handle, "conn_str", conn_str, conn_len);
        nvs_commit(state->nvs_handle);
    }
    azureiothub_reconnect(state);

    return 0;

fail:
    jd_free(hub_name);
    jd_free(device_id);
    jd_free(sas_key);
    jd_free(hub_name_enc);
    jd_free(device_id_enc);
    return -1;
}

#if 1
static const uint32_t glows[] = {
    [JD_AZURE_IOT_HUB_HEALTH_CONNECTION_STATUS_CONNECTED] = JD_GLOW_CLOUD_CONNECTED_TO_CLOUD,
    [JD_AZURE_IOT_HUB_HEALTH_CONNECTION_STATUS_DISCONNECTED] = JD_GLOW_CLOUD_NOT_CONNECTED_TO_CLOUD,
    [JD_AZURE_IOT_HUB_HEALTH_CONNECTION_STATUS_CONNECTING] = JD_GLOW_CLOUD_CONNECTING_TO_CLOUD,
    [JD_AZURE_IOT_HUB_HEALTH_CONNECTION_STATUS_DISCONNECTING] =
        JD_GLOW_CLOUD_NOT_CONNECTED_TO_CLOUD,
};
#endif

void azureiothub_process(srv_t *state) {
    if (state->push_watchdog_period_ms && in_past_ms(state->watchdog_timer_ms)) {
        ESP_LOGE("JD", "cloud watchdog reset\n");
        target_reset();
    }

    if (jd_should_sample(&state->reconnect_timer, 500000)) {
#if 1
        if (!wifi_is_connected())
            jd_glow(JD_GLOW_CLOUD_CONNECTING_TO_NETWORK);
        else
            jd_glow(glows[state->conn_status]);
#endif

        if (wifi_is_connected() &&
            state->conn_status == JD_AZURE_IOT_HUB_HEALTH_CONNECTION_STATUS_DISCONNECTED &&
            state->hub_name && state->waiting_for_net) {
            state->waiting_for_net = false;
            azureiothub_reconnect(state);
        }
    }

    if (jd_should_sample_ms(&state->flush_timer, state->push_period_ms)) {
        aggbuffer_flush();
    }
}

void azureiothub_handle_packet(srv_t *state, jd_packet_t *pkt) {
    switch (pkt->service_command) {
    case JD_AZURE_IOT_HUB_HEALTH_CMD_SET_CONNECTION_STRING:
        set_conn_string(state, (char *)pkt->data, pkt->service_size, 1);
        return;

    case JD_AZURE_IOT_HUB_HEALTH_CMD_CONNECT:
        azureiothub_reconnect(state);
        return;

    case JD_AZURE_IOT_HUB_HEALTH_CMD_DISCONNECT:
        azureiothub_disconnect(state);
        return;

    case JD_GET(JD_AZURE_IOT_HUB_HEALTH_REG_HUB_NAME):
        jd_respond_string(pkt, state->hub_name);
        return;

    case JD_GET(JD_AZURE_IOT_HUB_HEALTH_REG_HUB_DEVICE_ID):
        jd_respond_string(pkt, state->device_id);
        return;
    }

    switch (service_handle_register_final(state, pkt, azureiothub_regs)) {
    case JD_AZURE_IOT_HUB_HEALTH_REG_PUSH_PERIOD:
    case JD_AZURE_IOT_HUB_HEALTH_REG_PUSH_WATCHDOG_PERIOD:
        if (state->push_period_ms < 1000)
            state->push_period_ms = 1000;
        if (state->push_period_ms > 24 * 3600 * 1000)
            state->push_period_ms = 24 * 3600 * 1000;

        if (state->push_watchdog_period_ms) {
            if (state->push_watchdog_period_ms < state->push_period_ms * 3)
                state->push_watchdog_period_ms = state->push_period_ms * 3;
            feed_watchdog(state);
        }
        break;
    }
}

static srv_t *_aziot_state;

SRV_DEF(azureiothub, JD_SERVICE_CLASS_AZURE_IOT_HUB_HEALTH);
void azureiothub_init(void) {
    SRV_ALLOC(azureiothub);

    aggbuffer_init(&azureiothub_cloud);

    state->conn_status = JD_AZURE_IOT_HUB_HEALTH_CONNECTION_STATUS_DISCONNECTED;
    state->waiting_for_net = true;
    state->push_period_ms = 5000;

#if 0
    size_t connlen;
    char *conn = nvs_get_blob_a(state->nvs_handle, "conn_str", &connlen);
    if (conn)
        set_conn_string(state, conn, connlen, 0);
#endif

    _aziot_state = state;

}

int azureiothub_publish(const void *msg, unsigned len) {
    srv_t *state = _aziot_state;
    if (state->conn_status != JD_AZURE_IOT_HUB_HEALTH_CONNECTION_STATUS_CONNECTED)
        return -1;
    int qos = 0;
    int retain = 0;
    bool store = true;
    // TODO:
    if (esp_mqtt_client_enqueue(state->client, topic, msg, len, qos, retain, store) < 0)
        return -2;

    feed_watchdog(state);
    LOG("send: >>%s<<", (const char *)msg);

    jd_blink(JD_BLINK_CLOUD_UPLOADED);

    return 0;
}

static int publish_and_free(char *msg) {
    int r = azureiothub_publish(msg, strlen(msg), NULL);
    jd_free(msg);
    return r;
}

int azureiothub_publish_values(const char *label, int numvals, double *vals) {
    uint64_t self = jd_device_id();
    char *msg = jd_sprintf_a("{\"device\":\"%-s\", \"label\":%-s, \"values\":%-s}",
                             jd_to_hex_a(&self, sizeof(self)), jd_json_escape(label),
                             double_array_to_json(numvals, vals));
    return publish_and_free(msg);
}

int azureiothub_publish_bin(const void *data, unsigned datasize) {
    char *hex = jd_to_hex_a(data, datasize);
    return publish_and_free(hex);
}

int azureiothub_is_connected(void) {
    srv_t *state = _aziot_state;
    return state->conn_status == JD_AZURE_IOT_HUB_HEALTH_CONNECTION_STATUS_CONNECTED;
}

int azureiothub_respond_method(uint32_t method_id, uint32_t status, int numvals, double *vals) {
    char *msg = double_array_to_json(numvals, vals);

    nx_azure_iot_hub_client_command_message_response(
                 &nx_context_ptr->iothub_client, status,
                 context_ptr, context_length, 
                 msg, strlen(msg), NX_WAIT_FOREVER)

    char *topic = jd_sprintf_a("$iothub/methods/res/%d/?$rid=%d", status, method_id);

// TODO
    int r = azureiothub_publish(msg, strlen(msg), topic);

    jd_free(msg);
    jd_free(topic);

    return r;
}


// for Cloud Adapter (jacscloud.c):
const jacscloud_api_t azureiothub_cloud = {
    .upload = azureiothub_publish_values,
    .agg_upload = aggbuffer_upload,
    .bin_upload = azureiothub_publish_bin,
    .is_connected = azureiothub_is_connected,
    .max_bin_upload_size = 1024, // just a guess
    .respond_method = azureiothub_respond_method,
};