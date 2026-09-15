/******************************************************************************
 * @file    code_storage.h
 * @brief   เก็บรหัส (sequence ของ SHORT/LONG) ไว้ใน RAM เท่านั้น
 *          (ยืนยันแล้วว่าไฟดับ = รหัสหาย ไม่ persist ลง Flash)
 *
 *          ตั้งแต่เพิ่ม CRC: ทุกครั้งที่ CodeStorage_Compare() ถูกเรียก จะ
 *          เช็คก่อนว่าข้อมูลรหัสที่เก็บไว้ยังตรงกับ CRC ที่บันทึกไว้ตอน
 *          commit ล่าสุดไหม (ป้องกัน RAM corruption) ถ้าไม่ตรงจะกู้กลับเป็น
 *          รหัส default อัตโนมัติ
 *
 *          ตอนนี้ยังไม่มีฟังก์ชันตั้งรหัสใหม่ (Commit) เพราะโหมดตั้งรหัส
 *          ยังไม่ implement — module นี้จึงทำหน้าที่แค่ "มีรหัส default
 *          ให้เทียบ" เพื่อให้ทดสอบ flow ปลดล็อกหลักได้ก่อน
 ******************************************************************************/
#ifndef CODE_STORAGE_H
#define CODE_STORAGE_H

#include <stdint.h>
#include <stdbool.h>
#include "code_decoder.h"

/** ความยาวรหัสสูงสุดที่ buffer รองรับ (ตาม proposal: 4-8 หลัก) */
#define CODE_STORAGE_MAX_LENGTH   (8U)

/** ตั้งค่ารหัส default (ตาม proposal: "มีรหัสเริ่มต้นคือ สั้น-สั้น-สั้น-สั้น") */
void CodeStorage_Init(void);

/**
 * @brief  เทียบรหัสที่ป้อนเข้ามากับรหัสที่เก็บไว้
 * @param  p_symbols : buffer ของสัญลักษณ์ที่ผู้ใช้ป้อน
 * @param  length    : จำนวนสัญลักษณ์ที่ป้อน
 * @return true ถ้าตรงกันทุกตัวและความยาวเท่ากัน, false ถ้าไม่ตรง
 */
bool CodeStorage_Compare(CodeSymbol_t const * p_symbols, uint8_t length);

/** อ่านความยาวของรหัสที่เก็บไว้ปัจจุบัน */
uint8_t CodeStorage_GetLength(void);

#endif /* CODE_STORAGE_H */
