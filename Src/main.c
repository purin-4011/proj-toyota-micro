/******************************************************************************
 * @file    main.c
 * @brief   Digital Combination Lock - Bring-up test (Week 1-2 scope)
 *          เขียนแบบ Register-level (bare-metal) ล้วนๆ ตามที่สอนในคอร์ส
 *          ไม่ใช้ HAL/LL และไม่พึ่งพา CMSIS struct ใดๆ
 *
 *          ผลที่ควรเห็นเมื่อทดสอบจริงบนบอร์ด:
 *            - กดปุ่มสั้น (< 500ms) แล้วปล่อย -> LED เขียว (PA5) ติดชั่วครู่
 *            - กดปุ่มยาว (>= 500ms) แล้วปล่อย -> LED แดง (PA6) ติดชั่วครู่
 *            - LED ดับเองภายใน ~300ms หลังติด (ควบคุมผ่าน TIM3 tick,
 *              ไม่มี delay/blocking ในโค้ดเลยสักบรรทัด)
 *
 *          ยังไม่มี: state machine, code storage, CRC, ADC, UART, 7-segment
 *          (ตาม Timeline สัปดาห์ 3-4 ในแผนที่วางไว้)
 ******************************************************************************/
#include "gpio_driver.h"
#include "exti_driver.h"
#include "timer_driver.h"
#include "code_decoder.h"
#include "app_config.h"

/* ตัวแปรที่ถูกแตะทั้งจาก ISR (EXTI4, TIM3) และ main loop ต้องเป็น volatile */
static uint32_t volatile s_press_start_tick = 0U;
static uint16_t volatile s_led_hold_ticks_remaining = 0U;

#define MAIN_LED_HOLD_TICKS   (3U)   /* 3 tick x 100ms = 300ms */

static void Main_ExtiEventHandler(EXTI_Edge_t edge);
static void Main_Tim3TickHandler(void);
static void Main_HardwareInit(void);

int main(void)
{
    Main_HardwareInit();

    /* main loop ว่างเปล่าโดยตั้งใจ - งานทั้งหมดขับเคลื่อนด้วย interrupt
     * (EXTI4 สำหรับปุ่ม, TIM3 สำหรับ periodic tick) ตามเกณฑ์ "ห้าม Polling" */
    for (;;)
    {
        /* Week 3-4: จุดนี้จะเรียก lock_fsm state machine เมื่อพัฒนาเสร็จ */
    }
}

static void Main_HardwareInit(void)
{
    /* --- LED outputs --- */
    GPIO_Driver_EnableClock(APP_LED_GREEN_PORT);
    GPIO_Driver_Init(APP_LED_GREEN_PORT, APP_LED_GREEN_PIN, GPIO_MODE_OUTPUT, GPIO_PULL_NONE);
    GPIO_Driver_WritePin(APP_LED_GREEN_PORT, APP_LED_GREEN_PIN, GPIO_PIN_RESET);

    GPIO_Driver_EnableClock(APP_LED_RED_PORT);
    GPIO_Driver_Init(APP_LED_RED_PORT, APP_LED_RED_PIN, GPIO_MODE_OUTPUT, GPIO_PULL_NONE);
    GPIO_Driver_WritePin(APP_LED_RED_PORT, APP_LED_RED_PIN, GPIO_PIN_RESET);

    /* --- Timing subsystem (Two-Tier Architecture) --- */
    Timer_Driver_TIM2_Init();
    Timer_Driver_TIM3_Init(Main_Tim3TickHandler);

    /* --- Button input (EXTI4 บน PB4) --- */
    EXTI_Driver_Init(Main_ExtiEventHandler);
}

/**
 * @brief  เรียกจาก EXTI4 ISR เมื่อปุ่มถูกกดหรือปล่อย
 *         คำนวณ duration จาก TIM2 tick แล้ว classify ผ่าน code_decoder
 */
static void Main_ExtiEventHandler(EXTI_Edge_t const edge)
{
    if (edge == EXTI_EDGE_RISING)
    {
        s_press_start_tick = Timer_Driver_TIM2_GetTick();
    }
    else /* EXTI_EDGE_FALLING */
    {
        uint32_t const release_tick = Timer_Driver_TIM2_GetTick();
        uint32_t const duration_ms  = release_tick - s_press_start_tick;
        CodeSymbol_t const symbol   = CodeDecoder_Classify(duration_ms);

        if (symbol == CODE_SYMBOL_SHORT)
        {
            GPIO_Driver_WritePin(APP_LED_GREEN_PORT, APP_LED_GREEN_PIN, GPIO_PIN_SET);
            GPIO_Driver_WritePin(APP_LED_RED_PORT, APP_LED_RED_PIN, GPIO_PIN_RESET);
        }
        else
        {
            GPIO_Driver_WritePin(APP_LED_RED_PORT, APP_LED_RED_PIN, GPIO_PIN_SET);
            GPIO_Driver_WritePin(APP_LED_GREEN_PORT, APP_LED_GREEN_PIN, GPIO_PIN_RESET);
        }

        /* ให้ TIM3 tick มาดับ LED ให้เองภายหลัง (ไม่ blocking ใน ISR) */
        s_led_hold_ticks_remaining = (uint16_t) MAIN_LED_HOLD_TICKS;
    }
}

/**
 * @brief  เรียกจาก TIM3 ISR ทุก 100ms — ตอนนี้นับถอยหลังดับ LED เท่านั้น
 *         (Week 3-4: ขยายเป็น mode-switching handler สำหรับ input-timeout
 *          1.5s และ lockout countdown 30s ตามที่ออกแบบไว้)
 */
static void Main_Tim3TickHandler(void)
{
    if (s_led_hold_ticks_remaining > 0U)
    {
        s_led_hold_ticks_remaining--;

        if (s_led_hold_ticks_remaining == 0U)
        {
            GPIO_Driver_WritePin(APP_LED_GREEN_PORT, APP_LED_GREEN_PIN, GPIO_PIN_RESET);
            GPIO_Driver_WritePin(APP_LED_RED_PORT, APP_LED_RED_PIN, GPIO_PIN_RESET);
        }
        else
        {
            /* ยังนับถอยหลังไม่ครบ - ไม่ทำอะไรเพิ่ม */
        }
    }
    else
    {
        /* ไม่มี LED ค้างอยู่ - ไม่ทำอะไร */
    }
}
