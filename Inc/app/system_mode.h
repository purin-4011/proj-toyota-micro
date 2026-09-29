/******************************************************************************
 * @file    system_mode.h
 * @brief   Pure logic module (ไม่แตะ hardware) — สถานะระดับระบบ อยู่เหนือ
 *          lock_fsm / setup_mode:
 *
 *            ACTIVE   : ใช้งานปกติ
 *            SLEEP    : ไม่มีคนอยู่ใกล้ (สว่าง) และไม่มีการกดปุ่มนาน 15 วินาที
 *                       -> ดับจอ, ไม่รับปุ่ม, ลด interrupt ให้ CPU หลับ
 *                       ตื่นเมื่อมีคนมาบังแสง (LDR มืด)
 *            DISABLED : อุณหภูมิผิดปกติ -> ปฏิเสธทุกการกดปุ่ม ค้างไว้จนกว่า
 *                       admin จะสั่ง RESET ทาง UART (สำคัญกว่า SLEEP เสมอ)
 *
 *          หลักการ: เซนเซอร์ทำให้ระบบ "ล็อกแน่นขึ้น/ใช้งานไม่ได้" เท่านั้น
 *          ไม่มีสิทธิ์ปลดล็อก (fail-secure)
 ******************************************************************************/
#ifndef SYSTEM_MODE_H
#define SYSTEM_MODE_H

#include <stdint.h>
#include <stdbool.h>

typedef enum
{
    SYSTEM_MODE_ACTIVE = 0U,
    SYSTEM_MODE_SLEEP,
    SYSTEM_MODE_DISABLED
} SystemMode_t;

typedef enum
{
    SYSTEM_NOTIFY_ENTER_SLEEP = 0U,
    SYSTEM_NOTIFY_WAKE,
    SYSTEM_NOTIFY_ENTER_DISABLED,
    SYSTEM_NOTIFY_EXIT_DISABLED
} SystemMode_Notification_t;

typedef void (*SystemMode_NotifyCallback_t)(SystemMode_Notification_t notification);

void SystemMode_Init(SystemMode_NotifyCallback_t notify_callback);

/**
 * @brief  เรียกทุก tick (100ms)
 * @param  can_sleep : ระบบว่างพอจะหลับได้ไหม (lock_fsm IDLE และไม่อยู่ใน
 *                     Setup Mode) — ถ้าไม่ว่าง จะไม่นับเวลาเข้า sleep
 * @param  presence  : มีคนอยู่ใกล้ (บังแสง) หรือไม่
 */
void SystemMode_OnTick(bool can_sleep, bool presence);

/** มีการกดปุ่ม/คำสั่ง admin: รีเซ็ตเวลานับเข้า sleep (ถ้าหลับอยู่จะตื่น) */
void SystemMode_OnUserActivity(void);

/** อุณหภูมิผิดปกติ -> DISABLED จากทุกสถานะ */
void SystemMode_OnTamper(void);

/** admin RESET -> ออกจาก DISABLED กลับเป็น ACTIVE */
void SystemMode_ClearTamper(void);

SystemMode_t SystemMode_Get(void);

#endif /* SYSTEM_MODE_H */
