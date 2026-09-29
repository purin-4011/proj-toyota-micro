# Digital Combination Lock — STM32F411RE (Nucleo-64 + STEO Training Shield)

โปรเจคกุญแจล็อกดิจิทัล 2 ชั้น (รหัสปุ่มกด SHORT/LONG + ตำแหน่งโพเทนชิโอมิเตอร์)
เขียนแบบ **register-level bare-metal C** (ไม่ใช้ HAL/LL) แยก Driver/Application
layer ชัดเจน ตามเกณฑ์วิชา

> **หมายเหตุสำคัญ**: โปรเจคนี้ต่อยอดจากไฟล์ที่ผู้ใช้ยืนยันแล้วว่าทำงานถูกต้อง
> (`A3.zip`) ส่วน Setup Mode ที่เพิ่มเข้ามาใหม่ถูกเขียนแบบ "คัดลอกโครงสร้าง
> EXTI ที่ทดสอบผ่านแล้ว (PB4) มาปรับสำหรับปุ่มใหม่แต่ละตัว" แทนที่จะเขียน
> เป็น generic function ตัวเดียวรองรับทุกขา เพื่อลดความเสี่ยงที่จะพลาดจุดใด
> จุดหนึ่งของโค้ดเดิมที่ทดสอบผ่านแล้วระหว่างทำให้ generic

## โครงสร้างโปรเจค

```
Inc/
  drivers/   gpio, exti, timer, seven_segment, adc, uart, crc  (แตะ register/NVIC ตรงๆ)
  app/       app_config, code_decoder, code_storage, dial_lock,
             lock_fsm, setup_mode, admin_command               (pure logic)
Src/
  drivers/*.c
  app/*.c
  main.c            <- ผูก driver เข้ากับ application ทั้งหมด (จุดเดียวที่มี ISR wiring)
  syscalls_stub.c   <- stub เฉย ๆ ให้ linker หา _read/_write/... เจอ (newlib-nano)
```

**การตั้งค่า CubeIDE**: ต้องเพิ่ม `Inc/drivers` และ `Inc/app` เข้าไปใน
Project Properties → C/C++ General → Paths and Symbols → Includes (ทั้ง GNU C)

## Pin Map (Arduino-style / STM32 style)

| หน้าที่ | Arduino | STM32 pin | EXTI Line | หมายเหตุ |
|---|---|---|---|---|
| ปุ่มกรอกรหัสหลัก | D5 | PB4 | EXTI4 (dedicated) | active-low, internal pull-up — เหมือนเดิมไม่แก้ไข |
| ปุ่มเข้า/ออก Setup Mode | D4 | PB5 | EXTI9_5 (shared) | active-low, internal pull-up |
| ปุ่มเพิ่มจำนวนหลัก (Setup) | D2 | PA10 | EXTI15_10 (shared) | active-low, internal pull-up |
| ปุ่มลดจำนวนหลัก (Setup) | D3 | PB3 | EXTI3 (dedicated) | active-low, internal pull-up |
| LED feedback กดปุ่ม (สีฟ้าบนชิลด์นี้) | D13 | PA5 | | |
| LED สำเร็จ (เขียวจริง) | D10 | PB6 | | |
| LED ผิด/ล็อกเอาต์ | D12 | PA6 | | |
| LED Setup Mode (เหลือง) | D11 | PA7 | | |
| Potentiometer | A0 | PA4 (ADC1_IN4) | | |
| UART Admin (ผ่าน ST-Link VCP) | - | PA2 (TX) / PA3 (RX) | | 9600 8N1 |
| 7-segment BCD 2^0..2^3 | - | PC7 / PA8 / PB10 / PA9 | | |

## สถาปัตยกรรมเวลา (Two-Tier Timing)

- **TIM2**: นับ ms แบบ free-running ไม่มี interrupt ใช้วัดระยะเวลากดปุ่ม (SHORT/LONG)
- **TIM3**: interrupt ทุก 100 ms ขับ tick หลักของแอป (input timeout, lockout
  countdown, unlock-hold timeout, LED blink) ผ่าน `LockFsm_OnTick()`

## ฟีเจอร์หลัก

