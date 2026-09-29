/******************************************************************************
 * @file    adc_driver.c
 * @brief   Implementation ของ ADC driver (register-level, raw address)
 *          สำหรับ ADC1 Channel 4 (PA4 - potentiometer)
 ******************************************************************************/
#include "adc_driver.h"
#include "stm32f411xe.h"   /* เฉพาะ IRQn_Type enum + NVIC_EnableIRQ()/SetPriority()
                             * ตามที่สไลด์ 0500_Interrupts.pdf บังคับให้ใช้ CMSIS
                             * สำหรับ NVIC โดยเฉพาะ (peripheral register อื่น
                             * ด้านล่างยังคง raw address ตามที่สอนใน 0100_GPIO.pdf) */

#define REG32(addr)  (*(volatile uint32_t *)(addr))

/* --- GPIOA (PA4 = ADC1_IN4) --- */
#define GPIOA_BASE               (0x40020000UL)
#define GPIOA_OFFSET_MODER       (0x00UL)
#define ADC_POT_PIN               (4U)   /* PA4 */

/* --- RCC --- */
#define RCC_BASE                  (0x40023800UL)
#define RCC_OFFSET_AHB1ENR        (0x30UL)
#define RCC_OFFSET_APB2ENR        (0x44UL)
#define RCC_AHB1ENR_GPIOAEN_BIT   (0U)
#define RCC_APB2ENR_ADC1EN_BIT    (8U)

/* --- ADC1 (RM0383 Section 11.12) --- */
#define ADC1_BASE                 (0x40012000UL)
#define ADC_OFFSET_SR             (0x00UL)
#define ADC_OFFSET_CR1            (0x04UL)
#define ADC_OFFSET_CR2            (0x08UL)
#define ADC_OFFSET_SMPR2          (0x10UL)
#define ADC_OFFSET_HTR            (0x24UL)
#define ADC_OFFSET_LTR            (0x28UL)
#define ADC_OFFSET_SQR3           (0x34UL)
#define ADC_OFFSET_DR             (0x4CUL)

#define ADC_SR_AWD_BIT            (0U)
#define ADC_SR_EOC_BIT            (1U)

#define ADC_CR1_AWDCH_SHIFT       (0U)
#define ADC_CR1_EOCIE_BIT         (5U)
#define ADC_CR1_AWDIE_BIT         (6U)
#define ADC_CR1_AWDSGL_BIT        (9U)
#define ADC_CR1_AWDEN_BIT         (23U)

#define ADC_CR2_ADON_BIT          (0U)
#define ADC_CR2_CONT_BIT          (1U)
#define ADC_CR2_SWSTART_BIT       (30U)

#define ADC_POT_CHANNEL           (4U)   /* ADC1_IN4 = PA4 */
#define ADC_SMPR2_CH4_SHIFT       (12U)  /* channel 4 field อยู่ที่ bit [14:12] */
/* 111 = 480 cycles (ค่าช้าสุด) — เดิมใช้ 28 cycles ทำให้ EOC interrupt เกิด
 * ถี่ถึง ~200,000 ครั้ง/วินาที จน CPU วนอยู่ใน ADC ISR เกือบตลอด และ
 * UART (priority เท่ากัน) แทบไม่ได้ทำงาน -> ตัวอักษรออกช้า
 * potentiometer หมุนด้วยมือ ไม่จำเป็นต้องอ่านเร็วขนาดนั้น */
#define ADC_SAMPLE_TIME_480CYC    (0x7U)

/* --- ADC Common register (ใช้ร่วมกันทุก ADC) — ตั้ง prescaler ของ ADC clock --- */
#define ADC_COMMON_BASE           (0x40012300UL)
#define ADC_COMMON_OFFSET_CCR     (0x04UL)
#define ADC_CCR_ADCPRE_SHIFT      (16U)
#define ADC_CCR_ADCPRE_DIV8       (0x3U)  /* 11 = PCLK2/8 = 16MHz/8 = 2MHz */

/** ตำแหน่งใน Vector Table ของ ADC_IRQHandler (ADC1/2/3 ใช้ร่วมกัน, RM0383 Table 38) */
#define ADC_IRQN                   (18U)

static ADC_EocCallback_t volatile s_eoc_callback = (ADC_EocCallback_t) 0;
static ADC_WatchdogCallback_t volatile s_watchdog_callback = (ADC_WatchdogCallback_t) 0;
static uint16_t volatile s_latest_value = 0U;

