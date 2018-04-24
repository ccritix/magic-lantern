/**
 * Reverse engineering on the fly
 * todo: make it a module
 */

#include "compiler.h"
#include "string.h"
#include "qemu-util.h"

extern int printf(const char * format, ...);

static inline char hex_digit(uint32_t x)
{
    x &= 0xF;
    if (x < 10)
    {
        return '0' + x;
    }

    return 'A' + x - 10;
}

static const char * repr(const char * str)
{
    static char buf[128];
    int i = 0;
    for (const char * p = str; (*p) && i < COUNT(buf)-1; p++)
    {
        char c = *p;

        switch (c)
        {
            case 32 ... 126:
                buf[i++] = c;
                break;
            case '\n':
                buf[i++] = '\\';
                buf[i++] = 'n';
                break;
            case '\r':
                buf[i++] = '\\';
                buf[i++] = 'r';
                break;
            case '\t':
                buf[i++] = '\\';
                buf[i++] = 't';
                break;
            case '\b':
                buf[i++] = '\\';
                buf[i++] = 'b';
                break;
            default:
                buf[i++] = '\\';
                buf[i++] = 'x';
                buf[i++] = hex_digit(c >> 4);
                buf[i++] = hex_digit(c);
                break;
        }
    }

    /* null-terminate */
    buf[i] = 0;

    return buf;
}

#define MEM(x) (*(volatile uint32_t *)(x))
#define STMFD 0xe92d0000

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

