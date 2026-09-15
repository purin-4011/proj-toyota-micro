/******************************************************************************
 * @file    lock_fsm.h
 * @brief   State machine หลักของ Digital Combination Lock
 *
 *          SCOPE ปัจจุบัน: เฉพาะ flow ปลดล็อก (IDLE -> ENTERING ->
 *          UNLOCKED / LOCKED_OUT) ยังไม่มีโหมดตั้งรหัสใหม่ (Setup mode)
 *          เพราะรอ ADC (potentiometer) สำหรับเลือกความยาวรหัสตามที่
 *          proposal ระบุไว้ก่อน — จะกลับมาต่อส่วนนี้ทีหลัง
 *
 *          Module นี้ไม่แตะ hardware โดยตรงเลย (ไม่ include driver ใดๆ)
 *          สื่อสารกับภายนอกผ่าน callback (LockFsm_NotifyCallback_t) เท่านั้น
 *          เพื่อให้ Application ชั้นบน (main.c) เป็นคนตัดสินใจว่าจะแสดงผล
 *          อย่างไร (ตอนนี้คือ LED, ในอนาคตจะเป็น 7-segment ด้วย)
 ******************************************************************************/
#ifndef LOCK_FSM_H
#define LOCK_FSM_H

#include <stdint.h>
#include "code_decoder.h"

typedef enum
{
    LOCK_STATE_IDLE = 0U,
    LOCK_STATE_ENTERING,
    LOCK_STATE_UNLOCKED,
    LOCK_STATE_LOCKED_OUT
} LockState_t;

/** เหตุการณ์ที่ lock_fsm แจ้งออกไปให้ Application ชั้นบนไปแสดงผลต่อ */
typedef enum
{
    LOCK_NOTIFY_KEY_ACCEPTED = 0U,  /* รับสัญลักษณ์เข้ามา 1 ตัว (ไฟกระพริบสั้นๆ ตอบรับ) */
    LOCK_NOTIFY_UNLOCK_SUCCESS,     /* รหัสถูกต้อง */
    LOCK_NOTIFY_UNLOCK_FAIL,        /* รหัสผิด (แต่ยังไม่ครบ 3 ครั้ง) */
    LOCK_NOTIFY_LOCKOUT_ENTER,      /* ผิดครบ 3 ครั้ง เข้าสู่ lockout */
    LOCK_NOTIFY_RETURN_TO_IDLE      /* จบช่วงแสดงผล (UNLOCKED หรือ LOCKOUT) กลับสู่ปกติ */
} LockFsm_Notification_t;

typedef void (*LockFsm_NotifyCallback_t)(LockFsm_Notification_t notification);

/**
 * @brief  เริ่มต้น lock_fsm: ตั้งรหัส default ผ่าน code_storage และรีเซ็ต
 *         สถานะทั้งหมดกลับเป็น IDLE
 * @param  notify_callback : ฟังก์ชันที่ Application ชั้นบน implement ไว้รับ
 *                           เหตุการณ์ (ห้าม NULL)
 */
void LockFsm_Init(LockFsm_NotifyCallback_t notify_callback);

/**
 * @brief  เรียกทุกครั้งที่ code_decoder แปลง duration เป็นสัญลักษณ์ได้แล้ว
 *         (เรียกจาก EXTI release handler ผ่าน main.c)
 * @param  symbol            : สัญลักษณ์ที่ได้จาก CodeDecoder_Classify()
 * @param  press_duration_ms : เวลากดจริง (ms) - ตอนนี้ยังไม่ได้ใช้ค่านี้
 *                             ใน logic (สงวนไว้สำหรับโหมดตั้งรหัสในอนาคต
 *                             ที่ต้องแยกกดค้างยาวพิเศษออกจากรหัสปกติ)
 */
void LockFsm_OnSymbol(CodeSymbol_t symbol, uint32_t press_duration_ms);

/**
 * @brief  เรียกทุก 100ms จาก TIM3 tick (ผ่าน main.c) เพื่อขับเคลื่อน
 *         timeout ต่างๆ ภายใน state machine (input timeout, lockout countdown,
 *         unlock display hold)
 */
void LockFsm_OnTick(void);

/** อ่านสถานะปัจจุบันของ state machine */
LockState_t LockFsm_GetState(void);

/** อ่านจำนวนครั้งที่กดผิดติดต่อกัน ณ ปัจจุบัน (0-3) */
uint8_t LockFsm_GetWrongAttemptCount(void);

/**
 * @brief  อ่านจำนวนวินาทีที่เหลือของ lockout countdown (สำหรับแสดงบน
 *         7-segment) คืนค่า 0 เสมอถ้าไม่ได้อยู่ในสถานะ LOCKED_OUT
 */
uint16_t LockFsm_GetLockoutSecondsRemaining(void);

/**
 * @brief  อ่านจำนวนสัญลักษณ์ (SHORT/LONG) ที่ป้อนเข้ามาแล้วในรอบปัจจุบัน
 *         (สำหรับแสดงบน 7-segment ว่ากดไปกี่หลักแล้ว) คืนค่า 0 ถ้ายังไม่ได้
 *         เริ่มป้อนหรือเพิ่งจบรอบไปแล้ว
 */
uint8_t LockFsm_GetEntryCount(void);

#endif /* LOCK_FSM_H */
