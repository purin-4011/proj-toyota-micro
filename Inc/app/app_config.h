/******************************************************************************
 * @file    app_config.h
 * @brief   ค่าคงที่ระดับ Application: pin mapping และ threshold ต่างๆ
 *          รวมไว้ที่เดียวเพื่อแก้ pin/threshold ได้จากจุดเดียว
 ******************************************************************************/
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <stdint.h>
#include "gpio_driver.h"

/* ---------------------------------------------------------------------- *
 * Pin Mapping (อ้างอิงจาก Block Diagram ใน Project Proposal)
 * ---------------------------------------------------------------------- */
#define APP_BUTTON_PORT        GPIOB_BASE
#define APP_BUTTON_PIN         (4U)   /* PB4  - EXTI4 */

#define APP_LED_GREEN_PORT     GPIOA_BASE
#define APP_LED_GREEN_PIN      (5U)   /* PA7 (D11) - ลองใหม่เพราะ PA5 จริงๆ
                                       * เป็นสีฟ้า ไม่ใช่เขียว (เจอจากทดสอบจริง)
                                       * ถ้า PA7 ไม่ใช่เขียวอีก ให้ลองสลับเป็น
                                       * GPIOB_BASE / pin 6 (PB6, D10) แทน */

#define APP_LED_G_PORT     GPIOB_BASE
#define APP_LED_G_PIN      (6U)

#define APP_LED_RED_PORT       GPIOA_BASE
#define APP_LED_RED_PIN        (6U)   /* PA6  - ผิด/ล็อกอยู่ */

#define APP_LED_YELLOW_PORT    GPIOA_BASE
#define APP_LED_YELLOW_PIN     (7U)   /* PA7 (D11) - ติดค้างระหว่างอยู่ใน Setup Mode */

/* ---------------------------------------------------------------------- *
 * ปุ่ม Setup Mode (เพิ่มใหม่)
 * - PB5 (D4)  : กดค้าง >= APP_SETUP_ENTRY_HOLD_MS เพื่อเข้า Setup Mode,
 *               กดสั้นเพื่อ confirm/exit แต่ละขั้นตอนภายใน Setup Mode
 * - PA10 (D2) : เพิ่มจำนวนหลักรหัส (เฉพาะตอนขั้นเลือกจำนวนหลัก)
 * - PB3 (D3)  : ลดจำนวนหลักรหัส (เฉพาะตอนขั้นเลือกจำนวนหลัก)
 * ---------------------------------------------------------------------- */
#define APP_SETUP_BUTTON_PORT   GPIOB_BASE
#define APP_SETUP_BUTTON_PIN    (5U)

#define APP_DIGIT_UP_PORT       GPIOA_BASE
#define APP_DIGIT_UP_PIN        (10U)

#define APP_DIGIT_DOWN_PORT     GPIOB_BASE
#define APP_DIGIT_DOWN_PIN      (3U)

/* ---------------------------------------------------------------------- *
 * Timing Thresholds (หน่วย: มิลลิวินาที)
 * ---------------------------------------------------------------------- */
#define APP_PRESS_THRESHOLD_MS      (500U)
#define APP_INPUT_TIMEOUT_MS        (1500U)
#define APP_LOCKOUT_DURATION_SEC    (9U)
#define APP_MAX_WRONG_ATTEMPTS      (3U)
#define APP_SETUP_ENTRY_HOLD_MS     (3000U)  /* ต้องกด PB5 ค้างนานเท่านี้ถึงจะเข้า Setup Mode */

/* ---------------------------------------------------------------------- *
 * แปลงเวลาเป็นจำนวน TIM3 tick (1 tick = 100ms ตาม TIMER_DRIVER_TICK_MS)
 * เพื่อให้ lock_fsm นับ tick ได้ตรงๆ โดยไม่ต้องคูณ/หารซ้ำทุกครั้งที่เรียก
 * ---------------------------------------------------------------------- */
#define APP_INPUT_TIMEOUT_TICKS     (15U)   /* 1500ms / 100ms ต่อ tick */
#define APP_LOCKOUT_TICKS           (100U)   /* 9s / 100ms ต่อ tick */
#define APP_UNLOCK_HOLD_TICKS       (50U)  /* 10s ตามที่ proposal ระบุให้
                                              * แสดงผลปลดล็อกสำเร็จค้างไว้ */
#define APP_TICKS_PER_SECOND        (10U)   /* 1000ms / 100ms ต่อ tick */

/* ---------------------------------------------------------------------- *
 * 7-Segment BCD input pins — ยืนยันแล้วจาก Guide_Exam_1.pdf (ตาราง pin
 * mapping อย่างเป็นทางการ พร้อมรูป Training Shield 1 Rev 03.00 จริง)
 *
 * หมายเหตุจากสไลด์: ถ้าใช้ shield สีน้ำเงิน (blue shield) ป้าย silkscreen
 * บนบอร์ดจะ "ผิด" ให้ยึดตาม pin mapping นี้ (ของ shield สีแดง) แทนเสมอ
 * ---------------------------------------------------------------------- */
#define APP_SEG_BCD_A_PORT   GPIOC_BASE   /* 2^0 (LSB) -> PC7 */
#define APP_SEG_BCD_A_PIN    (7U)
#define APP_SEG_BCD_B_PORT   GPIOA_BASE   /* 2^1       -> PA8 */
#define APP_SEG_BCD_B_PIN    (8U)
#define APP_SEG_BCD_C_PORT   GPIOB_BASE   /* 2^2       -> PB10 */
#define APP_SEG_BCD_C_PIN    (10U)
#define APP_SEG_BCD_D_PORT   GPIOA_BASE   /* 2^3 (MSB) -> PA9 */
#define APP_SEG_BCD_D_PIN    (9U)

#endif /* APP_CONFIG_H */
