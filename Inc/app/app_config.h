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
#define APP_LED_GREEN_PIN      (5U)   /* PA5  - ปลดล็อกสำเร็จ */

#define APP_LED_RED_PORT       GPIOA_BASE
#define APP_LED_RED_PIN        (6U)   /* PA6  - ผิด/ล็อกอยู่ */

/* ---------------------------------------------------------------------- *
 * Timing Thresholds (หน่วย: มิลลิวินาที)
 * ---------------------------------------------------------------------- */
#define APP_PRESS_THRESHOLD_MS      (500U)
#define APP_INPUT_TIMEOUT_MS        (1500U)
#define APP_LOCKOUT_DURATION_SEC    (30U)
#define APP_MAX_WRONG_ATTEMPTS      (3U)

#endif /* APP_CONFIG_H */
