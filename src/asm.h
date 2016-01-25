#ifndef _asm_h
#define _asm_h

char* asm_guess_func_name_from_string(uint32_t func_addr);
uint32_t find_func_called_before_string_ref(char* ref_string);
uint32_t locate_func_start(uint32_t search_from, uint32_t stop_addr);
uint32_t find_func_from_string(char* string, int Rd, uint32_t max_start_offset);

#endif
