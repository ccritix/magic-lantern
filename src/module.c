
#include "dryos.h"
#include "menu.h"
#include "console.h"
#include "libtcc.h"
#include "module.h"
#include "config.h"
#include "string.h"
#include "property.h"

#ifndef CONFIG_MODULES_MODEL_SYM
#error Not defined file name with symbols
#endif
#define MAGIC_SYMBOLS                 CARD_DRIVE"ML/MODULES/"CONFIG_MODULES_MODEL_SYM

/* unloads TCC after linking the modules */
/* note: this breaks module_exec and ETTR */
//~ #define CONFIG_TCC_UNLOAD

extern int sscanf(const char *str, const char *format, ...);


/* this must be public as it is used by modules */
char *module_card_drive = CARD_DRIVE;

static module_entry_t module_list[MODULE_COUNT_MAX];

#ifdef CONFIG_TCC_UNLOAD
static void* module_code = NULL;
#else
static TCCState *module_state = NULL;
#endif

static struct menu_entry module_submenu[];
static struct menu_entry module_menu[];

CONFIG_INT("module.autoload", module_autoload_enabled, 0);
CONFIG_INT("module.console", module_console_enabled, 0);
char *module_lockfile = MODULE_PATH"LOADING.LCK";

static struct msg_queue * module_mq = 0;
#define MSG_MODULE_LOAD_ALL 1
#define MSG_MODULE_UNLOAD_ALL 2

void module_load_all(void)
{ 
    msg_queue_post(module_mq, MSG_MODULE_LOAD_ALL); 
}
void module_unload_all(void)
{
    msg_queue_post(module_mq, MSG_MODULE_UNLOAD_ALL); 
}

static void _module_load_all(uint32_t);
static void _module_unload_all(void);

static int module_load_symbols(TCCState *s, char *filename)
{
    uint32_t size = 0;
    FILE* file = NULL;
    char *buf = NULL;
    uint32_t count = 0;
    uint32_t pos = 0;

    if( FIO_GetFileSize( filename, &size ) != 0 )
    {
        console_printf("Error loading '%s': File does not exist\n", filename);
        return -1;
    }
    buf = alloc_dma_memory(size);
    if(!buf)
    {
        console_printf("Error loading '%s': File too large\n", filename);
        return -1;
    }

    file = FIO_Open(filename, O_RDONLY | O_SYNC);
    if(!file)
    {
        console_printf("Error loading '%s': File does not exist\n", filename);
        free_dma_memory(buf);
        return -1;
    }
    FIO_ReadFile(file, buf, size);
    FIO_CloseFile(file);

    while(buf[pos])
    {
        char address_buf[16];
        char symbol_buf[128];
        uint32_t length = 0;
        uint32_t address = 0;

        while(buf[pos + length] && buf[pos + length] != ' ' && length < sizeof(address_buf))
        {
            address_buf[length] = buf[pos + length];
            length++;
        }
        address_buf[length] = '\000';

        pos += length + 1;
        length = 0;

        while(buf[pos + length] && buf[pos + length] != '\r' && buf[pos + length] != '\n' && length < sizeof(symbol_buf))
        {
            symbol_buf[length] = buf[pos + length];
            length++;
        }
        symbol_buf[length] = '\000';

        pos += length + 1;
        length = 0;

        while(buf[pos + length] && (buf[pos + length] == '\r' || buf[pos + length] == '\n'))
        {
            pos++;
        }
        sscanf(address_buf, "%x", &address);

        tcc_add_symbol(s, symbol_buf, (void*)address);
        count++;
    }
    //console_printf("Added %d Magic Lantern symbols\n", count);


    /* these are just to make the code compile */
    void longjmp();
    void setjmp();

    /* ToDo: parse the old plugin sections as all needed OS stubs are already described there */
    tcc_add_symbol(s, "msleep", &msleep);
    tcc_add_symbol(s, "longjmp", &longjmp);
    tcc_add_symbol(s, "strcpy", &strcpy);
    tcc_add_symbol(s, "setjmp", &setjmp);
    tcc_add_symbol(s, "alloc_dma_memory", &alloc_dma_memory);
    tcc_add_symbol(s, "free_dma_memory", &free_dma_memory);
    tcc_add_symbol(s, "vsnprintf", &vsnprintf);
    tcc_add_symbol(s, "strlen", &strlen);
    tcc_add_symbol(s, "memcpy", &memcpy);
    tcc_add_symbol(s, "console_printf", &console_printf);
    tcc_add_symbol(s, "task_create", &task_create);

    free_dma_memory(buf);
    return 0;
}

/* this is not perfect, as .Mo and .mO aren't detected. important? */
static int module_valid_filename(char* filename)
{
    int n = strlen(filename);
    if ((n > 3) && (streq(filename + n - 3, ".MO") || streq(filename + n - 3, ".mo")) && (filename[0] != '.') && (filename[0] != '_'))
        return 1;
    return 0;
}

