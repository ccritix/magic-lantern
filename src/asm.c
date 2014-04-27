/**
 * Reverse engineering on the fly
 * todo: make it a module
 */

#include "compiler.h"
#include "string.h"

static uint32_t ror(uint32_t word, uint32_t count)
{
    return word >> count | word << (32 - count);
}

static uint32_t decode_immediate_shifter_operand(uint32_t insn)
{
    uint32_t inmed_8 = insn & 0xFF;
    uint32_t rotate_imm = (insn & 0xF00) >> 7;
    return ror(inmed_8, rotate_imm);
}

/* returns true if the machine will not lock up when dereferencing ptr */
int is_sane_ptr(uint32_t ptr)
{
    switch (ptr & 0xF0000000)
    {
        case 0x00000000:
        case 0x10000000:
        case 0x40000000:
        case 0x50000000:
        case 0xF0000000:
            return 1;
    }
    return 0;
}

int looks_like_string(uint32_t addr)
{
    if (!is_sane_ptr(addr))
    {
        return 0;
    }
    
    int min_len = 4;
    int max_len = 100;
    
    for (uint32_t p = addr; p < addr + max_len; p++)
    {
        char c = *(char*)p;
        if (c == 0 && p > addr + min_len)
        {
            return 1;
        }
        if (c < 32 || c > 127)
        {
            return 0;
        }
    }
    return 0;
}

char* asm_guess_func_name_from_string(uint32_t addr)
{
    for (uint32_t i = addr; i < addr + 4 * 20; i += 4 )
    {
        uint32_t insn = *(uint32_t*)i;
        if( (insn & 0xFFFFF000) == 0xe28f2000 ) // add R2, pc, #offset - should catch strings passed to DebugMsg
        {
            int offset = decode_immediate_shifter_operand(insn);
            int pc = i;
            int dest = pc + offset + 8;
            if (looks_like_string(dest))
                return (char*) dest;
        }
    }
    return "";
}
