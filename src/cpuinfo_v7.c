// cpuinfo, ARMv7 specific parts

static char linebuf[128]; // fixed buffer for outputting dynamic strings from interpreter functions

static const char *two_nth_str[] = {
"1", "2", "4", "8", "16", "32", "64", "128", "256", "512", "1K", "2K", "4K", "8K", "16K", "32K"
};

static const char *two_on_nth(unsigned val) {
    if (val < 16) {
        return two_nth_str[val];
    }
    return "invalid";
}

static const char *two_on_nth_granule(unsigned val) {
    if (val == 0) {
        return "no info";
    }
    else if (val > 9) {
        return "reserved";
    }
    else {
        return two_nth_str[val];
    }
    return "invalid";
}

const struct cpuinfo_bitfield_desc_s cpuinf_feat0[] = {
    {4,"ARM inst set"},
    {4,"Thumb inst set"},
    {4,"Jazelle inst set"},
    {4,"ThumbEE inst set"},
    {16,"-"},
    {}
};

const struct cpuinfo_bitfield_desc_s cpuinf_feat1[] = {
    {4,"Programmers' model"},
    {4,"Security extensions"},
    {4,"Microcontr. prog model"},
    {20,"-"},
    {}
};

const struct cpuinfo_bitfield_desc_s cpuinf_mmfr0[] = {
    {4,"VMSA support"},
    {4,"PMSA support"},
    {4,"Cache coherence"},
    {4,"Outer shareable"},
    {4,"TCM support"},
    {4,"Auxiliary registers"},
    {4,"FCSE support"},
    {4,"-"},
    {}
};

const struct cpuinfo_bitfield_desc_s cpuinf_mmfr1[] = {
    {4,"L1 Harvard cache VA"},
    {4,"L1 unified cache VA"},
    {4,"L1 Harvard cache s/w"},
    {4,"L1 unified cache s/w"},
    {4,"L1 Harvard cache"},
    {4,"L1 unified cache"},
    {4,"L1 cache test & clean"},
    {4,"Branch predictor"},
    {}
};

const struct cpuinfo_bitfield_desc_s cpuinf_mmfr2[] = {
    {4,"L1 Harvard fg prefetch"},
    {4,"L1 Harvard bg prefetch"},
    {4,"L1 Harvard range"},
    {4,"Harvard TLB"},
    {4,"Unified TLB"},
    {4,"Mem barrier"},
    {4,"WFI stall"},
    {4,"HW access flag"},
    {}
};

const struct cpuinfo_bitfield_desc_s cpuinf_mmfr3[] = {
    {4,"Cache maintain MVA"},
    {4,"Cache maintain s/w"},
    {4,"BP maintain"},
    {16,"-"},
    {4,"Supersection support"},
    {}
};

const struct cpuinfo_bitfield_desc_s cpuinf_isar0[] = {
    {4,"Swap instrs"},
    {4,"Bitcount instrs"},
    {4,"Bitfield instrs"},
    {4,"CmpBranch instrs"},
    {4,"Coproc instrs"},
    {4,"Debug instrs"},
    {4,"Divide instrs"},
    {4,"-"},
    {}
};

const struct cpuinfo_bitfield_desc_s cpuinf_isar1[] = {
    {4,"Endian instrs"},
    {4,"Exception instrs"},
    {4,"Exception AR instrs"},
    {4,"Extend instrs"},
    {4,"IfThen instrs"},
    {4,"Immediate instrs"},
    {4,"Interwork instrs"},
    {4,"Jazelle instrs"},
    {}
};

const struct cpuinfo_bitfield_desc_s cpuinf_isar2[] = {
    {4,"LoadStore instrs"},
    {4,"Memhint instrs"},
    {4,"MultiAccess Interruptible instructions"},
    {4,"Mult instrs"},
    {4,"MultS instrs"},
    {4,"MultU instrs"},
    {4,"PSR AR instrs"},
    {4,"Reversal instrs"},
    {}
};

