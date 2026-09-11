/******************************************************************************
 * @file    code_decoder.c
 * @brief   Implementation ของ code_decoder (pure logic, ไม่แตะ hardware)
 ******************************************************************************/
#include "code_decoder.h"
#include "app_config.h"

CodeSymbol_t CodeDecoder_Classify(uint32_t const duration_ms)
{
    CodeSymbol_t result;

    if (duration_ms < (uint32_t) APP_PRESS_THRESHOLD_MS)
    {
        result = CODE_SYMBOL_SHORT;
    }
    else
    {
        result = CODE_SYMBOL_LONG;
    }

    return result;
}
