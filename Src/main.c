/******************************************************************************
 * @file    main.c
 * @brief   Digital Combination Lock - Core unlock flow (IDLE -> ENTERING ->
 *          UNLOCKED / LOCKED_OUT) เขียนแบบ Register-level (bare-metal)
 *
 *          รหัส default (ตาม proposal): SHORT-SHORT-SHORT-SHORT
 *          ทดสอบ: กดปุ่มสั้น 4 ครั้งติดกัน (แต่ละครั้งเว้นไม่เกิน 1.5s)
 *          แล้วหยุด 1.5 วิ -> ควรเห็น LED เขียวติดค้าง ~10 วิ
 *
 *          พฤติกรรมที่ควรเห็น:
 *            - ทุกครั้งที่กดปุ่ม (ไม่ว่า SHORT/LONG) -> LED เขียวกระพริบสั้นๆ
 *              ตอบรับ (KEY_ACCEPTED) พร้อม 7-segment แสดงจำนวนหลักที่กด
 *              ไปแล้ว (1, 2, 3, ...)
 *            - หยุดกด 1.5 วิ แล้วรหัสถูก -> LED เขียวติดค้าง 10 วิ
 *            - หยุดกด 1.5 วิ แล้วรหัสผิด (ยังไม่ครบ 3 ครั้ง) -> LED แดง
 *              กระพริบสั้นๆ แล้วกลับ IDLE ให้ป้อนใหม่
 *            - ผิดครบ 3 ครั้งติดกัน -> LED แดงติดค้าง 9 วิ (lockout) พร้อม
 *              7-segment นับถอยหลัง 9,8,7,...,0 ระหว่างนี้กดปุ่มอะไรก็ไม่มีผล
 *
 *          ล็อกชั้นที่ 2 (potentiometer dial lock):
 *            - ตอน IDLE, 7-segment โชว์โซนปัจจุบันของ potentiometer (1-9)
 *              ให้ผู้ใช้หมุนหาตำแหน่งได้ โซนเป้าหมาย default = 5
 *            - ถ้า poten อยู่ถูกโซนอยู่แล้วตอนเริ่มกดปุ่มแรก -> เปิด ADC
 *              Analog Watchdog เฝ้าดูไม่ให้หลุดระหว่างกด ถ้าหลุดกลางคัน
 *              (มือไปโดน) -> ยกเลิกทันที ไฟแดงกะพริบเตือน กลับ IDLE
 *              (ไม่นับเป็นรหัสผิด เพราะถือเป็นอุบัติเหตุ)
 *            - ถ้า poten อยู่ผิดโซนตั้งแต่ก่อนกดปุ่มแรก -> ไม่เปิด watchdog
 *              เลย ปล่อยให้กดรหัสจนจบตามปกติ แล้วไปนับเป็น "กรอกผิด 1
 *              ครั้ง" ตอน validate เหมือนกดรหัสปุ่มผิด (ไม่ใช่การตัดจบ
 *              ทันที เพราะไม่ใช่การหลุดระหว่างกด แต่ผิดตั้งแต่ต้น)
 *            - ปลดล็อกสำเร็จได้ก็ต่อเมื่อผ่านทั้ง 2 เงื่อนไข (dial ถูกโซน
 *              + รหัสปุ่มถูก) พร้อมกันตอน validate
 *
 *          ยังไม่มี: โหมดตั้งรหัสใหม่ (Setup mode - รอออกแบบ UI เพิ่ม), CRC
 *          (ตาม Timeline ที่เหลือ)
 ******************************************************************************/
#include "gpio_driver.h"
#include "exti_driver.h"
#include "timer_driver.h"
#include "seven_segment_driver.h"
#include "uart_driver.h"
#include "adc_driver.h"
#include "crc_driver.h"
#include "code_decoder.h"
#include "lock_fsm.h"
#include "admin_command.h"
#include "dial_lock.h"
#include "setup_mode.h"
#include "app_config.h"

/* จำนวน tick (100ms/tick) ที่ต้องการให้ LED กระพริบสั้นๆ ตอบรับการกด/ผิด */
#define MAIN_BLINK_HOLD_TICKS   (3U)   /* 3 x 100ms = 300ms */