const struct cpuinfo_bitfield_desc_s cpuinf_isar3[] = {
    {4,"Saturate instrs"},
    {4,"SIMD instrs"},
    {4,"SVC instrs"},
    {4,"SynchPrim instrs"},
    {4,"TabBranch instrs"},
    {4,"ThumbCopy instrs"},
    {4,"TrueNOP instrs"},
    {4,"T2 Exec Env instrs"},
    {}
};

const struct cpuinfo_bitfield_desc_s cpuinf_isar4[] = {
    {4,"Unprivileged instrs"},
    {4,"WithShifts instrs"},
    {4,"Writeback instrs"},
    {4,"SMC instrs"},
    {4,"Barrier instrs"},
    {4,"SynchPrim_instrs_frac"},
    {4,"PSR_M instrs"},
    {4,"-"},
    {}
};

const struct cpuinfo_bitfield_desc_s cpuinf_isar5[] = {
    {32,"-"},
    {}
};

const struct cpuinfo_bitfield_desc_s cpuinf_ctr[] = {
    {4,"Icache min words/line", two_on_nth},
    {10,"(zero)"},
    {2,"L1 Icache policy"},
    {4,"Dcache min words/line", two_on_nth},
    {4,"Exclusives Reservation Granule", two_on_nth_granule},
    {4,"Cache Writeback Granule", two_on_nth_granule},
    {1,"(zero)"},
    {3,"(register format)"},
    {}
};

static const char *ctype_str(unsigned val) {
    switch (val) {
        case 0: return "no cache";
        case 1: return "Icache only";
        case 2: return "Dcache only";
        case 3: return "Separate Icache, Dcache";
        case 4: return "Unified cache";
    }
    return "-";
}

const struct cpuinfo_bitfield_desc_s cpuinf_clidr[] = {
    {3,"Cache type, level1", ctype_str},
    {3,"Cache type, level2", ctype_str},
    {3,"Cache type, level3", ctype_str},
    {3,"Cache type, level4", ctype_str},
    {3,"Cache type, level5", ctype_str},
    {3,"Cache type, level6", ctype_str},
    {3,"Cache type, level7", ctype_str},
    {3,"Cache type, level8", ctype_str},
    {3,"Level of coherency"},
    {3,"Level of unification"},
    {2,"(zero)"},
    {}
};

const struct cpuinfo_bitfield_desc_s cpuinf_csselr[] = {
    {1,"Instruction, not data"},
    {3,"Level"},
    {28,"(unknown)"},
    {}
};

static const char *ccsidr_linesize(unsigned val) {
    return two_nth_str[val+2];
}

static const char *ccsidr_plusone(unsigned val) {
    sprintf(linebuf,"%i",val+1);
    return linebuf;
}

const struct cpuinfo_bitfield_desc_s cpuinf_ccsidr[] = {
    {3,"Line size in words", ccsidr_linesize},
    {10,"Associativity", ccsidr_plusone},
    {15,"Number of sets", ccsidr_plusone},
    {1,"Write allocation"},
    {1,"Read allocation"},
    {1,"Write back"},
    {1,"Write through"},
    {}
};

static const char *cache_tcm_size_str(unsigned val) {
    if (val == 0) 
        return "0";
    if (val < 3 || val > 14)
        return "invalid";
    return reg_sizes[val-3];
}

static const char *cache_tcm_addr_str(unsigned val) {
    sprintf(linebuf,"0x%08x",val<<12);
    return linebuf;
}

const struct cpuinfo_bitfield_desc_s cpuinf_tcmreg[] = {
    {1,"Enabled"},
    {1,"-"},
    {5,"Size", cache_tcm_size_str},
    {5,"-"},
    {20,"Base address", cache_tcm_addr_str},
    {}
};

const struct cpuinfo_bitfield_desc_s cpuinf_mputype[] = {
    {1,"S"},
    {7,"-"},
    {8,"Num of MPU regions"},
    {}
};