void ADC_Driver_Init(ADC_EocCallback_t const eoc_callback)
{
    uint32_t moder_val;
    uint32_t smpr2_val;
    volatile uint32_t stab_delay;

    if (eoc_callback != (ADC_EocCallback_t) 0)
    {
        s_eoc_callback = eoc_callback;

        /* 1) เปิด clock GPIOA และ ADC1 */
        REG32(RCC_BASE + RCC_OFFSET_AHB1ENR) |= (1UL << RCC_AHB1ENR_GPIOAEN_BIT);
        REG32(RCC_BASE + RCC_OFFSET_APB2ENR) |= (1UL << RCC_APB2ENR_ADC1EN_BIT);

        /* 2) ตั้ง PA4 เป็นโหมด Analog (11) ตามที่ ADC ต้องการ */
        moder_val = REG32(GPIOA_BASE + GPIOA_OFFSET_MODER);
        moder_val |= (0x3UL << (ADC_POT_PIN * 2U));   /* 11 = Analog mode */
        REG32(GPIOA_BASE + GPIOA_OFFSET_MODER) = moder_val;

        /* 3) ตั้ง sample time ของ channel 4 */
        smpr2_val = REG32(ADC1_BASE + ADC_OFFSET_SMPR2);
        smpr2_val &= ~(0x7UL << ADC_SMPR2_CH4_SHIFT);
        smpr2_val |= (ADC_SAMPLE_TIME_480CYC << ADC_SMPR2_CH4_SHIFT);
        REG32(ADC1_BASE + ADC_OFFSET_SMPR2) = smpr2_val;

        /* 3.1) ลด ADC clock เป็น PCLK2/8 = 2 MHz (ค่า reset คือ /2 = 8 MHz)
         *      เวลาแปลง 1 ครั้ง = (480 + 12) cycles / 2 MHz = 246 us
         *      -> EOC interrupt ~4,000 ครั้ง/วินาที (ลดลง ~50 เท่า) ยังเร็ว
         *      พอให้ 7-segment และ Analog Watchdog ตอบสนองทันทีในสายตาคน */
        REG32(ADC_COMMON_BASE + ADC_COMMON_OFFSET_CCR) =
            (REG32(ADC_COMMON_BASE + ADC_COMMON_OFFSET_CCR) & ~(0x3UL << ADC_CCR_ADCPRE_SHIFT))
            | (ADC_CCR_ADCPRE_DIV8 << ADC_CCR_ADCPRE_SHIFT);

        /* 4) ตั้งลำดับการแปลงค่าให้มีแค่ channel เดียว (SQR3.SQ1 = channel 4,
         *    SQR1.L เป็น 0000 อยู่แล้วตั้งแต่ reset = 1 conversion พอดี) */
        REG32(ADC1_BASE + ADC_OFFSET_SQR3) = (uint32_t) ADC_POT_CHANNEL;

        /* 5) ตั้ง AWDCH ไว้ล่วงหน้าเป็น channel เดียวกัน (ใช้ตอนเปิด
         *    watchdog ทีหลัง) และเปิด EOC interrupt (ยังไม่เปิด AWD ตอนนี้) */
        REG32(ADC1_BASE + ADC_OFFSET_CR1) = (ADC_POT_CHANNEL << ADC_CR1_AWDCH_SHIFT)
                                           | (1UL << ADC_CR1_EOCIE_BIT)
                                           | (1UL << ADC_CR1_AWDSGL_BIT);

        /* 6) เปิด ADC (ADON) ก่อน แล้วหน่วงเวลาสั้นๆ ให้ ADC เสถียร
         *    (ตาม datasheet ต้องการเวลา stabilization ก่อนเริ่มแปลงค่า) */
        REG32(ADC1_BASE + ADC_OFFSET_CR2) |= (1UL << ADC_CR2_ADON_BIT);

        for (stab_delay = 0UL; stab_delay < 1000UL; stab_delay++)
        {
            /* หน่วงเวลาเปล่าๆ รอ ADC เสถียร (ไม่ใช่ polling เพราะไม่ได้
             * เช็คเงื่อนไขจาก flag ใดๆ แค่หน่วงเวลาคงที่สั้นๆ ครั้งเดียว
             * ตอน init เท่านั้น ไม่เกี่ยวกับ data acquisition) */
        }

        /* 7) เปิด continuous conversion mode แล้วสั่งเริ่มแปลงค่าด้วย
         *    software trigger (SWSTART) */
        REG32(ADC1_BASE + ADC_OFFSET_CR2) |= (1UL << ADC_CR2_CONT_BIT);
        REG32(ADC1_BASE + ADC_OFFSET_CR2) |= (1UL << ADC_CR2_SWSTART_BIT);

        /* 8) ตั้ง priority = 3 (เท่ากับ UART เพราะไม่ time-critical เท่า
         *    ปุ่ม/countdown) แล้วเปิด NVIC ให้ ADC_IRQn ผ่าน CMSIS function */
        /* priority = 4 (ต่ำสุดในระบบ) ให้ต่ำกว่า UART (3) เพื่อให้ UART
         * ขัดจังหวะ ADC ได้เสมอ ข้อความจึงไม่ถูก ADC แย่งเวลา */
        NVIC_SetPriority(ADC_IRQn, 4U);
        NVIC_EnableIRQ(ADC_IRQn);
    }
    else
    {
        /* MISRA: else บังคับ — callback เป็น NULL จะไม่ init อะไรเลย */
    }
}