/* ความยาวสูงสุดของบรรทัดคำสั่ง admin (คำสั่งที่ยาวที่สุดคือ "LOCKOUT" = 7
 * ตัวอักษร เผื่อไว้ให้พอสำหรับ error/edge case) */
#define MAIN_ADMIN_LINE_MAX_LEN  (16U)

/* ความยาวสูงสุดของข้อความ UART ที่ประกอบขึ้นเองใน Setup Mode (ยาวสุดคือ
 * ข้อความสรุปตอน COMMITTED เช่น "SETUP SAVED: zone=9, 8 digit,
 * short-short-...-long\r\n") */
#define MAIN_UART_MSG_MAX_LEN   (100U)

/* ตัวแปรที่ถูกแตะทั้งจาก ISR และ main loop ต้องเป็น volatile ตาม MISRA-C */
static uint32_t volatile s_press_start_tick = 0U;
static uint32_t volatile s_setup_press_start_tick = 0U;
static uint16_t volatile s_green_blink_ticks_remaining = 0U;
static uint16_t volatile s_red_blink_ticks_remaining = 0U;

/* buffer สะสมบรรทัดคำสั่ง admin ที่รับมาทีละ byte จาก UART RX interrupt */
static char s_admin_line_buffer[MAIN_ADMIN_LINE_MAX_LEN];
static uint8_t s_admin_line_index = 0U;


static void Main_ExtiEventHandler(EXTI_Edge_t edge);
static void Main_ExtiSetupButtonHandler(EXTI_Edge_t edge);
static void Main_ExtiDigitUpHandler(EXTI_Edge_t edge);
static void Main_ExtiDigitDownHandler(EXTI_Edge_t edge);
static void Main_Tim3TickHandler(void);
static void Main_LockNotifyHandler(LockFsm_Notification_t notification);
static void Main_SetupNotifyHandler(SetupMode_Notification_t notification);
static void Main_UartRxHandler(uint8_t received_byte);
static void Main_AdcEocHandler(uint16_t raw_value);
static void Main_AdcWatchdogHandler(void);
static void Main_HardwareInit(void);

static void Main_AppendChar(char * p_buf, uint8_t * p_len, char c);
static void Main_AppendStr(char * p_buf, uint8_t * p_len, char const * p_str);
static void Main_AppendDigit(char * p_buf, uint8_t * p_len, uint8_t digit);

