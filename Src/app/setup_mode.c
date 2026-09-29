/******************************************************************************
 * @file    setup_mode.c
 * @brief   Implementation ของ setup_mode (pure logic - เรียก code_storage
 *          และ dial_lock ตรงๆ ได้ เช่นเดียวกับ lock_fsm เพราะทั้งคู่เป็น
 *          pure logic module ไม่มี interrupt เกี่ยวข้อง)
 ******************************************************************************/
#include "setup_mode.h"
#include "code_storage.h"
#include "dial_lock.h"
#include "app_config.h"

static SetupState_t s_state = SETUP_STATE_INACTIVE;
static SetupMode_NotifyCallback_t s_notify_callback = (SetupMode_NotifyCallback_t) 0;

static uint8_t s_target_digit_count = 0U;
static CodeSymbol_t s_entry_buffer[CODE_STORAGE_MAX_LENGTH];
static uint8_t s_entry_count = 0U;
static uint8_t s_last_committed_zone = 0U;

static void SetupMode_Notify(SetupMode_Notification_t const notification)
{
    if (s_notify_callback != (SetupMode_NotifyCallback_t) 0)
    {
        s_notify_callback(notification);
    }
    else
    {
        /* ไม่มี callback ลงทะเบียนไว้ - ไม่ทำอะไร */
    }
}

void SetupMode_Init(SetupMode_NotifyCallback_t const notify_callback)
{
    s_notify_callback = notify_callback;
    s_state = SETUP_STATE_INACTIVE;
    s_target_digit_count = 0U;
    s_entry_count = 0U;
    s_last_committed_zone = 0U;
}

void SetupMode_OnButtonPB5(uint32_t const press_duration_ms)
{
    if (s_state == SETUP_STATE_INACTIVE)
    {
        if (press_duration_ms >= (uint32_t) APP_SETUP_ENTRY_HOLD_MS)
        {
            s_state = SETUP_STATE_SELECT_LENGTH;
            s_target_digit_count = 0U;
            s_entry_count = 0U;
            SetupMode_Notify(SETUP_NOTIFY_ENTERED);
        }
        else
        {
            /* กดสั้นเกินไป (ไม่ถึงเวลาที่กำหนด) - ไม่เข้า Setup Mode */
        }
    }
    else if (s_state == SETUP_STATE_SELECT_LENGTH)
    {
        if (s_target_digit_count == 0U)
        {
            SetupMode_Notify(SETUP_NOTIFY_WARNING_NO_LENGTH);
            /* ค้างอยู่ที่ SELECT_LENGTH ต่อ - ยังไม่ได้ตั้งจำนวนหลัก */
        }
        else
        {
            s_state = SETUP_STATE_ENTER_CODE;
            s_entry_count = 0U;
            SetupMode_Notify(SETUP_NOTIFY_CODE_START);
        }
    }
    else if (s_state == SETUP_STATE_ENTER_CODE)
    {
        if (s_entry_count < s_target_digit_count)
        {
            SetupMode_Notify(SETUP_NOTIFY_WARNING_INCOMPLETE_CODE);
            /* ค้างอยู่ที่ ENTER_CODE ต่อ - ยังกรอกรหัสไม่ครบ */
        }
        else
        {
            /* กรอกครบแล้ว - บันทึกโซนปัจจุบัน + รหัสที่กรอก แล้วแจ้งผล
             * ก่อนเคลียร์ buffer (สำคัญ: ต้องแจ้ง COMMITTED ก่อนเคลียร์
             * เพื่อให้ผู้ฟัง callback ยังอ่าน SetupMode_GetEnteredSymbol()/
             * SetupMode_GetTargetDigitCount() ได้ค่าที่ถูกต้องระหว่าง
             * ประมวลผล notification นี้) */
            s_last_committed_zone = DialLock_GetCurrentZone();
            CodeStorage_Commit(s_entry_buffer, s_entry_count);
            DialLock_SetTargetZone(s_last_committed_zone);

            s_state = SETUP_STATE_INACTIVE;
            SetupMode_Notify(SETUP_NOTIFY_COMMITTED);

            s_target_digit_count = 0U;
            s_entry_count = 0U;
        }
    }
    else
    {
        /* MISRA: else บังคับ - ไม่ควรเกิด */
    }
}

void SetupMode_OnDigitIncrement(void)
{
    if (s_state == SETUP_STATE_SELECT_LENGTH)
    {
        if (s_target_digit_count < (uint8_t) CODE_STORAGE_MAX_LENGTH)
        {
            s_target_digit_count++;
        }
        else
        {
            /* ถึงขีดจำกัดสูงสุดแล้ว - ไม่เพิ่มต่อ */
        }
        SetupMode_Notify(SETUP_NOTIFY_DIGIT_COUNT_CHANGED);
    }
    else
    {
        /* ไม่อยู่ในขั้นตอนเลือกจำนวนหลัก - ไม่มีผล */
    }
}

void SetupMode_OnDigitDecrement(void)
{
    if (s_state == SETUP_STATE_SELECT_LENGTH)
    {
        if (s_target_digit_count > 0U)
        {
            s_target_digit_count--;
        }
        else
        {
            /* ถึงขีดจำกัดต่ำสุดแล้ว - ไม่ลดต่อ */
        }
        SetupMode_Notify(SETUP_NOTIFY_DIGIT_COUNT_CHANGED);
    }
    else
    {
        /* ไม่อยู่ในขั้นตอนเลือกจำนวนหลัก - ไม่มีผล */
    }
}

void SetupMode_OnCodeSymbol(CodeSymbol_t const symbol)
{
    if (s_state == SETUP_STATE_ENTER_CODE)
    {
        if (s_entry_count < s_target_digit_count)
        {
            s_entry_buffer[s_entry_count] = symbol;
            s_entry_count++;
            SetupMode_Notify(SETUP_NOTIFY_CODE_SYMBOL);
        }
        else
        {
            /* กรอกครบตามจำนวนที่ตั้งไว้แล้ว - ปุ่มที่กดเพิ่มถูกทิ้ง
             * (ผู้ใช้ต้องกด PB5 เพื่อ confirm/exit ต่อ) */
        }
    }
    else
    {
        /* ไม่อยู่ในขั้นตอนกรอกรหัส - ไม่มีผล */
    }
}

void SetupMode_Cancel(void)
{
    s_state = SETUP_STATE_INACTIVE;
    s_target_digit_count = 0U;
    s_entry_count = 0U;
}

SetupState_t SetupMode_GetState(void)
{
    return s_state;
}

uint8_t SetupMode_GetTargetDigitCount(void)
{
    return s_target_digit_count;
}

uint8_t SetupMode_GetEnteredCount(void)
{
    return s_entry_count;
}

CodeSymbol_t SetupMode_GetEnteredSymbol(uint8_t const index)
{
    CodeSymbol_t result;

    if (index < (uint8_t) CODE_STORAGE_MAX_LENGTH)
    {
        result = s_entry_buffer[index];
    }
    else
    {
        result = CODE_SYMBOL_SHORT;   /* ค่า default ปลอดภัยเมื่อ index เกินขอบเขต */
    }

    return result;
}

uint8_t SetupMode_GetLastCommittedZone(void)
{
    return s_last_committed_zone;
}