const struct cpuinfo_bitfield_desc_s cpuinf_mpubase[] = {
    {32,"Base address"},
    {}
};

static const char *mpu_region_size_str(unsigned val) {
    if (val < 4 || val > 31)
        return "invalid";
    if (val < 11)
        return two_nth_str[val+1];
    return reg_sizes[val-11];
}

static const char *bitfield8(unsigned val) {
    linebuf[8] = 0;
    int n;
    for (n=0; n<8; n++) {
        linebuf[7-n] = (val & (1<<n))?'1':'0';
    }
    return linebuf;
}

const struct cpuinfo_bitfield_desc_s cpuinf_mpusizeen[] = {
    {1,"Enabled"},
    {5,"Size", mpu_region_size_str},
    {2,"-"},
    {8,"Sub-regions disabled", bitfield8},
    {}
};

static const char *mpu_rattr(unsigned val) {
    char *s="";
    char *s2="";
    char *t;
    t = (val&4)?"Shared":"Non-shared";
    if (val&0x20) {
        switch (val&3) {
            case 0: s = "Inner Non-cacheable"; break;
            case 1: s = "Inner Write-back, write-allocate"; break;
            case 2: s = "Inner Write-through, no write-allocate"; break;
            case 3: s = "Inner Write-back, no write-allocate"; break;
        }
        switch ((val&0x18)>>3) {
            case 0: s2 = "Outer Non-cacheable"; break;
            case 1: s2 = "Outer Write-back, write-allocate"; break;
            case 2: s2 = "Outer Write-through, no write-allocate"; break;
            case 3: s2 = "Outer Write-back, no write-allocate"; break;
        }
        sprintf(linebuf,"%s; %s; %s",s, s2, t);
    }
    else {
        switch (val&0x1B) {
            case 0: s = "Strongly ordered, shareable"; t=""; break;
            case 1: s = "Shareable device"; t="Shareable"; break;
            case 2: s = "Outer and Inner write-through, no write-allocate"; break;
            case 3: s = "Outer and Inner write-back, no write-allocate"; break;
            case 8: s = "Outer and Inner Non-cacheable"; break;
            case 11: s = "Outer and Inner write-back, write-allocate"; break;
            case 16: s = "Non-shareable Device"; t=""; break;
            default: s = "(reserved)"; t="";
        }
        sprintf(linebuf,"%s; %s",s, t);
    }
    return linebuf;
}

const struct cpuinfo_bitfield_desc_s cpuinf_accesscontrol[] = {
    {6,"Region attributes", mpu_rattr},
    {2,"-"},
    {3,"Access permission", regperm_str},
    {1,"-"},
    {1,"Execute never"},
    {}
};

const struct cpuinfo_bitfield_desc_s cpuinf_generic[] = {
    {32,"(raw value)"},
    {}
};

