/******************************************************************************
 * @file    uart_driver.h
 * @brief   UART driver (register-level, raw address) สำหรับ USART2
 *          (PA2=TX, PA3=RX — ขา virtual COM port มาตรฐานของ Nucleo-F411RE
 *          ต่อผ่าน ST-Link ในตัว ไม่ต้องเดินสายเพิ่ม)
 *
 *          ทำไมต้องเป็น Interrupt ล้วนๆ (ทั้ง RX และ TX) ไม่ใช้ Polling เลย:
 *          - RX: admin พิมพ์คำสั่งเข้ามาได้ทุกเวลาแบบคาดเดาไม่ได้ ขณะที่
 *            main loop ว่างเปล่า (งานอื่นขับเคลื่อนด้วย interrupt หมด) การ
 *            polling UART จะทำให้ main loop กลายเป็น busy-wait ที่ขัดกับ
 *            สถาปัตยกรรมทั้งระบบ และเสี่ยงพลาดอักขระถ้าไปเพิ่ม logic อื่น
 *            ใน loop ภายหลัง
 *          - TX: ส่งข้อความตอบกลับแบบ non-blocking เพื่อไม่ให้ CPU ค้างรอ
 *            จนกว่าจะส่งครบทุกตัวอักษร (ตามเกณฑ์ "ห้ามใช้ Polling")
 ******************************************************************************/
#ifndef UART_DRIVER_H
#define UART_DRIVER_H

#include <stdint.h>

/** เรียกทุกครั้งที่ได้รับ 1 byte ทาง RX (ตีความเอง เช่น สะสมเป็นบรรทัด) */
typedef void (*UART_RxByteCallback_t)(uint8_t received_byte);

/**
 * @brief  เริ่มต้น USART2 ที่ baud rate 9600 (เลือกค่าที่ทนต่อความคลาดเคลื่อน
 *         ของ HSI ได้ดี ไม่ต้องพึ่งพา external crystal ที่แม่นกว่า)
 *         เปิดทั้ง RX interrupt และเตรียม TX ให้พร้อมส่งแบบ interrupt-driven
 * @param  rx_callback : ฟังก์ชันที่ถูกเรียกทุกครั้งที่รับ byte ใหม่ (ห้าม NULL)
 */
void UART_Driver_Init(UART_RxByteCallback_t rx_callback);

/**
 * @brief  ส่งข้อความ (null-terminated string) แบบ interrupt-driven
 *         (ไม่ blocking รอจนส่งเสร็จ) ความยาวสูงสุดจำกัดไว้ภายใน driver
 * @param  p_str : string ที่ต้องการส่ง (ต้องมี '\0' ปิดท้าย)
 * @note   ถ้าเรียกซ้ำขณะที่ยังส่งข้อความก่อนหน้าไม่เสร็จ ข้อความใหม่จะ
 *         ถูกละทิ้ง (เหมาะกับ use case นี้ที่ admin console ใช้งานไม่ถี่มาก
 *         ถ้าต้องการ queue หลายข้อความพร้อมกันในอนาคต ต้องขยายเป็น ring buffer)
 */
void UART_Driver_SendString(char const * p_str);

#endif /* UART_DRIVER_H */