static void _module_load_all(uint32_t list_only)
{
    TCCState *state = NULL;
    uint32_t module_cnt = 0;
    struct fio_file file;
    uint32_t update_properties = 0;

#ifdef CONFIG_MODULE_UNLOAD
    /* ensure all modules are unloaded */
    console_printf("Unloading modules...\n");
    _module_unload_all();
#endif

#ifdef CONFIG_TCC_UNLOAD
    if (module_code)
#else
    if (module_state)
#endif
    {
        console_printf("Modules already loaded.\n");
        beep();
        return;
    }

    /* initialize linker */
    state = tcc_new();
    tcc_set_options(state, "-nostdlib");
    if(module_load_symbols(state, MAGIC_SYMBOLS) < 0)
    {
        NotifyBox(2000, "Missing symbol file: " MAGIC_SYMBOLS );
        tcc_delete(state);
        return;
    }

    console_printf("Scanning modules...\n");
    struct fio_dirent * dirent = FIO_FindFirstEx( MODULE_PATH, &file );
    if( IS_ERROR(dirent) )
    {
        NotifyBox(2000, "Module dir missing" );
        tcc_delete(state);
        return;
    }

    do
    {
        if (file.mode & ATTR_DIRECTORY) continue; // is a directory
        if (module_valid_filename(file.name))
        {
            char module_name[MODULE_FILENAME_LENGTH];

            /* get filename, remove extension and append _init to get the init symbol */
            //console_printf("  [i] found: %s\n", file.name);
            
            /* ensure the buffer is null terminated */
            memset(module_name, 0x00, sizeof(module_name));
            strncpy(module_name, file.name, MODULE_NAME_LENGTH);
            strncpy(module_list[module_cnt].filename, file.name, MODULE_FILENAME_LENGTH);

            uint32_t pos = 0;
            while(module_name[pos])
            {
                /* extension starting? terminate string */
                if(module_name[pos] == '.')
                {
                    module_name[pos] = '\000';
                    break;
                }
                else if(module_name[pos] >= 'A' && module_name[pos] <= 'Z')
                {
                    /* make lowercase */
                    module_name[pos] |= 0x20;
                }
                pos++;
            }
            strncpy(module_list[module_cnt].name, module_name, sizeof(module_list[module_cnt].name));
            
            /* check for a .dis file that tells the module is disabled */
            char disable_file[MODULE_FILENAME_LENGTH];
            snprintf(disable_file, sizeof(disable_file), MODULE_PATH"%s.dis", module_list[module_cnt].name);
            
            /* if disable-file is existent, dont load module */
            if(config_flag_file_setting_load(disable_file))
            {
                module_list[module_cnt].enabled = 0;
                snprintf(module_list[module_cnt].status, sizeof(module_list[module_cnt].status), "Off");
                snprintf(module_list[module_cnt].long_status, sizeof(module_list[module_cnt].long_status), "Module disabled");
                //console_printf("  [i] %s\n", module_list[module_cnt].long_status);
            }
            else
            {
                module_list[module_cnt].enabled = 1;
            
                if(list_only)
                {
                    snprintf(module_list[module_cnt].status, sizeof(module_list[module_cnt].status), "");
                    snprintf(module_list[module_cnt].long_status, sizeof(module_list[module_cnt].long_status), "Module not loaded");
                }
                else
                {
                    snprintf(module_list[module_cnt].status, sizeof(module_list[module_cnt].status), "???");
                    snprintf(module_list[module_cnt].long_status, sizeof(module_list[module_cnt].long_status), "Seems linking failed. Unknown symbols?");
                }
            }

            module_cnt++;
            if (module_cnt >= MODULE_COUNT_MAX)
            {
                NotifyBox(2000, "Too many modules" );
                break;
            }
        }
    } while( FIO_FindNextEx( dirent, &file ) == 0);
    FIO_CleanupAfterFindNext_maybe(dirent);

    /* dont load anything, just return */
    if(list_only)
    {
        tcc_delete(state);
        return;
    }
    
    /* load modules */
    console_printf("Load modules...\n");
    for (uint32_t mod = 0; mod < module_cnt; mod++)
    {
        if(module_list[mod].enabled)
        {
            console_printf("  [i] load: %s\n", module_list[mod].filename);
            snprintf(module_list[mod].long_filename, sizeof(module_list[mod].long_filename), "%s%s", MODULE_PATH, module_list[mod].filename);
            int32_t ret = tcc_add_file(state, module_list[mod].long_filename);
            module_list[mod].valid = 1;

            /* seems bad, disable it */
            if(ret < 0)
            {
                module_list[mod].error = 1;
                snprintf(module_list[mod].status, sizeof(module_list[mod].status), "FileErr");
                snprintf(module_list[mod].long_status, sizeof(module_list[mod].long_status), "Load failed: %s, ret 0x%02X");
                console_printf("  [E] %s\n", module_list[mod].long_status);
            }
        }
    }

    console_printf("Linking..\n");
#ifdef CONFIG_TCC_UNLOAD
    int32_t size = tcc_relocate(state, NULL);
    int32_t reloc_status = -1;
    
    if (size > 0)
    {
        /* TCC allocates up to 2x the memory needed (e.g. raw_rec: uses ~17K but TCC reserves 34) */
        /* raw_rec + file_man + pic_view + ettr: used 37.2K, allocated 74.4K */
        /** tccrun.c:
         * / * double the size of the buffer for got and plt entries
         *  XXX: calculate exact size for them? * /
         *  offset *= 2;
         */
        /* but we can recover it; the space will add up when loading large and/or many modules */

        size = ALIGN32SUP(size);
        void* buf = (void*) tcc_malloc(size);
        
        /* mark the allocated space, so we know how much it was actually used */
        for (uint32_t* p = buf; p < buf + size; p++)
            *p = 0x12345678;

        reloc_status = tcc_relocate(state, buf);
        
        /* recover unused space */
        uint32_t* end = buf + size - 4;
        while ((void*)end > buf && *end == 0x12345678) end--;
        end++;
        int new_size = (void*)end - buf;
        buf = (void*)tcc_realloc(buf, new_size);
        console_printf("Memory: before %dK, after %dK\n", size/1024, new_size/1024);

        /* http://repo.or.cz/w/tinycc.git/commit/6ed6a36a51065060bd5e9bb516b85ff796e05f30 */
        clean_d_cache();

        module_code = buf;
    }
    if(size < 0 || reloc_status < 0)
#else
    int32_t ret = tcc_relocate(state, TCC_RELOCATE_AUTO);
    if(ret < 0)
#endif
    {
        console_printf("  [E] failed to link modules\n");
        for (uint32_t mod = 0; mod < module_cnt; mod++)
        {
            if(module_list[mod].enabled)
            {
                module_list[mod].error = 1;
                snprintf(module_list[mod].status, sizeof(module_list[mod].status), "Err");
                snprintf(module_list[mod].long_status, sizeof(module_list[mod].long_status), "Linking failed");
            }
        }
        tcc_delete(state);
        return;
    }
    
    /* load modules symbols */
    console_printf("Register modules...\n");
    for (uint32_t mod = 0; mod < module_cnt; mod++)
    {
        if(module_list[mod].valid && module_list[mod].enabled && !module_list[mod].error)
        {
            char module_info_name[32];

            /* now check for info structure */
            snprintf(module_info_name, sizeof(module_info_name), "%s%s", STR(MODULE_INFO_PREFIX), module_list[mod].name);
            module_list[mod].info = tcc_get_symbol(state, module_info_name);
            snprintf(module_info_name, sizeof(module_info_name), "%s%s", STR(MODULE_STRINGS_PREFIX), module_list[mod].name);
            module_list[mod].strings = tcc_get_symbol(state, module_info_name);
            snprintf(module_info_name, sizeof(module_info_name), "%s%s", STR(MODULE_PARAMS_PREFIX), module_list[mod].name);
            module_list[mod].params = tcc_get_symbol(state, module_info_name);
            snprintf(module_info_name, sizeof(module_info_name), "%s%s", STR(MODULE_PROPHANDLERS_PREFIX), module_list[mod].name);
            module_list[mod].prop_handlers = tcc_get_symbol(state, module_info_name);
            snprintf(module_info_name, sizeof(module_info_name), "%s%s", STR(MODULE_CBR_PREFIX), module_list[mod].name);
            module_list[mod].cbr = tcc_get_symbol(state, module_info_name);
            snprintf(module_info_name, sizeof(module_info_name), "%s%s", STR(MODULE_CONFIG_PREFIX), module_list[mod].name);
            module_list[mod].config = tcc_get_symbol(state, module_info_name);

            /* check if the module symbol is defined. simple check for valid memory address just in case. */
            if((uint32_t)module_list[mod].info > 0x1000)
            {
                if(module_list[mod].info->api_magic == MODULE_MAGIC)
                {
                    if((module_list[mod].info->api_major == MODULE_MAJOR) && (module_list[mod].info->api_minor <= MODULE_MINOR))
                    {
                        /* this module seems to be sane */
                    }
                    else
                    {
                        module_list[mod].error = 1;
                        snprintf(module_list[mod].status, sizeof(module_list[mod].status), "OldAPI");
                        snprintf(module_list[mod].long_status, sizeof(module_list[mod].long_status), "Wrong version (v%d.%d, expected v%d.%d)\n", module_list[mod].info->api_major, module_list[mod].info->api_minor, MODULE_MAJOR, MODULE_MINOR);
                        console_printf("  [E] %s\n", module_list[mod].long_status);
                    }
                }
                else
                {
                    module_list[mod].error = 1;
                    snprintf(module_list[mod].status, sizeof(module_list[mod].status), "Magic");
                    snprintf(module_list[mod].long_status, sizeof(module_list[mod].long_status), "Invalid Magic (0x%X)\n", module_list[mod].info->api_magic);
                    console_printf("  [E] %s\n", module_list[mod].long_status);
                }
            }
            else
            {
                module_list[mod].error = 1;
                snprintf(module_list[mod].status, sizeof(module_list[mod].status), "noInfo");
                snprintf(module_list[mod].long_status, sizeof(module_list[mod].long_status), "No info structure found (addr 0x%X)\n", (uint32_t)module_list[mod].info);
                console_printf("  [E] %s\n", module_list[mod].long_status);
            }
        }
    }
    
    console_printf("Load configs...\n");
    for (uint32_t mod = 0; mod < module_cnt; mod++)
    {
        if(module_list[mod].enabled && module_list[mod].valid && !module_list[mod].error)
        {
            char filename[64];
            snprintf(filename, sizeof(filename), "%s%s.cfg", MODULE_PATH, module_list[mod].name);
            module_config_load(filename, &module_list[mod]);
        }
    }
    
    /* go through all modules and initialize them */
    console_printf("Init modules...\n");
    for (uint32_t mod = 0; mod < module_cnt; mod++)
    {
        if(module_list[mod].valid && module_list[mod].enabled && !module_list[mod].error)
        {
            console_printf("  [i] Init: '%s'\n", module_list[mod].name);
            if(0)
            {
                console_printf("  [i] info    at: 0x%08X\n", (uint32_t)module_list[mod].info);
                console_printf("  [i] strings at: 0x%08X\n", (uint32_t)module_list[mod].strings);
                console_printf("  [i] params  at: 0x%08X\n", (uint32_t)module_list[mod].params);
                console_printf("  [i] props   at: 0x%08X\n", (uint32_t)module_list[mod].prop_handlers);
                console_printf("  [i] cbr     at: 0x%08X\n", (uint32_t)module_list[mod].cbr);
                console_printf("  [i] config  at: 0x%08X\n", (uint32_t)module_list[mod].config);
                console_printf("-----------------------------\n");
            }
            
            /* initialize module */
            if(module_list[mod].info->init)
            {
                module_list[mod].info->init();
            }
            
            /* register property handlers */
            if(module_list[mod].prop_handlers)
            {
                module_prophandler_t **props = module_list[mod].prop_handlers;
                while(*props != NULL)
                {
                    update_properties = 1;
                    console_printf("  [i] prop %s\n", (*props)->name);
                    prop_add_handler((*props)->property, (*props)->handler);
                    props++;
                }
            }
            
            if(0)
            {
                console_printf("-----------------------------\n");
            }
            snprintf(module_list[mod].status, sizeof(module_list[mod].status), "OK");
            snprintf(module_list[mod].long_status, sizeof(module_list[mod].long_status), "Module loaded successfully");
        }
    }
    
    
    if(update_properties)
    {
        prop_update_registration();
    }
    
    #ifdef CONFIG_TCC_UNLOAD
    tcc_delete(state);
    #else
    module_state = state;
    #endif
    
    console_printf("Modules loaded\n");
    
    if(!module_console_enabled)
    {
        console_hide();
    }
}

