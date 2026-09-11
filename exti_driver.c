/******************************************************************************
 * @file    exti_driver.c
 * @brief   Implementation ของ EXTI driver (register-level, raw address)
 *          สำหรับปุ่ม PB4 (EXTI Line 4)
 ******************************************************************************/
#include "exti_driver.h"
#include "stm32f411xe.h"   /* ต้องใช้เฉพาะ IRQn_Type enum + NVIC_EnableIRQ()
                             * ตามที่สไลด์ 0500_Interrupts.pdf ระบุชัดว่า
                             * "ต้องใช้ CMSIS" สำหรับตั้งค่า NVIC โดยเฉพาะ
                             * (peripheral register อื่นด้านล่างยังคง define
                             *  แบบ raw address ตามที่สอนใน 0100_GPIO.pdf) */

#define REG32(addr)  (*(volatile uint32_t *)(addr))

/* --- GPIOB (อ้างอิง RM0383 Section 6.4) --- */
#define GPIOB_BASE               (0x40020400UL)
#define GPIOB_OFFSET_MODER       (0x00UL)
#define GPIOB_OFFSET_PUPDR       (0x0CUL)
#define GPIOB_OFFSET_IDR         (0x10UL)

/* --- RCC (Section 6.3) --- */
#define RCC_BASE                 (0x40023800UL)
#define RCC_OFFSET_AHB1ENR       (0x30UL)
#define RCC_OFFSET_APB2ENR       (0x44UL)
#define RCC_AHB1ENR_GPIOBEN_BIT  (1U)
#define RCC_APB2ENR_SYSCFGEN_BIT (14U)

/* --- SYSCFG (Section 8.2) : ผูก EXTI line เข้ากับ GPIO port --- */
#define SYSCFG_BASE              (0x40013800UL)
#define SYSCFG_OFFSET_EXTICR2    (0x0CUL)   /* EXTICR2 ครอบคลุม EXTI4-EXTI7 */
#define SYSCFG_EXTICR2_PORTB_SEL (0x1UL)    /* nibble แรกของ EXTICR2 = EXTI4, 0001=PortB */

/* --- EXTI (Section 12.3) --- */
#define EXTI_BASE                (0x40013C00UL)
#define EXTI_OFFSET_IMR          (0x00UL)
#define EXTI_OFFSET_RTSR         (0x08UL)
#define EXTI_OFFSET_FTSR         (0x0CUL)
#define EXTI_OFFSET_PR           (0x14UL)

#define EXTI_LINE4_BIT             (4U)

static EXTI_Callback_t volatile s_event_callback = (EXTI_Callback_t) 0;

