/******************************************************************************
 * @file    core_driver.h
 * @brief   Core driver — ตั้งค่าส่วนของ CPU Cortex-M4 (ไม่ใช่ peripheral):
 *          - เปิด FPU (จำเป็นก่อนใช้ float/logf/powf ในการคำนวณ NTC/LDR
 *            ตามที่ Lab 4.2 ให้เพิ่ม SCB->CPACR)
 *          - Sleep mode ด้วยคำสั่ง WFI (Wait For Interrupt): CPU หยุดทำงาน
 *            จนกว่าจะมี interrupt เข้ามา ส่วน peripheral ยังทำงานต่อ
 ******************************************************************************/
#ifndef CORE_DRIVER_H
#define CORE_DRIVER_H

/**
 * @brief  เปิด FPU และตั้งให้ debugger ยังเชื่อมต่อได้ระหว่าง sleep
 *         ต้องเรียกเป็นอย่างแรกใน main() ก่อนโค้ดที่ใช้ float ทุกตัว
 */
void Core_Driver_Init(void);

/**
 * @brief  ให้ CPU เข้า Sleep mode จนกว่าจะมี interrupt ถัดไป
 *         (เรียกใน main loop — ทุกงานขับเคลื่อนด้วย interrupt อยู่แล้ว)
 */
void Core_Driver_WaitForInterrupt(void);

#endif /* CORE_DRIVER_H */
