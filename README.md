# Digital Combination Lock — เริ่มต้นจากศูนย์ (Register-level / Bare-metal)

โค้ดชุดนี้เขียนตามสไลด์คอร์สที่ตรวจสอบแล้ว (`0000_Intro.pdf`, `0100_GPIO.pdf`,
`0500_Interrupts.pdf`) แบบ **hybrid ตามที่คอร์สสอนจริง**:

- **Peripheral register (GPIO, EXTI, RCC, SYSCFG, Timer)** → define เป็น raw
  address ตรงๆ (`#define X (*(volatile uint32_t*)(addr))`) ตามที่สอนใน
  `0100_GPIO.pdf` Lab 1.1 Step 3
- **NVIC (เปิด/ตั้ง priority interrupt)** → ใช้ CMSIS function
  `NVIC_EnableIRQ()` / `NVIC_SetPriority()` เพราะสไลด์ `0500_Interrupts.pdf`
  ระบุชัดว่า **"ต้องใช้ CMSIS"** สำหรับส่วนนี้โดยเฉพาะ (ไม่ใช่ raw register)

ด้วยเหตุนี้ไฟล์ `exti_driver.c` และ `timer_driver.c` จึง `#include "stm32f411xe.h"`
(เพื่อได้ enum `EXTI4_IRQn`/`TIM3_IRQn` และฟังก์ชัน NVIC) ส่วน `gpio_driver.c`
ไม่ต้อง include เลยเพราะไม่แตะ NVIC — ไฟล์ .c/.h ทั้งหมดในนี้ **ใช้งานได้ทันที
โดยไม่ต้องมีโปรเจคหรือโค้ดอะไรมาก่อนเลย** ขอแค่ทำตามขั้นตอนที่ 0 ด้านล่าง

---

## ขั้นตอนที่ 0: สร้างโปรเจค STM32CubeIDE จากศูนย์

(สรุปจาก `0000_Intro.pdf` Chapter 0.8 ให้ทำตามนี้ทีละขั้น)

1. เปิด STM32CubeIDE (เวอร์ชัน 1.15.1 ขึ้นไป)
2. เลือก workspace folder ที่ต้องการ แล้วกด **Launch**
3. ดาวน์โหลดไฟล์ Library จากลิงก์ในสไลด์คอร์ส:
   `https://steo.moodlecloud.com/pluginfile.php/2643/mod_folder/content/0/Supplement/Library.zip`
   แล้ว extract วางไว้ใน workspace folder ที่เลือกไว้ (จะได้โฟลเดอร์ `Library`)
4. คลิก **File > New > STM32 Project**
5. ไปที่แท็บ **Board Selector** พิมพ์ `F411RE` ใน Commercial Part Number
   แล้วเลือกบอร์ดในลิสต์ กด **Next**
6. ตั้งชื่อโปรเจค (เช่น `Digital_Combination_Lock`)
   ที่ **Targeted Project Type** เลือก **Empty** แล้วกด **Finish**
7. คลิกขวาที่โปรเจค > **Properties**
8. ไปที่ **C/C++ General > Paths and Symbols > Includes** แท็บ
9. กด **Add** แล้วเพิ่ม 2 โฟลเดอร์นี้:
   - `Workspace\Library\CMSIS-DEVICE-F4\Include`
   - `Workspace\Library\CMSIS\Core\Include`
10. กด **Apply and Close** แล้วถ้ามี popup ถาม **Rebuild Index** ให้กดยืนยัน

ตอนนี้คุณจะมีโปรเจคเปล่าพร้อมใช้งาน (มี `Src/main.c`, `Src/system_stm32f4xx.c`,
`Src/startup_stm32f411xetx.s` และ linker script ให้อัตโนมัติจากตัว wizard)

---

## ขั้นตอนที่ 1: คัดลอกไฟล์จาก zip นี้เข้าไปในโปรเจค

1. เปิดโฟลเดอร์ `Digital_Combination_Lock` ที่แตกจาก zip นี้
2. คัดลอกไฟล์ `.h` ทั้งหมดจาก `Inc/drivers/` และ `Inc/app/` ไปวางที่โฟลเดอร์
   `Inc/` ของโปรเจคจริงใน CubeIDE (คงชื่อไฟล์ไว้ตามเดิม)