const struct cpuinfo_word_desc_s cpuinfo_desc[]={
    {"ID", cpuinf_id },
    {"Cache type", cpuinf_ctr },
    {"TCM type", cpuinf_generic },
    {"MPU type", cpuinf_mputype },
    {"Processor feature 0", cpuinf_feat0 },
    {"Processor feature 1", cpuinf_feat1 },
    {"Debug feature", cpuinf_generic },
    {"Aux feature", cpuinf_generic },
    {"Mem model feature 0", cpuinf_mmfr0 },
    {"Mem model feature 1", cpuinf_mmfr1 },
    {"Mem model feature 2", cpuinf_mmfr2 },
    {"Mem model feature 3", cpuinf_mmfr3 },
    {"ISA feature 0", cpuinf_isar0 },
    {"ISA feature 1", cpuinf_isar1 },
    {"ISA feature 2", cpuinf_isar2 },
    {"ISA feature 3", cpuinf_isar3 },
    {"ISA feature 4", cpuinf_isar4 },
    {"ISA feature 5", cpuinf_isar5 },
    {"Cache level ID", cpuinf_clidr },
    {"Cache size ID reg (data, level0)", cpuinf_ccsidr },
    {"Cache size ID reg (inst, level0)", cpuinf_ccsidr },
    {"Build options 1", cpuinf_generic },
    {"Build options 2", cpuinf_generic },
    {"ATCM region reg", cpuinf_tcmreg },
    {"BTCM region reg", cpuinf_tcmreg },
    {"MPU region 0 base", cpuinf_mpubase },
    {"MPU region 0 size & enable", cpuinf_mpusizeen },
    {"MPU region 0 access control", cpuinf_accesscontrol },
    {"MPU region 1 base", cpuinf_mpubase },
    {"MPU region 1 size & enable", cpuinf_mpusizeen },
    {"MPU region 1 access control", cpuinf_accesscontrol },
    {"MPU region 2 base", cpuinf_mpubase },
    {"MPU region 2 size & enable", cpuinf_mpusizeen },
    {"MPU region 2 access control", cpuinf_accesscontrol },
    {"MPU region 3 base", cpuinf_mpubase },
    {"MPU region 3 size & enable", cpuinf_mpusizeen },
    {"MPU region 3 access control", cpuinf_accesscontrol },
    {"MPU region 4 base", cpuinf_mpubase },
    {"MPU region 4 size & enable", cpuinf_mpusizeen },
    {"MPU region 4 access control", cpuinf_accesscontrol },
    {"MPU region 5 base", cpuinf_mpubase },
    {"MPU region 5 size & enable", cpuinf_mpusizeen },
    {"MPU region 5 access control", cpuinf_accesscontrol },
    {"MPU region 6 base", cpuinf_mpubase },
    {"MPU region 6 size & enable", cpuinf_mpusizeen },
    {"MPU region 6 access control", cpuinf_accesscontrol },
    {"MPU region 7 base", cpuinf_mpubase },
    {"MPU region 7 size & enable", cpuinf_mpusizeen },
    {"MPU region 7 access control", cpuinf_accesscontrol },
    //{"Floating Point System ID register", cpuinf_generic },
    //{"Media and VFP Feature Register 0", cpuinf_generic },
    //{"Media and VFP Feature Register 1", cpuinf_generic },
    {}
};


