/******************************************************************************
 * @file    timer_driver.h
 * @brief   Timer driver (register-level, raw address) ตาม
 *          "Two-Tier Timing Architecture":
 *
 *          TIER 1 - TIM2 : Free-running millisecond counter (ไม่มี interrupt)
 *                          Application layer อ่านค่ามาคำนวณ duration เอง
 *          TIER 2 - TIM3 : Periodic interrupt ทุก 100ms
 *                          ใช้ทำทั้ง input-timeout (1.5s) และ lockout
 *                          countdown (30s) ผ่าน mode-switching ใน
 *                          Application layer (driver ไม่รู้เรื่อง mode)
 *
 *          CLOCK ASSUMPTION: timer clock = 16 MHz (HSI default ของ
 *          Nucleo-F411RE ที่ยังไม่ได้ตั้ง PLL เพิ่ม) ถ้าเปลี่ยน SystemClock
 *          ภายหลัง ต้องแก้ TIMER_DRIVER_TIMCLK_HZ ด้านล่างให้ตรงความถี่จริง
 ******************************************************************************/
#ifndef TIMER_DRIVER_H
#define TIMER_DRIVER_H

#include <stdint.h>

#define TIMER_DRIVER_TIMCLK_HZ   (16000000UL)
#define TIMER_DRIVER_TICK_MS     (100U)

typedef void (*Timer_TickCallback_t)(void);

/** เริ่มต้น TIM2 เป็น free-running millisecond counter (นับ 1 ครั้ง = 1ms) */
void Timer_Driver_TIM2_Init(void);

/** อ่านค่า tick ปัจจุบันของ TIM2 (หน่วย ms) */
uint32_t Timer_Driver_TIM2_GetTick(void);

/** เริ่มต้น TIM3 ให้ interrupt ทุก 100ms พร้อมเปิด NVIC
 *  @param callback : ฟังก์ชันที่ถูกเรียกทุก 100ms (ห้าม NULL) */
void Timer_Driver_TIM3_Init(Timer_TickCallback_t callback);

#endif /* TIMER_DRIVER_H */
