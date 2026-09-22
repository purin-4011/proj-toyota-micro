/******************************************************************************
 * @file    dial_lock.h
 * @brief   แปลงค่า ADC ดิบจาก potentiometer (0-4095) เป็นโซน 1-9 และเทียบ
 *          กับโซนเป้าหมาย (ล็อกชั้นที่ 2 ตามที่ตกลงกัน) — เป็น pure logic
 *          ไม่แตะ hardware โดยตรง (เหมือน code_decoder/code_storage)
 *
 *          โซนเป้าหมาย default = 5 (hardcode ไว้ก่อน คู่กับรหัส default
 *          SHORT-SHORT-SHORT-SHORT เหมือนที่ code_storage ทำ) รอ Setup mode
 *          จริงในอนาคตค่อยทำให้ตั้งค่าใหม่ได้
 ******************************************************************************/
#ifndef DIAL_LOCK_H
#define DIAL_LOCK_H

#include <stdint.h>
#include <stdbool.h>

/** จำนวนโซนทั้งหมด (แบ่งช่วงหมุนเต็ม 0-4095 ออกเป็น 9 ช่วง ตรงกับเลข 1-9
 *  บน 7-segment พอดี ไม่ต้อง map เพิ่ม) */
#define DIAL_LOCK_ZONE_COUNT   (9U)

/** โซนเป้าหมาย default */
#define DIAL_LOCK_DEFAULT_TARGET_ZONE   (5U)

/** เตรียมค่าเริ่มต้น (โซนเป้าหมาย = default) */
void DialLock_Init(void);

/**
 * @brief  เรียกทุกครั้งที่ ADC แปลงค่าใหม่เสร็จ (ผ่าน EOC callback ของ
 *         adc_driver) เพื่ออัพเดตโซนปัจจุบันที่เก็บไว้ภายใน module
 * @param  raw_value : ค่าดิบจาก ADC (0-4095)
 */
void DialLock_UpdateRaw(uint16_t raw_value);

/** อ่านโซนปัจจุบัน (1-9) จากค่า ADC ล่าสุดที่อัพเดตไว้ */
uint8_t DialLock_GetCurrentZone(void);

/** เช็คว่าโซนปัจจุบันตรงกับโซนเป้าหมายหรือไม่ */
bool DialLock_IsAtTargetZone(void);

/**
 * @brief  คำนวณขอบเขตค่า ADC ดิบ (low, high) ของโซนเป้าหมาย สำหรับเอาไป
 *         ตั้งค่า Analog Watchdog ใน adc_driver
 * @param  p_low, p_high : ตัวชี้ที่จะรับค่าขอบเขตออกไป (ห้าม NULL)
 */
void DialLock_GetTargetZoneBounds(uint16_t * p_low, uint16_t * p_high);

/**
 * @brief  ตั้งโซนเป้าหมายใหม่ (ใช้ตอน Setup Mode บันทึกค่าที่ผู้ใช้เลือก)
 * @param  zone : โซนเป้าหมายใหม่ (จะถูก clamp ให้อยู่ใน [1, DIAL_LOCK_ZONE_COUNT])
 */
void DialLock_SetTargetZone(uint8_t zone);

#endif /* DIAL_LOCK_H */