static void _module_unload_all(void)
{
/* unloading is not yet clean, we can end up with tasks running from freed memory or stuff like that */
#ifdef CONFIG_MODULE_UNLOAD
    if(module_state)
    {
        TCCState *state = module_state;
        module_state = NULL;
        
        /* unregister all property handlers */
        prop_reset_registration();
        
        /* deinit and clean all modules */
        for(int mod = 0; mod < MODULE_COUNT_MAX; mod++)
        {
            if(module_list[mod].valid && module_list[mod].enabled && !module_list[mod].error)
            {
                if(module_list[mod].info && module_list[mod].info->deinit)
                {
                    module_list[mod].info->deinit();
                }
            }
            module_list[mod].valid = 0;
            module_list[mod].enabled = 0;
            module_list[mod].error = 0;
            module_list[mod].info = NULL;
            module_list[mod].strings = NULL;
            module_list[mod].params = NULL;
            module_list[mod].prop_handlers = NULL;
            module_list[mod].cbr = NULL;
            strcpy(module_list[mod].name, "");
            strcpy(module_list[mod].filename, "");
        }

        /* release the global module state */
        tcc_delete(state);
    }
#endif
}

void *module_load(char *filename)
{
    int ret = -1;
    TCCState *state = NULL;

    state = tcc_new();
    tcc_set_options(state, "-nostdlib");

    if(module_load_symbols(state, MAGIC_SYMBOLS) < 0)
    {
        NotifyBox(2000, "Missing symbol file: " MAGIC_SYMBOLS );
        tcc_delete(state);
        return NULL;
    }

    ret = tcc_add_file(state, filename);
    if(ret < 0)
    {
        tcc_delete(state);
        return NULL;
    }

    ret = tcc_relocate(state, TCC_RELOCATE_AUTO);
    if(ret < 0)
    {
        tcc_delete(state);
        return NULL;
    }

    return (void*)state;
}