3. คัดลอกไฟล์ `.c` ทั้งหมดจาก `Src/drivers/` และ `Src/app/` ไปวางที่โฟลเดอร์
   `Src/` ของโปรเจคจริง
4. **ลบเนื้อหาใน `Src/main.c` เดิมของโปรเจคทิ้งทั้งหมด** แล้ววางเนื้อหาจาก
   `Src/main.c` ในไฟล์นี้แทน
5. คลิกขวาโปรเจค > **Refresh** (หรือกด F5) ให้ CubeIDE เห็นไฟล์ใหม่ทั้งหมด

## ขั้นตอนที่ 2: Build และ Upload

1. กดปุ่ม **Build** (ค้อน) — ถ้าไม่มี error จะเห็น `Build Finished` ที่ Console
2. เสียบบอร์ด Nucleo-F411RE ผ่าน USB
3. กดปุ่ม **Run/Debug** เพื่ออัพโหลดโปรแกรมลงบอร์ด
4. ทดสอบ: กดปุ่มที่ต่อกับขา **PB4** สั้นๆ แล้วปล่อย -> ไฟเขียว (PA5) ติดชั่วครู่
   กดค้างเกิน 0.5 วิ แล้วปล่อย -> ไฟแดง (PA6) ติดชั่วครู่

---

## โครงสร้างไฟล์และหน้าที่

| ไฟล์ | หน้าที่ |
|---|---|
| `Inc/drivers/gpio_driver.h` + `Src/drivers/gpio_driver.c` | Init/Read/Write GPIO แบบ raw register address |
| `Inc/drivers/exti_driver.h` + `Src/drivers/exti_driver.c` | จับ press/release ปุ่ม PB4 ผ่าน EXTI4 |
| `Inc/drivers/timer_driver.h` + `Src/drivers/timer_driver.c` | TIM2 free-running (ms tick) + TIM3 periodic 100ms |
| `Inc/app/code_decoder.h` + `Src/app/code_decoder.c` | pure logic แปลง duration -> SHORT/LONG |
| `Inc/app/app_config.h` | pin mapping + threshold รวมจุดเดียว |
| `Src/main.c` | ต่อทุกอย่างเข้าด้วยกัน เป็น bring-up test |

**ทำไมไม่มีไฟล์ `.c` ของ `app_config.h`?** เพราะเป็นแค่ค่าคงที่ (`#define`)
ไม่มี logic ให้ implement จึงมีแค่ header อย่างเดียว

---

## หมายเหตุสำคัญก่อนทดสอบจริง

- โค้ดสมมติว่า timer clock = 16 MHz (HSI default) — ถ้าตั้ง SystemClock เป็น
  ความถี่อื่นภายหลัง ต้องแก้ `TIMER_DRIVER_TIMCLK_HZ` ใน `timer_driver.h`
- ปุ่มสมมติว่าต่อแบบ **active-low พร้อม internal pull-up** (กด = 0V, ปล่อย = 3.3V)
  ถ้าวงจรจริงของ Training Shield ต่างจากนี้ ต้องแก้ logic ใน `EXTI4_IRQHandler`
- Threshold SHORT/LONG ตั้งไว้ 500ms เป็นค่าเริ่มต้น ควรปรับจากการทดสอบจริง

## ยังไม่ได้ทำ (ตาม Timeline สัปดาห์ 2-4)

- [ ] `lock_fsm` — state machine เต็มรูปแบบ (IDLE -> ENTERING -> VALIDATING -> LOCKOUT)
- [ ] `code_storage` — เก็บรหัสที่ตั้งไว้ + ต่อ CRC hardware unit
- [ ] ADC driver (DMA) — อ่าน potentiometer ตั้งความยาวรหัส
- [ ] UART driver (DMA) — ส่ง audit log ออก USART2
- [ ] 7-segment driver — แสดงผลสถานะ/countdown
- [ ] ต่อ TIM3 tick handler เข้ากับ input-timeout (1.5s) และ lockout (30s) จริง
- [ ] ตัดสินใจเรื่อง persistence (RAM vs Flash) และ edge case เปลี่ยนความยาวรหัส
