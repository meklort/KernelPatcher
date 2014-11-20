/*
 * Copyright (c) 2011 Evan Lojewski. All rights reserved.
 *
 *
 * This work is licensed under the
 *  Creative Commons Attribution-NonCommercial 3.0 Unported License.
 *  To view a copy of this license, visit http://creativecommons.org/licenses/by-nc/3.0/.
 */

#include "kernel_patcher.h"
#include "libsaio.h"
#include <libsaio/platform.h>
#include "modules.h"


#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#define HEADER      __FILE__ "[" TOSTRING(__LINE__) "]: "

/**
 ** Locate the last instance of _OSKextLog inside of __ZN12KLDBootstrap23readPrelinkedExtensionsEP7section
 ** and replace it with __ZN12KLDBootstrap20readBooterExtensionsEv so that mkexts can be loaded with prelinked kernels.
 **/
void patch_readStartupExtensions(void* kernelData)
{
    UInt8* bytes = (UInt8*)kernelData;
	int arch     = determineKernelArchitecture(kernelData);
    bool is64bit = (arch == KERNEL_64);
 
    symbolList_t* readPrelinkedExtensions  = is64bit ?
												lookup_kernel_symbol("__ZN12KLDBootstrap23readPrelinkedExtensionsEP10section_64") :
												lookup_kernel_symbol("__ZN12KLDBootstrap23readPrelinkedExtensionsEP7section");     // 32bit
    if(!readPrelinkedExtensions) 
    {
		printf(HEADER "Unable to locate KLDBootstrap::readPrelinkedExtensions(void* section%s)\n", is64bit ? "_64": "");
		return;
    }
	
    symbolList_t* getsegbyname         = lookup_kernel_symbol("_getsegbyname");
    symbolList_t* readBooterExtensions = lookup_kernel_symbol("__ZN12KLDBootstrap20readBooterExtensionsEv");
    symbolList_t* OSKextLog            = lookup_kernel_symbol("_OSKextLog");
    
    section_t* __KLD = lookup_section("__KLD","__text");
    
    
	UInt32 readBooterExtensionsLocation     = readBooterExtensions    ? readBooterExtensions->addr    - __KLD->address + __KLD->offset: 0; 
	UInt32 readPrelinkedExtensionsLocation  = readPrelinkedExtensions ? readPrelinkedExtensions->addr - __KLD->address + __KLD->offset : 0; 
	UInt32 OSKextLogLocation                = OSKextLog               ? OSKextLog->addr               - __KLD->address + __KLD->offset : 0; 
	UInt32 getsegbynameLocation             = getsegbyname            ? getsegbyname->addr            - __KLD->address + __KLD->offset : 0;
    
    UInt32 patchLocation = readPrelinkedExtensionsLocation;

    patchLocation                -= (UInt32)kernelData;
    OSKextLogLocation            -= (UInt32)kernelData;
    readBooterExtensionsLocation -= (UInt32)kernelData;
	getsegbynameLocation         -= (UInt32)kernelData;

    // Step 1: Locate the first getsegbyname() call inside of __ZN12KLDBootstrap23readPrelinkedExtensionsEP10section
	while(  
		  (bytes[patchLocation -1] != 0xE8) || /* Call */
		  ( ( (UInt32)(getsegbynameLocation - patchLocation  - 4) ) != (UInt32)((bytes[patchLocation + 0] << 0  | 
                                                                                            bytes[patchLocation + 1] << 8  | 
                                                                                            bytes[patchLocation + 2] << 16 |
                                                                                            bytes[patchLocation + 3] << 24)))
		  )
	{
		patchLocation++;
	}
    patchLocation++;

    
    // Second getsegbyname() call...
    while(  
		  (bytes[patchLocation -1] != 0xE8) || /* Call */
		  ( ( (UInt32)(getsegbynameLocation - patchLocation  - 4) ) != (UInt32)((bytes[patchLocation + 0] << 0  | 
                                                                                 bytes[patchLocation + 1] << 8  | 
                                                                                 bytes[patchLocation + 2] << 16 |
                                                                                 bytes[patchLocation + 3] << 24)))
		  )
	{
		patchLocation++;
	}
    patchLocation++;

    // First OSKextLog() call after two getsegbyname calls
    while(
		  (bytes[patchLocation -1] != 0xE8) || /* Call */
		  ( ( (UInt32)(OSKextLogLocation - patchLocation  - 4) ) != (UInt32)((bytes[patchLocation + 0] << 0  | 
                                                                                 bytes[patchLocation + 1] << 8  | 
                                                                                 bytes[patchLocation + 2] << 16 |
                                                                                 bytes[patchLocation + 3] << 24)))
		  )
	{
		patchLocation--;
	}
    
   //printf("patchLocation at 0x%X\n", patchLocation - __KLD->offset + __KLD->address); 

    
    // Step 2: remove the _OSKextLog call, this call takes the form (32bit):
    // 00886a73	movl	$0x00887508,0x08(%esp)
    // 00886a7b	movl	$0x00010084,0x04(%esp)
    // 00886a83	movl	$0x00000000,(%esp)
    // 00886a8a	calll	0x006060c0
    // 00886a8f
    // This is a total of 28 bytes 
    int i = 0;
    
    if(is64bit)
    {
        // TODO: Calculate size programaticaly
        patchLocation -= 0x14; // 64bit
        for(i = 0; i < 0x10; i++) bytes[++patchLocation] = 0x90;
        
    }
    else
    {
        patchLocation -= 0x19; // 32bit
        for(i = 0; i < 0x15; i++) bytes[++patchLocation] = 0x90;
    
    }

    // Step 3: Add a call to __ZN12KLDBootstrap21readStartupExtensionsEv.
    // This takes the form of
    // 00886ea6	movl	%esi,(%esp)
    // 00886ea9	calll	0x00886716
    // 00886eae
	
	if(!is64bit)
	{
		// 32bit
		bytes[patchLocation] = 0x89;
		bytes[++patchLocation] = 0x34;
		bytes[++patchLocation] = 0x24;
	}
	else
	{
		// 64bit
		// movq	%rbx,%rdi
		bytes[patchLocation] = 0x48;
		bytes[++patchLocation] = 0x89;
		bytes[++patchLocation] = 0xdf;		
	}
	++patchLocation;	// 0xe8 -> call
    UInt32 rel = patchLocation+5; /* Update relative address */
    bytes[++patchLocation] = (readBooterExtensionsLocation - rel) >> 0;
    bytes[++patchLocation] = (readBooterExtensionsLocation - rel) >> 8;
    bytes[++patchLocation] = (readBooterExtensionsLocation - rel) >> 16;
    bytes[++patchLocation] = (readBooterExtensionsLocation - rel) >> 24;

	printf(HEADER "KLDBootstrap::readBooterExtensions() call injected into KLDBootstrap::readPrelinkedExtensions(void* section%s)\n", is64bit ? "_64" : "");

}
