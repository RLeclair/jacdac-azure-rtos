#include "azjacdac.h"

const char app_dev_class_name[] = "B-L475E-IOT01A Jacscript";
#define DEV_CLASS 0x38e4f122

uint32_t app_get_device_class(void) {
    return DEV_CLASS;
}
