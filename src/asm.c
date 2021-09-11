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

static int seems_to_be_string(char* addr)
{
    int len = strlen(addr);
    if (len > 4 && len < 100)
    {
        for (char* c = addr; *c; c++)
        {
            if (*c < 7 || *c > 127) return 0;
        }
        return 1;
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
            if (seems_to_be_string((char*) dest))
                return (char*) dest;
        }
    }
    return "";
}
uint32_t find_func_from_string(const char * ref_string, uint32_t Rd, uint32_t max_start_offset)
{
    qprintf("find_func_from_string('%s', R%d, max_off=%x)\n", repr(ref_string), Rd, max_start_offset);

    /* only scan the bootloader RAM area */
    for (uint32_t i = 0x100000; i < 0x110000; i += 4 )
    {
        /* look for: add Rd, pc, #offset */
        uint32_t insn = MEM(i);
        if( (insn & 0xFFFFF000) == (0xe28f0000 | (Rd << 12)) )
        {
            /* let's check if it refers to our string */
            int offset = decode_immediate_shifter_operand(insn);
            int pc = i;
            int dest = pc + offset + 8;
            if (strcmp((char*)dest, ref_string) == 0)
            {
                uint32_t func_start = locate_func_start(i, 0x100000);
                if (func_start && (i > func_start) && (i < func_start + max_start_offset))
                {
                    /* looks OK, start of function is not too far from the string reference */
                    qprintf("... found %x\n", func_start);
                    return func_start;
                }
            }
        }
    }
    
    return 0;
}

uint32_t find_func_from_string_thumb(const char * ref_string, uint32_t Rd, uint32_t max_start_offset)
{
    qprintf("find_func_from_string_thumb('%s', R%d, max_off=%x)\n", repr(ref_string), Rd, max_start_offset);

    /* only scan the bootloader RAM area */
    for (uint32_t i = 0x100000; i < 0x110000; i += 2 )
    {
        /* look for: add Rd, pc, #offset */
        uint32_t insn = MEM(i);
        if ((insn & 0x0000FF00) == (0x0000A000 | (Rd << 8))) /* T2S p.112: ADR Rd, #offset */
        {
            /* let's check if it refers to our string */
            uint32_t string_offset = (insn & 0xFF) << 2;
            uint32_t pc = (i + 4) & ~3;    /* not sure */
            char * found_string = (char *)(pc + string_offset);
            //qprint("%x: %x %s\n", i, found_string, repr(found_string));

            if (strcmp(found_string, ref_string) == 0)
            {
                uint32_t func_start = locate_func_start_thumb(i, 0x100000);
                qprintf("%x: %x %s %x\n", i, found_string, repr(found_string), func_start);
                if (func_start && (i > func_start) && (i < func_start + max_start_offset))
                {
                    /* looks OK, start of function is not too far from the string reference */
                    qprintf("... found %x\n", func_start + 1);
                    return func_start + 1;
                }
            }
        }
    }
    
    return 0;
}
