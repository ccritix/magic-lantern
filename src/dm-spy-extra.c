/**
 * Some extra logging functions via DebugMsg.
 * It traces user-defined function calls, such as TryPostEvent, state objects, EDMAC and whatever else you throw at it.
 * 
 * Includes a generic logging function => simply plug the addresses into logged_functions[] and should be ready to go.
 * 
 * TODO:
 * - move to module? (I also need it as core functionality, for researching the startup process)
 * - import the patched addresses from stubs
 */

#include "dryos.h"
#include "gdb.h"
#include "patch.h"
#include "state-object.h"

static void generic_log(breakpoint_t *bkpt);
static void state_transition_log(breakpoint_t *bkpt);

struct logged_func
{
    uint32_t addr;                              /* Logged address (usually at the start of the function; will be passed to gdb_add_watchpoint) */
    char* name;                                 /* Breakpoint (function) name (optional, can be NULL) */
    int num_args;                               /* How many arguments does your function have? (will try to print them) */
    void (*log_func)(breakpoint_t* bkpt);       /* if generic_log is not enough, you may use a custom logging function */
    breakpoint_t * bkpt;                        /* internal */
};

static struct logged_func logged_functions[] = {
    #ifdef CONFIG_5D2
    { 0xff9b9198, "StateTransition", 4 , state_transition_log },
    { 0xff9b989c, "TryPostEvent", 4 },
    { 0xff9b8f24, "TryPostStageEvent", 4 },

    { 0xFF9A462C, "ConnectReadEDmac", 2 },
    { 0xFF9A4604, "ConnectWriteEDmac", 2 },
    { 0xFF9A4798, "RegisterEDmacCompleteCBR", 3 },
    { 0xFF9A45E8, "SetEDmac", 4 },
    { 0xFF9A464C, "StartEDmac", 2 },
    #endif
};

/* returns true if the machine will not lock up when dereferencing ptr */
static int is_sane_ptr(uint32_t ptr)
{
    switch (ptr & 0xF0000000)
    {
        case 0x00000000:
        case 0x10000000:
        case 0x40000000:
        case 0x50000000:
        case 0xF0000000:
            return 1;
    }
    return 0;
}

static int looks_like_string(uint32_t addr)
{
    if (!is_sane_ptr(addr))
    {
        return 0;
    }
    
    int min_len = 4;
    int max_len = 100;
    
    for (uint32_t p = addr; p < addr + max_len; p++)
    {
        char c = *(char*)p;
        if (c == 0 && p > addr + min_len)
        {
            return 1;
        }
        if (c < 32 || c > 127)
        {
            return 0;
        }
    }
    return 0;
}

static void generic_log(breakpoint_t *bkpt)
{
    uint32_t pc = bkpt->ctx[15];
    int num_args = 0;
    char* func_name = 0;
    
    for (int i = 0; i < COUNT(logged_functions); i++)
    {
        if (logged_functions[i].addr == pc)
        {
            num_args = logged_functions[i].num_args;
            func_name = logged_functions[i].name;
            break;
        }
    }
    
    char msg[200];
    int len;

    if (func_name)
    {
        len = snprintf(msg, sizeof(msg), "*** %s(", func_name);
    }
    else
    {
        len = snprintf(msg, sizeof(msg), "*** FUNC(%x)(", pc);
    }

    /* only for first 4 args for now */
    for (int i = 0; i < num_args; i++)
    {
        uint32_t arg = bkpt->ctx[i];
        if (looks_like_string(arg))
        {
            len += snprintf(msg + len, sizeof(msg) - len, "\"%s\"", arg);
        }
        else if (is_sane_ptr(arg) && looks_like_string(MEM(arg)))
        {
            len += snprintf(msg + len, sizeof(msg) - len, "&\"%s\"", MEM(arg));
        }
        else
        {
            len += snprintf(msg + len, sizeof(msg) - len, "0x%x", arg);
        }
        
        if (i < num_args -1)
        {
            len += snprintf(msg + len, sizeof(msg) - len, ", ");
        }
    }
    len += snprintf(msg + len, sizeof(msg) - len, ")");
    
    DryosDebugMsg(0, 0, msg);
}

static void state_transition_log(breakpoint_t *bkpt)
{
    struct state_object * state = (void*) bkpt->ctx[0];
    int old_state = state->current_state;
    char* state_name = (char*) state->name;
    int input = bkpt->ctx[2];
    int next_state = state->state_matrix[old_state + state->max_states * input].next_state;

    DryosDebugMsg(0, 0, 
        "*** %s: (%d) --%d--> (%d)                              "
        "x=%x z=%x t=%x", state_name, old_state, input, next_state,
        bkpt->ctx[1], bkpt->ctx[3], bkpt->ctx[4]
    );
}

void dm_spy_extra_install()
{
    gdb_setup();

    for (int i = 0; i < COUNT(logged_functions); i++)
    {
        if (logged_functions[i].addr)
        {
            logged_functions[i].bkpt = gdb_add_watchpoint(
                logged_functions[i].addr, 0,
                logged_functions[i].log_func ? logged_functions[i].log_func : generic_log
            );
        }
    }
}

void dm_spy_extra_uninstall()
{
    for (int i = 0; i < COUNT(logged_functions); i++)
    {
        if (logged_functions[i].bkpt)
        {
            gdb_delete_bkpt(logged_functions[i].bkpt);
            logged_functions[i].bkpt = 0;
        }
    }
}
