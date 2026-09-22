/******************************************************************************
 * @file    exti_driver.c
 * @brief   Implementation ของ EXTI driver (register-level, raw address)
 *          สำหรับปุ่มทั้ง 4 ตัว — แต่ละฟังก์ชัน Init เขียนแบบคัดลอกจากของ
 *          เดิม (PB4/EXTI4) ที่ทดสอบผ่านแล้ว แล้วปรับ port/pin/IRQ ตามปุ่ม
 *          ใหม่ทีละตัว (ไม่ใช้ generic function ตัวเดียวรองรับทุกขา)
 ******************************************************************************/
#include "exti_driver.h"
#include "stm32f411xe.h"   /* ต้องใช้เฉพาะ IRQn_Type enum + NVIC_EnableIRQ()
                             * ตามที่สไลด์ 0500_Interrupts.pdf ระบุชัดว่า
                             * "ต้องใช้ CMSIS" สำหรับตั้งค่า NVIC โดยเฉพาะ
                             * (peripheral register อื่นด้านล่างยังคง define
                             *  แบบ raw address ตามที่สอนใน 0100_GPIO.pdf) */

#define REG32(addr)  (*(volatile uint32_t *)(addr))

/* --- GPIOA/GPIOB (อ้างอิง RM0383 Section 6.4) --- */
#define GPIOA_BASE               (0x40020000UL)
#define GPIOB_BASE               (0x40020400UL)
#define GPIO_OFFSET_MODER        (0x00UL)
#define GPIO_OFFSET_PUPDR        (0x0CUL)
#define GPIO_OFFSET_IDR          (0x10UL)

/* --- RCC (Section 6.3) --- */
#define RCC_BASE                 (0x40023800UL)
#define RCC_OFFSET_AHB1ENR       (0x30UL)
#define RCC_OFFSET_APB2ENR       (0x44UL)
#define RCC_AHB1ENR_GPIOAEN_BIT  (0U)
#define RCC_AHB1ENR_GPIOBEN_BIT  (1U)
#define RCC_APB2ENR_SYSCFGEN_BIT (14U)

/* --- SYSCFG (Section 8.2) : ผูก EXTI line เข้ากับ GPIO port --- */
#define SYSCFG_BASE              (0x40013800UL)
#define SYSCFG_OFFSET_EXTICR1    (0x08UL)   /* EXTICR1 ครอบคลุม EXTI0-EXTI3 */
#define SYSCFG_OFFSET_EXTICR2    (0x0CUL)   /* EXTICR2 ครอบคลุม EXTI4-EXTI7 */
#define SYSCFG_OFFSET_EXTICR3    (0x10UL)   /* EXTICR3 ครอบคลุม EXTI8-EXTI11 */
#define SYSCFG_EXTICR_PORTA_SEL  (0x0UL)
#define SYSCFG_EXTICR_PORTB_SEL  (0x1UL)

/* --- EXTI (Section 12.3) --- */
#define EXTI_BASE                (0x40013C00UL)
#define EXTI_OFFSET_IMR          (0x00UL)
#define EXTI_OFFSET_RTSR         (0x08UL)
#define EXTI_OFFSET_FTSR         (0x0CUL)
#define EXTI_OFFSET_PR           (0x14UL)

/* --- หมายเลข EXTI line ของแต่ละปุ่ม --- */
#define EXTI_LINE_PB4_BIT         (4U)    /* ปุ่มกรอกรหัสหลัก */
#define EXTI_LINE_PB5_BIT         (5U)    /* ปุ่ม Setup Mode */
#define EXTI_LINE_PA10_BIT        (10U)   /* ปุ่มเพิ่มจำนวนหลัก */
#define EXTI_LINE_PB3_BIT         (3U)    /* ปุ่มลดจำนวนหลัก */

static EXTI_Callback_t volatile s_pb4_callback  = (EXTI_Callback_t) 0;
static EXTI_Callback_t volatile s_pb5_callback  = (EXTI_Callback_t) 0;
static EXTI_Callback_t volatile s_pa10_callback = (EXTI_Callback_t) 0;
static EXTI_Callback_t volatile s_pb3_callback  = (EXTI_Callback_t) 0;