void EXTI_Driver_Init(EXTI_Callback_t const callback)
{
    uint32_t moder_val;
    uint32_t pupdr_val;
    uint32_t exticr_val;

    if (callback != (EXTI_Callback_t) 0)
    {
        s_event_callback = callback;

        /* 1) เปิด clock GPIOB และ SYSCFG */
        REG32(RCC_BASE + RCC_OFFSET_AHB1ENR) |= (1UL << RCC_AHB1ENR_GPIOBEN_BIT);
        REG32(RCC_BASE + RCC_OFFSET_APB2ENR) |= (1UL << RCC_APB2ENR_SYSCFGEN_BIT);

        /* 2) ตั้ง PB4 เป็น input (00) + pull-up (01)
         *    ปุ่มบน Training Shield ต่อแบบ active-low: กด = 0V, ปล่อย = 3.3V */
        moder_val = REG32(GPIOB_BASE + GPIOB_OFFSET_MODER);
        moder_val &= ~(0x3UL << (EXTI_LINE4_BIT * 2U));
        REG32(GPIOB_BASE + GPIOB_OFFSET_MODER) = moder_val;

        pupdr_val = REG32(GPIOB_BASE + GPIOB_OFFSET_PUPDR);
        pupdr_val &= ~(0x3UL << (EXTI_LINE4_BIT * 2U));
        pupdr_val |= (0x1UL << (EXTI_LINE4_BIT * 2U));
        REG32(GPIOB_BASE + GPIOB_OFFSET_PUPDR) = pupdr_val;

        /* 3) ผูก EXTI Line 4 เข้ากับ Port B ผ่าน SYSCFG_EXTICR2 */
        exticr_val = REG32(SYSCFG_BASE + SYSCFG_OFFSET_EXTICR2);
        exticr_val &= ~(0xFUL << 0U);
        exticr_val |= (SYSCFG_EXTICR2_PORTB_SEL << 0U);
        REG32(SYSCFG_BASE + SYSCFG_OFFSET_EXTICR2) = exticr_val;

        /* 4) เปิดทั้ง rising และ falling trigger เพื่อจับทั้งกดและปล่อย */
        REG32(EXTI_BASE + EXTI_OFFSET_RTSR) |= (1UL << EXTI_LINE4_BIT);
        REG32(EXTI_BASE + EXTI_OFFSET_FTSR) |= (1UL << EXTI_LINE4_BIT);

        /* 5) unmask ให้ EXTI line 4 สร้าง interrupt ได้ */
        REG32(EXTI_BASE + EXTI_OFFSET_IMR) |= (1UL << EXTI_LINE4_BIT);

        /* 6) ตั้ง priority = 1 (สูง เพราะพลาดจับปุ่มเสียหายกว่า periodic tick อื่น)
         *    แล้วเปิด NVIC ให้ EXTI4_IRQn ผ่าน CMSIS function ตามที่สไลด์สอน
         *    (NVIC_SetPriority เป็น CMSIS function มาตรฐานเช่นกัน แม้สไลด์จะ
         *     ไม่ได้ยกตัวอย่างไว้ตรงๆ แต่ใช้หลักการเดียวกับ NVIC_EnableIRQ) */
        NVIC_SetPriority(EXTI4_IRQn, 1U);
        NVIC_EnableIRQ(EXTI4_IRQn);
    }
    else
    {
        /* MISRA: else บังคับ — callback เป็น NULL จะไม่ init อะไรเลย */
    }
}

/**
 * @brief  ISR ของ EXTI Line 4 — ชื่อฟังก์ชันนี้ต้องตรงกับที่ startup file
 *         (startup_stm32f411xetx.s ที่ CubeIDE gen ให้อัตโนมัติ) กำหนดไว้
 *         เป็น weak symbol ของตำแหน่ง EXTI4 ใน vector table
 */
void EXTI4_IRQHandler(void)
{
    uint32_t idr_val;

    if ((REG32(EXTI_BASE + EXTI_OFFSET_PR) & (1UL << EXTI_LINE4_BIT)) != 0U)
    {
        /* Clear pending bit ก่อน (เขียน 1 เพื่อ clear ตาม datasheet) */
        REG32(EXTI_BASE + EXTI_OFFSET_PR) = (1UL << EXTI_LINE4_BIT);

        if (s_event_callback != (EXTI_Callback_t) 0)
        {
            idr_val = REG32(GPIOB_BASE + GPIOB_OFFSET_IDR);

            if ((idr_val & (1UL << EXTI_LINE4_BIT)) == 0U)
            {
                /* อ่านได้ 0 = เพิ่งถูกกดลง */
                s_event_callback(EXTI_EDGE_RISING);
            }
            else
            {
                /* อ่านได้ 1 = เพิ่งถูกปล่อย */
                s_event_callback(EXTI_EDGE_FALLING);
            }
        }
        else
        {
            /* ไม่มี callback ลงทะเบียนไว้ - ไม่ทำอะไร (ป้องกัน crash) */
        }
    }
    else
    {
        /* Pending bit ไม่ตรงกับ line ที่รอ - ไม่ควรเกิดขึ้น แต่ handle ไว้กัน MISRA */
    }
}
