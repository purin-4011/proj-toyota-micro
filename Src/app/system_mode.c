/******************************************************************************
 * @file    system_mode.c
 * @brief   Implementation ของ system_mode (pure logic, ไม่แตะ hardware)
 ******************************************************************************/
#include "system_mode.h"
#include "app_config.h"

static SystemMode_t volatile s_mode = SYSTEM_MODE_ACTIVE;
static SystemMode_NotifyCallback_t s_notify_callback = (SystemMode_NotifyCallback_t) 0;
static uint16_t volatile s_idle_ticks = 0U;

static void SystemMode_Notify(SystemMode_Notification_t const notification)
{
    if (s_notify_callback != (SystemMode_NotifyCallback_t) 0)
    {
        s_notify_callback(notification);
    }
    else
    {
        /* ไม่มี callback ลงทะเบียนไว้ - ไม่ทำอะไร */
    }
}

static void SystemMode_Wake(void)
{
    s_mode = SYSTEM_MODE_ACTIVE;
    s_idle_ticks = 0U;
    SystemMode_Notify(SYSTEM_NOTIFY_WAKE);
}

void SystemMode_Init(SystemMode_NotifyCallback_t const notify_callback)
{
    s_notify_callback = notify_callback;
    s_mode = SYSTEM_MODE_ACTIVE;
    s_idle_ticks = 0U;
}

void SystemMode_OnTick(bool const can_sleep, bool const presence)
{
    switch (s_mode)
    {
        case SYSTEM_MODE_ACTIVE:
            if (can_sleep && (!presence))
            {
                s_idle_ticks++;

                if (s_idle_ticks >= (uint16_t) APP_SLEEP_IDLE_TICKS)
                {
                    s_mode = SYSTEM_MODE_SLEEP;
                    s_idle_ticks = 0U;
                    SystemMode_Notify(SYSTEM_NOTIFY_ENTER_SLEEP);
                }
                else
                {
                    /* ยังไม่ครบเวลา */
                }
            }
            else
            {
                /* มีคนอยู่ใกล้ หรือระบบกำลังทำงานอยู่ - เริ่มนับใหม่ */
                s_idle_ticks = 0U;
            }
            break;

        case SYSTEM_MODE_SLEEP:
            if (presence)
            {
                SystemMode_Wake();
            }
            else
            {
                /* ยังไม่มีคน - หลับต่อ */
            }
            break;

        case SYSTEM_MODE_DISABLED:
            /* ออกได้ทางเดียวคือ SystemMode_ClearTamper() (admin RESET) */
            break;

        default:
            /* MISRA: default บังคับ - ไม่ควรเกิด */
            break;
    }
}

void SystemMode_OnUserActivity(void)
{
    if (s_mode == SYSTEM_MODE_ACTIVE)
    {
        s_idle_ticks = 0U;
    }
    else if (s_mode == SYSTEM_MODE_SLEEP)
    {
        SystemMode_Wake();
    }
    else
    {
        /* DISABLED - การใช้งานไม่มีผล */
    }
}

void SystemMode_OnTamper(void)
{
    if (s_mode != SYSTEM_MODE_DISABLED)
    {
        s_mode = SYSTEM_MODE_DISABLED;
        s_idle_ticks = 0U;
        SystemMode_Notify(SYSTEM_NOTIFY_ENTER_DISABLED);
    }
    else
    {
        /* อยู่ใน DISABLED แล้ว */
    }
}

void SystemMode_ClearTamper(void)
{
    if (s_mode == SYSTEM_MODE_DISABLED)
    {
        s_mode = SYSTEM_MODE_ACTIVE;
        s_idle_ticks = 0U;
        SystemMode_Notify(SYSTEM_NOTIFY_EXIT_DISABLED);
    }
    else
    {
        /* ไม่ได้อยู่ใน DISABLED - ไม่ต้องทำอะไร */
    }
}

SystemMode_t SystemMode_Get(void)
{
    return s_mode;
}
