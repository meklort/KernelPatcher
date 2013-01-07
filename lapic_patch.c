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


void patch_lapic_init(void* kernelData)
{
	section_t* txt = lookup_section("__TEXT","__text");
	if(!txt)
	{
		printf(HEADER "Unable to locate __TEXT,__text\n");
		return;
	}
	
	UInt8 panicIndex = 0;
	UInt8* bytes = (UInt8*)kernelData;
	
	symbolList_t *symbol = lookup_kernel_symbol("_lapic_init");
	UInt32 patchLocation = symbol ? symbol->addr - txt->address + txt->offset: 0; 
	if(symbol == 0 || symbol->addr == 0)
	{
		printf(HEADER "Unable to locate %s\n", "_lapic_init");
		return;
		
	}
	
	symbol = lookup_kernel_symbol("_panic");
	UInt32 panicAddr = symbol ? symbol->addr - txt->address: 0; 
	if(symbol == 0 || symbol->addr == 0)
	{
		printf(HEADER "Unable to locate %s\n", "_panic");
		return;
	}
	
	patchLocation -= (UInt32)kernelData;	// Remove offset
	panicAddr -= (UInt32)kernelData;	// Remove offset
    
	
	
	
	// Locate the (panicIndex + 1) panic call
	while(panicIndex < 3)	// Find the third panic call
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
		patchLocation++;
		panicIndex++;
	}
	patchLocation--;	// Remove extra increment from the < 3 while loop
	
	bytes[--patchLocation] = 0x90;	
	bytes[++patchLocation] = 0x90;
	bytes[++patchLocation] = 0x90;
	bytes[++patchLocation] = 0x90;
	bytes[++patchLocation] = 0x90;
	
	printf(HEADER "lapic_init panic removed.\n");
	
}

