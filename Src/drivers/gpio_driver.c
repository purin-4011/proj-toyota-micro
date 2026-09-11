/******************************************************************************
 * @file    gpio_driver.c
 * @brief   Implementation ของ GPIO driver (register-level, raw address)
 ******************************************************************************/
#include "gpio_driver.h"

/* ---------------------------------------------------------------------- *
 * Register offset ภายใน GPIO port (เหมือนกันทุก port ต่างกันแค่ base address)
 * อ้างอิง RM0383 Section 6.4 (GPIO registers)
 * ---------------------------------------------------------------------- */
#define GPIO_OFFSET_MODER    (0x00UL)
#define GPIO_OFFSET_PUPDR    (0x0CUL)
#define GPIO_OFFSET_IDR      (0x10UL)
#define GPIO_OFFSET_ODR      (0x14UL)
#define GPIO_OFFSET_BSRR     (0x18UL)

/* ---------------------------------------------------------------------- *
 * RCC (สำหรับเปิด clock ของแต่ละ GPIO port) — RM0383 Section 6.3.10
 * ---------------------------------------------------------------------- */
#define RCC_BASE             (0x40023800UL)
#define RCC_OFFSET_AHB1ENR   (0x30UL)
#define RCC_AHB1ENR_GPIOAEN_BIT (0U)
#define RCC_AHB1ENR_GPIOBEN_BIT (1U)
#define RCC_AHB1ENR_GPIOCEN_BIT (2U)

/** Macro ช่วยแปลง address ธรรมดาให้เป็น volatile 32-bit register access
 *  (คือหัวใจของเทคนิค memory-mapped I/O ที่สอนใน Chapter 0.4) */
#define REG32(addr)  (*(volatile uint32_t *)(addr))

#define GPIO_BITS_PER_PIN  (2U)
#define GPIO_FIELD_MASK    (0x3UL)
#define GPIO_INVALID_BIT   (0xFFU)

/* ---------------------------------------------------------------------- *
 * Public function implementations
 * ---------------------------------------------------------------------- */

void GPIO_Driver_EnableClock(uint32_t const port_base)
{
    uint32_t clk_bit;

    if (port_base == GPIOA_BASE)
    {
        clk_bit = RCC_AHB1ENR_GPIOAEN_BIT;
    }
    else if (port_base == GPIOB_BASE)
    {
        clk_bit = RCC_AHB1ENR_GPIOBEN_BIT;
    }
    else if (port_base == GPIOC_BASE)
    {
        clk_bit = RCC_AHB1ENR_GPIOCEN_BIT;
    }
    else
    {
        clk_bit = GPIO_INVALID_BIT;
    }

    if (clk_bit != GPIO_INVALID_BIT)
    {
        REG32(RCC_BASE + RCC_OFFSET_AHB1ENR) |= (1UL << clk_bit);
    }
    else
    {
        /* MISRA: else บังคับ — port ที่ไม่รองรับ (โปรเจคนี้ใช้แค่ A/B/C) */
    }
}

void GPIO_Driver_Init(uint32_t const port_base,
                       uint8_t const pin,
                       GPIO_Mode_t const mode,
                       GPIO_Pull_t const pull)
{
    uint32_t const shift = (uint32_t) pin * GPIO_BITS_PER_PIN;
    uint32_t moder_val;
    uint32_t pupdr_val;

    moder_val = REG32(port_base + GPIO_OFFSET_MODER);
    moder_val &= ~(GPIO_FIELD_MASK << shift);
    moder_val |= ((uint32_t) mode << shift);
    REG32(port_base + GPIO_OFFSET_MODER) = moder_val;

    pupdr_val = REG32(port_base + GPIO_OFFSET_PUPDR);
    pupdr_val &= ~(GPIO_FIELD_MASK << shift);
    pupdr_val |= ((uint32_t) pull << shift);
    REG32(port_base + GPIO_OFFSET_PUPDR) = pupdr_val;
}

void GPIO_Driver_WritePin(uint32_t const port_base,
                           uint8_t const pin,
                           GPIO_PinState_t const state)
{
    if (state == GPIO_PIN_SET)
    {
        /* เขียนครึ่งล่างของ BSRR = SET (atomic, ปลอดภัยกว่า read-modify-write ODR) */
        REG32(port_base + GPIO_OFFSET_BSRR) = (1UL << pin);
    }
    else
    {
        /* เขียนครึ่งบนของ BSRR (bit 16-31) = RESET */
        REG32(port_base + GPIO_OFFSET_BSRR) = (1UL << (pin + 16U));
    }
}

void GPIO_Driver_TogglePin(uint32_t const port_base, uint8_t const pin)
{
    REG32(port_base + GPIO_OFFSET_ODR) ^= (1UL << pin);
}

GPIO_PinState_t GPIO_Driver_ReadPin(uint32_t const port_base, uint8_t const pin)
{
    GPIO_PinState_t result;
    uint32_t const idr_val = REG32(port_base + GPIO_OFFSET_IDR);

    if ((idr_val & (1UL << pin)) != 0U)
    {
        result = GPIO_PIN_SET;
    }
    else
    {
        result = GPIO_PIN_RESET;
    }

    return result;
}
