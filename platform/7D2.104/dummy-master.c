asm(
    ".text\n"
    ".globl _start\n"
    "_start:\n"

    /* jump to Canon firmware */
    /* note: with B 0xFE0A0000, gcc inserts a veneer */
    "LDR PC, =0xFE0A0000\n"
);
