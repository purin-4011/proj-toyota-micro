/******************************************************************************
 * @file    seven_segment_driver.c
 * @brief   Implementation ของ 7-segment BCD driver
 *          ใช้ gpio_driver เป็นชั้นล่าง (ไม่แตะ register ตรงๆ ในไฟล์นี้)
 ******************************************************************************/
#include "seven_segment_driver.h"
#include "gpio_driver.h"
#include "app_config.h"

#define SEVEN_SEGMENT_MAX_DIGIT   (9U)
#define SEVEN_SEGMENT_BIT_A       (0x1U)
#define SEVEN_SEGMENT_BIT_B       (0x2U)
#define SEVEN_SEGMENT_BIT_C       (0x4U)
#define SEVEN_SEGMENT_BIT_D       (0x8U)

/** แปลง bit เป็น GPIO_PinState_t (0/1 -> RESET/SET) */
static GPIO_PinState_t SevenSegment_BitToPinState(uint8_t const value, uint8_t const bit_mask)
{
    GPIO_PinState_t result;

    if ((value & bit_mask) != 0U)
    {
        result = GPIO_PIN_SET;
    }
    else
    {
        result = GPIO_PIN_RESET;
    }

    return result;
}

void SevenSegment_Driver_Init(void)
{
    GPIO_Driver_EnableClock(APP_SEG_BCD_A_PORT);
    GPIO_Driver_Init(APP_SEG_BCD_A_PORT, APP_SEG_BCD_A_PIN, GPIO_MODE_OUTPUT, GPIO_PULL_NONE);

    GPIO_Driver_EnableClock(APP_SEG_BCD_B_PORT);
    GPIO_Driver_Init(APP_SEG_BCD_B_PORT, APP_SEG_BCD_B_PIN, GPIO_MODE_OUTPUT, GPIO_PULL_NONE);

    GPIO_Driver_EnableClock(APP_SEG_BCD_C_PORT);
    GPIO_Driver_Init(APP_SEG_BCD_C_PORT, APP_SEG_BCD_C_PIN, GPIO_MODE_OUTPUT, GPIO_PULL_NONE);

    GPIO_Driver_EnableClock(APP_SEG_BCD_D_PORT);
    GPIO_Driver_Init(APP_SEG_BCD_D_PORT, APP_SEG_BCD_D_PIN, GPIO_MODE_OUTPUT, GPIO_PULL_NONE);

    SevenSegment_Driver_ShowDigit(0U);
}

void SevenSegment_Driver_ShowDigit(uint8_t digit)
{
    if (digit > SEVEN_SEGMENT_MAX_DIGIT)
    {
        digit = SEVEN_SEGMENT_MAX_DIGIT;
    }
    else
    {
        /* อยู่ในช่วง 0-9 แล้ว ไม่ต้องปรับ */
    }

    GPIO_Driver_WritePin(APP_SEG_BCD_A_PORT, APP_SEG_BCD_A_PIN,
                          SevenSegment_BitToPinState(digit, SEVEN_SEGMENT_BIT_A));
    GPIO_Driver_WritePin(APP_SEG_BCD_B_PORT, APP_SEG_BCD_B_PIN,
                          SevenSegment_BitToPinState(digit, SEVEN_SEGMENT_BIT_B));
    GPIO_Driver_WritePin(APP_SEG_BCD_C_PORT, APP_SEG_BCD_C_PIN,
                          SevenSegment_BitToPinState(digit, SEVEN_SEGMENT_BIT_C));
    GPIO_Driver_WritePin(APP_SEG_BCD_D_PORT, APP_SEG_BCD_D_PIN,
                          SevenSegment_BitToPinState(digit, SEVEN_SEGMENT_BIT_D));
}
