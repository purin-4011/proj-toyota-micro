/******************************************************************************
 * @file    core_driver.c
 * @brief   Implementation ของ core driver (register-level, raw address)
 ******************************************************************************/
#include <stdint.h>
#include "core_driver.h"

#define REG32(addr)  (*(volatile uint32_t *)(addr))

/* --- SCB CPACR (Coprocessor Access Control, PM0214 Section 4.6.1) ---
 * CP10 (bit 21:20) และ CP11 (bit 23:22) = 11 -> Full access ให้ FPU */
#define SCB_CPACR_ADDR            (0xE000ED88UL)
#define SCB_CPACR_CP10_CP11_FULL  (0xFUL << 20U)

/* --- DBGMCU_CR (RM0383 Section 23.16.3) ---
 * DBG_SLEEP = 1: ให้ clock ของ debug ยังทำงานตอน CPU อยู่ใน Sleep mode
 * ไม่งั้น ST-Link อาจเชื่อมต่อ/แฟลชไม่ได้เพราะ CPU หลับอยู่เกือบตลอด */
#define DBGMCU_CR_ADDR            (0xE0042004UL)
#define DBGMCU_CR_DBG_SLEEP_BIT   (0U)

void Core_Driver_Init(void)
{
    REG32(SCB_CPACR_ADDR) |= SCB_CPACR_CP10_CP11_FULL;
    __asm volatile ("dsb");
    __asm volatile ("isb");

    REG32(DBGMCU_CR_ADDR) |= (1UL << DBGMCU_CR_DBG_SLEEP_BIT);
}

void Core_Driver_WaitForInterrupt(void)
{
    __asm volatile ("wfi");
}
