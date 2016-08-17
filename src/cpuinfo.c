#include "stdio.h"
#include "stdlib.h"
#include "string.h"

/* for DIGIC 6 */
#define THUMB_FW

struct cpuinfo_bitfield_desc_s {
    unsigned bits;
    const char *name;
    const char *(*desc_fn)(unsigned val);
};

struct cpuinfo_word_desc_s {
    const char *name;
    const struct cpuinfo_bitfield_desc_s *fields;
};

const struct cpuinfo_bitfield_desc_s cpuinf_id[] = {
    {4,"Revision"},
    {12,"Part"},
    {4,"ARM Arch"},
    {4,"Variant"},
    {8,"Implementor"},
    {}
};

static const char *reg_sizes[] = {
    "4K", "8K", "16K", "32K", "64K", "128K", "256K", "512K", 
    "1M", "2M", "4M", "8M", "16M", "32M", "64M", "128M", "256M", "512M",
    "1G", "2G", "4G",
};

static const char *regperm_str(unsigned val) {
    switch(val) {
        case 0: return "P:-- U:--";
        case 1: return "P:RW U:--";
        case 2: return "P:RW U:R-";
        case 3: return "P:RW U:RW";
        case 5: return "P:R- U:--";
        case 6: return "P:R- U:R-";
        default:
            return "P:?? U:??";
    }
}

#ifdef THUMB_FW
    #include "cpuinfo_v7.c"
#else
    #include "cpuinfo_v5.c"
#endif

// note, this is how many we know, nothing to do with how many debug_read_cpuinfo knows
#define NUM_CPUINFO_WORDS ((sizeof(cpuinfo_desc)/sizeof(cpuinfo_desc[0])) - 1)

void cpuinfo_finish(unsigned dummy);
void cpuinfo_get_info(unsigned *results);

void cpuinfo_print(void) {
    unsigned cpuinfo[NUM_CPUINFO_WORDS];
    int i,j;
    unsigned fieldval,wordval;
    unsigned mask,bits;
    FILE *finfo;
    char *p;
    cpuinfo_get_info(cpuinfo);
    for(i = 0; cpuinfo_desc[i].name; i++) {
        wordval = cpuinfo[i];
        printf("%-10s 0x%08X\n",cpuinfo_desc[i].name,wordval);
        
        for(j=0; cpuinfo_desc[i].fields[j].name; j++) {
            bits = cpuinfo_desc[i].fields[j].bits;
            mask = ~(0xFFFFFFFF << bits);
            fieldval = wordval & mask;
            printf("  %-20s 0x%X %d",cpuinfo_desc[i].fields[j].name,fieldval,fieldval);
            if(cpuinfo_desc[i].fields[j].desc_fn) {
                printf(" [%s]",cpuinfo_desc[i].fields[j].desc_fn(fieldval));
            }
            printf("\n");
            wordval >>= bits;
        }
    }
}