int main(void)
{
    Main_HardwareInit();

    /* main loop ว่างเปล่าโดยตั้งใจ - งานทั้งหมดขับเคลื่อนด้วย interrupt
     * (EXTI4 สำหรับปุ่ม, TIM3 สำหรับ periodic tick) ตามเกณฑ์ "ห้าม Polling" */
    for (;;)
    {
        /* รอออกแบบ UI เพิ่มก่อนค่อยทำโหมดตั้งรหัสใหม่ (Setup mode) */
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

    GPIO_Driver_EnableClock(APP_LED_G_PORT);
    GPIO_Driver_Init(APP_LED_G_PORT, APP_LED_G_PIN, GPIO_MODE_OUTPUT, GPIO_PULL_NONE);
    GPIO_Driver_WritePin(APP_LED_G_PORT, APP_LED_G_PIN, GPIO_PIN_RESET);

    GPIO_Driver_EnableClock(APP_LED_YELLOW_PORT);
    GPIO_Driver_Init(APP_LED_YELLOW_PORT, APP_LED_YELLOW_PIN, GPIO_MODE_OUTPUT, GPIO_PULL_NONE);
    GPIO_Driver_WritePin(APP_LED_YELLOW_PORT, APP_LED_YELLOW_PIN, GPIO_PIN_RESET);

    /* --- Timing subsystem (Two-Tier Architecture) --- */
    Timer_Driver_TIM2_Init();
    Timer_Driver_TIM3_Init(Main_Tim3TickHandler);

    /* --- Button input (EXTI4 บน PB4) --- */
    EXTI_Driver_Init(Main_ExtiEventHandler);

    /* --- ปุ่ม Setup Mode ใหม่ (PB5/PA10/PB3) --- */
    EXTI_Driver_InitSetupButton(Main_ExtiSetupButtonHandler);
    EXTI_Driver_InitDigitUpButton(Main_ExtiDigitUpHandler);
    EXTI_Driver_InitDigitDownButton(Main_ExtiDigitDownHandler);

    /* --- 7-segment (BCD) สำหรับแสดง lockout countdown --- */
    SevenSegment_Driver_Init();

    /* --- UART (USART2) สำหรับ Admin Mode: รับคำสั่งจาก PC ผ่าน ST-Link VCP --- */
    UART_Driver_Init(Main_UartRxHandler);
    UART_Driver_SendString("Digital Combination Lock - Admin console ready\r\n");

    /* --- ADC (potentiometer, PA4) สำหรับล็อกชั้นที่ 2 (dial lock) --- */
    ADC_Driver_Init(Main_AdcEocHandler);

    /* --- CRC (ตรวจสอบความถูกต้องของรหัสที่เก็บไว้ ป้องกัน RAM corruption)
     *     ต้องเปิดก่อน LockFsm_Init() เพราะข้างในจะเรียก CodeStorage_Init()
     *     ซึ่งต้องใช้ CRC hardware คำนวณค่าอ้างอิงทันที --- */
    CRC_Driver_Init();

    /* --- State machine หลักของระบบล็อก --- */
    LockFsm_Init(Main_LockNotifyHandler);

    /* --- Setup Mode (ต้อง init หลัง LockFsm_Init เพราะ Main_SetupNotifyHandler
     *     จะเรียก LockFsm_ForceIdle() ตอนเข้า Setup Mode) --- */
    SetupMode_Init(Main_SetupNotifyHandler);
}

/**
 * @brief  เรียกจาก EXTI4 ISR เมื่อปุ่มถูกกดหรือปล่อย
 *         คำนวณ duration จาก TIM2 tick แล้วส่งต่อให้ lock_fsm หรือ
 *         setup_mode ตัดสินใจ ขึ้นอยู่กับว่าตอนนี้อยู่ใน Setup Mode
 *         ขั้นตอนกรอกรหัสหรือไม่ (main.c ไม่ตัดสินใจ logic เองแล้ว
 *         แค่เป็นสะพานเชื่อม)
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

        if (SetupMode_GetState() == SETUP_STATE_ENTER_CODE)
        {
            /* อยู่ระหว่างกรอกรหัสใหม่ใน Setup Mode - ส่งให้ setup_mode
             * แทน lock_fsm พร้อมกระพริบ LED feedback เหมือนโหมดปกติ */
            SetupMode_OnCodeSymbol(symbol);

            GPIO_Driver_WritePin(APP_LED_GREEN_PORT, APP_LED_GREEN_PIN, GPIO_PIN_SET);
            s_green_blink_ticks_remaining = (uint16_t) MAIN_BLINK_HOLD_TICKS;
        }
        else if (SetupMode_GetState() == SETUP_STATE_INACTIVE)
        {
            LockFsm_OnSymbol(symbol, duration_ms);
        }
        else
        {
            /* SETUP_STATE_SELECT_LENGTH - ปุ่มนี้ไม่มีบทบาทในขั้นนี้ */
        }
    }
}

/**
 * @brief  เรียกจาก EXTI ISR ของปุ่ม Setup Mode (PB5) — วัดระยะเวลากดค้าง
 *         เสมอ แล้วส่งให้ setup_mode ตัดสินใจความหมายตามสถานะปัจจุบัน
 */
static void Main_ExtiSetupButtonHandler(EXTI_Edge_t const edge)
{
    if (edge == EXTI_EDGE_RISING)
    {
        s_setup_press_start_tick = Timer_Driver_TIM2_GetTick();
    }
    else /* EXTI_EDGE_FALLING */
    {
        uint32_t const release_tick = Timer_Driver_TIM2_GetTick();
        uint32_t const duration_ms  = release_tick - s_setup_press_start_tick;

        SetupMode_OnButtonPB5(duration_ms);
    }
}

