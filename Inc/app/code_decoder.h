/******************************************************************************
 * @file    code_decoder.h
 * @brief   แปลง "ระยะเวลาที่กดปุ่มค้างไว้ (ms)" ให้เป็นสัญลักษณ์ SHORT / LONG
 *          Module นี้เป็น pure logic ไม่แตะ hardware โดยตรง (ทดสอบแยกได้ง่าย)
 ******************************************************************************/
#ifndef CODE_DECODER_H
#define CODE_DECODER_H

#include <stdint.h>

typedef enum
{
    CODE_SYMBOL_SHORT = 0U,
    CODE_SYMBOL_LONG  = 1U
} CodeSymbol_t;

/**
 * @brief  จำแนก duration การกดปุ่มเป็น SHORT หรือ LONG
 * @param  duration_ms : ระยะเวลาที่ปุ่มถูกกดค้างไว้ (หน่วย ms)
 */
CodeSymbol_t CodeDecoder_Classify(uint32_t duration_ms);

#endif /* CODE_DECODER_H */