1. **กรอกรหัส SHORT/LONG** ผ่าน PB4 — < 500 ms = SHORT, >= 500 ms = LONG
2. **ล็อกสองชั้น**: ต้องกรอกรหัสถูก **และ** โพเทนชิโอมิเตอร์อยู่ในโซนเป้าหมาย
   (9 โซน) พร้อมกัน ถ้าหมุนโพเทนชิโอมิเตอร์หลุดโซนกลางคันระหว่างกรอกรหัส
   (ที่เริ่มต้นถูกโซนอยู่แล้ว) จะถูกตรวจจับทันทีด้วย **ADC Analog Watchdog**
3. **Lockout 9 วินาที** หลังกรอกผิดครบ 3 ครั้ง พร้อมนับถอยหลังบน 7-segment
4. **CRC-32 integrity check**: ทุกครั้งที่เทียบรหัส จะคำนวณ CRC ของรหัสที่
   เก็บใน RAM ใหม่เทียบกับค่าอ้างอิง ถ้าไม่ตรง (ข้อมูลเสียหาย) จะ self-heal
   กลับเป็นรหัส default อัตโนมัติ
5. **UART Admin Mode**: พิมพ์คำสั่ง `UNLOCK`, `LOCKOUT`, `RESET`, `SHOWPASS`,
   `STATUS` แล้ว Enter
6. **Setup Mode** — ดูวิธีทดสอบด้านล่าง
7. **Sleep เมื่อไม่มีคน (LDR, PA1)**: สว่าง + ไม่กดปุ่ม 15 วินาที -> ดับจอ ไม่รับปุ่ม
   CPU หลับด้วย WFI / มีคนมาบังแสง (แสงลดลงต่ำกว่า 50% ของค่าตอนบูต) -> ตื่น
   ไม่ sleep ระหว่างกรอกรหัส/ปลดล็อก/lockout/Setup Mode
8. **Temperature tamper (NTC, PA0)**: อุณหภูมิต่างจากค่าปกติเกิน 3 องศา
   ติดกัน 0.6 วินาที -> DISABLED (LED แดงกระพริบ ปฏิเสธทุกปุ่ม) ค้างจนกว่า
   admin สั่ง `RESET` ค่าปกติขยับตามห้องช้าๆ (~5 นาที) จึงไม่ alarm ตอนห้อง
   ค่อยๆ ร้อน/เย็น — ถ้าสาย NTC ขาดก็จะ alarm ด้วย (fail-secure)
   NTC + LDR อ่านผ่าน ADC **injected group** ทุก 200ms (TIM3 สั่ง, JEOC interrupt)

## Setup Mode — วิธีทดสอบ

1. ที่โหมดปกติ (รอกรอกรหัส) กด **PB5 ค้างไว้ >= 3 วินาที** แล้วปล่อย → ไฟเหลือง
   (PA7) ติดค้าง, UART พิมพ์ `SETUP MODE` (การกรอกรหัสปกติที่ค้างอยู่ ถ้ามี
   จะถูกยกเลิกอย่างปลอดภัย โดยไม่แตะรหัสเดิมหรือตัวนับกรอกผิด)
2. หมุนโพเทนชิโอมิเตอร์ไปโซนที่ต้องการ — เลขโซนขึ้นบน 7-segment ตลอด Setup Mode
3. กด **PA10** เพื่อเพิ่มจำนวนหลักรหัส (เริ่มที่ 0), กด **PB3** เพื่อลด
   (clamp 0-8) — UART พิมพ์ `Digits: N` ทุกครั้งที่เปลี่ยน
4. ได้จำนวนหลักที่ต้องการแล้ว (ต้อง > 0) กด **PB5** สั้น ๆ — UART พิมพ์
   `Enter code (short/long)` แล้วเริ่มกรอกรหัสใหม่ผ่าน **PB4** ทุกปุ่มที่กด
   จะ echo ออก UART เช่น `Key 1: short`
5. กรอกครบแล้วกด **PB5** อีกครั้งเพื่อยืนยัน/ออก — UART พิมพ์สรุป เช่น
   `SETUP SAVED: zone=6, 3 digit, short-short-long` แล้วบันทึกรหัส+โซนใหม่
   (ผ่าน CRC เดียวกับที่ใช้ตรวจสอบตอนปลดล็อก) ไฟเหลืองดับ กลับสู่โหมดปกติ

## ข้อควรระวังด้านฮาร์ดแวร์

- ทุกขาที่ใช้เป็น GPIO ทำงานที่ 3.3V logic เท่านั้น — ห้ามต่อสัญญาณ 5V ตรง ๆ
  เข้าขา MCU โดยไม่มี level shifter
