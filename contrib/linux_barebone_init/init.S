

.section .data
.ascii "not needed"

.section .bss
.ascii "not needed"

.section .text

.global _start
.global _exit
.global main

_start:
main:
    mov     r2, #0x39
    mov     r1, pc
    add     r1, #0x18
    mov     r0, #1
    mov     r7, #4
    svc     0
_exit:
    sub     r0, r0, r0
    mov     r7, $0x1
    svc 0

message:
.ascii "\n\n   /bin/init says  --> Magic Lantern Linux rocks <--\n\n\n"