uint16_t ADC_Driver_GetLatestValue(void)
{
    return s_latest_value;
}

void ADC_Driver_EnableWatchdog(uint16_t const low, uint16_t const high,
                                ADC_WatchdogCallback_t const callback)
{
    if (callback != (ADC_WatchdogCallback_t) 0)
    {
        s_watchdog_callback = callback;

        /* ตั้งขอบเขตช่วงที่ยอมรับได้ (12-bit) */
        REG32(ADC1_BASE + ADC_OFFSET_LTR) = (uint32_t) low;
        REG32(ADC1_BASE + ADC_OFFSET_HTR) = (uint32_t) high;

        /* เปิด AWDEN (เฝ้าดู regular channel) และ AWDIE (สร้าง interrupt) */
        REG32(ADC1_BASE + ADC_OFFSET_CR1) |= (1UL << ADC_CR1_AWDEN_BIT)
                                            | (1UL << ADC_CR1_AWDIE_BIT);
    }
    else
    {
        /* MISRA: else บังคับ — callback เป็น NULL จะไม่เปิด watchdog */
    }
}

void ADC_Driver_DisableWatchdog(void)
{
    REG32(ADC1_BASE + ADC_OFFSET_CR1) &= ~((1UL << ADC_CR1_AWDEN_BIT)
                                          | (1UL << ADC_CR1_AWDIE_BIT));
    s_watchdog_callback = (ADC_WatchdogCallback_t) 0;
}

/**
 * @brief  ISR ของ ADC1/2/3 (ใช้ vector เดียวกันทั้ง 3 ตัว แต่โปรเจคนี้ใช้
 *         แค่ ADC1) จัดการทั้ง EOC (แปลงค่าเสร็จ) และ AWD (ค่าหลุดช่วง)
 */
void ADC_IRQHandler(void)
{
    uint32_t const sr_val = REG32(ADC1_BASE + ADC_OFFSET_SR);

    /* --- EOC: แปลงค่าเสร็จ 1 รอบ --- */
    if ((sr_val & (1UL << ADC_SR_EOC_BIT)) != 0U)
    {
        /* อ่าน DR จะ clear EOC ให้อัตโนมัติตาม datasheet */
        s_latest_value = (uint16_t) (REG32(ADC1_BASE + ADC_OFFSET_DR) & 0xFFFUL);

        if (s_eoc_callback != (ADC_EocCallback_t) 0)
        {
            s_eoc_callback(s_latest_value);
        }
        else
        {
            /* ไม่มี callback ลงทะเบียนไว้ - ไม่ทำอะไร */
        }
    }
    else
    {
        /* ไม่ใช่ EOC - ไม่ทำอะไรในส่วนนี้ */
    }

    /* --- AWD: ค่าออกนอกช่วงที่กำหนดไว้ --- */
    if ((sr_val & (1UL << ADC_SR_AWD_BIT)) != 0U)
    {
        /* Clear AWD flag (เขียน 0 เพื่อ clear ตาม datasheet) */
        REG32(ADC1_BASE + ADC_OFFSET_SR) &= ~(1UL << ADC_SR_AWD_BIT);

        if (s_watchdog_callback != (ADC_WatchdogCallback_t) 0)
        {
            s_watchdog_callback();
        }
        else
        {
            /* ไม่มี callback ลงทะเบียนไว้ - ไม่ทำอะไร */
        }
    }
    else
    {
        /* ไม่ใช่ AWD - ไม่ทำอะไรในส่วนนี้ */
    }
}
