/*
 * Copyright (c) 2009-2011 Evan Lojewski. All rights reserved.
 *
 */

#include "kernel_patcher.h"
#include "modules.h"
#include "libsaio.h"

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#define HEADER      __FILE__ "[" TOSTRING(__LINE__) "]: "

/**
 ** SleepEnabler.kext replacement (for those that need it)
 ** Located the KERN_INVALID_ARGUMENT return and replace it with KERN_SUCCESS
 **/
void patch_pmCPUExitHaltToOff(void* kernelData)
{
	section_t* txt = lookup_section("__TEXT","__text");
	if(!txt)
	{
		printf(HEADER "Unable to locate __TEXT,__text\n");
		return;
	}
	
	UInt8* bytes = (UInt8*)kernelData;
    
	symbolList_t *symbol = lookup_kernel_symbol("_PmCpuExitHaltToOff"); // 10.6ish
    if(!symbol)   symbol = lookup_kernel_symbol("_pmCPUExitHaltToOff"); // 10.8+
    
	UInt32 patchLocation = symbol ? symbol->addr - txt->address + txt->offset: 0;
	
	if(symbol == 0 || symbol->addr == 0)
	{
		printf("Unable to locate _pmCPUExitHaltToOff\n");
		return;
	}
	
	patchLocation -= (UInt32)kernelData;	// Remove offset
	
	
	
	while(bytes[patchLocation - 1]	!= 0xB8 ||
		  bytes[patchLocation]		!= 0x04 ||	// KERN_INVALID_ARGUMENT (0x00000004)
		  bytes[patchLocation + 1]	!= 0x00 ||	// KERN_INVALID_ARGUMENT
		  bytes[patchLocation + 2]	!= 0x00 ||	// KERN_INVALID_ARGUMENT
		  bytes[patchLocation + 3]	!= 0x00)	// KERN_INVALID_ARGUMENT
        
	{
		patchLocation++;
	}
	bytes[patchLocation] = 0x00;	// KERN_SUCCESS;
}

void patch_pmKextRegister(void* kernelData)
{
	section_t* txt = lookup_section("__TEXT","__text");
	if(!txt)
	{
		printf(HEADER "Unable to locate __TEXT,__text\n");
		return;
	}
	
	UInt8* bytes = (UInt8*)kernelData;
    
	symbolList_t *symbol = lookup_kernel_symbol("_pmKextRegister");
	if(symbol == 0 || symbol->addr == 0)
	{
		//printf("Unable to locate %s\n", "_commpage_stuff_routine");
		return;
		
	}
	
	UInt32 patchLocation = symbol->addr - txt->address + txt->offset; 
    
	
	symbol = lookup_kernel_symbol("_panic");
	if(symbol == 0 || symbol->addr == 0)
	{
		printf("Unable to locate %s\n", "_panic");
		return;
	}
	UInt32 panicAddr = symbol->addr - txt->address; 
    
	patchLocation -= (UInt32)kernelData;
	panicAddr -= (UInt32)kernelData;
	
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
	
	// Replace panic with nops
	bytes[patchLocation + 0] = 0x90;
	bytes[patchLocation + 1] = 0x90;
	bytes[patchLocation + 2] = 0x90;
	bytes[patchLocation + 3] = 0x90;
	bytes[patchLocation + 4] = 0x90;
	
	
}
