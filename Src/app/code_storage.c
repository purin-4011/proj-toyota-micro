/******************************************************************************
 * @file    code_storage.c
 * @brief   Implementation ของ code_storage (pure logic, เก็บใน RAM)
 ******************************************************************************/
#include "code_storage.h"

#define CODE_STORAGE_DEFAULT_LENGTH (4U)

static CodeSymbol_t s_stored_symbols[CODE_STORAGE_MAX_LENGTH];
static uint8_t s_stored_length = 0U;

/* รหัส default ตาม proposal: สั้น-สั้น-สั้น-สั้น */
static CodeSymbol_t const s_default_code[CODE_STORAGE_DEFAULT_LENGTH] =
{
    CODE_SYMBOL_SHORT,
    CODE_SYMBOL_SHORT,
    CODE_SYMBOL_SHORT,
    CODE_SYMBOL_SHORT
};

void CodeStorage_Init(void)
{
    uint8_t i;

    for (i = 0U; i < CODE_STORAGE_DEFAULT_LENGTH; i++)
    {
        s_stored_symbols[i] = s_default_code[i];
    }

    s_stored_length = CODE_STORAGE_DEFAULT_LENGTH;
}

bool CodeStorage_Compare(CodeSymbol_t const * const p_symbols, uint8_t const length)
{
    bool result;
    uint8_t i;

    if (length != s_stored_length)
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
