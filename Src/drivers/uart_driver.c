/******************************************************************************
 * @file    uart_driver.c
 * @brief   Implementation ของ UART driver (register-level, raw address)
 *          สำหรับ USART2 (PA2=TX, PA3=RX)
 ******************************************************************************/
#include "uart_driver.h"
#include "stm32f411xe.h"   /* เฉพาะ IRQn_Type enum + NVIC_EnableIRQ()/SetPriority()
                             * ตามที่สไลด์ 0500_Interrupts.pdf บังคับให้ใช้ CMSIS
                             * สำหรับ NVIC โดยเฉพาะ (peripheral register อื่น
                             * ด้านล่างยังคง raw address ตามที่สอนใน 0100_GPIO.pdf) */

#define REG32(addr)  (*(volatile uint32_t *)(addr))

/* --- GPIOA (PA2 = TX, PA3 = RX) --- */
#define GPIOA_BASE               (0x40020000UL)
#define GPIOA_OFFSET_MODER       (0x00UL)
#define GPIOA_OFFSET_AFRL        (0x20UL)   /* Alternate Function ของ pin 0-7 */
#define UART_TX_PIN               (2U)      /* PA2 */
#define UART_RX_PIN               (3U)      /* PA3 */
#define UART_AF_USART2            (0x7U)    /* AF7 = USART1/2/3 บน STM32F4 */

/* --- RCC --- */
#define RCC_BASE                  (0x40023800UL)
#define RCC_OFFSET_AHB1ENR        (0x30UL)
#define RCC_OFFSET_APB1ENR        (0x40UL)
#define RCC_AHB1ENR_GPIOAEN_BIT   (0U)
#define RCC_APB1ENR_USART2EN_BIT  (17U)

/* --- USART2 (RM0383 Section 19.6) --- */
#define USART2_BASE               (0x40004400UL)
#define USART_OFFSET_SR           (0x00UL)
#define USART_OFFSET_DR           (0x04UL)
#define USART_OFFSET_BRR          (0x08UL)
#define USART_OFFSET_CR1          (0x0CUL)

#define USART_SR_TXE_BIT          (7U)
#define USART_SR_RXNE_BIT         (5U)

#define USART_CR1_UE_BIT          (13U)
#define USART_CR1_TXEIE_BIT       (7U)
#define USART_CR1_RXNEIE_BIT      (5U)
#define USART_CR1_TE_BIT          (3U)
#define USART_CR1_RE_BIT          (2U)

/** ตำแหน่งใน Vector Table ของ USART2_IRQHandler (RM0383 Table 38) */
#define USART2_IRQN                (38U)

/** BRR สำหรับ 115200 baud ที่ APB1 clock = 16MHz (HSI default, ไม่มี PLL)
 *  (ปรับจาก 9600 -> 115200 เพราะตัวอักษรขึ้นบน serial terminal ช้าเกินไป
 *  ที่ 9600; 115200 คือค่ามาตรฐานที่ terminal ทั่วไปรองรับและยัง error
 *  ต่ำพอสำหรับ USART ที่ไม่มี external crystal)
 *  USARTDIV = 16,000,000 / (16 * 115200) = 8.6806
 *  Mantissa = 8 (0x8), Fraction = round(0.6806*16) = 11 (0xB)
 *  BRR = (Mantissa << 4) | Fraction = 0x8B (error จริง ~0.08%) */
#define UART_BRR_115200_AT_16MHZ    (0x008BUL)

/** ความยาวสูงสุดของข้อความที่ส่งออกได้ต่อครั้ง (รวม null terminator) */
#define UART_TX_BUFFER_SIZE        (128U)   /* เดิม 64 - ขยายเพราะข้อความรหัส 8 หลัก
                                              * ("PASS: zone=5, 8 digit, long-long-...")
                                              * ยาวได้ถึง ~80 ตัวอักษร จะถูกตัดท้าย */

static UART_RxByteCallback_t volatile s_rx_callback = (UART_RxByteCallback_t) 0;

/* --- TX state (interrupt-driven, ส่งทีละ byte ผ่าน TXE interrupt) --- */
static char s_tx_buffer[UART_TX_BUFFER_SIZE];
static uint8_t volatile s_tx_length = 0U;
static uint8_t volatile s_tx_index = 0U;
static uint8_t volatile s_tx_busy = 0U;

