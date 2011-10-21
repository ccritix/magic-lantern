#include <dryos.h>
#include <property.h>

// look on camera menu or review sites to get custom function numbers

int get_htp() { return GetCFnData(1, 3); }
void set_htp(int value) { SetCFnData(1, 3, value); }

int get_mlu() { return GetCFnData(2, 5); }
void set_mlu(int value) { SetCFnData(2, 5, value); }

int cfn_get_af_button_assignment() { return GetCFnData(3, 1); }
void cfn_set_af_button(int value) { SetCFnData(3, 1, value); }

int get_cfn_function_for_set_button() { return GetCFnData(3, 2); }

// on some cameras, ALO is CFn
PROP_INT(PROP_ALO, alo);
int get_alo() { return alo; }

void set_alo(int value)
{
	value = COERCE(value, 0, 3);
	prop_request_change(PROP_ALO, &value, 4);
}
