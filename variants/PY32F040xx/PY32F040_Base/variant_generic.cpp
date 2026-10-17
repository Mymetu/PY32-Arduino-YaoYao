/*
 *******************************************************************************
 * Copyright (c) 2023, AirM2M
 * All rights reserved.
 *
 * This software component is licensed by ST under BSD 3-Clause license,
 * the "License"; You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                        opensource.org/licenses/BSD-3-Clause
 *
 *******************************************************************************
 */
#if defined(PY32F040x6) || defined(PY32F040x8) || defined(PY32F040x9) || defined(PY32F040xB) || \
    defined(PY32F040Cx6) || defined(PY32F040Cx8) || defined(PY32F040Cx9) || defined(PY32F040CxB)
#include "pins_arduino.h"

const PinName digitalPin[] = {
    PA_0,  PA_1,  PA_2,  PA_3,  PA_4,  PA_5,  PA_6,  PA_7,
    PA_8,  PA_9,  PA_10, PA_11, PA_12, PA_13, PA_14, PA_15,
    PB_0,  PB_1,  PB_2,  PB_3,  PB_4,  PB_5,  PB_6,  PB_7,
    PB_8,  PB_9,  PB_10, PB_11, PB_12, PB_13, PB_14, PB_15,
    PF_0,  PF_1,  PF_2,  PF_5,  PF_6,  PF_8,  PF_9,
    PC_13, PC_14, PC_15,
};

const uint32_t analogInputPin[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 16, 17
};

/*
 * PY32F040: USART3 / USART4 的 RCC 时钟使能宏在 py32f0xx HAL 里并不存在，
 * 导致 Serial3/Serial4 的 begin() 对寄存器写入全部丢失（实测 CR1/BRR 始终为 0，
 * 而且 uart_init 里补的分支也没能让时钟生效）。
 * 在 main() 之前直接把这两个口的时钟打开、并把复位释放掉，
 * 保证 Serial3/Serial4 一调用 begin() 就能正常配置。
 * （__attribute__((constructor)) 在 __libc_init_array 阶段执行，早于 main/setup）
 */
extern "C" void py32f040_uart34_clk_init(void) __attribute__((constructor));
extern "C" void py32f040_uart34_clk_init(void)
{
#if defined(RCC_APBRSTR1_USART3RST)
    RCC->APBRSTR1 &= ~RCC_APBRSTR1_USART3RST;   /* 释放 USART3 复位 */
#endif
#if defined(RCC_APBRSTR1_USART4RST)
    RCC->APBRSTR1 &= ~RCC_APBRSTR1_USART4RST;   /* 释放 USART4 复位 */
#endif
#if defined(RCC_APBENR1_USART3EN)
    RCC->APBENR1 |= RCC_APBENR1_USART3EN;       /* 打开 USART3 时钟 */
#endif
#if defined(RCC_APBENR1_USART4EN)
    RCC->APBENR1 |= RCC_APBENR1_USART4EN;       /* 打开 USART4 时钟 */
#endif
}

#endif