static inline int32_t decode_signed_offset12(uint32_t insn)
{
    uint32_t off = (insn & 0x00000FFF);
    if (insn & (1 << 23))
        return off;
    return -off;
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

static uint32_t branch_destination(uint32_t insn, uint32_t pc)
{
    int32_t off = (((int32_t) insn & 0x00FFFFFF) << 8) >> 6;
    uint32_t dest = pc + off + 8;
    return dest;
}

char* asm_guess_func_name_from_string(uint32_t addr)
{
    qprintf("asm_guess_func_name_from_string(0x%X)\n", addr);

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

uint32_t locate_func_start(uint32_t search_from, uint32_t stop_addr)
{
    /* search backwards in memory for a STMFD */
    for(uint32_t i = search_from; i > stop_addr; i -= 4 )
    {
        if((MEM(i) & 0xFFFF0000) == (STMFD & 0xFFFF0000))
        {
            return i;
        }
    }
    return 0;
}

uint32_t locate_func_start_thumb(uint32_t search_from, uint32_t stop_addr)
{
    /* search backwards in memory for a PUSH or PUSH.W */
    for(uint32_t i = search_from; i > stop_addr; i -= 2 )
    {
        if ((MEM(i) & 0xA000FFFF) == 0x000E92D)
        {
            return i;
        }
        if ((MEM(i) & 0xFF00) == 0xB500)
        {
            return i;
        }
    }
    return 0;
}

uint32_t find_func_called_before_string_ref(char* ref_string)
{
    qprintf("find_func_called_before_string_ref('%s')\n", repr(ref_string));

    /* look for this pattern:
     * fffe1824:    eb0019a5    bl  @fromutil_disp_init
     * fffe1828:    e28f0e2e    add r0, pc, #736    ; *'Other models\n'
     */
    
    int found = 0;
    uint32_t answer = 0;
    
    /* only scan the bootloader RAM area */
    for (uint32_t i = 0x100000; i < 0x110000; i += 4 )
    {
        uint32_t this = MEM(i);
        uint32_t next = MEM(i+4);
        int is_bl         = ((this & 0xFF000000) == 0xEB000000);
        int is_string_ref = ((next & 0xFFFFF000) == 0xe28f0000); /* add R0, pc, #offset */
        if (is_bl && is_string_ref)
        {
            uint32_t string_offset = decode_immediate_shifter_operand(next);
            uint32_t pc = i + 4;
            char* string_addr = (void*) pc + string_offset + 8;
            if (strcmp(string_addr, ref_string) == 0)
            {
                /* bingo? */
                found++;
                uint32_t pc = i;
                uint32_t func_addr = branch_destination(this, pc);
                if (func_addr > 0x100000 && func_addr < 0x110000)
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

uint32_t find_func_called_before_string_ref_thumb(char* ref_string)
{
    qprintf("find_func_called_before_string_ref_thumb('%s')\n", repr(ref_string));

    /* look for this pattern:
     * E0010BFE 01 F0 82 FD    BL disp_init         ; E0012706
     * E0010C02 B3 A0          ADR  R0, #718        ; "File(*.fir) not found\n"
     */
   
    int found = 0;
    uint32_t answer = 0;
   
    /* only scan the bootloader RAM area */
    for (uint32_t i = 0x100000; i < 0x110000; i += 2 )
    {
        uint32_t this = MEM(i);
        uint32_t next = MEM(i+4);
        int is_bl         = ((this & 0xF800FC00) == 0xF800F000);    /* T2S p.134 (simplified) */
        int is_string_ref = ((next & 0x0000FF00) == 0x0000A000);    /* T2S p.112: ADR R0, #offset */
        if (is_bl && is_string_ref)
        {
            uint32_t string_offset = (next & 0xFF) << 2;
            uint32_t pc = (i + 4 + 4) & ~3;    /* not sure */
            char* string_addr = (char*)(pc + string_offset);
            //qprintf("%x: %x %s\n", i + 4, string_addr, repr(string_addr));
            if (strcmp(string_addr, ref_string) == 0)
            {
                /* bingo? */
                found++;
                uint32_t pc = (i + 4);
                uint32_t imm10 = this & 0x3FF;
                uint32_t imm11 = (this >> 16) & 0x7FF;
                uint32_t func_offset = (imm11 << 1) | (imm10 << 12);
                uint32_t func_addr = pc + func_offset;
                qprintf("%x: BL %x\n", i, func_addr);
                if (func_addr > 0x100000 && func_addr < 0x110000)
                {
                    /* looks ok? */
                    answer = func_addr + 1;
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
            //qprintf("%x: %x %s\n", i, found_string, repr(found_string));

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

uint32_t find_string_ref(char* string)
{
    /* only scan the bootloader RAM area */
    for (uint32_t i = 0x100000; i < 0x110000; i += 4 )
    {
        /* look for: add Rd, pc, #offset */
        uint32_t insn = MEM(i);
        if ((insn & 0xFFFF0000) == 0xe28f0000)
        {
            /* let's check if it refers to our string */
            int offset = decode_immediate_shifter_operand(insn);
            int pc = i;
            int dest = pc + offset + 8;
            if (strcmp((char*)dest, string) == 0)
            {
                return i;
            }
        }
    }

    return 0;
}

/* the function must be referencing the tag somehow (a constant value) */
static int func_has_tag(uint32_t func, uint32_t tag)
{
    for (uint32_t pc = func; pc < func + 0x100; pc += 4 )
    {
        /* look for: add Rd, pc, #offset */
        uint32_t insn = MEM(pc);
        if( (insn & 0xFF7F0000) == 0xE51F0000)
        {
            int offset = decode_signed_offset12(insn);
            int dest = pc + offset + 8;
            qprintf("%X: tag %x\n", pc, MEM(dest));
            if (MEM(dest) == tag)
            {
                printf(" - %X: tag %x\n", pc, MEM(dest));
                return 1;
            }
        }

        /* look for: MOV Rd, #imm */
        if( (insn & 0xFFFF0000) == 0xE3A00000)
        {
            uint32_t value = decode_immediate_shifter_operand(insn);
            if (value == tag)
            {
                printf(" - %X: tag imm %x\n", pc, value);
                return 1;
            }
        }

        if ((insn & 0xFFFF0000) == 0xe8bd0000)
        {
            /* LDMFD - end of function */
            break;
        }
    }
    return 0;
}

uint32_t find_func_called_after_string_ref(char * string, uint32_t tag)
{
    qprintf("find_func_called_after_string_ref('%s', tag=0x%X)\n", repr(string), tag);
    int found = 0;
    uint32_t answer = 0;

    uint32_t start = find_string_ref(string);
    printf(" - %x: %s\n", start, string);
    if (start)
    {
        for (uint32_t i = start + 4; i < start + 0x100; i += 4)
        {
            uint32_t insn = MEM(i);
            int is_bl = ((insn & 0xFF000000) == 0xEB000000);
            if (is_bl)
            {
                uint32_t pc = i;
                uint32_t func_addr = branch_destination(insn, pc);
                qprintf("%x: BL %x\n", pc, func_addr);

                if (func_addr > 0x100000 && func_addr < 0x110000)
                {
                    if (func_has_tag(func_addr, tag))
                    {
                        if (func_addr != answer) {
                            found++;
                        }
                        answer = func_addr;
                    }
                }
            }
            if ((insn & 0xFFFF0000) == 0xe8bd0000)
            {
                /* LDMFD - end of function */
                break;
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