void UART_Driver_Init(UART_RxByteCallback_t const rx_callback)
{
    uint32_t moder_val;
    uint32_t afrl_val;

    if (rx_callback != (UART_RxByteCallback_t) 0)
    {
        s_rx_callback = rx_callback;

        /* 1) เปิด clock GPIOA และ USART2 (พร้อม dummy read กัน erratum ของ
         *    STM32F4 ที่บางครั้งต้องหน่วงเล็กน้อยหลังเปิด clock ก่อนเข้าถึง
         *    peripheral นั้นทันที) */
        REG32(RCC_BASE + RCC_OFFSET_AHB1ENR) |= (1UL << RCC_AHB1ENR_GPIOAEN_BIT);
        (void) REG32(RCC_BASE + RCC_OFFSET_AHB1ENR);
        REG32(RCC_BASE + RCC_OFFSET_APB1ENR) |= (1UL << RCC_APB1ENR_USART2EN_BIT);
        (void) REG32(RCC_BASE + RCC_OFFSET_APB1ENR);

        /* 2) ตั้ง PA2/PA3 เป็นโหมด Alternate Function (10) แล้วเลือก AF7 */
        moder_val = REG32(GPIOA_BASE + GPIOA_OFFSET_MODER);
        moder_val &= ~(0x3UL << (UART_TX_PIN * 2U));
        moder_val |= (0x2UL << (UART_TX_PIN * 2U));   /* 10 = Alternate Function */
        moder_val &= ~(0x3UL << (UART_RX_PIN * 2U));
        moder_val |= (0x2UL << (UART_RX_PIN * 2U));
        REG32(GPIOA_BASE + GPIOA_OFFSET_MODER) = moder_val;

        afrl_val = REG32(GPIOA_BASE + GPIOA_OFFSET_AFRL);
        afrl_val &= ~(0xFUL << (UART_TX_PIN * 4U));
        afrl_val |= (UART_AF_USART2 << (UART_TX_PIN * 4U));
        afrl_val &= ~(0xFUL << (UART_RX_PIN * 4U));
        afrl_val |= (UART_AF_USART2 << (UART_RX_PIN * 4U));
        REG32(GPIOA_BASE + GPIOA_OFFSET_AFRL) = afrl_val;

        /* 3) ตั้ง baud rate */
        REG32(USART2_BASE + USART_OFFSET_BRR) = UART_BRR_115200_AT_16MHZ;

        /* 4) เปิด TE (transmit enable), RE (receive enable), RXNEIE
         *    (รับ interrupt ทุกครั้งที่มี byte เข้ามาใหม่) แล้วเปิด UE
         *    (ยังไม่เปิด TXEIE ตอนนี้ - จะเปิดเฉพาะตอนมีข้อความจะส่งจริง
         *     ใน UART_Driver_SendString เพื่อไม่ให้ interrupt รัวตอนว่าง) */
        REG32(USART2_BASE + USART_OFFSET_CR1) |= (1UL << USART_CR1_TE_BIT)
                                                 | (1UL << USART_CR1_RE_BIT)
                                                 | (1UL << USART_CR1_RXNEIE_BIT)
                                                 | (1UL << USART_CR1_UE_BIT);

        /* 5) ตั้ง priority = 3 (ต่ำสุดในบรรดา interrupt ที่ใช้ในระบบนี้
         *    เพราะ admin command ไม่ใช่ time-critical เท่าปุ่ม/countdown)
         *    แล้วเปิด NVIC ให้ USART2_IRQn ผ่าน CMSIS function ตามที่สไลด์สอน */
        NVIC_SetPriority(USART2_IRQn, 3U);
        NVIC_EnableIRQ(USART2_IRQn);
    }
    else
    {
        /* MISRA: else บังคับ — callback เป็น NULL จะไม่ init อะไรเลย */
    }
}

