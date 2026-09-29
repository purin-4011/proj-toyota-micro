/******************************************************************************
 * @file    env_monitor.c
 * @brief   Implementation ของ env_monitor (pure logic, ไม่แตะ hardware)
 *          ใช้ float + logf/powf (ต้องเปิด FPU ก่อน — ดู Core_Driver_Init)
 ******************************************************************************/
#include <math.h>
#include "env_monitor.h"
#include "app_config.h"

#define ENV_KELVIN_OFFSET     (273.15f)
#define ENV_MAX_LUX           (1000000.0f)

static EnvMonitor_NotifyCallback_t s_notify_callback = (EnvMonitor_NotifyCallback_t) 0;

static float volatile s_temperature_c = 0.0f;
static float volatile s_lux = 0.0f;
static float volatile s_baseline_temp_c = 0.0f;
static float volatile s_baseline_lux = 0.0f;

static float s_temp_sum = 0.0f;
static float s_lux_sum = 0.0f;
static uint8_t s_baseline_count = 0U;
static bool volatile s_baseline_ready = false;

static uint8_t s_abnormal_count = 0U;
static bool volatile s_tamper_latched = false;

/**
 * @brief  แปลงค่า ADC ดิบเป็นความต้านทานของเซนเซอร์ใน voltage divider
 *         R_sensor = Rx * Vout / (Vcc - Vout) และเพราะ Vref = Vcc = 3.3V
 *         จึงลดรูปเหลือ R_sensor = Rx * raw / (4095 - raw)
 *         (clamp raw ไว้ 1..4094 กันหารด้วยศูนย์และ log(0))
 */
static float EnvMonitor_RawToResistance(uint16_t raw, float const rx_ohm)
{
    if (raw < 1U)
    {
        raw = 1U;
    }
    else if (raw > 4094U)
    {
        raw = 4094U;
    }
    else
    {
        /* อยู่ในช่วงที่คำนวณได้ - ไม่ต้องปรับ */
    }

    return (rx_ohm * (float) raw) / (APP_ADC_MAX_RAW - (float) raw);
}

/** Beta equation (สไลด์ 0400_ADC หน้า 32) */
static float EnvMonitor_ResistanceToCelsius(float const r_ntc)
{
    float const t_kelvin = (APP_NTC_BETA * APP_NTC_T0_KELVIN)
                         / ((APP_NTC_T0_KELVIN * logf(r_ntc / APP_NTC_R0_OHM)) + APP_NTC_BETA);

    return t_kelvin - ENV_KELVIN_OFFSET;
}

/** สูตรเดียวกับ Lab 4.3: lux = 10^((log10(R) - OFFSET) / SLOPE) */
static float EnvMonitor_ResistanceToLux(float const r_ldr)
{
    float lux = powf(10.0f, (log10f(r_ldr) - APP_LDR_OFFSET) / APP_LDR_SLOPE);

    if (lux > ENV_MAX_LUX)
    {
        lux = ENV_MAX_LUX;
    }
    else
    {
        /* อยู่ในช่วงปกติ */
    }

    return lux;
}

static void EnvMonitor_Notify(EnvMonitor_Notification_t const notification)
{
    if (s_notify_callback != (EnvMonitor_NotifyCallback_t) 0)
    {
        s_notify_callback(notification);
    }
    else
    {
        /* ไม่มี callback ลงทะเบียนไว้ - ไม่ทำอะไร */
    }
}

/** เก็บ sample ช่วงแรกหลังบูตมาเฉลี่ยเป็นค่าปกติ */
static void EnvMonitor_CollectBaseline(float const temp_c, float const lux)
{
    s_temp_sum += temp_c;
    s_lux_sum += lux;
    s_baseline_count++;

    if (s_baseline_count >= (uint8_t) APP_ENV_BASELINE_SAMPLES)
    {
        s_baseline_temp_c = s_temp_sum / (float) s_baseline_count;
        s_baseline_lux = s_lux_sum / (float) s_baseline_count;
        s_baseline_ready = true;
    }
    else
    {
        /* ยังเก็บไม่ครบ */
    }
}