unsigned int module_get_symbol(void *module, char *symbol)
{
#ifdef CONFIG_TCC_UNLOAD
    return 0;
#else
    if (module == NULL) module = module_state;
    if (module == NULL) return 0;
    
    TCCState *state = (TCCState *)module;
    
    return tcc_get_symbol(state, symbol);
#endif
}


int module_exec(void *module, char *symbol, int count, ...)
{
#ifdef CONFIG_TCC_UNLOAD
    return -1;
#else
    int ret = -1;
    if (module == NULL) module = module_state;
    if (module == NULL) return ret;
    
    TCCState *state = (TCCState *)module;
    void *start_symbol = NULL;
    va_list args;
    va_start(args, count);

    start_symbol = tcc_get_symbol(state, symbol);

    /* check if the module symbol is defined. simple check for valid memory address just in case. */
    if((uint32_t)start_symbol > 0x1000)
    {
        /* no parameters, special case */
        if(count == 0)
        {
            uint32_t (*exec)() = start_symbol;
            ret = exec();
        }
        else
        {
            uint32_t (*exec)(uint32_t parm1, ...) = start_symbol;

            uint32_t parms[10];
            for(int parm = 0; parm < count; parm++)
            {
                parms[parm] = va_arg(args,uint32_t);
            }

            switch(count)
            {
                case 1:
                    ret = exec(parms[0]);
                    break;
                case 2:
                    ret = exec(parms[0], parms[1]);
                    break;
                case 3:
                    ret = exec(parms[0], parms[1], parms[2]);
                    break;
                case 4:
                    ret = exec(parms[0], parms[1], parms[2], parms[3]);
                    break;
                case 5:
                    ret = exec(parms[0], parms[1], parms[2], parms[3], parms[4]);
                    break;
                case 6:
                    ret = exec(parms[0], parms[1], parms[2], parms[3], parms[4], parms[5]);
                    break;
                case 7:
                    ret = exec(parms[0], parms[1], parms[2], parms[3], parms[4], parms[5], parms[6]);
                    break;
                case 8:
                    ret = exec(parms[0], parms[1], parms[2], parms[3], parms[4], parms[5], parms[6], parms[7]);
                    break;
                default:
                    /* so many parameters?! */
                    NotifyBox(2000, "Passing too many parameters to '%s'", symbol );
                    break;
            }
        }
    }
    va_end(args);
    return ret;
#endif
}


int module_unload(void *module)
{
#ifndef CONFIG_TCC_UNLOAD
    TCCState *state = (TCCState *)module;
    tcc_delete(state);
#endif
    return 0;
}


/* execute all callback routines of given type. maybe it will get extended to support varargs */
int FAST module_exec_cbr(unsigned int type)
{
    for(int mod = 0; mod < MODULE_COUNT_MAX; mod++)
    {
        module_cbr_t *cbr = module_list[mod].cbr;
        if(module_list[mod].valid && cbr)
        {
            while(cbr->name)
            {
                if(cbr->type == type)
                {
                    cbr->handler(cbr->ctx);
                }
                cbr++;
            }
        }
    }
    
    return 0;
}

/* translate camera specific key to portable module key */
#define MODULE_TRANSLATE_KEY(in,out,dest) if(dest == MODULE_KEY_PORTABLE) { if(in != -1){ if(key == in) { return out; } }} else {if(out != -1){ if(key == out) { return in; } }}

