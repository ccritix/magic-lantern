#ifndef _module_h_
#define _module_h_

#define MODULE_PATH                   CARD_DRIVE"ML/MODULES/"
#define MAGIC_SYMBOLS                 CARD_DRIVE"ML/MODULES/MAGIC.SYM"

#define MODULE_INFO_PREFIX            __module_info_
#define MODULE_STRINGS_PREFIX         __module_strings_
#define MODULE_PARAMS_PREFIX          __module_params_
#define MODULE_PROPHANDLERS_PREFIX    __module_prophandlers_
#define MODULE_PROPHANDLER_PREFIX     __module_prophandler_

#define MODULE_MAGIC                  0x5A
#define STR(x)                        STR_(x)
#define STR_(x)                       #x

#define MODULE_COUNT_MAX              15
#define MODULE_NAME_LENGTH            8
#define MODULE_FILENAME_LENGTH        64
#define MODULE_STATUS_LENGTH          64

/* update major if older modules will *not* be compatible */
#define MODULE_MAJOR 1
/* update minor if older modules will be compatible, but newer module will not run on older magic lantern versions */
#define MODULE_MINOR 0
/* update patch if nothing regarding to compatibility changes */
#define MODULE_PATCH 0

/* module description that every module should deliver - optional.
   if not supplied, only the symbols are visible to other plugins.
   this might be useful for libraries.
*/
typedef struct
{
    /* major determines the generic compatibilty, minor is backward compatible (e.g. new hooks) */
    const unsigned char api_magic;
    const unsigned char api_major;
    const unsigned char api_minor;
    const unsigned char api_patch;

    /* the plugin's name and init/deinit functions */
    const char *name;
    const char *long_name;
    unsigned int (*init) ();
    unsigned int (*deinit) ();

    /* some callbacks that may be needed by modules. more to come. ideas? needs? */
    void (*cb_shoot_task) (); /* called periodically from shoot task */
    void (*cb_pre_shoot) (); /* called before image is taken */
    void (*cb_post_shoot) (); /* called after image is taken */
} module_info_t;

/* modules can have parameters - optional */
typedef struct
{
    /* pointer to parameter in memory */
    const void *parameter;
    /* stringified type like "uint32_t", "int32_t". restrict to stdint.h types */
    const char *type;
    /* name of the parameter, must match to variable name */
    const char *name;
    /* description for the user */
    const char *desc;
} module_parminfo_t;

/* this struct supplies additional information like license, author etc - optional */
typedef struct
{
    const char *name;
    const char *value;
} module_strpair_t;

/* this struct supplies additional information like license, author etc - optional */
typedef struct
{
    const char *name;
    void (*handler)(unsigned int property, void * priv, void * addr, unsigned int len);
    const unsigned int property;
    const unsigned int property_length;
} module_prophandler_t;


/* index of all loaded modules */
typedef struct
{
    char name[MODULE_NAME_LENGTH+1];
    char filename[MODULE_FILENAME_LENGTH+1];
    char long_filename[MODULE_FILENAME_LENGTH+1];
    char status[MODULE_STATUS_LENGTH+1];
    char long_status[MODULE_STATUS_LENGTH+1];
    module_info_t *info;
    module_strpair_t *strings;
    module_parminfo_t *params;
    module_prophandler_t **prop_handlers;
    int valid;
} module_entry_t;