- กระแสสูงสุดต่อขา GPIO ของ STM32F411 อยู่ที่ประมาณ 20 mA
- `UART_TX_BUFFER_SIZE` (64 bytes ตามเดิม) — ข้อความ Setup Mode ที่ยาวที่สุด
  (สรุป 8 หลัก) ยาวประมาณ 55-60 ตัวอักษร ยังอยู่ในขอบเขต แต่ถ้าจะเพิ่ม
  ข้อความยาวกว่านี้ในอนาคตต้องขยายบัฟเฟอร์ด้วย
- `UART_Driver_SendString` ไม่มี message queue — ห้ามเรียกซ้ำสองครั้งติดกัน
  ก่อนข้อความแรกส่งจบ (ข้อความหลังจะถูกทิ้ง) main.c จึงประกอบข้อความ Setup
  Mode ทั้งหมดใน buffer เดียว (`Main_Append*`) ก่อนค่อยเรียกส่งครั้งเดียว

## หมายเหตุเกี่ยวกับปัญหา "LED ไม่ติด" ในซับมิตก่อนหน้า

การตรวจสอบโค้ดใน `A3.zip` (ทั้ง GPIO/EXTI/Timer/lock_fsm/main.c) แบบ
line-by-line ไม่พบ logic bug ใดที่จะอธิบายอาการ "LED ไม่ติดไม่ว่าจะกดปุ่มไหน"
ได้ — ผู้ใช้ยืนยันว่า `A3.zip` (ก่อนเพิ่ม Setup Mode) ทำงานถูกต้องอยู่แล้ว
และปัญหาเกิดจากไฟล์ที่ส่งให้รอบก่อนหน้า (ที่เขียน `exti_driver` ใหม่แบบ
generic function ตัวเดียว) แทน จึงตัดสินใจไม่ไล่บั๊กในไฟล์เก่านั้นต่อ
แต่กลับมาต่อยอด Setup Mode บน `A3.zip` โดยตรง และเลือกเขียน EXTI สำหรับ
ปุ่มใหม่แบบ "คัดลอกฟังก์ชันที่ทดสอบผ่านแล้ว" แทน generic function เพื่อ
ลดความเสี่ยงที่จะเกิดปัญหาแบบเดิมซ้ำ

## Peripheral ที่ใช้ (สรุปตามเกณฑ์วิชา)

| เกณฑ์ | Peripheral | เหตุผลที่ต้องใช้จริง (ไม่ใช่แค่ให้ครบ) |
|---|---|---|
| GPIO | ปุ่ม, LED, 7-segment BCD | พื้นฐานของ input/output ทั้งหมด |
| UART (interrupt) | USART2 Admin console | รับคำสั่งจากภายนอกแบบ asynchronous โดยไม่บล็อก main loop |
| ADC (interrupt) | ADC1 CH4 potentiometer | อ่านค่า analog ต่อเนื่องแบบไม่ blocking |
| EXTI | PB4/PB5/PA10/PB3 | จับจังหวะกด/ปล่อยปุ่มแบบทันที ไม่พลาด edge |
| Additional: Timer | TIM2 (free-run) + TIM3 (periodic) | วัดระยะเวลากดปุ่ม + ขับ tick หลักของระบบ |
| Additional: CRC | CRC-32 hardware | ตรวจสอบความถูกต้องของรหัสใน RAM แบบ real-time |
| Additional: ADC Analog Watchdog | ADC1 AWD | ตรวจจับการหลุดโซนโพเทนชิโอมิเตอร์แบบฮาร์ดแวร์ล้วน |

## สถานะ

ทุกไฟล์ผ่านการตรวจ syntax ด้วย `gcc -std=c99 -Wall -Wextra -fsyntax-only`
เรียบร้อย (ไม่มี warning) — ยังไม่ได้ build จริงบน CubeIDE/ARM toolchain
เพราะ environment นี้ไม่มี CMSIS/arm-none-eabi-gcc ให้ใช้ **กรุณา build บน
เครื่องจริงและทดสอบปุ่ม PB4 เดิมก่อนว่ายังทำงานเหมือน A3.zig ทุกประการ
ก่อนค่อยทดสอบ Setup Mode ต่อ** — ถ้าปุ่มเดิมมีปัญหาแปลว่าไม่เกี่ยวกับ
Setup Mode ที่เพิ่มเข้ามา ให้แจ้งอาการละเอียด (build ผ่านไหม, LED/7-segment/
UART ตัวไหนทำงานบ้าง) เพื่อตามหาสาเหตุต่อ