/* these are to ensure that the checked keys are defined. we have to ensure they're defined before using. are there better ways to ensure? */
#if !defined(BGMT_WHEEL_UP)
#define BGMT_WHEEL_UP -1
#endif
#if !defined(BGMT_WHEEL_DOWN)
#define BGMT_WHEEL_DOWN -1
#endif
#if !defined(BGMT_WHEEL_LEFT)
#define BGMT_WHEEL_LEFT -1
#endif
#if !defined(BGMT_WHEEL_RIGHT)
#define BGMT_WHEEL_RIGHT -1
#endif
#if !defined(BGMT_PRESS_SET)
#define BGMT_PRESS_SET -1
#endif
#if !defined(BGMT_UNPRESS_SET)
#define BGMT_UNPRESS_SET -1
#endif
#if !defined(BGMT_MENU)
#define BGMT_MENU -1
#endif
#if !defined(BGMT_INFO)
#define BGMT_INFO -1
#endif
#if !defined(BGMT_PLAY)
#define BGMT_PLAY -1
#endif
#if !defined(BGMT_TRASH)
#define BGMT_TRASH -1
#endif
#if !defined(BGMT_PRESS_DP)
#define BGMT_PRESS_DP -1
#endif
#if !defined(BGMT_UNPRESS_DP)
#define BGMT_UNPRESS_DP -1
#endif
#if !defined(BGMT_RATE)
#define BGMT_RATE -1
#endif
#if !defined(BGMT_REC)
#define BGMT_REC -1
#endif
#if !defined(BGMT_PRESS_ZOOMIN_MAYBE)
#define BGMT_PRESS_ZOOMIN_MAYBE -1
#endif
#if !defined(BGMT_LV)
#define BGMT_LV -1
#endif
#if !defined(BGMT_Q)
#define BGMT_Q -1
#endif
#if !defined(BGMT_PICSTYLE)
#define BGMT_PICSTYLE -1
#endif
#if !defined(BGMT_JOY_CENTER)
#define BGMT_JOY_CENTER -1
#endif
#if !defined(BGMT_PRESS_UP)
#define BGMT_PRESS_UP -1
#endif
#if !defined(BGMT_PRESS_UP_RIGHT)
#define BGMT_PRESS_UP_RIGHT -1
#endif
#if !defined(BGMT_PRESS_UP_LEFT)
#define BGMT_PRESS_UP_LEFT -1
#endif
#if !defined(BGMT_PRESS_RIGHT)
#define BGMT_PRESS_RIGHT -1
#endif
#if !defined(BGMT_PRESS_LEFT)
#define BGMT_PRESS_LEFT -1
#endif
#if !defined(BGMT_PRESS_DOWN_RIGHT)
#define BGMT_PRESS_DOWN_RIGHT -1
#endif
#if !defined(BGMT_PRESS_DOWN_LEFT)
#define BGMT_PRESS_DOWN_LEFT -1
#endif
#if !defined(BGMT_PRESS_DOWN)
#define BGMT_PRESS_DOWN -1
#endif
#if !defined(BGMT_UNPRESS_UDLR)
#define BGMT_UNPRESS_UDLR -1
#endif
#if !defined(BGMT_PRESS_HALFSHUTTER)
#define BGMT_PRESS_HALFSHUTTER -1
#endif
#if !defined(BGMT_UNPRESS_HALFSHUTTER)
#define BGMT_UNPRESS_HALFSHUTTER -1
#endif
#if !defined(BGMT_PRESS_FULLSHUTTER)
#define BGMT_PRESS_FULLSHUTTER -1
#endif
#if !defined(BGMT_UNPRESS_FULLSHUTTER)
#define BGMT_UNPRESS_FULLSHUTTER -1
#endif
#if !defined(BGMT_PRESS_FLASH_MOVIE)
#define BGMT_PRESS_FLASH_MOVIE -1
#endif
#if !defined(BGMT_UNPRESS_FLASH_MOVIE)
#define BGMT_UNPRESS_FLASH_MOVIE -1
#endif
int module_translate_event(struct event* event, int dest)
{
    int key = event->param;

    MODULE_TRANSLATE_KEY(BGMT_WHEEL_UP             , MODULE_KEY_WHEEL_UP             , dest);
    MODULE_TRANSLATE_KEY(BGMT_WHEEL_DOWN           , MODULE_KEY_WHEEL_DOWN           , dest);
    MODULE_TRANSLATE_KEY(BGMT_WHEEL_LEFT           , MODULE_KEY_WHEEL_LEFT           , dest);
    MODULE_TRANSLATE_KEY(BGMT_WHEEL_RIGHT          , MODULE_KEY_WHEEL_RIGHT          , dest);
    MODULE_TRANSLATE_KEY(BGMT_PRESS_SET            , MODULE_KEY_PRESS_SET            , dest);
    MODULE_TRANSLATE_KEY(BGMT_UNPRESS_SET          , MODULE_KEY_UNPRESS_SET          , dest);
    MODULE_TRANSLATE_KEY(BGMT_MENU                 , MODULE_KEY_MENU                 , dest);
    MODULE_TRANSLATE_KEY(BGMT_INFO                 , MODULE_KEY_INFO                 , dest);
    MODULE_TRANSLATE_KEY(BGMT_PLAY                 , MODULE_KEY_PLAY                 , dest);
    MODULE_TRANSLATE_KEY(BGMT_TRASH                , MODULE_KEY_TRASH                , dest);
    MODULE_TRANSLATE_KEY(BGMT_PRESS_DP             , MODULE_KEY_PRESS_DP             , dest);
    MODULE_TRANSLATE_KEY(BGMT_UNPRESS_DP           , MODULE_KEY_UNPRESS_DP           , dest);
    MODULE_TRANSLATE_KEY(BGMT_RATE                 , MODULE_KEY_RATE                 , dest);
    MODULE_TRANSLATE_KEY(BGMT_REC                  , MODULE_KEY_REC                  , dest);
    MODULE_TRANSLATE_KEY(BGMT_PRESS_ZOOMIN_MAYBE   , MODULE_KEY_PRESS_ZOOMIN         , dest);
    MODULE_TRANSLATE_KEY(BGMT_LV                   , MODULE_KEY_LV                   , dest);
    MODULE_TRANSLATE_KEY(BGMT_Q                    , MODULE_KEY_Q                    , dest);
    MODULE_TRANSLATE_KEY(BGMT_PICSTYLE             , MODULE_KEY_PICSTYLE             , dest);
    MODULE_TRANSLATE_KEY(BGMT_JOY_CENTER           , MODULE_KEY_JOY_CENTER           , dest);
    MODULE_TRANSLATE_KEY(BGMT_PRESS_UP             , MODULE_KEY_PRESS_UP             , dest);
    MODULE_TRANSLATE_KEY(BGMT_PRESS_UP_RIGHT       , MODULE_KEY_PRESS_UP_RIGHT       , dest);
    MODULE_TRANSLATE_KEY(BGMT_PRESS_UP_LEFT        , MODULE_KEY_PRESS_UP_LEFT        , dest);
    MODULE_TRANSLATE_KEY(BGMT_PRESS_RIGHT          , MODULE_KEY_PRESS_RIGHT          , dest);
    MODULE_TRANSLATE_KEY(BGMT_PRESS_LEFT           , MODULE_KEY_PRESS_LEFT           , dest);
    MODULE_TRANSLATE_KEY(BGMT_PRESS_DOWN_RIGHT     , MODULE_KEY_PRESS_DOWN_RIGHT     , dest);
    MODULE_TRANSLATE_KEY(BGMT_PRESS_DOWN_LEFT      , MODULE_KEY_PRESS_DOWN_LEFT      , dest);
    MODULE_TRANSLATE_KEY(BGMT_PRESS_DOWN           , MODULE_KEY_PRESS_DOWN           , dest);
    MODULE_TRANSLATE_KEY(BGMT_UNPRESS_UDLR         , MODULE_KEY_UNPRESS_UDLR         , dest);
    MODULE_TRANSLATE_KEY(BGMT_PRESS_HALFSHUTTER    , MODULE_KEY_PRESS_HALFSHUTTER    , dest);
    MODULE_TRANSLATE_KEY(BGMT_UNPRESS_HALFSHUTTER  , MODULE_KEY_UNPRESS_HALFSHUTTER  , dest);
    MODULE_TRANSLATE_KEY(BGMT_PRESS_FULLSHUTTER    , MODULE_KEY_PRESS_FULLSHUTTER    , dest);
    MODULE_TRANSLATE_KEY(BGMT_UNPRESS_FULLSHUTTER  , MODULE_KEY_UNPRESS_FULLSHUTTER  , dest);
    MODULE_TRANSLATE_KEY(BGMT_PRESS_FLASH_MOVIE    , MODULE_KEY_PRESS_FLASH_MOVIE    , dest);
    MODULE_TRANSLATE_KEY(BGMT_UNPRESS_FLASH_MOVIE  , MODULE_KEY_UNPRESS_FLASH_MOVIE  , dest);
    
    return 0;
}
#undef MODULE_TRANSLATE_KEY

