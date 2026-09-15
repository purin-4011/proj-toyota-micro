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
 *          ยังไม่มี: โหมดตั้งรหัสใหม่ (Setup mode - รอ ADC), ADC,
 *          CRC (ตาม Timeline ที่เหลือ)
 ******************************************************************************/
#include "gpio_driver.h"
#include "exti_driver.h"
#include "timer_driver.h"
#include "seven_segment_driver.h"
#include "uart_driver.h"
#include "code_decoder.h"
#include "lock_fsm.h"
#include "admin_command.h"
#include "app_config.h"

/* จำนวน tick (100ms/tick) ที่ต้องการให้ LED กระพริบสั้นๆ ตอบรับการกด/ผิด */
#define MAIN_BLINK_HOLD_TICKS   (3U)   /* 3 x 100ms = 300ms */

/* ความยาวสูงสุดของบรรทัดคำสั่ง admin (คำสั่งที่ยาวที่สุดคือ "LOCKOUT" = 7
 * ตัวอักษร เผื่อไว้ให้พอสำหรับ error/edge case) */
#define MAIN_ADMIN_LINE_MAX_LEN  (16U)

/* ตัวแปรที่ถูกแตะทั้งจาก ISR และ main loop ต้องเป็น volatile ตาม MISRA-C */
static uint32_t volatile s_press_start_tick = 0U;
static uint16_t volatile s_green_blink_ticks_remaining = 0U;
static uint16_t volatile s_red_blink_ticks_remaining = 0U;

/* buffer สะสมบรรทัดคำสั่ง admin ที่รับมาทีละ byte จาก UART RX interrupt */
static char s_admin_line_buffer[MAIN_ADMIN_LINE_MAX_LEN];
static uint8_t s_admin_line_index = 0U;


static void Main_ExtiEventHandler(EXTI_Edge_t edge);
static void Main_Tim3TickHandler(void);
static void Main_LockNotifyHandler(LockFsm_Notification_t notification);
static void Main_UartRxHandler(uint8_t received_byte);
static void Main_HardwareInit(void);

int main(void)
{
    Main_HardwareInit();

    /* main loop ว่างเปล่าโดยตั้งใจ - งานทั้งหมดขับเคลื่อนด้วย interrupt
     * (EXTI4 สำหรับปุ่ม, TIM3 สำหรับ periodic tick) ตามเกณฑ์ "ห้าม Polling" */
    for (;;)
    {
        /* รอ ADC เสร็จก่อนค่อยเพิ่มโหมดตั้งรหัสใหม่ (Setup mode) */
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

    /* --- Timing subsystem (Two-Tier Architecture) --- */
    Timer_Driver_TIM2_Init();
    Timer_Driver_TIM3_Init(Main_Tim3TickHandler);

    /* --- Button input (EXTI4 บน PB4) --- */
    EXTI_Driver_Init(Main_ExtiEventHandler);

    /* --- 7-segment (BCD) สำหรับแสดง lockout countdown --- */
    SevenSegment_Driver_Init();

    /* --- UART (USART2) สำหรับ Admin Mode: รับคำสั่งจาก PC ผ่าน ST-Link VCP --- */
    UART_Driver_Init(Main_UartRxHandler);
    UART_Driver_SendString("Digital Combination Lock - Admin console ready\r\n");

    /* --- State machine หลักของระบบล็อก --- */
    LockFsm_Init(Main_LockNotifyHandler);
}

/**
 * @brief  เรียกจาก EXTI4 ISR เมื่อปุ่มถูกกดหรือปล่อย
 *         คำนวณ duration จาก TIM2 tick แล้วส่งต่อให้ lock_fsm ตัดสินใจ
 *         (main.c ไม่ตัดสินใจ logic เองแล้ว แค่เป็นสะพานเชื่อม)
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

        LockFsm_OnSymbol(symbol, duration_ms);
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
            break;

        case LOCK_NOTIFY_UNLOCK_SUCCESS:
            GPIO_Driver_WritePin(APP_LED_G_PORT, APP_LED_G_PIN, GPIO_PIN_SET);
            GPIO_Driver_WritePin(APP_LED_RED_PORT, APP_LED_RED_PIN, GPIO_PIN_RESET);
            /* ไม่ตั้ง blink counter - ไฟติดค้างจนกว่า lock_fsm จะสั่ง
             * RETURN_TO_IDLE เองตอนครบ 10 วิ (ดูฟังก์ชัน Main_Tim3TickHandler) */
            break;

        case LOCK_NOTIFY_UNLOCK_FAIL:
            GPIO_Driver_WritePin(APP_LED_RED_PORT, APP_LED_RED_PIN, GPIO_PIN_SET);
            s_red_blink_ticks_remaining = (uint16_t) MAIN_BLINK_HOLD_TICKS;
            /* ป้อนรหัสรอบนี้จบแล้ว (ไม่ว่าจะถูกหรือผิด) -> ล้างตัวเลขนับหลัก
             * กลับเป็น 0 เตรียมรอรอบถัดไป */
            SevenSegment_Driver_ShowDigit(0U);
            break;

        case LOCK_NOTIFY_LOCKOUT_ENTER:
            GPIO_Driver_WritePin(APP_LED_RED_PORT, APP_LED_RED_PIN, GPIO_PIN_SET);
            GPIO_Driver_WritePin(APP_LED_GREEN_PORT, APP_LED_GREEN_PIN, GPIO_PIN_RESET);
            SevenSegment_Driver_ShowDigit((uint8_t) APP_LOCKOUT_DURATION_SEC);
            break;

        case LOCK_NOTIFY_RETURN_TO_IDLE:
            GPIO_Driver_WritePin(APP_LED_G_PORT, APP_LED_G_PIN, GPIO_PIN_RESET);
            GPIO_Driver_WritePin(APP_LED_RED_PORT, APP_LED_RED_PIN, GPIO_PIN_RESET);
            /* กลับสู่สถานะปกติแล้ว (ไม่ว่าจะจบจาก UNLOCKED หรือ LOCKED_OUT)
             * -> ล้างจอ 7-segment กลับเป็น 0 เตรียมรอบถัดไป */
            SevenSegment_Driver_ShowDigit(0U);
            break;

        default:
            /* MISRA: switch ต้องมี default case เสมอ แม้ enum จะครบทุกค่าแล้ว */
            break;
    }
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