/**
 * @brief  เรียกจาก EXTI ISR ของปุ่มเพิ่มจำนวนหลัก (PA10) — ทำงานตอนปล่อย
 *         ปุ่ม (EXTI_EDGE_FALLING ตามคอนเวนชันของ exti_driver นี้) เพื่อ
 *         กันการกดค้างแล้วนับซ้ำ
 */
static void Main_ExtiDigitUpHandler(EXTI_Edge_t const edge)
{
    if (edge == EXTI_EDGE_FALLING)
    {
        SetupMode_OnDigitIncrement();
    }
    else
    {
        /* EXTI_EDGE_RISING (เพิ่งกดลง) - ไม่ทำอะไร */
    }
}

/**
 * @brief  เรียกจาก EXTI ISR ของปุ่มลดจำนวนหลัก (PB3) — หลักการเดียวกับ
 *         Main_ExtiDigitUpHandler
 */
static void Main_ExtiDigitDownHandler(EXTI_Edge_t const edge)
{
    if (edge == EXTI_EDGE_FALLING)
    {
        SetupMode_OnDigitDecrement();
    }
    else
    {
        /* EXTI_EDGE_RISING (เพิ่งกดลง) - ไม่ทำอะไร */
    }
}

/**
 * @brief  เรียกจาก TIM3 ISR ทุก 100ms — ทำ 2 หน้าที่:
 *         1) นับถอยหลัง LED กระพริบสั้นๆ (blink feedback) แล้วดับเมื่อครบ
 *         2) ขับเคลื่อน timeout ต่างๆ ภายใน lock_fsm (input timeout,
 *            unlock display hold, lockout countdown)
 */
static void Main_Tim3TickHandler(void)
{
    if (s_green_blink_ticks_remaining > 0U)
    {
        s_green_blink_ticks_remaining--;

        if ((s_green_blink_ticks_remaining == 0U) && (LockFsm_GetState() != LOCK_STATE_UNLOCKED))
        {
            /* ดับไฟกระพริบ เว้นแต่กำลังอยู่ในสถานะ UNLOCKED ที่ต้องการให้
             * ไฟเขียวค้างไว้ต่อ (lock_fsm เป็นคนสั่งดับเองผ่าน
             * LOCK_NOTIFY_RETURN_TO_IDLE ตอนครบ 10 วิ) */
            GPIO_Driver_WritePin(APP_LED_GREEN_PORT, APP_LED_GREEN_PIN, GPIO_PIN_RESET);
        }
        else
        {
            /* ยังนับไม่ครบ หรือกำลังอยู่ในสถานะที่ต้องการให้ไฟค้าง */
        }
    }
    else
    {
        /* ไม่มีไฟกระพริบค้างอยู่ - ไม่ทำอะไร */
    }

    if (s_red_blink_ticks_remaining > 0U)
    {
        s_red_blink_ticks_remaining--;

        if ((s_red_blink_ticks_remaining == 0U) && (LockFsm_GetState() != LOCK_STATE_LOCKED_OUT))
        {
            GPIO_Driver_WritePin(APP_LED_RED_PORT, APP_LED_RED_PIN, GPIO_PIN_RESET);
        }
        else
        {
            /* ยังนับไม่ครบ หรือกำลังอยู่ในสถานะ LOCKED_OUT ที่ต้องการให้ไฟค้าง */
        }
    }
    else
    {
        /* ไม่มีไฟกระพริบค้างอยู่ - ไม่ทำอะไร */
    }

    LockFsm_OnTick();

    /* อัพเดตจอ 7-segment แสดง lockout countdown ทุก tick ที่อยู่ในสถานะนี้
     * (ยังไม่ทำ "blank" ตอนไม่ใช่ lockout เพราะ BCD driver IC ไม่มีขา
     * Blanking Input ต่อไว้ - ดู TODO ใน seven_segment_driver.h) */
    if (LockFsm_GetState() == LOCK_STATE_LOCKED_OUT)
    {
        SevenSegment_Driver_ShowDigit((uint8_t) LockFsm_GetLockoutSecondsRemaining());
    }
    else
    {
        /* ไม่อยู่ในสถานะ lockout - ไม่ต้องอัพเดตจอ */
    }
}

