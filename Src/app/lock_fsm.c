/******************************************************************************
 * @file    lock_fsm.c
 * @brief   Implementation ของ lock_fsm (pure logic, ไม่แตะ hardware)
 ******************************************************************************/
#include "lock_fsm.h"
#include "code_storage.h"
#include "app_config.h"

static LockState_t s_state = LOCK_STATE_IDLE;
static LockFsm_NotifyCallback_t volatile s_notify_callback = (LockFsm_NotifyCallback_t) 0;

static CodeSymbol_t s_entry_buffer[CODE_STORAGE_MAX_LENGTH];
static uint8_t s_entry_count = 0U;

static uint16_t s_idle_tick_counter = 0U;  /* นับ tick ระหว่างไม่มีการกด (สำหรับ input timeout) */
static uint16_t s_hold_tick_counter = 0U;  /* นับ tick ระหว่างแสดงผล UNLOCKED หรือ LOCKED_OUT */
static uint8_t s_wrong_attempt_count = 0U;

/* ---------------------------------------------------------------------- *
 * Private function prototypes
 * ---------------------------------------------------------------------- */
static void LockFsm_Notify(LockFsm_Notification_t notification);
static void LockFsm_PushSymbol(CodeSymbol_t symbol);
static void LockFsm_Validate(void);

/* ---------------------------------------------------------------------- *
 * Private function implementations
 * ---------------------------------------------------------------------- */

static void LockFsm_Notify(LockFsm_Notification_t const notification)
{
    if (s_notify_callback != (LockFsm_NotifyCallback_t) 0)
    {
        s_notify_callback(notification);
    }
    else
    {
        /* ไม่มี callback ลงทะเบียนไว้ - ไม่ทำอะไร */
    }
}

static void LockFsm_PushSymbol(CodeSymbol_t const symbol)
{
    if (s_entry_count < (uint8_t) CODE_STORAGE_MAX_LENGTH)
    {
        s_entry_buffer[s_entry_count] = symbol;
        s_entry_count++;
    }
    else
    {
        /* buffer เต็ม - ทิ้งสัญลักษณ์ส่วนเกิน (ป้องกัน overflow) */
    }
}

static void LockFsm_Validate(void)
{
    bool const matched = CodeStorage_Compare(s_entry_buffer, s_entry_count);

    if (matched)
    {
        s_state = LOCK_STATE_UNLOCKED;
        s_wrong_attempt_count = 0U;
        s_hold_tick_counter = 0U;
        LockFsm_Notify(LOCK_NOTIFY_UNLOCK_SUCCESS);
    }
    else
    {
        s_wrong_attempt_count++;

        if (s_wrong_attempt_count >= (uint8_t) APP_MAX_WRONG_ATTEMPTS)
        {
            s_state = LOCK_STATE_LOCKED_OUT;
            s_hold_tick_counter = 0U;
            LockFsm_Notify(LOCK_NOTIFY_LOCKOUT_ENTER);
        }
        else
        {
            s_state = LOCK_STATE_IDLE;
            LockFsm_Notify(LOCK_NOTIFY_UNLOCK_FAIL);
        }
    }

    s_entry_count = 0U;
    s_idle_tick_counter = 0U;
}

/* ---------------------------------------------------------------------- *
 * Public function implementations
 * ---------------------------------------------------------------------- */

void LockFsm_Init(LockFsm_NotifyCallback_t const notify_callback)
{
    CodeStorage_Init();

    s_notify_callback = notify_callback;
    s_state = LOCK_STATE_IDLE;
    s_entry_count = 0U;
    s_idle_tick_counter = 0U;
    s_hold_tick_counter = 0U;
    s_wrong_attempt_count = 0U;
}

void LockFsm_OnSymbol(CodeSymbol_t const symbol, uint32_t const press_duration_ms)
{
    /* press_duration_ms สงวนไว้สำหรับโหมดตั้งรหัสในอนาคต (ตรวจจับกดค้าง
     * >=3 วินาที) ยังไม่ใช้ใน scope ปัจจุบัน - cast เป็น void กัน warning */
    (void) press_duration_ms;

    if (s_state == LOCK_STATE_LOCKED_OUT)
    {
        /* ตาม proposal: ระหว่าง lockout กดปุ่มอะไรก็ไม่มีผล */
    }
    else if (s_state == LOCK_STATE_UNLOCKED)
    {
        /* ระหว่างแสดงผลปลดล็อกสำเร็จค้างไว้ ไม่รับ input ใหม่ */
    }
    else
    {
        /* LOCK_STATE_IDLE หรือ LOCK_STATE_ENTERING - รับสัญลักษณ์ */
        if (s_state == LOCK_STATE_IDLE)
        {
            s_state = LOCK_STATE_ENTERING;
        }
        else
        {
            /* อยู่ใน ENTERING อยู่แล้ว ไม่ต้องเปลี่ยน state */
        }

        LockFsm_PushSymbol(symbol);
        s_idle_tick_counter = 0U;
        LockFsm_Notify(LOCK_NOTIFY_KEY_ACCEPTED);
    }
}

void LockFsm_OnTick(void)
{
    if (s_state == LOCK_STATE_ENTERING)
    {
        s_idle_tick_counter++;

        if (s_idle_tick_counter >= (uint16_t) APP_INPUT_TIMEOUT_TICKS)
        {
            LockFsm_Validate();
        }
        else
        {
            /* ยังไม่ครบ timeout - รอ symbol ถัดไป */
        }
    }
    else if (s_state == LOCK_STATE_UNLOCKED)
    {
        s_hold_tick_counter++;

        if (s_hold_tick_counter >= (uint16_t) APP_UNLOCK_HOLD_TICKS)
        {
            s_state = LOCK_STATE_IDLE;
            LockFsm_Notify(LOCK_NOTIFY_RETURN_TO_IDLE);
        }
        else
        {
            /* ยังไม่ครบเวลาแสดงผลสำเร็จ */
        }
    }
    else if (s_state == LOCK_STATE_LOCKED_OUT)
    {
        s_hold_tick_counter++;

        if (s_hold_tick_counter >= (uint16_t) APP_LOCKOUT_TICKS)
        {
            s_state = LOCK_STATE_IDLE;
            s_wrong_attempt_count = 0U;
            LockFsm_Notify(LOCK_NOTIFY_RETURN_TO_IDLE);
        }
        else
        {
            /* ยังไม่ครบเวลา lockout */
        }
    }
    else
    {
        /* LOCK_STATE_IDLE: ไม่มีอะไรต้องทำทุก tick */
    }
}

LockState_t LockFsm_GetState(void)
{
    return s_state;
}

uint8_t LockFsm_GetWrongAttemptCount(void)
{
    return s_wrong_attempt_count;
}

uint16_t LockFsm_GetLockoutSecondsRemaining(void)
{
    uint16_t seconds_remaining;

    if (s_state == LOCK_STATE_LOCKED_OUT)
    {
        uint16_t const ticks_remaining = (uint16_t) APP_LOCKOUT_TICKS - s_hold_tick_counter;
        seconds_remaining = ticks_remaining / (uint16_t) APP_TICKS_PER_SECOND;
    }
    else
    {
        seconds_remaining = 0U;
    }

    return seconds_remaining;
}

uint8_t LockFsm_GetEntryCount(void)
{
    return s_entry_count;
}
