#ifndef JD_USER_CONFIG_H
#define JD_USER_CONFIG_H

#define DEVICE_DMESG_BUFFER_SIZE 1024

#include "dmesg.h"

#define JD_LOG DMESG
#define JD_WR_OVERHEAD 28

#ifndef NO_JACSCRIPT
#define JD_CLIENT 1
#endif

#define JD_MS_TIMER 1
#define JD_FREE_SUPPORTED 1
#define JD_ADVANCED_STRING 1
#define JD_LSTORE 1

#define PIN_LED 5

#define JD_RAW_FRAME 1

#define JD_FLASH_PAGE_SIZE 1024

#define JD_USB_BRIDGE 1

// probably not so useful on brains...
#define JD_CONFIG_WATCHDOG 0

#define JD_SEND_FRAME_SIZE 1024

void jdaz_wake_main(void);
#define JD_WAKE_MAIN() jdaz_wake_main()

#define JD_SIMPLE_ALLOC 0

#define PIN_JACDAC 0
#define USART_IDX 4
#define UART_PIN PIN_JACDAC
#define UART_PIN_AF 8

#define STM32L4

#endif
