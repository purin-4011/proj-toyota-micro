/******************************************************************************
 * @file    env_monitor.h
 * @brief   Pure logic module (ไม่แตะ hardware) — แปลงค่า ADC ดิบของ NTC และ
 *          LDR เป็นอุณหภูมิ (องศา) และความสว่าง (lux) แล้วตัดสินว่า
 *          1) มีคนอยู่ใกล้ไหม (แสงลดลงจากค่าปกติมาก = มีคนบังแสง)
 *          2) อุณหภูมิผิดปกติไหม (ต่างจากค่าปกติเกินเกณฑ์ = ถูกโจมตีด้วย
 *             ความร้อน/ความเย็น) — เมื่อเกิดแล้วจะ "ค้าง" (latched) จนกว่า
 *             admin จะสั่ง EnvMonitor_ClearTamper() ผ่านคำสั่ง RESET
 *
 *          "ค่าปกติ" (baseline) = ค่าเฉลี่ยของ sample แรกหลังบูต
 ******************************************************************************/
#ifndef ENV_MONITOR_H
#define ENV_MONITOR_H

#include <stdint.h>
#include <stdbool.h>

typedef enum
{
    ENV_NOTIFY_TAMPER_DETECTED = 0U    /* อุณหภูมิผิดปกติ (แจ้งครั้งเดียวตอนเริ่มค้าง) */
} EnvMonitor_Notification_t;

typedef void (*EnvMonitor_NotifyCallback_t)(EnvMonitor_Notification_t notification);

void EnvMonitor_Init(EnvMonitor_NotifyCallback_t notify_callback);

/**
 * @brief  ป้อนค่าดิบจาก ADC 1 รอบ (เรียกจาก JEOC callback ทุก 200ms)
 */
void EnvMonitor_OnSample(uint16_t ntc_raw, uint16_t ldr_raw);

/** มีคนอยู่ใกล้ (บังแสง) หรือไม่ — คืน false เสมอจนกว่าจะได้ค่าปกติแล้ว */
bool EnvMonitor_IsPresenceDetected(void);

/** อุณหภูมิผิดปกติค้างอยู่หรือไม่ */
bool EnvMonitor_IsTamperLatched(void);

/** ล้างสถานะอุณหภูมิผิดปกติ (admin RESET) — ถ้ายังผิดปกติอยู่จะเกิดใหม่ทันที */
void EnvMonitor_ClearTamper(void);

/** อุณหภูมิล่าสุด / ค่าปกติ หน่วย 0.1 องศา (เช่น 274 = 27.4 องศา) */
int32_t EnvMonitor_GetTemperatureDeciC(void);
int32_t EnvMonitor_GetBaselineTemperatureDeciC(void);

/** ความสว่างล่าสุด / ค่าปกติ หน่วย lux */
uint32_t EnvMonitor_GetLux(void);
uint32_t EnvMonitor_GetBaselineLux(void);

#endif /* ENV_MONITOR_H */
