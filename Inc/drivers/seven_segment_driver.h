/******************************************************************************
 * @file    seven_segment_driver.h
 * @brief   ควบคุมจอ 7-segment ผ่าน BCD driver IC บน Training Shield
 *          (ตาม intro slide: "1 7-segment display with BCD driver IC")
 *
 *          หลักการ: BCD driver IC (เช่น 7447/4511) จะแปลงรหัส 4-bit BCD
 *          เป็นลายไฟ 7 segment ให้เองในฮาร์ดแวร์ — driver ตัวนี้จึงแค่ต้อง
 *          ส่งเลข 0-9 ออกไปเป็น 4 บิตผ่าน GPIO 4 ขา ไม่ต้องคุมทีละ segment
 *
 *          Pin BCD ทั้ง 4 (APP_SEG_BCD_x_* ใน app_config.h) ยืนยันแล้วจาก
 *          Guide_Exam_1.pdf (ตาราง pin mapping อย่างเป็นทางการของ Training
 *          Shield 1 Rev 03.00): PC7, PA8, PB10, PA9 ตามลำดับ 2^0 ถึง 2^3
 ******************************************************************************/
#ifndef SEVEN_SEGMENT_DRIVER_H
#define SEVEN_SEGMENT_DRIVER_H

#include <stdint.h>

/** ตั้งค่าขา BCD ทั้ง 4 เป็น output และเริ่มต้นที่เลข 0 */
void SevenSegment_Driver_Init(void);

/**
 * @brief  แสดงเลข 0-9 บนจอ 7-segment ผ่านการส่งรหัส BCD
 * @param  digit : เลขที่ต้องการแสดง (0-9) ถ้าส่งค่าเกิน 9 จะถูก clamp เหลือ 9
 */
void SevenSegment_Driver_ShowDigit(uint8_t digit);

#endif /* SEVEN_SEGMENT_DRIVER_H */
