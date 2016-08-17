// cpuinfo, ARMv5 specific parts

static const char *cache_words_line_str(unsigned val) {
    switch(val) {
        case 2: return "8";
        default:
            return "Unknown";
    }
}

static const char *cache_tcm_size_str(unsigned val) {
    if (val == 0) 
        return "0";
    if (val < 3 || val > 11)
        return "invalid";
    return reg_sizes[val-3];
}


static const char *protreg_size_str(unsigned val) {
    if (val < 11 || val > 31)
        return "invalid";
    return reg_sizes[val - 11];
}

// "base" addresses actually overlap the rest of the register,
// because the lower 13 are implicitly zero due to alignment constraints
// here we return the actual address as a hex string
static const char *protreg_base_str(unsigned val) {
	static char str[11];
	sprintf(str,"0x%08X",val << 13);
	return str;
}


static const char *tcmcfg_size_str(unsigned val) {
    if ( val < 3 || val > 23 ) 
        return "invalid";
    return reg_sizes[val - 3];
}

const struct cpuinfo_bitfield_desc_s cpuinf_cachetype[] = {
    {2,"Icache words/line",cache_words_line_str},
    {1,"Icache absent"},
    {3,"Icache assoc"},
    {4,"Icache size",cache_tcm_size_str},
    {2,"Reserved0_2"},
    {2,"Dcache words/line",cache_words_line_str},
    {1,"Dcache absent"},
    {3,"Dcache assoc"},
    {4,"Dcache size",cache_tcm_size_str},
    {2,"Reserved1_2"},
    {1,"Harvard/unified"},
    {4,"Cache type"},
    {3,"Reserved2_3"},
    {}
};

const struct cpuinfo_bitfield_desc_s cpuinf_tcmtype[] = {
    {2,"Reserved0_2"},
    {1,"ITCM absent"},
    {3,"Reserved1_3"},
    {4,"ITCM size",cache_tcm_size_str},
    {4,"Reserved2_4"},
    {1,"DTCM absent"},
    {3,"Reserved3_2"},
    {4,"DTCM size",cache_tcm_size_str},
    {10,"Reserved4_10"},
    {}
};

const struct cpuinfo_bitfield_desc_s cpuinf_control[] = {
    {1,"Protect enable"},
    {1,"Reserved0_1"},
    {1,"Dcache enable"},
    {4,"Reserved1_4"},
    {1,"Big endian"},
    {4,"Reserved2_4"},
    {1,"Icache enable"},
    {1,"Alt vector"},
    {1,"Cache RRR"},
    {1,"Disble load TBIT"},
    {1,"DTCM enable"},
    {1,"DTCM mode"},
    {1,"ITCM enable"},
    {1,"ITCM mode"},
    {12,"Reserved3_12"},
    {}
};

const struct cpuinfo_bitfield_desc_s cpuinf_regbits[] = {
    {1,"Region 0"},
    {1,"Region 1"},
    {1,"Region 2"},
    {1,"Region 3"},
    {1,"Region 4"},
    {1,"Region 5"},
    {1,"Region 6"},
    {1,"Region 7"},
    {}
};

const struct cpuinfo_bitfield_desc_s cpuinf_protreg[] = {
    {1,"Enable"},
    {5,"Size",protreg_size_str},
    {7,"Undef0_7"},
    {19,"Base",protreg_base_str},
    {}
};

const struct cpuinfo_bitfield_desc_s cpuinf_regperms[] = {
    {4,"Region 0",regperm_str},
    {4,"Region 1",regperm_str},
    {4,"Region 2",regperm_str},
    {4,"Region 3",regperm_str},
    {4,"Region 4",regperm_str},
    {4,"Region 5",regperm_str},
    {4,"Region 6",regperm_str},
    {4,"Region 7",regperm_str},
    {}
};

const struct cpuinfo_bitfield_desc_s cpuinf_tcmcfg[] = {
    {1,"Reserved0_1"},
    {5,"Size",tcmcfg_size_str},
    {7,"Undef0_7"},
    {19,"Base",protreg_base_str},
    {}
};

const struct cpuinfo_word_desc_s cpuinfo_desc[]={
    {"ID", cpuinf_id },
    {"Cache type", cpuinf_cachetype },
    {"TCM type", cpuinf_tcmtype },
    {"Control", cpuinf_control },
    {"Protection Region 0",cpuinf_protreg },
    {"Protection Region 1",cpuinf_protreg },
    {"Protection Region 2",cpuinf_protreg },
    {"Protection Region 3",cpuinf_protreg },
    {"Protection Region 4",cpuinf_protreg },
    {"Protection Region 5",cpuinf_protreg },
    {"Protection Region 6",cpuinf_protreg },
    {"Protection Region 7",cpuinf_protreg },
    {"Region data perms",cpuinf_regperms },
    {"Region inst perms",cpuinf_regperms },
    {"DCache cfg", cpuinf_regbits },
    {"ICache cfg", cpuinf_regbits },
    {"Write buffer", cpuinf_regbits },
    {"DTCM cfg",cpuinf_tcmcfg },
    {"ITCM cfg",cpuinf_tcmcfg },
    {}
};


void __attribute__((naked,noinline)) cpuinfo_get_info(unsigned *results) {
    asm (
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
        "MRC    p15, 0, R1,c1,c0\n" // control bits
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"
        "MRC    p15, 0, R1,c6,c0\n" // protection region 0
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c6,c1\n" // protection region 1
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c6,c2\n" // protection region 2
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c6,c3\n" // protection region 3
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c6,c4\n" // protection region 4
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c6,c5\n" // protection region 5
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c6,c6\n" // protection region 6
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c6,c7\n" // protection region 7
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c5,c0,2\n" // data accesss perm
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c5,c0,3\n" // instruction accesss perm
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c2,c0\n" // data cache config
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"
        "MRC    p15, 0, R1,c2,c0,1\n" // instruction cache config
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"
        "MRC    p15, 0, R1,c3,c0\n" // write buffer config
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c9,c1\n" // DTCM config
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "MRC    p15, 0, R1,c9,c1,1\n" // ITCM config
        "ADD    R0, R0, #4\n"
        "STR    R1, [R0]\n"

        "BX     LR\n"

        :::"r0","r1"
    );
}
