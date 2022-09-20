/* Copyright (c) Microsoft Corporation.
   Licensed under the MIT License. */

#include <stdio.h>

#include "tx_api.h"

#include "board_init.h"
#include "sntp_client.h"
#include "stm_networking.h"

#include "nx_client.h"

#include "azure_config.h"

#include "azjacdac.h"

#define AZURE_THREAD_STACK_SIZE 4096
#define AZURE_THREAD_PRIORITY   4

TX_THREAD azure_thread;
ULONG azure_thread_stack[AZURE_THREAD_STACK_SIZE / sizeof(ULONG)];

#if 0
static void azure_thread_entry(ULONG parameter)
{
    UINT status;

    printf("Starting Azure thread\r\n\r\n");

    // Initialize the network
    if ((status = stm_network_init(WIFI_SSID, WIFI_PASSWORD, WIFI_MODE)))
    {
        printf("ERROR: Failed to initialize the network (0x%08x)\r\n", status);
    }

    else if ((status = azure_iot_nx_client_entry(&nx_ip, &nx_pool, &nx_dns_client, sntp_time)))
    {
        printf("ERROR: Failed to run Azure IoT (0x%04x)\r\n", status);
    }
}
#endif

TX_SEMAPHORE jd_sem;
static void jd_loop(ULONG parameter)
{
    while (1)
    {
        tx_semaphore_get(&jd_sem, 1);
        jd_process_everything();

        if (codalLogStore.ptr)
        {
            fwrite(codalLogStore.buffer, codalLogStore.ptr, 1, stdout);
            codalLogStore.ptr = 0;
        }
    }
}

void jdaz_wake_main(void)
{
    tx_semaphore_put(&jd_sem);
}

void tx_application_define(void* first_unused_memory)
{
#if 0
    // Create Azure thread
    UINT status = tx_thread_create(&azure_thread,
        "Azure Thread",
        azure_thread_entry,
        0,
        azure_thread_stack,
        AZURE_THREAD_STACK_SIZE,
        AZURE_THREAD_PRIORITY,
        AZURE_THREAD_PRIORITY,
        TX_NO_TIME_SLICE,
        TX_AUTO_START);
#endif

    UINT status = tx_thread_create(&azure_thread,
        "Jacdac Thread",
        jd_loop,
        0,
        azure_thread_stack,
        AZURE_THREAD_STACK_SIZE,
        AZURE_THREAD_PRIORITY,
        AZURE_THREAD_PRIORITY,
        TX_NO_TIME_SLICE,
        TX_AUTO_START);

    if (status != TX_SUCCESS)
    {
        printf("ERROR: Azure IoT thread creation failed\r\n");
    }
}

uint32_t now;

void init_jacscript_manager(void);

void app_init_services(void)
{
#ifdef PIN_PWR_EN
    if (board_infos[board_type].flags & BOARD_FLAG_PWR_ACTIVE_HI)
        pwr_cfg.en_active_high = 1;
    power_init(&pwr_cfg);
#endif
#ifndef NO_JACSCRIPT
    jd_role_manager_init();
    init_jacscript_manager();
#endif
    init_sensors();

#if 0
    wifi_init();
    azureiothub_init();
    jacscloud_init(&azureiothub_cloud);
#ifndef NO_JACSCRIPT
    tsagg_init(&azureiothub_cloud);
#endif
#endif
}

uint8_t cpu_mhz;

int main(void)
{
    // Initialize the board
    board_init();

    LL_RCC_HSI_Enable();
    while (!LL_RCC_HSI_IsReady())
        ;

  

    printf("starting!\n");

    DMESG("starting jacscript-esp32 %s", app_fw_version);

    cpu_mhz = 80;

    tx_semaphore_create(&jd_sem, "jdsem", 1);

    tim_init();
    uart_init_();

    jd_rx_init();
    jd_tx_init();
    jd_init();

    usb_init();

    // Enter the ThreadX kernel
    tx_kernel_enter();

    return 0;
}

const char app_fw_version[] = "v0.0.0";
