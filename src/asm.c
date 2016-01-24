/**
 * Reverse engineering on the fly
 * todo: make it a module
 */

#include "compiler.h"
#include "string.h"

#define MEM(x) (*(volatile uint32_t *)(x))

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
        uint32_t insn = MEM(i);
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

uint32_t find_func_called_before_string_ref(char* ref_string)
{
    /* look for this pattern:
     * fffe1824:    eb0019a5    bl  @fromutil_disp_init
     * fffe1828:    e28f0e2e    add r0, pc, #736    ; *'Other models\n'
     */
    
    int found = 0;
    uint32_t answer = 0;
    
    /* only scan the bootloader area */
    for (uint32_t i = 0xFFFE0000; i < 0xFFFFFFF0; i += 4 )
    {
        uint32_t this = MEM(i);
        uint32_t next = MEM(i+4);
        int is_bl         = ((this & 0xFF000000) == 0xEB000000);
        int is_string_ref = ((next & 0xFFFFF000) == 0xe28f0000); /* add R0, pc, #offset */
        if (is_bl && is_string_ref)
        {
            uint32_t string_offset = decode_immediate_shifter_operand(next);
            uint32_t pc = i + 4;
            char* string_addr = pc + string_offset + 8;
            if (strcmp(string_addr, ref_string) == 0)
            {
                /* bingo? */
                found++;
                uint32_t func_offset = (this & 0x00FFFFFF) << 2;
                uint32_t pc = i;
                uint32_t func_addr = pc + func_offset + 8;
                if (func_addr > 0xFFFE0000)
                {
                    /* looks ok? */
                    answer = func_addr;
                }
            }
        }
    }
    
    if (found == 1)
    {
        /* only return success if there's a single match (no ambiguity) */
        return answer;
    }
    
    return 0;
}
