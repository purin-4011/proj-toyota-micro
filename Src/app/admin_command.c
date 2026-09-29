/******************************************************************************
 * @file    admin_command.c
 * @brief   Implementation ของ admin_command (pure logic)
 ******************************************************************************/
#include "admin_command.h"

/**
 * @brief  เทียบ string 2 ตัวว่าเหมือนกันทุกตัวอักษรไหม (รวม null terminator)
 *         เขียนเองแทนการใช้ strcmp() จาก <string.h> เพื่อให้ชัดเจนว่าไม่มี
 *         dependency กับ libc ส่วนที่ไม่จำเป็น (ตาม MISRA-C แนวทางหลีกเลี่ยง
 *         ฟังก์ชัน string ที่มีความเสี่ยงด้าน bounds)
 */
static uint8_t AdminCommand_StringEquals(char const * const p_a, char const * const p_b)
{
    uint8_t result;
    uint8_t i;
    uint8_t mismatch_found;

    i = 0U;
    mismatch_found = 0U;

    while ((mismatch_found == 0U) && (p_a[i] != '\0') && (p_b[i] != '\0'))
    {
        if (p_a[i] != p_b[i])
        {
            mismatch_found = 1U;
        }
        else
        {
            i++;
        }
    }

    if ((mismatch_found == 0U) && (p_a[i] == p_b[i]))
    {
        /* ทั้งคู่ชนไปที่ '\0' พร้อมกัน แปลว่าตรงกันทุกตัวอักษร */
        result = 1U;
    }
    else
    {
        result = 0U;
    }

    return result;
}

AdminCommand_t AdminCommand_Parse(char const * const p_line)
{
    AdminCommand_t result;

    if (p_line == (char const *) 0)
    {
        result = ADMIN_CMD_UNKNOWN;
    }
    else if (AdminCommand_StringEquals(p_line, "UNLOCK") != 0U)
    {
        result = ADMIN_CMD_UNLOCK;
    }
    else if (AdminCommand_StringEquals(p_line, "LOCKOUT") != 0U)
    {
        result = ADMIN_CMD_LOCKOUT;
    }
    else if (AdminCommand_StringEquals(p_line, "RESET") != 0U)
    {
        result = ADMIN_CMD_RESET;
    }
    else if (AdminCommand_StringEquals(p_line, "SHOWPASS") != 0U)
    {
        result = ADMIN_CMD_SHOWPASS;
    }
    else
    {
        result = ADMIN_CMD_UNKNOWN;
    }

    return result;
}
