#ifndef __STM_H
#define __STM_H

// #include "board.h"

#include "stm32l4xx.h"

#include "stm32l4xx_hal_def.h"
#include "stm32l4xx_hal_rcc.h"

#include "stm32l4xx_ll_crc.h"
#include "stm32l4xx_ll_bus.h"
#include "stm32l4xx_ll_gpio.h"
#include "stm32l4xx_ll_exti.h"
#include "stm32l4xx_ll_dma.h"
#include "stm32l4xx_ll_rcc.h"
#include "stm32l4xx_ll_system.h"
#include "stm32l4xx_ll_cortex.h"
#include "stm32l4xx_ll_utils.h"
#include "stm32l4xx_ll_pwr.h"
#include "stm32l4xx_ll_usart.h"
#include "stm32l4xx_ll_tim.h"
#include "stm32l4xx_ll_lptim.h"
#include "stm32l4xx_ll_spi.h"
#include "stm32l4xx_ll_adc.h"
#include "stm32l4xx_ll_rtc.h"
#include "stm32l4xx_ll_i2c.h"
#include "stm32l4xx_ll_dmamux.h"


#define HSI_MHZ 16

// run at 48 MHz for compatibility with F0
#define PLL_MHZ 48

#define LL_EXTI_ClearFallingFlag_0_31 LL_EXTI_ClearFlag_0_31
#define LL_EXTI_IsActiveFallingFlag_0_31 LL_EXTI_IsActiveFlag_0_31

#endif