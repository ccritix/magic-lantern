/** \file
 * Key/value parser until we have a proper config
 */
#ifndef _config_h_
#define _config_h_

#define MAX_NAME_LEN		32
#define MAX_VALUE_LEN		32


struct config
{
	struct config *		next;
	char			name[ MAX_NAME_LEN ];
	char			value[ MAX_VALUE_LEN ];
};

extern struct config * global_config;

extern struct config *
config_parse(
	FILE *			file
);


extern char *
config_value(
	struct config *		config,
	const char *		name
);

extern int
config_int(
	struct config *		config,
	const char *		name,
	int			def
);


extern struct config *
config_parse_file(
	const char *		filename
);


#endif
