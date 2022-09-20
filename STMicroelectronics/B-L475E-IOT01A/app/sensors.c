// Sensors jacdac wrapping for BL475E

#include "jd_drivers.h"
#include "services/jd_services.h"
#include "stm32l475e_iot01_tsensor.h"
#include "stm32l475e_iot01_accelero.h"
#include "stm32l475e_iot01_gyro.h"
#include "stm32l475e_iot01_hsensor.h"
#include "stm32l475e_iot01_psensor.h"
#include <math.h>

static env_reading_t temp_r = {0, 512, -40 * 1024, 125 * 1024};
static env_reading_t humi_r = {0, 3584, 0, 100 * 1024};
static env_reading_t baro_r = {0, 103, 260 * 1024, 1100 * 1024};

void accelerometer_data_transform(int32_t sample[3]) 
{ 
    int32_t v0 = sample[0]; 
    int32_t v1 = sample[1]; 
    int32_t v2 = sample[2]; 
    sample[0]  = -v0;
    sample[1]  = v1;
    sample[2]  = -v2;
}

static void void_sensor_func(void) { }

static void* l475_get_temperature(void)
{
    temp_r.value = (int32_t) round(BSP_TSENSOR_ReadTemp() * 1024);
    return &temp_r;
}

static void* l475_get_humidity(void)
{
    humi_r.value = (int32_t) round(BSP_HSENSOR_ReadHumidity() * 1024);
    return &humi_r;
}

static void* l475_get_barometer(void)
{
    baro_r.value = (int32_t) round(BSP_PSENSOR_ReadPressure() * 1024 * 100);
    return &baro_r;
}


static void* l475_get_accelerometer(void)
{
    int16_t data[3];
    static int32_t sample[3];
    BSP_ACCELERO_AccGetXYZ(data);

    sample[0] = data[0] * 1024;
    sample[1] = data[1] * 1024;
    sample[2] = data[2] * 1024;

    return sample;
}

static void* l475_get_gyroscope(void)
{
    float data[3];
    static int32_t sample[3];
    BSP_GYRO_GetXYZ(data);

    sample[0] = ((int32_t)round(data[0])) * 1024;
    sample[1] = ((int32_t)round(data[1])) * 1024;
    sample[2] = ((int32_t)round(data[2])) * 1024;

    return sample;
}

const env_sensor_api_t temperature_l475 = {
    .init = void_sensor_func, 
    .process = void_sensor_func, 
    .get_reading = l475_get_temperature
};

const env_sensor_api_t l475_humidity = {
    .init = void_sensor_func,
    .process = void_sensor_func,
    .get_reading = l475_get_humidity

};

const env_sensor_api_t l475_barometer = {
    .init = void_sensor_func,
    .process = void_sensor_func,
    .get_reading = l475_get_barometer
};

const accelerometer_api_t l475_accelerometer = {
    .init        = void_sensor_func,
    .get_reading = l475_get_accelerometer,
    .sleep       = void_sensor_func
};

const gyroscope_api_t l475_gyroscope = {
    .init = void_sensor_func, 
    .get_reading = l475_get_gyroscope, 
    .sleep = void_sensor_func
};

void init_sensors(void)
{
    temperature_init(&temperature_l475);
    accelerometer_init(&l475_accelerometer);
    gyroscope_init(&l475_gyroscope);
    humidity_init(&l475_humidity);
    barometer_init(&l475_barometer);
}