/******************************************************************************
 * @file    crc_driver.h
 * @brief   CRC driver (register-level, raw address) ห่อ hardware CRC-32
 *          unit ที่มีอยู่ในตัวชิป STM32F411 อยู่แล้ว
 *
 *          ต่างจาก driver ตัวอื่นในโปรเจคนี้ตรงที่ **ไม่มี interrupt เลย**
 *          เพราะ CRC เป็นวงจรคำนวณแบบ combinational (ป้อนข้อมูลเข้า ->
 *          อ่านผลลัพธ์ออกได้ทันที ไม่ต้องรอ event ใดๆ) จึงไม่ต้องมี NVIC
 *          หรือ callback เหมือน driver ตัวอื่น
 ******************************************************************************/
#ifndef CRC_DRIVER_H
#define CRC_DRIVER_H

#include <stdint.h>

/** เปิดสัญญาณนาฬิกาให้ CRC unit (ต้องเรียกก่อนใช้งานครั้งแรกเสมอ) */
void CRC_Driver_Init(void);

/** รีเซ็ตค่า CRC ที่กำลังคำนวณสะสมอยู่ กลับไปเป็นค่าเริ่มต้น (0xFFFFFFFF)
 *  ต้องเรียกก่อนเริ่มคำนวณชุดข้อมูลใหม่ทุกครั้ง */
void CRC_Driver_Reset(void);

/**
 * @brief  ป้อนข้อมูล 1 คำ (32-bit) เข้าไปให้ฮาร์ดแวร์คำนวณสะสมต่อจากเดิม
 * @param  word : ข้อมูลที่จะป้อนเข้าไปคำนวณ
 * @return ค่า CRC ปัจจุบันหลังป้อนคำนี้แล้ว (อ่านได้ทันที ไม่ต้องรอ)
 */
uint32_t CRC_Driver_FeedWord(uint32_t word);

#endif /* CRC_DRIVER_H */
