#include "jdstm.h"
#include "main.h"

#include "usbd_cdc.h"
#include "usbd_cdc_if.h"
#include "usbd_desc.h"

void USB_HAL_GPIO_EXTI_Callback();
USBD_HandleTypeDef USBD_Device;

void usb_init()
{
    DMESG("staring USB");

    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWREx_EnableVddUSB();

    // exti_set_callback(13, USB_HAL_GPIO_EXTI_Callback, EXTI_RISING);

    USBD_Init(&USBD_Device, &VCP_Desc, 0);
    USBD_RegisterClass(&USBD_Device, USBD_CDC_CLASS);
    USBD_CDC_RegisterInterface(&USBD_Device, &USBD_CDC_JDUSB_fops);

    USBD_Start(&USBD_Device);
}
