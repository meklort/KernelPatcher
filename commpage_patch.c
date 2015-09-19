/*
 * Copyright (c) 2009-2011 Evan Lojewski. All rights reserved.
 *
 *
 * This work is licensed under the
 *  Creative Commons Attribution-NonCommercial 3.0 Unported License.
 *  To view a copy of this license, visit http://creativecommons.org/licenses/by-nc/3.0/.
 */

#include "kernel_patcher.h"
#include "modules.h"
#include "libsaio.h"

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#define HEADER      __FILE__ "[" TOSTRING(__LINE__) "]: "


void patch_commpage_stuff_routine(void* kernelData)
{
	section_t* txt = lookup_section("__TEXT","__text");
	if(!txt)
	{
		printf(HEADER "Unable to locate __TEXT,__text\n");
		return;
	}
	
	UInt8* bytes = (UInt8*)kernelData;
    
	symbolList_t *symbol = lookup_kernel_symbol("_commpage_stuff_routine");
    int numPatches = 1;
	if(symbol == 0 || symbol->addr == 0)
	{
        symbol = lookup_kernel_symbol("_commpage_set_nanotime");
        numPatches = 5;
        if(symbol == 0 || symbol->addr == 0)
        {
            printf(HEADER "Unable to locate %s\n", "_commpage_stuff_routine");

            return;
		}
        else
        {
        }
	}
	UInt32 patchLocation = symbol->addr - txt->address + txt->offset;
    symbol = lookup_kernel_symbol("_panic");

    if(symbol == 0 || symbol->addr == 0)
	{
		printf(HEADER "Unable to locate %s\n", "_panic");
		return;
	}
	UInt32 panicAddr = symbol->addr - txt->address;
    
    while(numPatches)
    {

	
    
	
	while(  
		  (bytes[patchLocation -1] != 0xE8) ||
		  ( ( (UInt32)(panicAddr - patchLocation  - 4) + txt->offset ) != (UInt32)((bytes[patchLocation + 0] << 0  | 
																					bytes[patchLocation + 1] << 8  | 
																					bytes[patchLocation + 2] << 16 |
																					bytes[patchLocation + 3] << 24)))
		  )
	{
		patchLocation++;
	}
	patchLocation--;
	
	// Replace panic with nops, same in both x86 and x86_64
	bytes[patchLocation + 0] = 0x90;
	bytes[patchLocation + 1] = 0x90;
	bytes[patchLocation + 2] = 0x90;
	bytes[patchLocation + 3] = 0x90;
	bytes[patchLocation + 4] = 0x90;
        patchLocation +=5;
    
    //printf("%s: _commpage_stuff_routine patched.\n", __FILE__);
        numPatches--;
    }
	
	printf(HEADER "_commpage_stuff_routine patched.\n", __FILE__);
}
