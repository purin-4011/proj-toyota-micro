/******************************************************************************
 * @file    exti_driver.h
 * @brief   EXTI driver (register-level, raw address) — จับ press/release
 *          ของปุ่มทั้ง 4 ตัวที่ใช้ในโปรเจค แต่ละปุ่มมีฟังก์ชัน Init ของ
 *          ตัวเอง เขียนแบบ "คัดลอกจากของเดิมที่ทดสอบผ่านแล้ว" (PB4/EXTI4)
 *          แล้วปรับ port/pin/IRQ ให้ตรงปุ่มใหม่ แทนที่จะเขียนเป็นฟังก์ชัน
 *          generic ตัวเดียวรองรับทุกขา — เพื่อลดความเสี่ยงที่จะพลาดจุดใด
 *          จุดหนึ่งของปุ่มที่ทดสอบผ่านแล้ว (PB4) ระหว่างทำให้ generic
 *
 *          DESIGN NOTE: Driver นี้ไม่รู้เรื่อง business logic ใดๆ ของ Digital
 *          Lock เลย หน้าที่เดียวคือแจ้ง rising/falling event ผ่าน callback
 *          -> นี่คือการแยก Driver/Application ตามเกณฑ์อาจารย์
 ******************************************************************************/
#ifndef EXTI_DRIVER_H
#define EXTI_DRIVER_H

#include <stdint.h>

typedef enum
{
    EXTI_EDGE_RISING  = 0U,   /* ปุ่มถูกกดลง (วงจร pull-up: กด = 0V) */
    EXTI_EDGE_FALLING = 1U    /* ปุ่มถูกปล่อย */
} EXTI_Edge_t;

typedef void (*EXTI_Callback_t)(EXTI_Edge_t edge);

/**
 * @brief  ปุ่มกรอกรหัสหลัก (PB4 / EXTI Line 4) — เหมือนกับเวอร์ชันที่ทดสอบ
 *         ผ่านแล้วทุกประการ ไม่มีการแก้ไข
 * @param  callback : ฟังก์ชันที่จะถูกเรียกเมื่อเกิด interrupt (ห้าม NULL)
 */
void EXTI_Driver_Init(EXTI_Callback_t callback);

/**
 * @brief  ปุ่ม Setup Mode (PB5 / EXTI Line 5) — กดค้างเพื่อเข้า/ออก Setup Mode
 * @param  callback : ฟังก์ชันที่จะถูกเรียกเมื่อเกิด interrupt (ห้าม NULL)
 */
void EXTI_Driver_InitSetupButton(EXTI_Callback_t callback);

/**
 * @brief  ปุ่มเพิ่มจำนวนหลักรหัส ตอน Setup Mode (PA10 / EXTI Line 10)
 * @param  callback : ฟังก์ชันที่จะถูกเรียกเมื่อเกิด interrupt (ห้าม NULL)
 */
void EXTI_Driver_InitDigitUpButton(EXTI_Callback_t callback);

/**
 * @brief  ปุ่มลดจำนวนหลักรหัส ตอน Setup Mode (PB3 / EXTI Line 3)
 * @param  callback : ฟังก์ชันที่จะถูกเรียกเมื่อเกิด interrupt (ห้าม NULL)
 */
void EXTI_Driver_InitDigitDownButton(EXTI_Callback_t callback);

#endif /* EXTI_DRIVER_H */