/** ตรวจอุณหภูมิผิดปกติ (ต้องผิดปกติติดกันหลาย sample กันค่ากระตุก) */
static void EnvMonitor_CheckTemperature(float const temp_c)
{
    float const delta = fabsf(temp_c - s_baseline_temp_c);

    if (s_tamper_latched)
    {
        /* ค้างอยู่แล้ว - รอ admin ล้าง ไม่ปรับค่าปกติระหว่างนี้ */
    }
    else if (delta > APP_TEMP_TAMPER_DELTA_C)
    {
        s_abnormal_count++;

        if (s_abnormal_count >= (uint8_t) APP_TEMP_TAMPER_CONFIRM_SAMPLES)
        {
            s_tamper_latched = true;
            EnvMonitor_Notify(ENV_NOTIFY_TAMPER_DETECTED);
        }
        else
        {
            /* ยังไม่ครบจำนวนที่ต้องยืนยัน */
        }
    }
    else
    {
        s_abnormal_count = 0U;
        /* ปกติ: ให้ค่าปกติขยับตามอุณหภูมิห้องอย่างช้าๆ */
        s_baseline_temp_c += (temp_c - s_baseline_temp_c) / APP_TEMP_BASELINE_TRACK_DIV;
    }
}

void EnvMonitor_Init(EnvMonitor_NotifyCallback_t const notify_callback)
{
    s_notify_callback = notify_callback;
    s_temp_sum = 0.0f;
    s_lux_sum = 0.0f;
    s_baseline_count = 0U;
    s_baseline_ready = false;
    s_abnormal_count = 0U;
    s_tamper_latched = false;
}

void EnvMonitor_OnSample(uint16_t const ntc_raw, uint16_t const ldr_raw)
{
    float const temp_c = EnvMonitor_ResistanceToCelsius(
                             EnvMonitor_RawToResistance(ntc_raw, APP_NTC_RX_OHM));
    float const lux = EnvMonitor_ResistanceToLux(
                          EnvMonitor_RawToResistance(ldr_raw, APP_LDR_RX_OHM));

    s_temperature_c = temp_c;
    s_lux = lux;

    if (s_baseline_ready)
    {
        EnvMonitor_CheckTemperature(temp_c);
    }
    else
    {
        EnvMonitor_CollectBaseline(temp_c, lux);
    }
}

bool EnvMonitor_IsPresenceDetected(void)
{
    bool result;

    if (s_baseline_ready)
    {
        result = (s_lux < ((s_baseline_lux * APP_PRESENCE_LUX_PERCENT) / 100.0f));
    }
    else
    {
        result = false;
    }

    return result;
}

bool EnvMonitor_IsTamperLatched(void)
{
    return s_tamper_latched;
}

void EnvMonitor_ClearTamper(void)
{
    s_abnormal_count = 0U;
    s_tamper_latched = false;
}

/** ปัดเศษ float (องศา) เป็นจำนวนเต็มหน่วย 0.1 องศา */
static int32_t EnvMonitor_ToDeci(float const value)
{
    float const scaled = value * 10.0f;
    int32_t result;

    if (scaled >= 0.0f)
    {
        result = (int32_t) (scaled + 0.5f);
    }
    else
    {
        result = (int32_t) (scaled - 0.5f);
    }

    return result;
}

int32_t EnvMonitor_GetTemperatureDeciC(void)
{
    return EnvMonitor_ToDeci(s_temperature_c);
}

int32_t EnvMonitor_GetBaselineTemperatureDeciC(void)
{
    return EnvMonitor_ToDeci(s_baseline_temp_c);
}

uint32_t EnvMonitor_GetLux(void)
{
    return (uint32_t) (s_lux + 0.5f);
}

uint32_t EnvMonitor_GetBaselineLux(void)
{
    return (uint32_t) (s_baseline_lux + 0.5f);
}