void __attribute__((naked,noinline)) cpuinfo_get_info(unsigned *results) {
    asm (
        ".syntax unified\n"
        ".code 16\n"
        ".align 2\n"
        "BX      PC\n"                  // switch to ARM mode
        ".code 32\n"

        "MRC    p15, 0, R1,c0,c0\n" // ident
        "STR    R1, [R0]\n"
        "MRC    p15, 0, R1,c0,c0,1\n" // cache
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"
        "MRC    p15, 0, R1,c0,c0,2\n" // TCM
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c0,c0,4\n" // MPU
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c0,c1,0\n" // ID_PFR0
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c0,c1,1\n" // ID_PFR1
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c0,c1,2\n" //
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c0,c1,3\n" //
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c0,c1,4\n" //
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c0,c1,5\n" //
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c0,c1,6\n" //
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c0,c1,7\n" //
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c0,c2,0\n" //
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c0,c2,1\n" //
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c0,c2,2\n" //
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c0,c2,3\n" //
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c0,c2,4\n" //
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c0,c2,5\n" //
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 1, R1,c0,c0,1\n" // CLIDR
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MOV    R1, #0\n"
        "MCR    p15, 2, R1,c0,c0,0\n" // CSSELR (data cache, level0)

        "MRC    p15, 1, R1,c0,c0,0\n" // CCSIDR (the currently selected one)
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MOV    R1, #1\n"
        "MCR    p15, 2, R1,c0,c0,0\n" // CSSELR (inst cache, level0)

        "MRC    p15, 1, R1,c0,c0,0\n" // CCSIDR (the currently selected one)
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c15,c2,0\n" // Build options 1 reg
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c15,c2,1\n" // Build options 2 reg
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c9,c1,1\n" // ATCM region reg
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c9,c1,0\n" // BTCM region reg
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MOV    R1, #0\n"
        "MCR    p15, 0, R1,c6,c2,0\n" // MPU Memory Region Number Register, region 0

        "MRC    p15, 0, R1,c6,c1,0\n" // MPU region base register
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"
        "MRC    p15, 0, R1,c6,c1,2\n" // MPU Region Size and Enable Register
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"
        "MRC    p15, 0, R1,c6,c1,4\n" // MPU Region Access Control Register
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MOV    R1, #1\n"
        "MCR    p15, 0, R1,c6,c2,0\n" // region 1

        "MRC    p15, 0, R1,c6,c1,0\n" // MPU region base register
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"
        "MRC    p15, 0, R1,c6,c1,2\n" // MPU Region Size and Enable Register
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"
        "MRC    p15, 0, R1,c6,c1,4\n" // MPU Region Access Control Register
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MOV    R1, #2\n"
        "MCR    p15, 0, R1,c6,c2,0\n" // region 2

        "MRC    p15, 0, R1,c6,c1,0\n" // MPU region base register
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"
        "MRC    p15, 0, R1,c6,c1,2\n" // MPU Region Size and Enable Register
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"
        "MRC    p15, 0, R1,c6,c1,4\n" // MPU Region Access Control Register
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MOV    R1, #3\n"
        "MCR    p15, 0, R1,c6,c2,0\n" // region 3

        "MRC    p15, 0, R1,c6,c1,0\n" // MPU region base register
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"
        "MRC    p15, 0, R1,c6,c1,2\n" // MPU Region Size and Enable Register
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"
        "MRC    p15, 0, R1,c6,c1,4\n" // MPU Region Access Control Register
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MOV    R1, #4\n"
        "MCR    p15, 0, R1,c6,c2,0\n" // region 4

        "MRC    p15, 0, R1,c6,c1,0\n" // MPU region base register
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"
        "MRC    p15, 0, R1,c6,c1,2\n" // MPU Region Size and Enable Register
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"
        "MRC    p15, 0, R1,c6,c1,4\n" // MPU Region Access Control Register
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MOV    R1, #5\n"
        "MCR    p15, 0, R1,c6,c2,0\n" // region 5

        "MRC    p15, 0, R1,c6,c1,0\n" // MPU region base register
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"
        "MRC    p15, 0, R1,c6,c1,2\n" // MPU Region Size and Enable Register
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"
        "MRC    p15, 0, R1,c6,c1,4\n" // MPU Region Access Control Register
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MOV    R1, #6\n"
        "MCR    p15, 0, R1,c6,c2,0\n" // region 6

        "MRC    p15, 0, R1,c6,c1,0\n" // MPU region base register
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"
        "MRC    p15, 0, R1,c6,c1,2\n" // MPU Region Size and Enable Register
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"
        "MRC    p15, 0, R1,c6,c1,4\n" // MPU Region Access Control Register
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MOV    R1, #7\n"
        "MCR    p15, 0, R1,c6,c2,0\n" // region 7

        "MRC    p15, 0, R1,c6,c1,0\n" // MPU region base register
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"
        "MRC    p15, 0, R1,c6,c1,2\n" // MPU Region Size and Enable Register
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"
        "MRC    p15, 0, R1,c6,c1,4\n" // MPU Region Access Control Register
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        //".word  0xEEF01A10\n" //"VMRS   R1, FPSID\n" // Floating Point System ID register
        //"ADD    R0, R0, #4\n"
        //"STR    R1, [R0]\n"

        //".word  0xEEF71A10\n" //"VMRS   R1, MVFR0\n" // Media and VFP Feature Register 0
        //"ADD    R0, R0, #4\n"
        //"STR    R1, [R0]\n"

        //".word  0xEEF61A10\n" //"VMRS   R1, MVFR1\n" // Media and VFP Feature Register 1
        //"ADD    R0, R0, #4\n"
        //"STR    R1, [R0]\n"

        "BX     LR\n"

        :::"r0","r1"
    );
}