void patch_lapic_interrupt(void* kernelData)
{
	section_t* txt = lookup_section("__TEXT","__text");
	if(!txt)
	{
		printf(HEADER "Unable to locate __TEXT,__text\n");
		return;
	}
	
	// NOTE: this is a hack untill I finish patch_lapic_configure
	UInt8* bytes = (UInt8*)kernelData;
	
	symbolList_t *symbol = lookup_kernel_symbol("_lapic_interrupt");
	if(symbol == 0 || symbol->addr == 0)
	{
		printf(HEADER "Unable to locate %s\n", "_lapic_interrupt");
		return;
		
	}
	
	UInt32 patchLocation = symbol->addr - txt->address + txt->offset; 
	
	
	symbol = lookup_kernel_symbol("_panic");
	if(symbol == 0 || symbol->addr == 0)
	{
		printf(HEADER "Unable to locate %s\n", "_panic");
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
	
	printf(HEADER "lapic_interrupt panic removed\n");
	
	
}


void patch_lapic_configure(void* kernelData)
{
	section_t* txt = lookup_section("__TEXT","__text");
	if(!txt)
	{
		printf(HEADER "Unable to locate __TEXT,__text\n");
		return;
	}
	
	UInt8* bytes = (UInt8*)kernelData;
	
	UInt32 patchLocation;
	UInt32 lapicStart;
	UInt32 lapicInterruptBase;
	
	symbolList_t *symbol = lookup_kernel_symbol("_lapic_configure");
	if(symbol == 0 || symbol->addr == 0)
	{
		msglog(HEADER "Unable to locate %s\n", "_lapic_configure");
		return;
	}
	patchLocation = symbol->addr - txt->address + txt->offset; 
	
	symbol = lookup_kernel_symbol("_lapic_start");
	if(symbol == 0 || symbol->addr == 0)
	{
		msglog(HEADER "Unable to locate %s\n", "_lapic_start");
		return;
	}
	lapicStart = symbol->addr; 
    
    
	symbol = lookup_kernel_symbol("_lapic_interrupt_base");
	if(symbol == 0 || symbol->addr == 0)
	{
		msglog(HEADER "Unable to locate %s\n", "_lapic_interrupt_base");
		return;
	}
	lapicInterruptBase = symbol->addr;
	patchLocation -= (UInt32)kernelData;
	lapicStart -= (UInt32)kernelData;
	lapicInterruptBase -= (UInt32)kernelData;
	
	
	// Looking for the following:
	//movl   _lapic_start,%e_x
	//addl   $0x00000320,%e_x
	//  8b 15 __ __ __ __ 81 c2 20 03 00 00
	while(  
		  (bytes[patchLocation - 2] != 0x8b) ||
		  //bytes[patchLocation -1] != 0x15) ||	// Register, we don't care what it is
		  ( lapicStart  != (UInt32)(
									(bytes[patchLocation + 0] << 0  | 
									 bytes[patchLocation + 1] << 8  | 
									 bytes[patchLocation + 2] << 16 |
									 bytes[patchLocation + 3] << 24
                                     )
                                    )
		   ) || 
		  (bytes[patchLocation + 4 ] != 0x81) ||
		  //(bytes[patchLocation + 5 ] != 0Cx2) ||	// register
		  (bytes[patchLocation + 6 ] != 0x20) ||
		  (bytes[patchLocation + 7 ] != 0x03) ||
		  (bytes[patchLocation + 8 ] != 0x00) ||
		  (bytes[patchLocation + 9] != 0x00)
          
		  )
	{
		patchLocation++;
	}
	patchLocation-=2;
    
	// NOTE: this is currently hardcoded, change it to be more resilient to changes
	// At a minimum, I should have this do a cheksup first and if not matching, remove the panic instead.
	
	// 8b 15 __ __ __ __ ->  movl		  _lapic_start,%edx (NOTE: this should already be here)
	/*
     bytes[patchLocation++] = 0x8B;
     bytes[patchLocation++] = 0x15;
     bytes[patchLocation++] = (lapicStart & 0x000000FF) >> 0;
     bytes[patchLocation++] = (lapicStart & 0x0000FF00) >> 8;
     bytes[patchLocation++] = (lapicStart & 0x00FF0000) >> 16;
     bytes[patchLocation++] = (lapicStart & 0xFF000000) >> 24;
     */
	patchLocation += 6;
	
	// 81 c2 60 03 00 00 -> addl		  $0x00000320,%edx
	/*
     bytes[patchLocation++] = 0x81;
     bytes[patchLocation++] = 0xC2;
     */
	patchLocation += 2;
	bytes[patchLocation++] = 0x60;
	/*
     bytes[patchLocation++];// = 0x03;
     bytes[patchLocation++];// = 0x00;
     bytes[patchLocation++];// = 0x00;
     */
    patchLocation += 3;
    
	// c7 02 00 04 00 00 -> movl		  $0x00000400,(%edx)
	bytes[patchLocation++] = 0xC7;
	bytes[patchLocation++] = 0x02;
	bytes[patchLocation++] = 0x00;
	bytes[patchLocation++] = 0x04;
	bytes[patchLocation++] = 0x00;
	bytes[patchLocation++] = 0x00;
	
	// 83 ea 40 -> subl		  $0x40,edx
	bytes[patchLocation++] = 0x83;
	bytes[patchLocation++] = 0xEA;
	bytes[patchLocation++] = 0x40;
    
	// a1 __ __ __ __ -> movl		  _lapic_interrupt_base,%eax
	bytes[patchLocation++] = 0xA1;
	bytes[patchLocation++] = (lapicInterruptBase & 0x000000FF) >> 0;
	bytes[patchLocation++] = (lapicInterruptBase & 0x0000FF00) >> 8;
	bytes[patchLocation++] = (lapicInterruptBase & 0x00FF0000) >> 16;
	bytes[patchLocation++] = (lapicInterruptBase & 0xFF000000) >> 24;
    
	// 83 c0 0e -> addl		  $0x0e,%eax
	bytes[patchLocation++] = 0x83;
	bytes[patchLocation++] = 0xC0;
	bytes[patchLocation++] = 0x0E;
    
	// 89 02 -> movl		  %eax,(%edx)
	bytes[patchLocation++] = 0x89;
	bytes[patchLocation++] = 0x02;
	
	// 81c230030000		  addl		  $0x00000330,%edx
	bytes[patchLocation++] = 0x81;
	bytes[patchLocation++] = 0xC2;
	bytes[patchLocation++] = 0x30;
	bytes[patchLocation++] = 0x03;
	bytes[patchLocation++] = 0x00;
	bytes[patchLocation++] = 0x00;
	
	// a1 __ __ __ __ -> movl		  _lapic_interrupt_base,%eax
	bytes[patchLocation++] = 0xA1;
	bytes[patchLocation++] = (lapicInterruptBase & 0x000000FF) >> 0;
	bytes[patchLocation++] = (lapicInterruptBase & 0x0000FF00) >> 8;
	bytes[patchLocation++] = (lapicInterruptBase & 0x00FF0000) >> 16;
	bytes[patchLocation++] = (lapicInterruptBase & 0xFF000000) >> 24;
	
	// 83 c0 0f -> addl		  $0x0f,%eax
	bytes[patchLocation++] = 0x83;
	bytes[patchLocation++] = 0xC0;
	bytes[patchLocation++] = 0x0F;
	
	// 89 02 -> movl		  %eax,(%edx)
	bytes[patchLocation++] = 0x89;
	bytes[patchLocation++] = 0x02;
	
	// 83 ea 10 -> subl		  $0x10,edx
	bytes[patchLocation++] = 0x83;
	bytes[patchLocation++] = 0xEA;
	bytes[patchLocation++] = 0x10;
    
	// a1 __ __ __ __ -> movl		  _lapic_interrupt_base,%eax
	bytes[patchLocation++] = 0xA1;
	bytes[patchLocation++] = (lapicInterruptBase & 0x000000FF) >> 0;
	bytes[patchLocation++] = (lapicInterruptBase & 0x0000FF00) >> 8;
	bytes[patchLocation++] = (lapicInterruptBase & 0x00FF0000) >> 16;
	bytes[patchLocation++] = (lapicInterruptBase & 0xFF000000) >> 24;
	
	// 83 c0 0c -> addl		  $0x0c,%eax
	bytes[patchLocation++] = 0x83;
	bytes[patchLocation++] = 0xC0;
	bytes[patchLocation++] = 0x0C;
    
	// 89 02 -> movl		  %eax,(%edx)
	bytes[patchLocation++] = 0x89;
	bytes[patchLocation++] = 0x02;
    
	// Replace remaining with nops
    
    
	bytes[patchLocation++] = 0x90;
	bytes[patchLocation++] = 0x90;
	bytes[patchLocation++] = 0x90;
	bytes[patchLocation++] = 0x90;
	//	bytes[patchLocation++] = 0x90; // double check the lenght of the patch...
	//	bytes[patchLocation++] = 0x90;
	
	printf(HEADER "_lapic_configure patched to initalized APIC correctly\n");
}