#define MODULE_INFO_START()                                     MODULE_INFO_START_(MODULE_INFO_PREFIX,MODULE_NAME)
#define MODULE_INFO_START_(prefix,modname)                      MODULE_INFO_START__(prefix,modname)
#define MODULE_INFO_START__(prefix,modname)                     module_info_t prefix##modname = \
                                                                {\
                                                                    .api_magic = MODULE_MAGIC, \
                                                                    .api_major = MODULE_MAJOR, \
                                                                    .api_minor = MODULE_MINOR, \
                                                                    .api_patch = MODULE_PATCH, \
                                                                    .name = #modname,
#define MODULE_INIT(func)                                           .init = &func,
#define MODULE_DEINIT(func)                                         .deinit = &func,
#define MODULE_LONGNAME(name)                                       .long_name = name,
#define MODULE_CB_SHOOT_TASK(func)                                  .cb_shoot_task = &func,
#define MODULE_CB_PRE_SHOOT(func)                                   .cb_pre_shoot = &func,
#define MODULE_CB_POST_SHOOT(func)                                  .cb_post_shoot = &func,
#define MODULE_INFO_END()                                       };
                                                                
#define MODULE_STRINGS_START()                                  MODULE_STRINGS_START_(MODULE_STRINGS_PREFIX,MODULE_NAME)
#define MODULE_STRINGS_START_(prefix,modname)                   MODULE_STRINGS_START__(prefix,modname)
#define MODULE_STRINGS_START__(prefix,modname)                  module_strpair_t prefix##modname[] = {
#define MODULE_STRING(field,value)                                  { field, value },
#define MODULE_STRINGS_END()                                        { (const char *)0, (const char *)0 }\
                                                                };
                                                                
#define MODULE_PARAMS_START()                                   MODULE_PARAMS_START_(MODULE_PARAMS_PREFIX,MODULE_NAME)
#define MODULE_PARAMS_START_(prefix,modname)                    MODULE_PARAMS_START__(prefix,modname)
#define MODULE_PARAMS_START__(prefix,modname)                   module_parminfo_t prefix##modname[] = {
#define MODULE_PARAM(var,typestr,descr)                             { .parameter = &var, .name = #var, .type = typestr, .desc = descr },
#define MODULE_PARAMS_END()                                         { (void *)0, (const char *)0, (const char *)0, (const char *)0 }\
                                                                };

#define MODULE_PROPHANDLERS_START()                             MODULE_PROPHANDLERS_START_(MODULE_PROPHANDLERS_PREFIX,MODULE_NAME,MODULE_PROPHANDLER_PREFIX)
#define MODULE_PROPHANDLERS_START_(prefix,modname,ph_prefix)    MODULE_PROPHANDLERS_START__(prefix,modname,ph_prefix)
#define MODULE_PROPHANDLERS_START__(prefix,modname,ph_prefix)   module_prophandler_t *prefix##modname[] = {
#define MODULE_PROPHANDLER(id)                                      MODULE_PROPHANDLER_(MODULE_PROPHANDLERS_PREFIX,MODULE_NAME,MODULE_PROPHANDLER_PREFIX,id)
#define MODULE_PROPHANDLER_(prefix,modname,ph_prefix,id)            MODULE_PROPHANDLER__(prefix,modname,ph_prefix,id)
#define MODULE_PROPHANDLER__(prefix,modname,ph_prefix,id)           &ph_prefix##modname##_##id##_block,
#define MODULE_PROPHANDLERS_END()                                   NULL\
                                                                };

#if defined(MODULE)
#define PROP_HANDLER(id)                                        MODULE_PROP_ENTRY_(MODULE_PROPHANDLER_PREFIX,MODULE_NAME, id)
#define MODULE_PROP_ENTRY_(prefix,modname,id)                   MODULE_PROP_ENTRY__(prefix,modname,id)
#define MODULE_PROP_ENTRY__(prefix,modname,id)                  void prefix##modname##_##id(unsigned int, void *, void *, unsigned int);\
                                                                module_prophandler_t prefix##modname##_##id##_block = { \
                                                                    .name            = #id, \
                                                                    .handler         = &prefix##modname##_##id, \
                                                                    .property        = id, \
                                                                    .property_length = 0, \
                                                                }; \
                                                                void prefix##modname##_##id( \
                                                                        unsigned int property, \
                                                                        void *       token, \
                                                                        void *       buf, \
                                                                        unsigned int len \
                                                                )
#endif


/* load all available modules. will be used on magic lantern boot */
void module_load_all(void);
void module_unload_all(void);

/* explicitely load a standalone module. this is comparable to an executable */
void *module_load(char *filename);
int module_exec(void *module, char *symbol, int count, ...);
int module_unload(void *module);



#endif
