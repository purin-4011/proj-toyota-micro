/******************************************************************************
 * @file    gpio_driver.h
 * @brief   GPIO driver — Register-level (bare-metal) สำหรับ STM32F411RE
 *          เขียนตามสไตล์ที่สอนในคอร์ส: define register เป็น address ตรงๆ
 *          ไม่พึ่งพา CMSIS struct (GPIOA->MODER แบบ HAL/LL ใช้)
 *
 *          MISRA-C notes:
 *          - fixed-width types (uint8_t/uint32_t) แทน int
 *          - ไม่มี dynamic memory
 *          - ทุกไฟล์มี header guard
 ******************************************************************************/
#ifndef GPIO_DRIVER_H
#define GPIO_DRIVER_H

#include <stdint.h>

/* ---------------------------------------------------------------------- *
 * Base address ของแต่ละ GPIO port (อ้างอิง RM0383 Memory Map)
 * ผู้เรียกใช้ driver นี้จะส่งค่าคงที่พวกนี้เข้ามาแทนการส่ง struct pointer
 * ---------------------------------------------------------------------- */
#define GPIOA_BASE   (0x40020000UL)
#define GPIOB_BASE   (0x40020400UL)
#define GPIOC_BASE   (0x40020800UL)

/* ---------------------------------------------------------------------- *
 * Public type definitions
 * ---------------------------------------------------------------------- */
typedef enum
{
    GPIO_MODE_INPUT  = 0x00U,
    GPIO_MODE_OUTPUT = 0x01U
} GPIO_Mode_t;

typedef enum
{
    GPIO_PULL_NONE = 0x00U,
    GPIO_PULL_UP   = 0x01U,
    GPIO_PULL_DOWN = 0x02U
} GPIO_Pull_t;

typedef enum
{
    GPIO_PIN_RESET = 0U,
    GPIO_PIN_SET   = 1U
} GPIO_PinState_t;

/* ---------------------------------------------------------------------- *
 * Public function prototypes
 * ---------------------------------------------------------------------- */

/**
 * @brief  เปิดสัญญาณนาฬิกาให้ GPIO port (ต้องเรียกก่อนใช้งาน port นั้นเสมอ)
 * @param  port_base : ที่อยู่ฐานของ port เช่น GPIOA_BASE, GPIOB_BASE, GPIOC_BASE
 */
void GPIO_Driver_EnableClock(uint32_t port_base);

/**
 * @brief  ตั้งค่าโหมดและ pull ของขา GPIO หนึ่งขา
 * @param  port_base : ที่อยู่ฐานของ port
 * @param  pin       : หมายเลขขา (0-15)
 * @param  mode      : GPIO_MODE_INPUT หรือ GPIO_MODE_OUTPUT
 * @param  pull      : ตัวเลือก pull-up/down/none
 */
void GPIO_Driver_Init(uint32_t port_base, uint8_t pin, GPIO_Mode_t mode, GPIO_Pull_t pull);

/**
 * @brief  เขียนค่าลอจิกให้ขา output (ใช้ BSRR เพื่อให้เป็น atomic operation)
 */
void GPIO_Driver_WritePin(uint32_t port_base, uint8_t pin, GPIO_PinState_t state);

/**
 * @brief  สลับสถานะขา output (toggle)
 */
void GPIO_Driver_TogglePin(uint32_t port_base, uint8_t pin);

/**
 * @brief  อ่านค่าลอจิกปัจจุบันของขา
 */
GPIO_PinState_t GPIO_Driver_ReadPin(uint32_t port_base, uint8_t pin);

#endif /* GPIO_DRIVER_H */