/* ========================================================================
 * ปุ่มกรอกรหัสหลัก - PB4 / EXTI Line 4 (เหมือนเวอร์ชันที่ทดสอบผ่านแล้ว
 * ทุกประการ ไม่มีการแก้ไขแม้แต่บรรทัดเดียว)
 * ======================================================================== */
void EXTI_Driver_Init(EXTI_Callback_t const callback)
{
    uint32_t moder_val;
    uint32_t pupdr_val;
    uint32_t exticr_val;

    if (callback != (EXTI_Callback_t) 0)
    {
        s_pb4_callback = callback;

        /* 1) เปิด clock GPIOB และ SYSCFG */
        REG32(RCC_BASE + RCC_OFFSET_AHB1ENR) |= (1UL << RCC_AHB1ENR_GPIOBEN_BIT);
        REG32(RCC_BASE + RCC_OFFSET_APB2ENR) |= (1UL << RCC_APB2ENR_SYSCFGEN_BIT);

        /* 2) ตั้ง PB4 เป็น input (00) + pull-up (01)
         *    ปุ่มบน Training Shield ต่อแบบ active-low: กด = 0V, ปล่อย = 3.3V */
        moder_val = REG32(GPIOB_BASE + GPIO_OFFSET_MODER);
        moder_val &= ~(0x3UL << (EXTI_LINE_PB4_BIT * 2U));
        REG32(GPIOB_BASE + GPIO_OFFSET_MODER) = moder_val;

        pupdr_val = REG32(GPIOB_BASE + GPIO_OFFSET_PUPDR);
        pupdr_val &= ~(0x3UL << (EXTI_LINE_PB4_BIT * 2U));
        pupdr_val |= (0x1UL << (EXTI_LINE_PB4_BIT * 2U));
        REG32(GPIOB_BASE + GPIO_OFFSET_PUPDR) = pupdr_val;

        /* 3) ผูก EXTI Line 4 เข้ากับ Port B ผ่าน SYSCFG_EXTICR2 (nibble แรก) */
        exticr_val = REG32(SYSCFG_BASE + SYSCFG_OFFSET_EXTICR2);
        exticr_val &= ~(0xFUL << 0U);
        exticr_val |= (SYSCFG_EXTICR_PORTB_SEL << 0U);
        REG32(SYSCFG_BASE + SYSCFG_OFFSET_EXTICR2) = exticr_val;

        /* 4) เปิดทั้ง rising และ falling trigger เพื่อจับทั้งกดและปล่อย */
        REG32(EXTI_BASE + EXTI_OFFSET_RTSR) |= (1UL << EXTI_LINE_PB4_BIT);
        REG32(EXTI_BASE + EXTI_OFFSET_FTSR) |= (1UL << EXTI_LINE_PB4_BIT);

        /* 5) unmask ให้ EXTI line 4 สร้าง interrupt ได้ */
        REG32(EXTI_BASE + EXTI_OFFSET_IMR) |= (1UL << EXTI_LINE_PB4_BIT);

        /* 6) ตั้ง priority = 1 แล้วเปิด NVIC ให้ EXTI4_IRQn ผ่าน CMSIS function */
        NVIC_SetPriority(EXTI4_IRQn, 1U);
        NVIC_EnableIRQ(EXTI4_IRQn);
    }
    else
    {
        /* MISRA: else บังคับ — callback เป็น NULL จะไม่ init อะไรเลย */
    }
}

