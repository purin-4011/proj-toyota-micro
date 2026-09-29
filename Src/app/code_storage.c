/******************************************************************************
 * @file    code_storage.c
 * @brief   Implementation ของ code_storage (เก็บใน RAM + ตรวจสอบความถูกต้อง
 *          ด้วย CRC ฮาร์ดแวร์)
 *
 *          หมายเหตุสถาปัตยกรรม: module นี้ยังถือเป็น "เกือบ pure logic"
 *          เหมือนเดิม เพราะ crc_driver ไม่มี interrupt/state แบบ async
 *          (ป้อนข้อมูลเข้า -> อ่านผลลัพธ์ออกได้ทันที เหมือนฟังก์ชันคณิตศาสตร์
 *          ธรรมดา) จึงต่างจาก driver ตัวอื่น (EXTI/Timer/ADC/UART) ที่ห้าม
 *          แตะจาก app layer โดยตรงเพราะมี callback/ISR เกี่ยวข้อง
 ******************************************************************************/
#include "code_storage.h"
#include "crc_driver.h"

#define CODE_STORAGE_DEFAULT_LENGTH (4U)

static CodeSymbol_t s_stored_symbols[CODE_STORAGE_MAX_LENGTH];
static uint8_t s_stored_length = 0U;
static uint32_t s_stored_code_crc = 0U;

/* รหัส default ตาม proposal: สั้น-สั้น-สั้น-สั้น */
static CodeSymbol_t const s_default_code[CODE_STORAGE_DEFAULT_LENGTH] =
{
    CODE_SYMBOL_SHORT,
    CODE_SYMBOL_SHORT,
    CODE_SYMBOL_SHORT,
    CODE_SYMBOL_SHORT
};

/**
 * @brief  คำนวณ CRC ของรหัส (ความยาว + สัญลักษณ์ทุกตัว) ผ่านฮาร์ดแวร์
 *         ใส่ความยาวเข้าไปคำนวณด้วย เพื่อให้จับความเสียหายที่ทำให้ความยาว
 *         เพี้ยนได้ด้วย ไม่ใช่แค่เนื้อหาสัญลักษณ์
 */
static uint32_t CodeStorage_ComputeCrc(CodeSymbol_t const * const p_symbols,
                                        uint8_t const length)
{
    uint32_t crc;
    uint8_t i;

    CRC_Driver_Reset();
    crc = CRC_Driver_FeedWord((uint32_t) length);

    for (i = 0U; i < length; i++)
    {
        crc = CRC_Driver_FeedWord((uint32_t) p_symbols[i]);
    }

    return crc;
}

void CodeStorage_Init(void)
{
    uint8_t i;

    for (i = 0U; i < CODE_STORAGE_DEFAULT_LENGTH; i++)
    {
        s_stored_symbols[i] = s_default_code[i];
    }

    s_stored_length = CODE_STORAGE_DEFAULT_LENGTH;

    /* คำนวณ CRC ของรหัส default เก็บไว้อ้างอิง สำหรับเช็คความถูกต้อง
     * ทุกครั้งที่มีการเทียบรหัสภายหลัง */
    s_stored_code_crc = CodeStorage_ComputeCrc(s_stored_symbols, s_stored_length);
}

bool CodeStorage_Compare(CodeSymbol_t const * const p_symbols, uint8_t const length)
{
    bool result;
    uint8_t i;
    uint32_t const current_crc = CodeStorage_ComputeCrc(s_stored_symbols, s_stored_length);

    if (current_crc != s_stored_code_crc)
    {
        /* ข้อมูลรหัสที่เก็บไว้เสียหาย (เช่น RAM bit-flip) ไม่ตรงกับ CRC ที่
         * บันทึกไว้ตอน commit ครั้งล่าสุด -> กู้กลับเป็นรหัส default ทันที
         * เพื่อความปลอดภัย (ไม่ปล่อยให้ใช้รหัสที่เพี้ยนไปโดยไม่รู้ตัว) แล้ว
         * ถือว่ารอบนี้เทียบไม่ผ่านเสมอ (ผู้ใช้ต้องลองใหม่ด้วยรหัสที่ถูกต้อง
         * ตามที่กู้คืนมา) */
        CodeStorage_Init();
        result = false;
    }
    else if (length != s_stored_length)
    {
        result = false;
    }
    else
    {
        result = true;

        for (i = 0U; i < length; i++)
        {
            if (p_symbols[i] != s_stored_symbols[i])
            {
                result = false;
            }
            else
            {
                /* สัญลักษณ์ตำแหน่งนี้ตรงกัน - ตรวจตำแหน่งถัดไปต่อ */
            }
        }
    }

    return result;
}

uint8_t CodeStorage_GetLength(void)
{
    return s_stored_length;
}

CodeSymbol_t CodeStorage_GetSymbol(uint8_t const index)
{
    CodeSymbol_t result;

    if (index < s_stored_length)
    {
        result = s_stored_symbols[index];
    }
    else
    {
        result = CODE_SYMBOL_SHORT;   /* index เกินความยาวรหัส - คืนค่าปลอดภัย */
    }

    return result;
}

void CodeStorage_Commit(CodeSymbol_t const * const p_symbols, uint8_t length)
{
    uint8_t i;

    /* กันขอบเขตไม่ให้เกิน buffer (Setup Mode ควร clamp มาให้แล้ว แต่กันไว้
     * อีกชั้นตามหลัก defensive programming) */
    if (length > (uint8_t) CODE_STORAGE_MAX_LENGTH)
    {
        length = (uint8_t) CODE_STORAGE_MAX_LENGTH;
    }
    else
    {
        /* อยู่ในขอบเขตแล้ว - ไม่ต้องทำอะไร */
    }

    for (i = 0U; i < length; i++)
    {
        s_stored_symbols[i] = p_symbols[i];
    }
    s_stored_length = length;

    /* คำนวณ CRC อ้างอิงใหม่ทันที เหมือนที่ CodeStorage_Init ทำกับรหัส
     * default - ทำให้ CodeStorage_Compare() ตรวจสอบรหัสใหม่นี้ได้ถูกต้อง
     * ตั้งแต่การเปรียบเทียบครั้งถัดไป */
    s_stored_code_crc = CodeStorage_ComputeCrc(s_stored_symbols, s_stored_length);
}