/**
 * @brief  เรียกจาก lock_fsm เมื่อมีเหตุการณ์เกิดขึ้น (ผ่าน callback ที่
 *         ลงทะเบียนไว้ตอน LockFsm_Init) - main.c เป็นคนตัดสินใจว่าจะ
 *         แสดงผลยังไง (ตอนนี้คือ LED เท่านั้น อนาคตจะเพิ่ม 7-segment)
 */
static void Main_LockNotifyHandler(LockFsm_Notification_t const notification)
{
    switch (notification)
    {
        case LOCK_NOTIFY_KEY_ACCEPTED:
            GPIO_Driver_WritePin(APP_LED_GREEN_PORT, APP_LED_GREEN_PIN, GPIO_PIN_SET);
            s_green_blink_ticks_remaining = (uint16_t) MAIN_BLINK_HOLD_TICKS;
            /* แสดงจำนวนหลักที่ป้อนไปแล้วบน 7-segment (1, 2, 3, ...) */
            SevenSegment_Driver_ShowDigit(LockFsm_GetEntryCount());

            /* ถ้านี่คือสัญลักษณ์แรกของรอบป้อนรหัสใหม่ (entry count เพิ่ง
             * กลายเป็น 1) และ potentiometer อยู่ถูกโซนเป้าหมายอยู่แล้ว
             * ตอนนี้พอดี -> เปิด Analog Watchdog เฝ้าดูไม่ให้หลุดโซน
             * ระหว่างกด (ป้องกันอุบัติเหตุมือไปโดน)
             *
             * ถ้า poten อยู่ผิดโซนอยู่แล้วตั้งแต่ก่อนกดปุ่มแรก -> "ไม่เปิด"
             * watchdog เลย เพราะไม่ใช่การ "หลุด" ระหว่างกด แต่ผิดตั้งแต่ต้น
             * ปล่อยให้ป้อนรหัสจนจบตามปกติ แล้วไปนับเป็น "กรอกผิด 1 ครั้ง"
             * ตอน validate เหมือนกดรหัสปุ่มผิดปกติ (ตามที่ต้องการ) */
            if (LockFsm_GetEntryCount() == 1U)
            {
                if (DialLock_IsAtTargetZone())
                {
                    uint16_t dial_low;
                    uint16_t dial_high;

                    DialLock_GetTargetZoneBounds(&dial_low, &dial_high);
                    ADC_Driver_EnableWatchdog(dial_low, dial_high, Main_AdcWatchdogHandler);
                }
                else
                {
                    /* poten ผิดโซนตั้งแต่ก่อนกดปุ่มแรก - ไม่เปิด watchdog
                     * รอบนี้ จะไปเจอผลตอน validate แทน */
                }
            }
            else
            {
                /* ไม่ใช่ตัวแรก - ถ้า watchdog เปิดอยู่แล้วก็เปิดต่อ ถ้าไม่ได้
                 * เปิดไว้ (เพราะผิดโซนตั้งแต่ต้น) ก็ยังคงไม่เปิด */
            }
            break;

        case LOCK_NOTIFY_UNLOCK_SUCCESS:
            GPIO_Driver_WritePin(APP_LED_G_PORT, APP_LED_G_PIN, GPIO_PIN_SET);
            GPIO_Driver_WritePin(APP_LED_RED_PORT, APP_LED_RED_PIN, GPIO_PIN_RESET);
            ADC_Driver_DisableWatchdog();
            /* ไม่ตั้ง blink counter - ไฟติดค้างจนกว่า lock_fsm จะสั่ง
             * RETURN_TO_IDLE เองตอนครบ 10 วิ (ดูฟังก์ชัน Main_Tim3TickHandler) */
            break;

        case LOCK_NOTIFY_UNLOCK_FAIL:
            GPIO_Driver_WritePin(APP_LED_RED_PORT, APP_LED_RED_PIN, GPIO_PIN_SET);
            s_red_blink_ticks_remaining = (uint16_t) MAIN_BLINK_HOLD_TICKS;
            ADC_Driver_DisableWatchdog();
            /* ป้อนรหัสรอบนี้จบแล้ว (ไม่ว่าจะถูกหรือผิด) -> ล้างตัวเลขนับหลัก
             * กลับเป็น 0 เตรียมรอรอบถัดไป */
            SevenSegment_Driver_ShowDigit(0U);
            break;

        case LOCK_NOTIFY_LOCKOUT_ENTER:
            GPIO_Driver_WritePin(APP_LED_RED_PORT, APP_LED_RED_PIN, GPIO_PIN_SET);
            GPIO_Driver_WritePin(APP_LED_GREEN_PORT, APP_LED_GREEN_PIN, GPIO_PIN_RESET);
            ADC_Driver_DisableWatchdog();
            SevenSegment_Driver_ShowDigit((uint8_t) APP_LOCKOUT_DURATION_SEC);
            break;

        case LOCK_NOTIFY_RETURN_TO_IDLE:
            GPIO_Driver_WritePin(APP_LED_G_PORT, APP_LED_G_PIN, GPIO_PIN_RESET);
            GPIO_Driver_WritePin(APP_LED_RED_PORT, APP_LED_RED_PIN, GPIO_PIN_RESET);
            ADC_Driver_DisableWatchdog();
            /* กลับสู่สถานะปกติแล้ว (ไม่ว่าจะจบจาก UNLOCKED หรือ LOCKED_OUT)
             * -> ล้างจอ 7-segment กลับเป็น 0 เตรียมรอบถัดไป (จอจะกลับไป
             * แสดงโซน potentiometer เองอัตโนมัติผ่าน Main_AdcEocHandler
             * ตั้งแต่ sample ถัดไปเพราะตอนนี้ state กลับเป็น IDLE แล้ว) */
            SevenSegment_Driver_ShowDigit(0U);
            break;

        case LOCK_NOTIFY_DIAL_VIOLATION:
            /* potentiometer หลุดโซนเป้าหมายระหว่างกำลังป้อนรหัส - ยกเลิก
             * รอบนี้ทันที เตือนด้วย LED แดงกะพริบสั้นๆ (ไม่ติดค้างเหมือน
             * lockout เพราะไม่ใช่ความผิดที่นับโทษ) */
            ADC_Driver_DisableWatchdog();
            GPIO_Driver_WritePin(APP_LED_RED_PORT, APP_LED_RED_PIN, GPIO_PIN_SET);
            s_red_blink_ticks_remaining = (uint16_t) MAIN_BLINK_HOLD_TICKS;
            SevenSegment_Driver_ShowDigit(0U);
            break;

        default:
            /* MISRA: switch ต้องมี default case เสมอ แม้ enum จะครบทุกค่าแล้ว */
            break;
    }
}

