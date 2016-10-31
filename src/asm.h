
#ifndef _asm_h
#define _asm_h

char* asm_guess_func_name_from_string(uint32_t func_addr);

/* returns true if the machine will not lock up when dereferencing ptr */
int is_sane_ptr(uint32_t ptr);

/* returns true if addr seems to be a pointer to a (short) string */
int looks_like_string(uint32_t addr);

#endif