int handle_module_keys(struct event * event)
{
    for(int mod = 0; mod < MODULE_COUNT_MAX; mod++)
    {
        module_cbr_t *cbr = module_list[mod].cbr;
        if(module_list[mod].valid && cbr)
        {
            while(cbr->name)
            {
                if(cbr->type == CBR_KEYPRESS)
                {
                    /* key got handled? */
                    if(!cbr->handler(module_translate_event(event, MODULE_KEY_PORTABLE)))
                    {
                        return 0;
                    }
                }
                if(cbr->type == CBR_KEYPRESS_RAW)
                {
                    /* key got handled? */
                    if(!cbr->handler((int)event))
                    {
                        return 0;
                    }
                }
                cbr++;
            }
        }
    }
    
    /* noone handled */
    return 1;
}

int module_display_filter_enabled()
{
#ifdef CONFIG_DISPLAY_FILTERS
    for(int mod = 0; mod < MODULE_COUNT_MAX; mod++)
    {
        module_cbr_t *cbr = module_list[mod].cbr;
        if(module_list[mod].valid && cbr)
        {
            while(cbr->name)
            {
                if(cbr->type == CBR_DISPLAY_FILTER)
                {
                    /* arg=0: should this display filter run? */
                    cbr->ctx = cbr->handler(0);
                    if (cbr->ctx)
                        return 1;
                }
                cbr++;
            }
        }
    }
#endif
    return 0;
}

int module_display_filter_update()
{
#ifdef CONFIG_DISPLAY_FILTERS
    for(int mod = 0; mod < MODULE_COUNT_MAX; mod++)
    {
        module_cbr_t *cbr = module_list[mod].cbr;
        if(module_list[mod].valid && cbr)
        {
            while(cbr->name)
            {
                if(cbr->type == CBR_DISPLAY_FILTER && cbr->ctx)
                {
                    /* arg!=0: draw the filtered image in these buffers */
                    struct display_filter_buffers buffers;
                    display_filter_get_buffers((uint32_t**)&(buffers.src_buf), (uint32_t**)&(buffers.dst_buf));
                    cbr->handler((intptr_t) &buffers);
                    break;
                }
                cbr++;
            }
        }
    }
#endif
    return 0;
}

static MENU_UPDATE_FUNC(module_menu_update_autoload)
{
    int mod_number = (int) entry->priv;

    MENU_SET_VALUE(module_list[mod_number].enabled?"ON":"OFF");
    if(module_list[mod_number].enabled)
    {
        if(!module_list[mod_number].error)
        {
            MENU_SET_ICON(MNI_ON, 0);
        }
        else
        {
            MENU_SET_ICON(MNI_OFF, 0);
        }
    }
    else
    {
        MENU_SET_ICON(MNI_NEUTRAL, 0);
    }
    
    MENU_SET_WARNING(MENU_WARN_ADVICE, module_list[mod_number].name);
}

static MENU_SELECT_FUNC(module_menu_update_select)
{
    char disable_file[MODULE_FILENAME_LENGTH];
    int mod_number = (int) priv;
    
    module_list[mod_number].enabled = !module_list[mod_number].enabled;
    snprintf(disable_file, sizeof(disable_file), MODULE_PATH"%s.dis", module_list[mod_number].name);
    config_flag_file_setting_save(disable_file, !module_list[mod_number].enabled);
}

static MENU_UPDATE_FUNC(module_menu_update_parameter)
{
    char *str = (char*)entry->priv;
    
    if(str)
    {
        MENU_SET_VALUE(str);
    }
    else
    {
        MENU_SET_VALUE("");
    }
}


static MENU_UPDATE_FUNC(module_menu_update_entry)
{
    int mod_number = (int) entry->priv;

    if(module_list[mod_number].valid)
    {
        if(module_list[mod_number].info && module_list[mod_number].info->long_name)
        {
            MENU_SET_NAME(module_list[mod_number].info->long_name);
        }
        else
        {
            MENU_SET_NAME(module_list[mod_number].name);
        }
        MENU_SET_ICON(MNI_ON, 0);
        MENU_SET_ENABLED(1);
        MENU_SET_VALUE(module_list[mod_number].status);
        MENU_SET_WARNING(MENU_WARN_ADVICE, module_list[mod_number].long_status);
    }
    else if(strlen(module_list[mod_number].filename))
    {
        MENU_SET_NAME(module_list[mod_number].filename);
        MENU_SET_ICON(MNI_OFF, 0);
        MENU_SET_ENABLED(1);
        MENU_SET_VALUE(module_list[mod_number].status);
        MENU_SET_WARNING(MENU_WARN_NOT_WORKING, module_list[mod_number].long_status);
    }
    else
    {
        MENU_SET_NAME("");
        MENU_SET_ICON(MNI_NONE, 0);
        MENU_SET_ENABLED(1);
        MENU_SET_VALUE("(nonexistent)");
        MENU_SET_HELP("You should never see this");
    }
}