/**
 * @brief  เรียกจาก setup_mode เมื่อมีเหตุการณ์เกิดขึ้น (ผ่าน callback ที่
 *         ลงทะเบียนไว้ตอน SetupMode_Init) — main.c เป็นคนตัดสินใจว่าจะ
 *         แสดงผลยังไง (ไฟเหลือง + ข้อความ UART) ตามสเปค 6 ข้อที่ตกลงกัน
 */
static void Main_SetupNotifyHandler(SetupMode_Notification_t const notification)
{
    char msg_buf[MAIN_UART_MSG_MAX_LEN];
    uint8_t msg_len = 0U;

    switch (notification)
    {
        case SETUP_NOTIFY_ENTERED:
            /* เข้า Setup Mode: ยกเลิกการกรอกรหัสปกติที่ค้างอยู่แบบปลอดภัย
             * (ไม่แตะรหัสเดิม/ตัวนับกรอกผิด) แล้วเปิดไฟเหลืองค้างไว้ */
            LockFsm_ForceIdle();
            GPIO_Driver_WritePin(APP_LED_YELLOW_PORT, APP_LED_YELLOW_PIN, GPIO_PIN_SET);
            UART_Driver_SendString("\r\nSETUP MODE\r\n");
            break;

        case SETUP_NOTIFY_DIGIT_COUNT_CHANGED:
            Main_AppendStr(msg_buf, &msg_len, "Digits: ");
            Main_AppendDigit(msg_buf, &msg_len, SetupMode_GetTargetDigitCount());
            Main_AppendStr(msg_buf, &msg_len, "\r\n");
            UART_Driver_SendString(msg_buf);
            break;

        case SETUP_NOTIFY_CODE_START:
            UART_Driver_SendString("Enter code (short/long)\r\n");
            break;

        case SETUP_NOTIFY_CODE_SYMBOL:
        {
            uint8_t const entered_count = SetupMode_GetEnteredCount();
            CodeSymbol_t const last_symbol = SetupMode_GetEnteredSymbol((uint8_t) (entered_count - 1U));

            Main_AppendStr(msg_buf, &msg_len, "Key ");
            Main_AppendDigit(msg_buf, &msg_len, entered_count);
            Main_AppendStr(msg_buf, &msg_len, ": ");
            Main_AppendStr(msg_buf, &msg_len, (last_symbol == CODE_SYMBOL_LONG) ? "long" : "short");
            Main_AppendStr(msg_buf, &msg_len, "\r\n");
            UART_Driver_SendString(msg_buf);
            break;
        }

        case SETUP_NOTIFY_WARNING_NO_LENGTH:
            UART_Driver_SendString("WARNING: set digit count first\r\n");
            break;

        case SETUP_NOTIFY_WARNING_INCOMPLETE_CODE:
            UART_Driver_SendString("WARNING: code not complete yet\r\n");
            break;

        case SETUP_NOTIFY_COMMITTED:
        {
            uint8_t const digit_count = SetupMode_GetTargetDigitCount();
            uint8_t i;

            Main_AppendStr(msg_buf, &msg_len, "SETUP SAVED: zone=");
            Main_AppendDigit(msg_buf, &msg_len, SetupMode_GetLastCommittedZone());
            Main_AppendStr(msg_buf, &msg_len, ", ");
            Main_AppendDigit(msg_buf, &msg_len, digit_count);
            Main_AppendStr(msg_buf, &msg_len, " digit, ");

            for (i = 0U; i < digit_count; i++)
            {
                if (i > 0U)
                {
                    Main_AppendChar(msg_buf, &msg_len, '-');
                }
                else
                {
                    /* ตัวแรก - ไม่ต้องใส่ขีดคั่น */
                }

                Main_AppendStr(msg_buf, &msg_len,
                    (SetupMode_GetEnteredSymbol(i) == CODE_SYMBOL_LONG) ? "long" : "short");
            }
            Main_AppendStr(msg_buf, &msg_len, "\r\n");
            UART_Driver_SendString(msg_buf);

            GPIO_Driver_WritePin(APP_LED_YELLOW_PORT, APP_LED_YELLOW_PIN, GPIO_PIN_RESET);
            break;
        }

        default:
            /* MISRA: default บังคับ - ไม่ควรเกิด */
            break;
    }
}

