asm(
    ".text\n"
    ".globl _start\n"
    "_start:\n"

    "B  0xFE0A0000\n"   /* jump to Canon firmware */
);
