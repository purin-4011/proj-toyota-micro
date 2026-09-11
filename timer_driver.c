/******************************************************************************
 * @file    timer_driver.c
 * @brief   Implementation ของ Timer driver (register-level, raw address)
 ******************************************************************************/
#include "timer_driver.h"
#include "stm32f411xe.h"   /* ต้องใช้เฉพาะ IRQn_Type enum + NVIC_EnableIRQ()
                             * ตามที่สไลด์ 0500_Interrupts.pdf ระบุว่า "ต้องใช้
                             * CMSIS" สำหรับตั้งค่า NVIC โดยเฉพาะ (peripheral
                             * register อื่นด้านล่างยังคง raw address ตามเดิม) */

#define REG32(addr)  (*(volatile uint32_t *)(addr))

/* --- TIM2 / TIM3 base address (RM0383 Section 2.3, Memory Map) --- */
#define TIM2_BASE              (0x40000000UL)
#define TIM3_BASE               (0x40000400UL)

/* --- Register offset ภายใน timer (เหมือนกันทั้ง TIM2/TIM3) --- */
#define TIM_OFFSET_CR1          (0x00UL)
#define TIM_OFFSET_DIER         (0x0CUL)
#define TIM_OFFSET_SR           (0x10UL)
#define TIM_OFFSET_CNT          (0x24UL)
#define TIM_OFFSET_PSC          (0x28UL)
#define TIM_OFFSET_ARR          (0x2CUL)

#define TIM_CR1_CEN_BIT         (0U)
#define TIM_DIER_UIE_BIT        (0U)
#define TIM_SR_UIF_BIT          (0U)

/* --- RCC (Section 6.3.10, APB1ENR) --- */
#define RCC_BASE                (0x40023800UL)
#define RCC_OFFSET_APB1ENR      (0x40UL)
#define RCC_APB1ENR_TIM2EN_BIT  (0U)
#define RCC_APB1ENR_TIM3EN_BIT  (1U)

/* PSC คำนวณให้ counter clock = 1 kHz (1 tick = 1ms)
 * TIMCLK / (PSC + 1) = 1000  ->  PSC = (TIMCLK / 1000) - 1 */
#define TIMER_DRIVER_PSC_FOR_1KHZ  ((uint32_t)((TIMER_DRIVER_TIMCLK_HZ / 1000UL) - 1UL))
#define TIMER_DRIVER_TIM3_ARR      ((uint32_t)(TIMER_DRIVER_TICK_MS - 1U))

static Timer_TickCallback_t volatile s_tim3_tick_callback = (Timer_TickCallback_t) 0;

void Timer_Driver_TIM2_Init(void)
{
    /* 1) เปิด clock ให้ TIM2 */
    REG32(RCC_BASE + RCC_OFFSET_APB1ENR) |= (1UL << RCC_APB1ENR_TIM2EN_BIT);

    /* 2) ตั้ง prescaler ให้ counter clock = 1kHz */
    REG32(TIM2_BASE + TIM_OFFSET_PSC) = TIMER_DRIVER_PSC_FOR_1KHZ;

    /* 3) ตั้ง Auto-Reload สูงสุด (TIM2 เป็น 32-bit timer) เพื่อให้เป็น
     *    free-running แทบไม่ overflow (ที่ 1ms/tick ~= 49.7 วันถึง wrap) */
    REG32(TIM2_BASE + TIM_OFFSET_ARR) = 0xFFFFFFFFUL;

    /* 4) reset counter แล้วเริ่มนับ (ไม่เปิด interrupt — เป็นแค่นาฬิกากลาง) */
    REG32(TIM2_BASE + TIM_OFFSET_CNT) = 0UL;
    REG32(TIM2_BASE + TIM_OFFSET_CR1) |= (1UL << TIM_CR1_CEN_BIT);
}

uint32_t Timer_Driver_TIM2_GetTick(void)
{
    return REG32(TIM2_BASE + TIM_OFFSET_CNT);
}

void Timer_Driver_TIM3_Init(Timer_TickCallback_t const callback)
{
    if (callback != (Timer_TickCallback_t) 0)
    {
        s_tim3_tick_callback = callback;

        /* 1) เปิด clock ให้ TIM3 */
        REG32(RCC_BASE + RCC_OFFSET_APB1ENR) |= (1UL << RCC_APB1ENR_TIM3EN_BIT);

        /* 2) ตั้ง prescaler เหมือน TIM2 (counter clock = 1kHz) */
        REG32(TIM3_BASE + TIM_OFFSET_PSC) = TIMER_DRIVER_PSC_FOR_1KHZ;

        /* 3) ตั้ง ARR ให้ overflow ทุก 100ms ตาม design */
        REG32(TIM3_BASE + TIM_OFFSET_ARR) = TIMER_DRIVER_TIM3_ARR;
        REG32(TIM3_BASE + TIM_OFFSET_CNT) = 0UL;

        /* 4) เปิด Update Interrupt Enable */
        REG32(TIM3_BASE + TIM_OFFSET_DIER) |= (1UL << TIM_DIER_UIE_BIT);

        /* 5) ตั้ง priority = 2 (ต่ำกว่า EXTI4 ตามที่ตกลงไว้ในสถาปัตยกรรม)
         *    แล้วเปิด NVIC ให้ TIM3_IRQn ผ่าน CMSIS function ตามที่สไลด์สอน */
        NVIC_SetPriority(TIM3_IRQn, 2U);
        NVIC_EnableIRQ(TIM3_IRQn);

        /* 6) เริ่มนับ */
        REG32(TIM3_BASE + TIM_OFFSET_CR1) |= (1UL << TIM_CR1_CEN_BIT);
    }
    else
    {
        /* MISRA: else บังคับ - callback เป็น NULL จะไม่ init อะไรเลย */
    }
}

/**
 * @brief  ISR ของ TIM3 — ทำงานทุก 100ms ตาม design
 *         ชื่อฟังก์ชันต้องตรงกับที่ startup file กำหนดไว้เป็น weak symbol
 */
void TIM3_IRQHandler(void)
{
    if ((REG32(TIM3_BASE + TIM_OFFSET_SR) & (1UL << TIM_SR_UIF_BIT)) != 0U)
    {
        /* Clear update interrupt flag (เขียน 0 เพื่อ clear ตาม datasheet) */
        REG32(TIM3_BASE + TIM_OFFSET_SR) &= ~(1UL << TIM_SR_UIF_BIT);

        if (s_tim3_tick_callback != (Timer_TickCallback_t) 0)
        {
            s_tim3_tick_callback();
        }
        else
        {
            /* ไม่มี callback ลงทะเบียนไว้ - ไม่ทำอะไร */
        }
    }
    else
    {
        /* flag อื่นที่ไม่ใช่ update interrupt - ไม่ควรเกิด แต่ handle ไว้กัน MISRA */
    }
}
