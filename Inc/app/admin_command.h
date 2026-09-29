/******************************************************************************
 * @file    admin_command.h
 * @brief   ตีความบรรทัดคำสั่ง (string) ที่รับจาก UART เป็นคำสั่งที่ระบบ
 *          เข้าใจ — เป็น pure logic ไม่แตะ hardware โดยตรง (ทดสอบแยกได้ง่าย
 *          เหมือน code_decoder)
 *
 *          คำสั่งที่รองรับ (พิมพ์ใหญ่ทั้งหมด แล้วกด Enter):
 *            UNLOCK   -> สั่งปลดล็อกทันที
 *            LOCKOUT  -> สั่งเข้าสู่ lockout ทันที
 *            RESET    -> ปกติ: สั่งรีเซ็ตรหัสและโซนเป้าหมายกลับเป็นค่า default
 *                        ตอน DISABLED (อุณหภูมิผิดปกติ): ล้าง alarm อย่างเดียว
 *                        รหัสและโซนที่ตั้งไว้ยังอยู่ครบ
 *            SHOWPASS -> แสดงรหัสผ่านและโซนเป้าหมายปัจจุบันทาง UART
 *            STATUS   -> แสดงสถานะระบบ อุณหภูมิ และความสว่าง
 ******************************************************************************/
#ifndef ADMIN_COMMAND_H
#define ADMIN_COMMAND_H

#include <stdint.h>

typedef enum
{
    ADMIN_CMD_UNKNOWN = 0U,
    ADMIN_CMD_UNLOCK,
    ADMIN_CMD_LOCKOUT,
    ADMIN_CMD_RESET,
    ADMIN_CMD_SHOWPASS,
    ADMIN_CMD_STATUS
} AdminCommand_t;

/**
 * @brief  ตีความบรรทัดคำสั่ง (null-terminated string, ไม่รวม \r\n)
 * @param  p_line : บรรทัดคำสั่งที่ประกอบมาจาก UART RX
 * @return ค่า enum ตามคำสั่ง หรือ ADMIN_CMD_UNKNOWN ถ้าไม่ตรงกับคำสั่งใดเลย
 */
AdminCommand_t AdminCommand_Parse(char const * p_line);

#endif /* ADMIN_COMMAND_H */