void EXTI4_IRQHandler(void)
{
    uint32_t idr_val;

    if ((REG32(EXTI_BASE + EXTI_OFFSET_PR) & (1UL << EXTI_LINE_PB4_BIT)) != 0U)
    {
        REG32(EXTI_BASE + EXTI_OFFSET_PR) = (1UL << EXTI_LINE_PB4_BIT);

        if (s_pb4_callback != (EXTI_Callback_t) 0)
        {
            idr_val = REG32(GPIOB_BASE + GPIO_OFFSET_IDR);

            if ((idr_val & (1UL << EXTI_LINE_PB4_BIT)) == 0U)
            {
                s_pb4_callback(EXTI_EDGE_RISING);   /* อ่านได้ 0 = เพิ่งถูกกดลง */
            }
            else
            {
                s_pb4_callback(EXTI_EDGE_FALLING);  /* อ่านได้ 1 = เพิ่งถูกปล่อย */
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

/* ========================================================================
 * ปุ่ม Setup Mode - PB5 / EXTI Line 5 (คัดลอกจาก EXTI_Driver_Init ด้านบน
 * แล้วปรับ pin เป็น 5 และ IRQn เป็น EXTI9_5_IRQn เพราะ line 5-9 ใช้ vector
 * ร่วมกัน — เนื่องจากมีแค่ปุ่มนี้ตัวเดียวที่ใช้ range นี้ จึงตรวจ pending
 * bit ของ line 5 เท่านั้นพอ ไม่ต้องวนเช็คทั้ง 5-9)
 * ======================================================================== */
void EXTI_Driver_InitSetupButton(EXTI_Callback_t const callback)
{
    uint32_t moder_val;
    uint32_t pupdr_val;
    uint32_t exticr_val;

    if (callback != (EXTI_Callback_t) 0)
    {
        s_pb5_callback = callback;

        REG32(RCC_BASE + RCC_OFFSET_AHB1ENR) |= (1UL << RCC_AHB1ENR_GPIOBEN_BIT);
        REG32(RCC_BASE + RCC_OFFSET_APB2ENR) |= (1UL << RCC_APB2ENR_SYSCFGEN_BIT);

        moder_val = REG32(GPIOB_BASE + GPIO_OFFSET_MODER);
        moder_val &= ~(0x3UL << (EXTI_LINE_PB5_BIT * 2U));
        REG32(GPIOB_BASE + GPIO_OFFSET_MODER) = moder_val;

        pupdr_val = REG32(GPIOB_BASE + GPIO_OFFSET_PUPDR);
        pupdr_val &= ~(0x3UL << (EXTI_LINE_PB5_BIT * 2U));
        pupdr_val |= (0x1UL << (EXTI_LINE_PB5_BIT * 2U));
        REG32(GPIOB_BASE + GPIO_OFFSET_PUPDR) = pupdr_val;

        /* Line 5 อยู่ nibble ที่ 2 ของ EXTICR2 (bit 4:7) -> shift 4 */
        exticr_val = REG32(SYSCFG_BASE + SYSCFG_OFFSET_EXTICR2);
        exticr_val &= ~(0xFUL << 4U);
        exticr_val |= (SYSCFG_EXTICR_PORTB_SEL << 4U);
        REG32(SYSCFG_BASE + SYSCFG_OFFSET_EXTICR2) = exticr_val;

        REG32(EXTI_BASE + EXTI_OFFSET_RTSR) |= (1UL << EXTI_LINE_PB5_BIT);
        REG32(EXTI_BASE + EXTI_OFFSET_FTSR) |= (1UL << EXTI_LINE_PB5_BIT);
        REG32(EXTI_BASE + EXTI_OFFSET_IMR) |= (1UL << EXTI_LINE_PB5_BIT);

        NVIC_SetPriority(EXTI9_5_IRQn, 1U);
        NVIC_EnableIRQ(EXTI9_5_IRQn);
    }
    else
    {
        /* MISRA: else บังคับ — callback เป็น NULL จะไม่ init อะไรเลย */
    }
}

void EXTI9_5_IRQHandler(void)
{
    uint32_t idr_val;

    if ((REG32(EXTI_BASE + EXTI_OFFSET_PR) & (1UL << EXTI_LINE_PB5_BIT)) != 0U)
    {
        REG32(EXTI_BASE + EXTI_OFFSET_PR) = (1UL << EXTI_LINE_PB5_BIT);

        if (s_pb5_callback != (EXTI_Callback_t) 0)
        {
            idr_val = REG32(GPIOB_BASE + GPIO_OFFSET_IDR);

            if ((idr_val & (1UL << EXTI_LINE_PB5_BIT)) == 0U)
            {
                s_pb5_callback(EXTI_EDGE_RISING);
            }
            else
            {
                s_pb5_callback(EXTI_EDGE_FALLING);
            }
        }
        else
        {
            /* ไม่มี callback ลงทะเบียนไว้ - ไม่ทำอะไร */
        }
    }
    else
    {
        /* ไม่ใช่ line 5 (โปรเจคนี้ใช้แค่ line 5 ใน vector นี้ -
         * ไม่ควรเกิดจาก line อื่น แต่ handle ไว้กัน MISRA) */
    }
}

/* ========================================================================
 * ปุ่มเพิ่มจำนวนหลัก - PA10 / EXTI Line 10 (คัดลอกจาก EXTI_Driver_Init
 * ปรับเป็น GPIOA, pin 10, IRQn เป็น EXTI15_10_IRQn)
 * ======================================================================== */
void EXTI_Driver_InitDigitUpButton(EXTI_Callback_t const callback)
{
    uint32_t moder_val;
    uint32_t pupdr_val;
    uint32_t exticr_val;

    if (callback != (EXTI_Callback_t) 0)
    {
        s_pa10_callback = callback;

        REG32(RCC_BASE + RCC_OFFSET_AHB1ENR) |= (1UL << RCC_AHB1ENR_GPIOAEN_BIT);
        REG32(RCC_BASE + RCC_OFFSET_APB2ENR) |= (1UL << RCC_APB2ENR_SYSCFGEN_BIT);

        moder_val = REG32(GPIOA_BASE + GPIO_OFFSET_MODER);
        moder_val &= ~(0x3UL << (EXTI_LINE_PA10_BIT * 2U));
        REG32(GPIOA_BASE + GPIO_OFFSET_MODER) = moder_val;

        pupdr_val = REG32(GPIOA_BASE + GPIO_OFFSET_PUPDR);
        pupdr_val &= ~(0x3UL << (EXTI_LINE_PA10_BIT * 2U));
        pupdr_val |= (0x1UL << (EXTI_LINE_PA10_BIT * 2U));
        REG32(GPIOA_BASE + GPIO_OFFSET_PUPDR) = pupdr_val;

        /* Line 10 อยู่ nibble ที่ 3 ของ EXTICR3 (bit 8:11) -> shift 8 */
        exticr_val = REG32(SYSCFG_BASE + SYSCFG_OFFSET_EXTICR3);
        exticr_val &= ~(0xFUL << 8U);
        exticr_val |= (SYSCFG_EXTICR_PORTA_SEL << 8U);
        REG32(SYSCFG_BASE + SYSCFG_OFFSET_EXTICR3) = exticr_val;

        REG32(EXTI_BASE + EXTI_OFFSET_RTSR) |= (1UL << EXTI_LINE_PA10_BIT);
        REG32(EXTI_BASE + EXTI_OFFSET_FTSR) |= (1UL << EXTI_LINE_PA10_BIT);
        REG32(EXTI_BASE + EXTI_OFFSET_IMR) |= (1UL << EXTI_LINE_PA10_BIT);

        NVIC_SetPriority(EXTI15_10_IRQn, 1U);
        NVIC_EnableIRQ(EXTI15_10_IRQn);
    }
    else
    {
        /* MISRA: else บังคับ — callback เป็น NULL จะไม่ init อะไรเลย */
    }
}

void EXTI15_10_IRQHandler(void)
{
    uint32_t idr_val;

    if ((REG32(EXTI_BASE + EXTI_OFFSET_PR) & (1UL << EXTI_LINE_PA10_BIT)) != 0U)
    {
        REG32(EXTI_BASE + EXTI_OFFSET_PR) = (1UL << EXTI_LINE_PA10_BIT);

        if (s_pa10_callback != (EXTI_Callback_t) 0)
        {
            idr_val = REG32(GPIOA_BASE + GPIO_OFFSET_IDR);

            if ((idr_val & (1UL << EXTI_LINE_PA10_BIT)) == 0U)
            {
                s_pa10_callback(EXTI_EDGE_RISING);
            }
            else
            {
                s_pa10_callback(EXTI_EDGE_FALLING);
            }
        }
        else
        {
            /* ไม่มี callback ลงทะเบียนไว้ - ไม่ทำอะไร */
        }
    }
    else
    {
        /* ไม่ใช่ line 10 (โปรเจคนี้ใช้แค่ line 10 ใน vector นี้ -
         * ไม่ควรเกิดจาก line อื่น แต่ handle ไว้กัน MISRA) */
    }
}

/* ========================================================================
 * ปุ่มลดจำนวนหลัก - PB3 / EXTI Line 3 (คัดลอกจาก EXTI_Driver_Init
 * ปรับเป็น pin 3, IRQn เป็น EXTI3_IRQn - เป็น dedicated vector ของตัวเอง)
 * ======================================================================== */
void EXTI_Driver_InitDigitDownButton(EXTI_Callback_t const callback)
{
    uint32_t moder_val;
    uint32_t pupdr_val;
    uint32_t exticr_val;

    if (callback != (EXTI_Callback_t) 0)
    {
        s_pb3_callback = callback;

        REG32(RCC_BASE + RCC_OFFSET_AHB1ENR) |= (1UL << RCC_AHB1ENR_GPIOBEN_BIT);
        REG32(RCC_BASE + RCC_OFFSET_APB2ENR) |= (1UL << RCC_APB2ENR_SYSCFGEN_BIT);

        moder_val = REG32(GPIOB_BASE + GPIO_OFFSET_MODER);
        moder_val &= ~(0x3UL << (EXTI_LINE_PB3_BIT * 2U));
        REG32(GPIOB_BASE + GPIO_OFFSET_MODER) = moder_val;

        pupdr_val = REG32(GPIOB_BASE + GPIO_OFFSET_PUPDR);
        pupdr_val &= ~(0x3UL << (EXTI_LINE_PB3_BIT * 2U));
        pupdr_val |= (0x1UL << (EXTI_LINE_PB3_BIT * 2U));
        REG32(GPIOB_BASE + GPIO_OFFSET_PUPDR) = pupdr_val;

        /* Line 3 อยู่ nibble ที่ 4 ของ EXTICR1 (bit 12:15) -> shift 12 */
        exticr_val = REG32(SYSCFG_BASE + SYSCFG_OFFSET_EXTICR1);
        exticr_val &= ~(0xFUL << 12U);
        exticr_val |= (SYSCFG_EXTICR_PORTB_SEL << 12U);
        REG32(SYSCFG_BASE + SYSCFG_OFFSET_EXTICR1) = exticr_val;

        REG32(EXTI_BASE + EXTI_OFFSET_RTSR) |= (1UL << EXTI_LINE_PB3_BIT);
        REG32(EXTI_BASE + EXTI_OFFSET_FTSR) |= (1UL << EXTI_LINE_PB3_BIT);
        REG32(EXTI_BASE + EXTI_OFFSET_IMR) |= (1UL << EXTI_LINE_PB3_BIT);

        NVIC_SetPriority(EXTI3_IRQn, 1U);
        NVIC_EnableIRQ(EXTI3_IRQn);
    }
    else
    {
        /* MISRA: else บังคับ — callback เป็น NULL จะไม่ init อะไรเลย */
    }
}

void EXTI3_IRQHandler(void)
{
    uint32_t idr_val;

    if ((REG32(EXTI_BASE + EXTI_OFFSET_PR) & (1UL << EXTI_LINE_PB3_BIT)) != 0U)
    {
        REG32(EXTI_BASE + EXTI_OFFSET_PR) = (1UL << EXTI_LINE_PB3_BIT);

        if (s_pb3_callback != (EXTI_Callback_t) 0)
        {
            idr_val = REG32(GPIOB_BASE + GPIO_OFFSET_IDR);

            if ((idr_val & (1UL << EXTI_LINE_PB3_BIT)) == 0U)
            {
                s_pb3_callback(EXTI_EDGE_RISING);
            }
            else
            {
                s_pb3_callback(EXTI_EDGE_FALLING);
            }
        }
        else
        {
            /* ไม่มี callback ลงทะเบียนไว้ - ไม่ทำอะไร */
        }
    }
    else
    {
        /* Pending bit ไม่ตรงกับ line ที่รอ - ไม่ควรเกิดขึ้น แต่ handle ไว้กัน MISRA */
    }
}