static MENU_SELECT_FUNC(module_menu_select_empty)
{
}


/* check which modules are loaded and hide others */
static void module_menu_update()
{
    int mod_number = 0;
    struct menu_entry * entry = module_menu;

    while (entry)
    {
        /* only update those which display module information */
        if(entry->update == module_menu_update_entry)
        {
            if(module_list[mod_number].valid)
            {
                MENU_SET_SHIDDEN(0);
            }
            else if(strlen(module_list[mod_number].filename))
            {
                MENU_SET_SHIDDEN(0);
            }
            else
            {
                MENU_SET_SHIDDEN(1);
            }
            mod_number++;
        }
        entry = entry->next;
    }
}

/* check which modules are loaded and hide others */
static void module_submenu_update(int mod_number)
{
    /* displaying two menus before parameters */
    int entry = 1;

    /* set autoload menu's priv to module id */
    module_submenu[0].priv = (void*)mod_number;

    /* make sure this module is being used */
    if(module_list[mod_number].valid && !module_list[mod_number].error)
    {
        module_strpair_t *strings = module_list[mod_number].strings;
        module_parminfo_t *parms = module_list[mod_number].params;
        module_cbr_t *cbr = module_list[mod_number].cbr;
        module_prophandler_t **props = module_list[mod_number].prop_handlers;

        if (strings)
        {
            if(module_submenu[entry].priv != MENU_EOL_PRIV)
            {
                module_submenu[entry].name = "----Information---";
                module_submenu[entry].priv = (void*)0;
                module_submenu[entry].update = module_menu_update_parameter;
                module_submenu[entry].select = module_menu_select_empty;
                module_submenu[entry].shidden = 0;
                entry++;
            }
            
            while((strings->name != NULL) && (module_submenu[entry].priv != MENU_EOL_PRIV))
            {
                module_submenu[entry].name = strings->name;
                module_submenu[entry].priv = (void*)strings->value;
                module_submenu[entry].update = module_menu_update_parameter;
                module_submenu[entry].select = module_menu_select_empty;
                module_submenu[entry].shidden = 0;
                strings++;
                entry++;
            }
        }
        
        if (parms)
        {
            if(module_submenu[entry].priv != MENU_EOL_PRIV)
            {
                module_submenu[entry].name = "----Parameters----";
                module_submenu[entry].priv = (void*)0;
                module_submenu[entry].update = module_menu_update_parameter;
                module_submenu[entry].select = module_menu_select_empty;
                module_submenu[entry].shidden = 0;
                entry++;
            }

            while((parms->name != NULL) && (module_submenu[entry].priv != MENU_EOL_PRIV))
            {
                module_submenu[entry].name = parms->name;
                module_submenu[entry].priv = (void*)parms->type;
                module_submenu[entry].help = parms->desc;
                module_submenu[entry].update = module_menu_update_parameter;
                module_submenu[entry].select = module_menu_select_empty;
                module_submenu[entry].shidden = 0;
                parms++;
                entry++;
            }
        }
        
        if (props && *props)
        {
            if(module_submenu[entry].priv != MENU_EOL_PRIV)
            {
                module_submenu[entry].name = "----Properties----";
                #if !defined(CONFIG_UNREGISTER_PROP)
                module_submenu[entry].priv = " (no support)";
                #endif
                module_submenu[entry].update = module_menu_update_parameter;
                module_submenu[entry].select = module_menu_select_empty;
                module_submenu[entry].shidden = 0;
                entry++;
            }

            while((*props != NULL) && (module_submenu[entry].priv != MENU_EOL_PRIV))
            {
                module_submenu[entry].name = (*props)->name;
                module_submenu[entry].priv = (void*)0;
                module_submenu[entry].help = "";
                module_submenu[entry].update = module_menu_update_parameter;
                module_submenu[entry].select = module_menu_select_empty;
                module_submenu[entry].shidden = 0;
                props++;
                entry++;
            }
        }
        
        if (cbr)
        {
            if(module_submenu[entry].priv != MENU_EOL_PRIV)
            {
                module_submenu[entry].name = "----Callbacks-----";
                module_submenu[entry].priv = (void*)0;
                module_submenu[entry].update = module_menu_update_parameter;
                module_submenu[entry].select = module_menu_select_empty;
                module_submenu[entry].shidden = 0;
                entry++;
            }

            while((cbr->name != NULL) && (module_submenu[entry].priv != MENU_EOL_PRIV))
            {
                module_submenu[entry].name = cbr->name;
                module_submenu[entry].priv = (void*)cbr->symbol;
                module_submenu[entry].help = "";
                module_submenu[entry].update = module_menu_update_parameter;
                module_submenu[entry].select = module_menu_select_empty;
                module_submenu[entry].shidden = 0;
                cbr++;
                entry++;
            }
        }
    }

    /* disable other entries */
    while(module_submenu[entry].priv != MENU_EOL_PRIV)
    {
        module_submenu[entry].shidden = 1;
        entry++;
    }
}

static MENU_SELECT_FUNC(module_menu_load)
{
    console_show();
    module_load_all();
}

static MENU_SELECT_FUNC(module_menu_unload)
{
    module_unload_all();
}

static MENU_SELECT_FUNC(module_open_submenu)
{
    int mod_number = (int)priv;
    module_submenu_update(mod_number);
    menu_open_submenu();
}

static MENU_SELECT_FUNC(console_toggle)
{
    module_console_enabled = !module_console_enabled;
    if (module_console_enabled)
        console_show();
    else
        console_hide();
}

