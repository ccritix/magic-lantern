
#ifndef ml_lua_shim_h
#define ml_lua_shim_h

//#include <dryos.h>
#include <console.h>
#include <string.h>

#define err_printf(fmt,...) console_show(), printf(fmt, ## __VA_ARGS__)
#define lua_writestringerror(s,p) (err_printf((s), (p)))
#define lua_writestring(s,l) (err_printf("%s",(s)))
#define strcoll(a,b) strcmp(a,b)

int do_nothing();
char* my_getenv(const char* name);
void my_abort();
int ftoa(char *s, float n);

#endif
