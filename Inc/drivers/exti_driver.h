/******************************************************************************
 * @file    exti_driver.h
 * @brief   EXTI driver (register-level, raw address) — จับ press/release
 *          ของปุ่ม PB4 ผ่าน EXTI Line 4
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
 * @brief  ตั้งค่า EXTI Line 4 ให้ทำงานกับขา PB4 ครบทุกขั้นตอน:
 *         เปิด clock GPIOB+SYSCFG, ตั้ง PB4 เป็น input pull-up,
 *         ผูก EXTI4 เข้ากับ Port B, เปิด rising+falling trigger, เปิด NVIC
 * @param  callback : ฟังก์ชันที่จะถูกเรียกเมื่อเกิด interrupt (ห้าม NULL)
 */
void EXTI_Driver_Init(EXTI_Callback_t callback);

#endif /* EXTI_DRIVER_H */
