/***
 Touchscreen
 
 @author Magic Lantern Team
 @copyright 2016
 @license GPL
 @module touch
 */

#include <dryos.h>
#include <string.h>
#include "lua_common.h"

extern int is_camera(const char * model, const char * firmware);

typedef int (*touch_cbr)(int,int,int,int);

static touch_cbr * hijack_touch_cbr_ptr = NULL;
static touch_cbr canon_touch_cbr_ptr = NULL;

struct msg_queue * lua_touch_queue = NULL;

#define TOUCH_UP   0
#define TOUCH_MOVE 1
#define TOUCH_DOWN 2

struct lua_touch_handler
{
    struct lua_touch_handler * next;
    lua_State * L;
    int touch_handler_ref;
    int type;
};

typedef union
{
    struct
    {
        unsigned int x                : 12;
        unsigned int y                : 12;
        unsigned int num_touch_points : 4;
        unsigned int touch_id         : 4;
    } fields;
    uint32_t packed;
} lua_touch_event_t;

static int lua_touch_task_running = 0;
static struct lua_touch_handler * touch_handlers = NULL;

static void lua_touch_task(int unused)
{
    lua_touch_queue = msg_queue_create("lua_touch_queue", 100);
    uint8_t previous_state[16] = {0};
    TASK_LOOP
    {
        lua_touch_event_t event;
        int err = msg_queue_receive(lua_touch_queue, &event.packed, 0);
        
        if(err) continue;
        
        int type = event.fields.num_touch_points == 0 ? TOUCH_UP : event.fields.num_touch_points == previous_state[event.fields.touch_id] ? TOUCH_MOVE : TOUCH_DOWN;
        previous_state[event.fields.touch_id] = event.fields.num_touch_points;
        
        struct lua_touch_handler * current;
        for(current = touch_handlers; current; current = current->next)
        {
            if(current->type == type && current->touch_handler_ref != LUA_NOREF)
            {
                lua_State * L = current->L;
                struct semaphore * sem = NULL;
                if(!lua_take_semaphore(L, 100, &sem) && sem)
                {
                    if(lua_rawgeti(L, LUA_REGISTRYINDEX, current->touch_handler_ref) == LUA_TFUNCTION)
                    {
                        lua_pushinteger(L, event.fields.x);
                        lua_pushinteger(L, event.fields.y);
                        lua_pushinteger(L, event.fields.touch_id);
                        if(docall(L, 3, 0))
                        {
                            err_printf("script touch handler failed:\n %s\n", lua_tostring(L, -1));
                        }
                    }
                    give_semaphore(sem);
                }
                else
                {
                    printf("lua semaphore timeout: touch %d %d (%dms)\n", event.fields.touch_id, event.fields.num_touch_points, 0);
                }
            }
        }
    }
    lua_touch_task_running = 0;
}

static int touch_event_cbr(int x, int y, int num_touch_points, int touch_id)
{
    if(lua_touch_queue && lua_touch_task_running)
    {
        lua_touch_event_t event;
        event.fields.x = x;
        event.fields.y = y;
        event.fields.num_touch_points = num_touch_points;
        event.fields.touch_id = touch_id;
        msg_queue_post(lua_touch_queue, event.packed);
    }
    return canon_touch_cbr_ptr ? canon_touch_cbr_ptr(x,y,num_touch_points,touch_id) : 0;
}

static void set_touch_handler(lua_State * L, int type, int function_ref)
{
    if(!lua_touch_task_running)
    {
        lua_touch_task_running = 1;
        task_create("lua_touch_task", 0x1c, 0x8000, lua_touch_task, 0);
    }
    struct lua_touch_handler * current;
    for(current = touch_handlers; current; current = current->next)
    {
        if(current->L == L && current->type == type)
        {
            if(current->touch_handler_ref != LUA_NOREF)
            {
                luaL_unref(L, LUA_REGISTRYINDEX, current->touch_handler_ref);
            }
            current->touch_handler_ref = function_ref;
            return;
        }
    }
    if(function_ref != LUA_NOREF)
    {
        struct lua_touch_handler * new_entry = malloc(sizeof(struct lua_touch_handler));
        if(new_entry)
        {
            new_entry->next = touch_handlers;
            new_entry->L = L;
            new_entry->type = type;
            new_entry->touch_handler_ref = function_ref;
            touch_handlers = new_entry;
        }
        else
        {
            luaL_error(L, "malloc error creating touch handler");
        }
    }
}

static void lua_touch_init()
{
    static int is_initialized = 0;
    if (is_initialized) return;
    is_initialized = 1;
    if (is_camera("EOSM", "2.0.2"))
    {
        hijack_touch_cbr_ptr = (touch_cbr*)0x4D3F8;
    }
    else if (is_camera("700D", "1.1.4"))
    {
        hijack_touch_cbr_ptr = (touch_cbr*)0x4D858;
    }
    
    if (hijack_touch_cbr_ptr)
    {
        canon_touch_cbr_ptr = *hijack_touch_cbr_ptr;
        *hijack_touch_cbr_ptr = &touch_event_cbr;
    }
}

static int luaCB_touch_index(lua_State * L)
{
    LUA_PARAM_STRING_OPTIONAL(key, 2, "");
    /// Get whether or not touchscreen input is available
    // @tfield int is_available
    if(!strcmp(key, "is_available"))
    {
        lua_touch_init();
        lua_pushboolean(L, hijack_touch_cbr_ptr != NULL);
    }
    else
    {
        lua_rawget(L, 1);
    }
    return 1;
}

static int luaCB_touch_newindex(lua_State * L)
{
    lua_touch_init();
    if (hijack_touch_cbr_ptr == NULL) return luaL_error(L, "touchscreen is unavailable on this camera");
    LUA_PARAM_STRING_OPTIONAL(key, 2, "");
    /// Touch event handler. Called when the touch is released
    // @tparam int x
    // @tparam int y
    // @tparam int id multi-touch id (i.e. which finger)
    // @function up
    if(!strcmp(key, "up"))
    {
        lua_pushvalue(L, 3);
        set_touch_handler(L, TOUCH_UP, lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : LUA_NOREF);
    }
    /// Touch event handler. Called when the finger moves across the screen
    // @tparam int x
    // @tparam int y
    // @tparam int id multi-touch id (i.e. which finger)
    // @function move
    else if(!strcmp(key, "move"))
    {
        lua_pushvalue(L, 3);
        set_touch_handler(L, TOUCH_MOVE, lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : LUA_NOREF);
    }
    /// Touch event handler. Called when the screen is first touched
    // @tparam int x
    // @tparam int y
    // @tparam int id multi-touch id (i.e. which finger)
    // @function down
    else if(!strcmp(key, "down"))
    {
        lua_pushvalue(L, 3);
        set_touch_handler(L, TOUCH_DOWN, lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : LUA_NOREF);
    }
    else if(!strcmp(key, "is_available"))
    {
        return luaL_error(L, "'%s' is readonly", key);
    }
    else
    {
        lua_rawset(L, 1);
    }
    return 0;
}

const luaL_Reg touchlib[] =
{
    {NULL, NULL}
};

LUA_LIB(touch)