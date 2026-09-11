/******************************************************************************
 * @file    syscalls_stub.c
 * @brief   Minimal syscall stubs ที่ newlib-nano ต้องการเสมอ (แม้ไม่ได้ใช้
 *          printf/scanf เลยก็ตาม) เพื่อตั้งค่า stdin/stdout/stderr ตอนเริ่ม
 *          โปรแกรม โปรเจคแบบ "Empty" ใน CubeIDE ไม่ได้ gen ไฟล์นี้ให้
 *          อัตโนมัติ (ต่างจากโปรเจคที่เลือก firmware package เต็ม) จึงต้อง
 *          เพิ่มเองเพื่อให้ linker หาสัญลักษณ์เหล่านี้เจอ
 *
 *          Stub เหล่านี้ทำงานแบบ "ปลอดภัยที่สุดเท่าที่จะทำได้" คือไม่ทำ
 *          อะไรจริงจัง (ไม่มี UART/semihosting ผูกไว้) เพราะโปรเจคนี้ยังไม่ได้
 *          ใช้ printf ผ่าน UART จริง ถ้าในอนาคตต้องการ retarget printf ไปออก
 *          ทาง USART2 ให้แก้ฟังก์ชัน _write ด้านล่างนี้แทน
 ******************************************************************************/
#include <sys/stat.h>

int _close(int file)
{
    (void) file;
    return -1;
}

int _lseek(int file, int ptr, int dir)
{
    (void) file;
    (void) ptr;
    (void) dir;
    return 0;
}

int _read(int file, char * ptr, int len)
{
    (void) file;
    (void) ptr;
    (void) len;
    return 0;
}

int _write(int file, char * ptr, int len)
{
    (void) file;
    (void) ptr;
    /* คืนค่าจำนวน byte ที่ "เขียนสำเร็จ" เท่ากับ len เสมอ เพื่อไม่ให้
     * libc คิดว่าเขียนผิดพลาดแล้ว retry วนลูป */
    return len;
}