/* ------------------------------------------------------------------------
 * ตัวช่วยประกอบข้อความ UART (เฉพาะตัวเลขหลักเดียว 0-9 พอ เพราะทุกค่า
 * dynamic ที่ต้องแสดงใน Setup Mode เป็นเลขหลักเดียวทั้งหมด) — จำเป็นต้อง
 * ประกอบให้เสร็จใน buffer เดียวก่อนค่อยเรียก UART_Driver_SendString()
 * เพราะฟังก์ชันนั้นไม่มี message queue (เรียกซ้ำก่อนส่งจบจะถูกทิ้ง)
 * ------------------------------------------------------------------------ */
static void Main_AppendChar(char * const p_buf, uint8_t * const p_len, char const c)
{
    if (*p_len < (uint8_t) (MAIN_UART_MSG_MAX_LEN - 1U))
    {
        p_buf[*p_len] = c;
        (*p_len)++;
    }
    else
    {
        /* buffer เต็ม - ทิ้งอักขระส่วนเกิน (ป้องกัน overflow) */
    }
    p_buf[*p_len] = '\0';
}

static void Main_AppendStr(char * const p_buf, uint8_t * const p_len, char const * const p_str)
{
    uint8_t i = 0U;

    while (p_str[i] != '\0')
    {
        Main_AppendChar(p_buf, p_len, p_str[i]);
        i++;
    }
}

static void Main_AppendDigit(char * const p_buf, uint8_t * const p_len, uint8_t const digit)
{
    uint8_t const clamped = (digit > 9U) ? 9U : digit;

    Main_AppendChar(p_buf, p_len, (char) ('0' + clamped));
}

