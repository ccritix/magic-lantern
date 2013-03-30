/* TCC Hello World example */
/* It compiles C programs on the camera and runs them as scripts at native speed! */
/* Memory usage: 150K as ARM, 115K as Thumb */

/* You will have to link this with libtcc.a and dietlib. */

/* Based on tests/tcctest.c from tcc-0.9.26 */

#include "dryos.h"
#include "libtcc.h"

/* this function is called by the generated code */
int add(int a, int b)
{
    return a + b;
}

void tcc_init_library(TCCState *s)
{
    /* as a test, we add a symbol that the compiled program can use.
       You may also open a dll with tcc_add_dll() and use symbols from that */
    tcc_add_symbol(s, "add", add);
}

int tcc_compile_and_run(char* filename)
{
    int exit_code = 0;
    
    /* Read the source file */
    int size;
    char* source = (char*)read_entire_file(filename, &size);
    if (!source)
    {
        printf("Error loading '%s'\n", filename);
        exit_code = 1; goto end;
    }

    TCCState *s;
    int (*main)(int,int);

    /* create a TCC compilation context */
    s = tcc_new();
    if (!s)
        { exit_code = 1; goto end; }
    
    /* do not load standard libraries - we don't have them */
    tcc_set_options(s, "-nostdinc -nostdlib");

    /* MUST BE CALLED before any compilation */
    tcc_set_output_type(s, TCC_OUTPUT_MEMORY);

    /* compile our program from string */
    if (tcc_compile_string(s, source) == -1)
        { exit_code = 1; goto end; }

    tcc_init_library(s);

    /* relocate the code */
    if (tcc_relocate(s, TCC_RELOCATE_AUTO) < 0)
        { exit_code = 1; goto end; }

    /* get entry symbol */
    main = tcc_get_symbol(s, "main");
    if (!main)
    {
        printf("Could not find main()\n");
        exit_code = 1; goto end; 
    }
    
    /* http://repo.or.cz/w/tinycc.git/commit/6ed6a36a51065060bd5e9bb516b85ff796e05f30 */
    clean_d_cache();
    
    /* run the code */
    main(0, 0);

end:
    /* delete the compilation state and the source code buffer */
    if (s) tcc_delete(s);
    if (source) free_dma_memory(source);
    return exit_code;
}

void exit(int code) 
{
    console_printf("exit(%d)\n", code); 
    while(1) msleep(100); // fixme: stop the task and exit cleanly
}

int fprintf(FILE* unused, const char * fmt, ...)
{
    va_list            ap;
    char buf[128];
    va_start( ap, fmt );
    vsnprintf( buf, sizeof(buf)-1, fmt, ap );
    va_end( ap );
    console_puts(buf);
    return 0;
}

int printf(const char * fmt, ...)
{
    va_list            ap;
    char buf[128];
    va_start( ap, fmt );
    vsnprintf( buf, sizeof(buf)-1, fmt, ap );
    va_end( ap );
    console_puts(buf);
    return 0;
}

int puts(const char * fmt)
{
    console_puts(fmt);
    console_puts("\n");
    return 0;
}

int fputs(FILE* unused, const char * fmt)
{
    console_puts(fmt);
    return 0;
}

int putchar(int c)
{
    console_puts(&c);
    return c;
}

// no file I/O for now, but feel free to implement it

/* i don't really understand FIO_SeekSkipFile etc yet, so build seek-able open function */
typedef struct
{
    int size;
    int pos;
    char data;
} filehandle_t;

int open(const char *pathname, int flags)
{
    unsigned size = 0;
    FILE* file = NULL;
    filehandle_t *handle = NULL;
    
    if( FIO_GetFileSize( pathname, &size ) != 0 )
    {
        printf("Error loading '%s': File does not exist\n", pathname);
        return -1;
    }
    handle = alloc_dma_memory(sizeof(filehandle_t) + size);
    if(!handle)
    {
        printf("Error loading '%s': File too large\n", pathname);
        return -1;
    }
    
    handle->size = size;
    handle->pos = 0;
    
    file = FIO_Open(pathname, flags);
    if(!file)
    {
        printf("Error loading '%s': File does not exist\n", pathname);
        free_dma_memory(handle);
        return -1;
    }
    FIO_ReadFile(file, &handle->data, size);
    FIO_CloseFile(file);
    
    return (int)handle;
}

int read(int fd, void *buf, int size)
{
    filehandle_t *handle = (filehandle_t *)fd;
    int count = (size + handle->pos < handle->size)? (size) : (handle->size - handle->pos);
    
    memcpy(buf, ((uint32_t)&handle->data) + handle->pos, count);
    handle->pos += count;
    
    return count;
}

int close(int fd)
{
    free_dma_memory(fd);
    return 0;
}

int lseek(int fd, int offset, int whence)
{
    filehandle_t *handle = (filehandle_t *)fd;
    
    switch(whence)
    {
        case 0:
            handle->pos = offset;
            break;
        case 1:
            handle->pos += offset;
            break;
        case 2:
            handle->pos = handle->size - offset;
            break;
    }
    
    return handle->pos;
}

FILE* fopen(const char * filename, const char * modes)
{
    console_printf("fopen('%s', '%s')\n", filename, modes);
    if (modes[0] == 'r')
        return FIO_Open(filename, O_RDONLY | O_SYNC);
    else if (modes[0] == 'w')
        return FIO_CreateFileEx(filename);
    else printf("fopen: %s n/a\n", modes);
    return NULL;
}

int fclose(FILE* stream)
{
    FIO_CloseFile(stream);
    return 0;
}

size_t fread(void* ptr, size_t size, size_t count, FILE* stream)
{
    return FIO_ReadFile(stream, ptr, size * count);
}

size_t fwrite( const void * ptr, size_t size, size_t count, FILE * stream )
{
    return FIO_WriteFile(stream, ptr, size * count);
}

#define DUMMY(x) int x() { printf( #x "\n "); return 0; }

DUMMY(fputc)
DUMMY(fdopen)
DUMMY(unlink)
DUMMY(getenv)
DUMMY(time)
DUMMY(localtime)
DUMMY(getcwd)
DUMMY(system)
int _impure_ptr;



/*
for debugging: compile TCC with CFLAGS+=-finstrument-functions

void __cyg_profile_func_enter(void *this_fn, void *call_site)
                              __attribute__((no_instrument_function));

void __cyg_profile_func_enter(void *this_fn, void *call_site) {
  printf("ENTER: %x, from %x\n", this_fn, call_site);
}

void __cyg_profile_func_exit(void *this_fn, void *call_site)
                             __attribute__((no_instrument_function));
void __cyg_profile_func_exit(void *this_fn, void *call_site) {
  printf("EXIT:  %x, from %x\n", this_fn, call_site);
}

*/