void UART_Driver_SendString(char const * const p_str)
{
    uint8_t i;

    if ((p_str != (char const *) 0) && (s_tx_busy == 0U))
    {
        i = 0U;

        while ((p_str[i] != '\0') && (i < (UART_TX_BUFFER_SIZE - 1U)))
        {
            s_tx_buffer[i] = p_str[i];
            i++;
        }

        s_tx_length = i;

        if (s_tx_length > 0U)
        {
            s_tx_busy = 1U;

            /* เขียน byte แรกออกไปเลยทันที (เขียนครั้งเดียว ไม่ใช่ loop รอ
             * จึงไม่นับเป็น polling) เพราะตอนนี้ USART ว่างอยู่แน่นอน TXE
             * ต้องเป็น 1 อยู่แล้ว แทนที่จะเปิด TXEIE แล้วหวังว่า hardware
             * จะ trigger interrupt ทันทีเอง (เขียนแบบนี้ชัดเจนกว่าและไม่
             * พึ่งพฤติกรรมที่อาจกำกวม) จากนั้นให้ ISR ส่ง byte ที่เหลือต่อ */
            REG32(USART2_BASE + USART_OFFSET_DR) = (uint32_t) s_tx_buffer[0];
            s_tx_index = 1U;

            if (s_tx_index < s_tx_length)
            {
                REG32(USART2_BASE + USART_OFFSET_CR1) |= (1UL << USART_CR1_TXEIE_BIT);
            }
            else
            {
                /* ข้อความมีตัวอักษรเดียว ส่งจบในทีเดียว ไม่ต้องเปิด interrupt */
                s_tx_busy = 0U;
            }
        }
        else
        {
            /* string ว่างเปล่า - ไม่มีอะไรต้องส่ง */
        }
    }
    else
    {
        /* p_str เป็น NULL หรือกำลังส่งข้อความก่อนหน้าอยู่ - ละทิ้งคำขอนี้
         * (ดู note ใน header เรื่อง single-message TX ไม่ใช่ ring buffer) */
    }
}

/**
 * @brief  ISR ของ USART2 — จัดการทั้ง RXNE (มี byte เข้ามาใหม่) และ
 *         TXE (พร้อมส่ง byte ถัดไป) ในฟังก์ชันเดียวกันตามที่ CubeIDE
 *         gen ชื่อ handler มาให้ใช้ร่วมกันทั้ง 2 เหตุการณ์
 */
void USART2_IRQHandler(void)
{
    uint32_t const sr_val = REG32(USART2_BASE + USART_OFFSET_SR);

    /* --- RXNE: มีข้อมูลเข้ามาใหม่ --- */
    if ((sr_val & (1UL << USART_SR_RXNE_BIT)) != 0U)
    {
        /* อ่าน DR จะ clear RXNE ให้อัตโนมัติตาม datasheet */
        uint8_t const received_byte = (uint8_t) (REG32(USART2_BASE + USART_OFFSET_DR) & 0xFFUL);

        if (s_rx_callback != (UART_RxByteCallback_t) 0)
        {
            s_rx_callback(received_byte);
        }
        else
        {
            /* ไม่มี callback ลงทะเบียนไว้ - ไม่ทำอะไร */
        }
    }
    else
    {
        /* ไม่ใช่ RXNE - ไม่ทำอะไรในส่วนนี้ */
    }

    /* --- TXE: พร้อมส่ง byte ถัดไป --- */
    if ((sr_val & (1UL << USART_SR_TXE_BIT)) != 0U)
    {
        if (s_tx_busy != 0U)
        {
            if (s_tx_index < s_tx_length)
            {
                /* เขียน DR จะ clear TXE ให้อัตโนมัติตาม datasheet */
                REG32(USART2_BASE + USART_OFFSET_DR) = (uint32_t) s_tx_buffer[s_tx_index];
                s_tx_index++;
            }
            else
            {
                /* ส่งครบแล้ว - ปิด TXE interrupt และเคลียร์สถานะ busy */
                REG32(USART2_BASE + USART_OFFSET_CR1) &= ~(1UL << USART_CR1_TXEIE_BIT);
                s_tx_busy = 0U;
            }
        }
        else
        {
            /* TXE ขึ้นแต่ไม่มีข้อความรอส่ง (ไม่ควรเกิดเพราะปิด TXEIE ไว้แล้ว
             * ตอนไม่ได้ส่ง แต่ handle ไว้กัน MISRA/edge case) */
            REG32(USART2_BASE + USART_OFFSET_CR1) &= ~(1UL << USART_CR1_TXEIE_BIT);
        }
    }
    else
    {
        /* ไม่ใช่ TXE - ไม่ทำอะไรในส่วนนี้ */
    }
}