/**
 * @brief  เรียกจาก USART2 ISR (ผ่าน callback ที่ลงทะเบียนไว้ตอน
 *         UART_Driver_Init) ทุกครั้งที่รับ byte ใหม่ 1 ตัว — สะสมเป็น
 *         บรรทัดคำสั่งจนกว่าจะเจอ '\r' หรือ '\n' แล้วตีความผ่าน
 *         admin_command ทันทีภายใน ISR context (การทำงานสั้นและไม่
 *         blocking ทั้งหมด ปลอดภัยที่จะทำใน ISR)
 */
static void Main_UartRxHandler(uint8_t const received_byte)
{
    if ((received_byte == (uint8_t) '\r') || (received_byte == (uint8_t) '\n'))
    {
        if (s_admin_line_index > 0U)
        {
            AdminCommand_t cmd;

            s_admin_line_buffer[s_admin_line_index] = '\0';
            cmd = AdminCommand_Parse(s_admin_line_buffer);

            switch (cmd)
            {
                case ADMIN_CMD_UNLOCK:
                    LockFsm_ForceUnlock();
                    UART_Driver_SendString("OK: Unlocked\r\n");
                    break;

                case ADMIN_CMD_LOCKOUT:
                    LockFsm_ForceLockout();
                    UART_Driver_SendString("OK: Lockout engaged\r\n");
                    break;

                case ADMIN_CMD_RESET:
                    LockFsm_ResetToDefault();
                    UART_Driver_SendString("OK: Code reset to default\r\n");
                    break;

                default:
                    /* ADMIN_CMD_UNKNOWN - พิมพ์ผิดหรือคำสั่งไม่รองรับ */
                    UART_Driver_SendString("ERROR: Unknown command\r\n");
                    break;
            }

            s_admin_line_index = 0U;
        }
        else
        {
            /* บรรทัดว่างเปล่า (กด Enter ติดกันหรือ \r\n มาคนละ byte) - ไม่ทำอะไร */
        }
    }
    else if (s_admin_line_index < (MAIN_ADMIN_LINE_MAX_LEN - 1U))
    {
        s_admin_line_buffer[s_admin_line_index] = (char) received_byte;
        s_admin_line_index++;
    }
    else
    {
        /* บรรทัดยาวเกินไป - ทิ้งตัวอักษรส่วนเกิน (ป้องกัน buffer overflow) */
    }
}

/**
 * @brief  เรียกจาก ADC ISR ทุกครั้งที่แปลงค่า potentiometer เสร็จ 1 ครั้ง
 *         (continuous mode ทำงานต่อเนื่องตลอดเวลา) — อัพเดตค่าให้ dial_lock
 *         แล้วถ้าตอนนี้ยังไม่ได้เริ่มป้อนรหัส (สถานะ IDLE) ให้โชว์โซน
 *         ปัจจุบันบน 7-segment เพื่อช่วยให้ผู้ใช้หมุนหาตำแหน่งได้
 */
static void Main_AdcEocHandler(uint16_t const raw_value)
{
    DialLock_UpdateRaw(raw_value);

    if (LockFsm_GetState() == LOCK_STATE_IDLE)
    {
        SevenSegment_Driver_ShowDigit(DialLock_GetCurrentZone());
    }
    else
    {
        /* กำลังป้อนรหัสหรืออยู่สถานะอื่น - จอ 7-segment ถูกใช้แสดงอย่างอื่น
         * อยู่แล้ว (จำนวนหลักที่กด/lockout countdown) ไม่ไปแตะ */
    }
}

/**
 * @brief  เรียกจาก ADC ISR เมื่อ Analog Watchdog ตรวจพบว่า potentiometer
 *         หลุดออกจากโซนเป้าหมาย (เปิดใช้งานเฉพาะตอนสถานะ ENTERING เท่านั้น
 *         ผ่าน Main_LockNotifyHandler) ส่งต่อให้ lock_fsm ยกเลิกรอบนี้ทันที
 */
static void Main_AdcWatchdogHandler(void)
{
    LockFsm_OnDialViolation();
}
