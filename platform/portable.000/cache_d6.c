#define CONFIG_DIGIC_VI
#include "compiler.h"
#include "consts.h"
#include "fullfat.h"
#include "md5.h"
#include "asm.h"

void sync_caches_d6()
{
	sync_caches();
}

void disable_caches_region1_ram_d6()
{
	disable_caches_region1_ram();
}
