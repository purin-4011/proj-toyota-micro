/******************************************************************************
 * @file    dial_lock.c
 * @brief   Implementation ของ dial_lock (pure logic, ไม่แตะ hardware)
 ******************************************************************************/
#include "dial_lock.h"

#define DIAL_LOCK_ADC_MAX        (4095U)   /* ADC 12-bit: 0-4095 */
#define DIAL_LOCK_ZONE_WIDTH     ((DIAL_LOCK_ADC_MAX + 1U) / DIAL_LOCK_ZONE_COUNT)

static uint16_t s_latest_raw_value = 0U;
static uint8_t s_target_zone = DIAL_LOCK_DEFAULT_TARGET_ZONE;

/**
 * @brief  แปลงค่า ADC ดิบเป็นหมายเลขโซน (1 ถึง DIAL_LOCK_ZONE_COUNT)
 */
static uint8_t DialLock_RawToZone(uint16_t const raw_value)
{
    uint8_t zone;
    uint32_t const zone_index = (uint32_t) raw_value / (uint32_t) DIAL_LOCK_ZONE_WIDTH;

    if (zone_index >= (uint32_t) DIAL_LOCK_ZONE_COUNT)
    {
        /* กันค่าที่อยู่ปลายสุดของช่วง (ปัดเศษ) ไม่ให้หลุดโซนสูงสุด */
        zone = (uint8_t) DIAL_LOCK_ZONE_COUNT;
    }
    else
    {
        zone = (uint8_t) (zone_index + 1U);
    }

    return zone;
}

void DialLock_Init(void)
{
    s_latest_raw_value = 0U;
    s_target_zone = DIAL_LOCK_DEFAULT_TARGET_ZONE;
}

void DialLock_UpdateRaw(uint16_t const raw_value)
{
    s_latest_raw_value = raw_value;
}

uint8_t DialLock_GetCurrentZone(void)
{
    return DialLock_RawToZone(s_latest_raw_value);
}

bool DialLock_IsAtTargetZone(void)
{
    bool result;

    if (DialLock_GetCurrentZone() == s_target_zone)
    {
        result = true;
    }
    else
    {
        result = false;
    }

    return result;
}

void DialLock_GetTargetZoneBounds(uint16_t * const p_low, uint16_t * const p_high)
{
    if ((p_low != (uint16_t *) 0) && (p_high != (uint16_t *) 0))
    {
        uint32_t low_calc;
        uint32_t high_calc;

        low_calc = ((uint32_t) s_target_zone - 1U) * (uint32_t) DIAL_LOCK_ZONE_WIDTH;
        high_calc = ((uint32_t) s_target_zone * (uint32_t) DIAL_LOCK_ZONE_WIDTH) - 1U;

        if (high_calc > (uint32_t) DIAL_LOCK_ADC_MAX)
        {
            /* โซนสุดท้ายอาจมีเศษเหลือจากการหารไม่ลงตัว ให้ครอบคลุมถึง
             * ค่าสูงสุดของ ADC จริง (4095) เสมอ */
            high_calc = (uint32_t) DIAL_LOCK_ADC_MAX;
        }
        else
        {
            /* อยู่ในขอบเขตปกติ ไม่ต้องปรับ */
        }

        *p_low = (uint16_t) low_calc;
        *p_high = (uint16_t) high_calc;
    }
    else
    {
        /* MISRA: else บังคับ — pointer เป็น NULL จะไม่เขียนอะไรออกไป */
    }
}
