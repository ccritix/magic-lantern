/***
 Text Input functions
 
 @author Magic Lantern Team
 @copyright 2014
 @license GPL
 @module ime
 */

#include <dryos.h>
#include <string.h>
#include <bmp.h>

#include "../ime_base/ime_base.h"

#include "lua_common.h"

static int update_cbr_ref = LUA_NOREF;
static lua_State * ime_state = NULL;
static int ime_status;
static struct semaphore * ime_sem = NULL;

static IME_UPDATE_FUNC(luaCB_ime_update_cbr)
{
    lua_State * L = ime_state;
    if(L && update_cbr_ref != LUA_NOREF)
    {
        if(lua_rawgeti(L, LUA_REGISTRYINDEX, update_cbr_ref) == LUA_TFUNCTION)
        {
            lua_pushstring(L, text);
            lua_pushinteger(L, caret_pos);
            lua_pushinteger(L, selection_length);
            if(docall(L, 3, 1))
            {
                if(lua_toboolean(L, -1))
                {
                    return IME_OK;
                }
            }
        }
    }
    return IME_CANCEL;
}

static IME_DONE_FUNC(luaCB_ime_done_cbr)
{
    ime_status = status;
    if(ime_sem) give_semaphore(ime_sem);
    return IME_OK;
}

/***
 Starts IME and returns the result, blocks until IME is complete
 Do not call from menu task (start a new task)
 @tparam[opt] string caption a message to display to the user
 @tparam[opt] string default_value the default value
 @tparam[opt=255] int max_length maximum length of the input text
 @tparam[opt] int charset character set @{constants.CHARSET}
 @tparam[opt] int codepage codepage @{constants.CODEPAGE}
 @tparam[opt] function update function called periodically to validate input
 @tparam[opt] int x x position of the IME dialog
 @tparam[opt] int y y position of the IME dialog
 @tparam[opt] int w width of the IME dialog
 @tparam[opt] int h height of the IME dialog
 @treturn string the text input by the use or nil if the user canceled
 @function gets
 */
static int luaCB_ime_gets(lua_State* L)
{
    if(ime_state) return luaL_error(L, "IME is unavailable, another script is currently using IME");
    int error = 0;
    LUA_PARAM_STRING_OPTIONAL(caption, 1, "");
    LUA_PARAM_STRING_OPTIONAL(default_value, 2, "");
    LUA_PARAM_INT_OPTIONAL(max_length, 3, 255);
    LUA_PARAM_INT_OPTIONAL(charset, 4, (int32_t)IME_CHARSET_ANY);
    LUA_PARAM_INT_OPTIONAL(codepage, 5, IME_UTF8);
    LUA_PARAM_INT_OPTIONAL(x, 7, 0);
    LUA_PARAM_INT_OPTIONAL(y, 8, 0);
    LUA_PARAM_INT_OPTIONAL(w, 9, 0);
    LUA_PARAM_INT_OPTIONAL(h, 10, 0);
    
    ime_state = L;
    if (lua_gettop(L) <= 6 && lua_isfunction(L, 6))
    {
        lua_pushvalue(L, 6);
        update_cbr_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    else
    {
        update_cbr_ref = LUA_NOREF;
    }
    char* text = malloc(sizeof(char) * (max_length + 1));
    strncpy(text, default_value, max_length);
    text[max_length] = 0x0;
    void* ctx = ime_base_start((char*)caption, text, max_length, codepage, charset, update_cbr_ref != LUA_NOREF ? &luaCB_ime_update_cbr : NULL, &luaCB_ime_done_cbr, x, y, w, h);
    if(ctx)
    {
        ime_sem = create_named_semaphore("LUA_IME_SEM", 0);
        take_semaphore(ime_sem, 0);
        if(ime_status == IME_OK)
        {
            lua_pushfstring(L, "%s", text);
        }
        else if(ime_status == IME_CANCEL)
        {
            lua_pushnil(L);
        }
        else
        {
            error = 1;
            lua_pushfstring(L, "%s", text);
        }
    }
    else
    {
        error = 1;
        lua_pushfstring(L, "IME is unavailable, please load an IME module");
    }
    
    //cleanup
    if(update_cbr_ref != LUA_NOREF)
    {
        luaL_unref(L, LUA_REGISTRYINDEX, update_cbr_ref);
    }
    ime_state = NULL;
    ime_sem = NULL;
    update_cbr_ref = LUA_NOREF;
    free(text);
    if(error) return lua_error(L);
    return 1;
}

static int luaCB_ime_index(lua_State * L)
{
    lua_rawget(L, 1);
    return 1;
}

static int luaCB_ime_newindex(lua_State * L)
{
    lua_rawset(L, 1);
    return 0;
}

const luaL_Reg imelib[] =
{
    {"gets", luaCB_ime_gets},
    {NULL, NULL}
};

LUA_LIB(ime)