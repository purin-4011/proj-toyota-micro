/******************************************************************************
 * @file    adc_driver.h
 * @brief   ADC driver (register-level, raw address) สำหรับ ADC1 Channel 4
 *          (PA4 - potentiometer) มี 2 ความสามารถ:
 *
 *          1) Continuous conversion + EOC interrupt:
 *             อ่านค่า potentiometer ต่อเนื่องตลอดเวลาผ่าน interrupt ทุกครั้ง
 *             ที่แปลงค่าเสร็จ (ไม่มี polling เลย) ใช้แสดงตำแหน่งบน 7-segment
 *             ระหว่างที่ผู้ใช้กำลังหมุนหา (สถานะ IDLE)
 *
 *          2) Analog Watchdog:
 *             วงจรเปรียบเทียบในตัว ADC เอง คอยเช็คว่าค่าที่แปลงได้อยู่ใน
 *             ช่วง [low, high] ที่กำหนดไว้หรือไม่ ถ้าออกนอกช่วงจะสร้าง
 *             interrupt ทันทีในฮาร์ดแวร์โดย CPU ไม่ต้องเปรียบเทียบเอง —
 *             ใช้ตรวจจับทันทีที่ potentiometer หลุดออกจากโซนเป้าหมาย
 *             ระหว่างกำลังป้อนรหัส (สถานะ ENTERING)
 ******************************************************************************/
#ifndef ADC_DRIVER_H
#define ADC_DRIVER_H

#include <stdint.h>

/** เรียกทุกครั้งที่แปลงค่าเสร็จ 1 ครั้ง (continuous mode) ส่งค่าดิบ 0-4095 */
typedef void (*ADC_EocCallback_t)(uint16_t raw_value);

/** เรียกเมื่อ Analog Watchdog ตรวจพบว่าค่าออกนอกช่วงที่กำหนดไว้ */
typedef void (*ADC_WatchdogCallback_t)(void);

/** เรียกเมื่อ injected group แปลงเสร็จครบ (JEOC) ส่งค่าดิบ NTC และ LDR (0-4095) */
typedef void (*ADC_InjectedCallback_t)(uint16_t ntc_raw, uint16_t ldr_raw);

/**
 * @brief  เริ่มต้น ADC1:
 *         - Regular group: Channel 4 (PA4, potentiometer) แปลงต่อเนื่อง +
 *           EOC interrupt (ยังไม่เปิด Analog Watchdog)
 *         - Injected group: Channel 0 (PA0, NTC) และ Channel 1 (PA1, LDR)
 *           แปลงเมื่อสั่ง ADC_Driver_StartInjected() + JEOC interrupt
 * @param  eoc_callback      : เรียกทุกครั้งที่ potentiometer แปลงเสร็จ (ห้าม NULL)
 * @param  injected_callback : เรียกเมื่อ NTC + LDR แปลงเสร็จครบ (ห้าม NULL)
 */
void ADC_Driver_Init(ADC_EocCallback_t eoc_callback, ADC_InjectedCallback_t injected_callback);

/** สั่งแปลง injected group (NTC + LDR) 1 รอบ ผลมาทาง injected callback */
void ADC_Driver_StartInjected(void);

/** หยุดการแปลง potentiometer แบบต่อเนื่อง (ใช้ตอน sleep) */
void ADC_Driver_PauseRegular(void);

/** กลับมาแปลง potentiometer แบบต่อเนื่อง (ใช้ตอนตื่นจาก sleep) */
void ADC_Driver_ResumeRegular(void);

/** อ่านค่าดิบล่าสุดที่แปลงได้ (0-4095) โดยไม่ต้องรอ conversion ใหม่ */
uint16_t ADC_Driver_GetLatestValue(void);

/**
 * @brief  เปิด Analog Watchdog ให้เฝ้าดู channel เดียวกับที่ Init ไว้
 *         ถ้าค่าที่แปลงได้ครั้งถัดไปอยู่นอกช่วง [low, high] จะเรียก
 *         callback ทันทีจาก ISR (เร็วกว่าการรอ CPU มาเปรียบเทียบเอง)
 * @param  low, high : ขอบเขตช่วงที่ยอมรับได้ (ค่าดิบ 0-4095)
 * @param  callback  : ฟังก์ชันที่ถูกเรียกเมื่อค่าหลุดช่วง (ห้าม NULL)
 */
void ADC_Driver_EnableWatchdog(uint16_t low, uint16_t high, ADC_WatchdogCallback_t callback);

/** ปิด Analog Watchdog (หยุดเฝ้าดูช่วง) */
void ADC_Driver_DisableWatchdog(void);

#endif /* ADC_DRIVER_H */
