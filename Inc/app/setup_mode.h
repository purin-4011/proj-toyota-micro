/******************************************************************************
 * @file    setup_mode.h
 * @brief   Pure logic module (ไม่แตะ hardware) — state machine ของ Setup
 *          Mode สำหรับให้ผู้ใช้ตั้งรหัสผ่านและโซนโพเทนชิโอมิเตอร์ใหม่ผ่าน
 *          ปุ่มกด โดยแสดงผลป้อนกลับทาง UART/7-segment/LED (จัดการใน main.c)
 *
 *          ขั้นตอนการทำงาน:
 *          1) INACTIVE  -> กด PB5 ค้างนาน >= APP_SETUP_ENTRY_HOLD_MS
 *                          -> เข้าสู่ SELECT_LENGTH (แจ้ง ENTERED)
 *          2) SELECT_LENGTH -> ใช้ PA10/PB3 เพิ่ม/ลดจำนวนหลัก (0-8) ->
 *                          กด PB5 (สั้นก็ได้) เพื่อ confirm -> ถ้าจำนวนหลัก
 *                          ยังเป็น 0 จะแจ้ง WARNING_NO_LENGTH แล้วค้างอยู่
 *                          ขั้นนี้ต่อ มิฉะนั้นไปขั้นถัดไป (แจ้ง CODE_START)
 *          3) ENTER_CODE -> กรอกรหัสผ่านปุ่มหลัก (ส่งเข้ามาทาง
 *                          SetupMode_OnCodeSymbol แต่ละครั้งแจ้ง CODE_SYMBOL)
 *                          -> กด PB5 อีกครั้งเพื่อ confirm+exit ถ้ากรอกยังไม่
 *                          ครบจะแจ้ง WARNING_INCOMPLETE_CODE แล้วค้างอยู่ขั้น
 *                          นี้ต่อ มิฉะนั้นจะบันทึกรหัส+โซนปัจจุบันลง storage
 *                          (แจ้ง COMMITTED) แล้วกลับสู่ INACTIVE
 ******************************************************************************/
#ifndef SETUP_MODE_H
#define SETUP_MODE_H

#include <stdint.h>
#include "code_decoder.h"

typedef enum
{
    SETUP_STATE_INACTIVE = 0U,
    SETUP_STATE_SELECT_LENGTH,
    SETUP_STATE_ENTER_CODE
} SetupState_t;

typedef enum
{
    SETUP_NOTIFY_ENTERED = 0U,              /* เพิ่งเข้าสู่ Setup Mode */
    SETUP_NOTIFY_DIGIT_COUNT_CHANGED,       /* จำนวนหลักที่เลือกเปลี่ยน (PA10/PB3) */
    SETUP_NOTIFY_CODE_START,                /* เริ่มขั้นตอนกรอกรหัสใหม่ */
    SETUP_NOTIFY_CODE_SYMBOL,               /* รับสัญลักษณ์รหัสใหม่เข้ามา 1 ตัว */
    SETUP_NOTIFY_WARNING_NO_LENGTH,         /* กด PB5 confirm ทั้งที่ยังไม่ได้ตั้งจำนวนหลัก */
    SETUP_NOTIFY_WARNING_INCOMPLETE_CODE,   /* กด PB5 exit ทั้งที่กรอกรหัสยังไม่ครบ */
    SETUP_NOTIFY_COMMITTED                  /* บันทึกรหัส+โซนใหม่สำเร็จ กลับสู่ INACTIVE */
} SetupMode_Notification_t;

typedef void (*SetupMode_NotifyCallback_t)(SetupMode_Notification_t notification);

/**
 * @brief  เริ่มต้น Setup Mode module: สถานะ INACTIVE, เคลียร์ตัวนับทั้งหมด
 */
void SetupMode_Init(SetupMode_NotifyCallback_t notify_callback);

/**
 * @brief  เรียกทุกครั้งที่ปล่อยปุ่ม PB5 (ปุ่ม Setup Mode) พร้อมระยะเวลาที่
 *         กดค้างไว้ (ms) — ความหมายของการกดจะแตกต่างกันไปตามสถานะปัจจุบัน
 *         (ดูรายละเอียดในคอมเมนต์ด้านบนของไฟล์)
 */
void SetupMode_OnButtonPB5(uint32_t press_duration_ms);

/**
 * @brief  เรียกเมื่อกดปุ่มเพิ่มจำนวนหลัก (PA10) — มีผลเฉพาะตอนสถานะ
 *         SELECT_LENGTH เท่านั้น, clamp ไม่เกิน CODE_STORAGE_MAX_LENGTH
 */
void SetupMode_OnDigitIncrement(void);

/**
 * @brief  เรียกเมื่อกดปุ่มลดจำนวนหลัก (PB3) — มีผลเฉพาะตอนสถานะ
 *         SELECT_LENGTH เท่านั้น, clamp ไม่ต่ำกว่า 0
 */
void SetupMode_OnDigitDecrement(void);

/**
 * @brief  เรียกเมื่อกดปุ่มหลัก (PB4) ระหว่างกรอกรหัสใหม่ — มีผลเฉพาะตอน
 *         สถานะ ENTER_CODE และยังกรอกไม่ครบจำนวนหลักที่ตั้งไว้เท่านั้น
 */
void SetupMode_OnCodeSymbol(CodeSymbol_t symbol);

/**
 * @brief  ยกเลิก Setup Mode ทันทีโดยไม่บันทึกอะไร (ใช้ตอนระบบเข้า DISABLED
 *         เพราะอุณหภูมิผิดปกติ) ไม่มี notification — ผู้เรียกจัดการ LED/UART เอง
 */
void SetupMode_Cancel(void);

SetupState_t SetupMode_GetState(void);
uint8_t SetupMode_GetTargetDigitCount(void);
uint8_t SetupMode_GetEnteredCount(void);
CodeSymbol_t SetupMode_GetEnteredSymbol(uint8_t index);
uint8_t SetupMode_GetLastCommittedZone(void);

#endif /* SETUP_MODE_H */
