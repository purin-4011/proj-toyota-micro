/******************************************************************************
 * @file    crc_driver.c
 * @brief   Implementation ของ CRC driver (register-level, raw address)
 ******************************************************************************/
#include "crc_driver.h"

#define REG32(addr)  (*(volatile uint32_t *)(addr))

/* --- RCC --- */
#define RCC_BASE                  (0x40023800UL)
#define RCC_OFFSET_AHB1ENR        (0x30UL)
#define RCC_AHB1ENR_CRCEN_BIT     (12U)

/* --- CRC (RM0383 Section 5) --- */
#define CRC_BASE                  (0x40023000UL)
#define CRC_OFFSET_DR             (0x00UL)
#define CRC_OFFSET_CR             (0x08UL)
#define CRC_CR_RESET_BIT          (0U)

void CRC_Driver_Init(void)
{
    REG32(RCC_BASE + RCC_OFFSET_AHB1ENR) |= (1UL << RCC_AHB1ENR_CRCEN_BIT);
}

void CRC_Driver_Reset(void)
{
    /* เขียน 1 ลงบิต RESET จะทำให้ DR กลับไปเป็นค่าเริ่มต้น 0xFFFFFFFF
     * (บิตนี้ hardware จะ clear ให้เองอัตโนมัติหลัง reset เสร็จ) */
    REG32(CRC_BASE + CRC_OFFSET_CR) |= (1UL << CRC_CR_RESET_BIT);
}

uint32_t CRC_Driver_FeedWord(uint32_t const word)
{
    /* เขียนข้อมูลลง DR -> ฮาร์ดแวร์คำนวณ CRC สะสมทันที (ใช้เวลาไม่กี่
     * clock cycle) แล้วอ่านค่า DR กลับมาเป็นผลลัพธ์ปัจจุบัน */
    REG32(CRC_BASE + CRC_OFFSET_DR) = word;
    return REG32(CRC_BASE + CRC_OFFSET_DR);
}
