/** \file
 * Property handler installation
 *
 * These handlers are registered to allow Magic Lantern to interact with
 * the Canon "properties" that are used to exchange globals.
 *
 * I'm not positive what the token_handler function does.  It appears
 * that all of Canon's property handlers have an associated function
 * that just stores a value to pass to the cleanup function.  Rather
 * than require Magic Lantern callers to create functions for every
 * property, a generic one is created for them.
 */

#include "dryos.h"
#include "property.h"


// This must be three instructions long to match the sizeof(token_handler)
asm(
".globl prop_token_handler_generic\n"
"prop_token_handler_generic:\n"
"	mov r1, pc\n"
"	str r0, [r1, #-12]\n"
"	bx lr\n"
);

extern void prop_token_handler_generic(void *);

void
prop_handler_init(
	struct prop_handler * const handler
)
{
	// Copy the generic token handler into the structure
	memcpy(
		handler->token_handler,
		prop_token_handler_generic,
		sizeof(handler->token_handler)
	);

	prop_register_slave(
		&handler->property,
		1,
		handler->handler,
		&handler->token,
		(void*) handler->token_handler
	);
}


static void
prop_init( void * unused )
{
	extern struct prop_handler _prop_handlers_start[];
	extern struct prop_handler _prop_handlers_end[];
	struct prop_handler * handler = _prop_handlers_start;

	for( ; handler < _prop_handlers_end ; handler++ )
		prop_handler_init(handler);
}


INIT_FUNC( __FILE__, prop_init );