static struct menu_entry module_submenu[] = {
        {
            .name = "Autoload module",
            .icon_type = MNI_ON,
            .max = 1,
            .update = module_menu_update_autoload,
            .select = module_menu_update_select,
            .help = "Load automatically on startup.",
        },
        {
            .help = "",
        },
        {
            .help = "",
        },
        {
            .help = "",
        },
        {
            .help = "",
        },
        {
            .help = "",
        },
        {
            .help = "",
        },
        {
            .help = "",
        },
        {
            .help = "",
        },
        {
            .help = "",
        },
        {
            .help = "",
        },
        {
            .help = "",
        },
        {
            .help = "",
        },
        {
            .help = "",
        },
        {
            .help = "",
        },
        {
            .help = "",
        },
        {
            .help = "",
        },
        {
            .help = "",
        },
        {
            .help = "",
        },
        {
            .help = "",
        },
        {
            .help = "",
        },
        {
            .help = "",
        },
        {
            .help = "",
        },
        MENU_EOL
};

#define MODULE_ENTRY(i) \
        { \
            .name = "Module", \
            .priv = (void*)i, \
            .select = module_open_submenu, \
            .select_Q = module_open_submenu, \
            .update = module_menu_update_entry, \
            .icon_type = IT_SUBMENU, \
            .submenu_width = 700, \
            .children = module_submenu, \
            .help = "", \
            .help2 = "", \
        },

static struct menu_entry module_menu[] = {
    {
        .name = "Load modules now...",
        .select = module_menu_load,
        .help = "Loads modules in "MODULE_PATH,
    },
#ifdef CONFIG_MODULE_UNLOAD
    {
        .name = "Unload modules now...",
        .select = module_menu_unload,
        .help = "Unload loaded modules",
    },
#endif
    {
        .name = "Autoload modules on startup",
        .priv = &module_autoload_enabled,
        .max = 1,
        .help = "Loads modules every startup",
    },
    {
        .name = "Show console",
        .priv = &module_console_enabled,
        .select = console_toggle,
        .max = 1,
        .help = "Keep console shown after modules were loaded",
    },
    {
        .name = "----Modules----",
        .icon_type = MNI_NONE,
        .select = module_menu_select_empty,
    },
    MODULE_ENTRY(0)
    MODULE_ENTRY(1)
    MODULE_ENTRY(2)
    MODULE_ENTRY(3)
    MODULE_ENTRY(4)
    MODULE_ENTRY(5)
    MODULE_ENTRY(6)
    MODULE_ENTRY(7)
    MODULE_ENTRY(8)
    MODULE_ENTRY(9)
    MODULE_ENTRY(10)
    MODULE_ENTRY(11)
    MODULE_ENTRY(12)
    MODULE_ENTRY(13)
    MODULE_ENTRY(14)
    MODULE_ENTRY(15)
    MODULE_ENTRY(16)
    MODULE_ENTRY(17)
    MODULE_ENTRY(18)
    MODULE_ENTRY(19)
    MODULE_ENTRY(20)
    MODULE_ENTRY(21)
    MODULE_ENTRY(22)
    MODULE_ENTRY(23)
    MODULE_ENTRY(24)
    MODULE_ENTRY(25)
    MODULE_ENTRY(26)
    MODULE_ENTRY(27)
    MODULE_ENTRY(28)
    MODULE_ENTRY(29)
    MODULE_ENTRY(30)
    MODULE_ENTRY(31)
};

struct config_var* module_config_var_lookup(int* ptr)
{
    for(int mod = 0; mod < MODULE_COUNT_MAX; mod++)
    {
        module_config_t * config = module_list[mod].config;
        if(module_list[mod].valid && config)
        {
            for (module_config_t * mconfig = config; mconfig && mconfig->name; mconfig++)
            {
                if (mconfig->ref->value == ptr)
                    return (struct config_var *) mconfig->ref;
            }
        }
    }
    return 0;
}

static void module_init()
{
    module_mq = (struct msg_queue *) msg_queue_create("module_mq", 1);
    menu_add("Modules", module_menu, COUNT(module_menu));
    module_menu_update();
}

void module_load_task(void* unused) 
{
    char *lockstr = "If you can read this, ML crashed last time. To save from faulty modules, autoload gets disabled.";

    if(module_autoload_enabled)
    {
        uint32_t size;
        if( FIO_GetFileSize( module_lockfile, &size ) == 0 )
        {
            /* uh, it seems the camera didnt shut down cleanly, skip module loading this time */
            msleep(1000);
            NotifyBox(10000, "Camera was not shut down cleanly.\r\nSkipping module loading." );
        }
        else
        {
            FILE *handle = FIO_CreateFileEx(module_lockfile);
            FIO_WriteFile(handle, lockstr, strlen(lockstr));
            FIO_CloseFile(handle);
            
            /* now load modules */
            _module_load_all(0);
            module_menu_update();
        }
    }
    else
    {
        /* only list modules */
        _module_load_all(1);
        module_menu_update();
    }
        
    /* main loop, also wait until clean shutdown */
    TASK_LOOP
    {
        int msg;
        int err = msg_queue_receive(module_mq, (struct event**)&msg, 200);
        if (err) continue;
        
        switch(msg)
        {
            case MSG_MODULE_LOAD_ALL:
                _module_load_all(0);
                module_menu_update();
                break;

            case MSG_MODULE_UNLOAD_ALL:
                _module_unload_all();
                _module_load_all(1);
                module_menu_update();
                beep();
                break;
            
            default:
                console_printf("invalid msg: %d\n", msg);
        }
    }
}

static void module_save_configs()
{
    /* save configuration */
    console_printf("Save configs...\n");
    for(int mod = 0; mod < MODULE_COUNT_MAX; mod++)
    {
        if(module_list[mod].valid && module_list[mod].enabled && !module_list[mod].error)
        {
            /* save config */
            char filename[64];
            snprintf(filename, sizeof(filename), "%s%s.cfg", MODULE_PATH, module_list[mod].name);

            uint32_t ret = module_config_save(filename, &module_list[mod]);
            if(ret)
            {
                console_printf("  [E] Error: %d\n", ret);
            }
        }
    }
}

/* clean shutdown, unlink lockfile */
int module_shutdown()
{
    _module_unload_all();
    
    module_save_configs();
    
    if(module_autoload_enabled)
    {
        /* remove lockfile */
        FIO_RemoveFile(module_lockfile);
    }
    return 0;
}

TASK_CREATE("module_load_task", module_load_task, 0, 0x1e, 0x4000 );

INIT_FUNC(__FILE__, module_init);

