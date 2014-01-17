/*
 * Copyright (c) 2009-2011 Evan Lojewski. All rights reserved.
 *
 */

#include "kernel_patcher.h"
#include "libsaio.h"
#include "platform.h"
#include "modules.h"


UInt32 get_cpuid_cpu_info_addr(void* kernelData);
UInt32 get_cpuid_family_addr(void* kernelData);
UInt32 get_cpuid_family_addr_64(void* kernelData);

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#define HEADER      __FILE__ "[" TOSTRING(__LINE__) "]: "

/**
 ** Locate the fisrt instance of _panic inside of _cpuid_set_info, and either remove it
 ** Or replace it so that the cpuid is set to a valid value.
 **/
void patch_cpuid_set_info_all(void* kernelData)
{
	switch(Platform.CPU.Model)
	{
		case CPUID_MODEL_ATOM:
			if(determineKernelArchitecture(kernelData) == KERNEL_32)
			{
                msglog(HEADER "Attempting to patch 32bit kernel for Atom cpu\n");
				patch_cpuid_set_info_32(kernelData, CPUFAMILY_INTEL_PENRYN, CPUID_MODEL_PENRYN); 
			}
			else 
			{
                msglog(HEADER "Attempting to patch 64bit kernel for Atom cpu\n");
				patch_cpuid_set_info_64(kernelData, CPUFAMILY_INTEL_PENRYN, CPUID_MODEL_PENRYN); 
                
			}
            
			break;
			
		default:
			if(determineKernelArchitecture(kernelData) == KERNEL_32)
			{
                msglog(HEADER "Attempting to patch 32bit kernel for unknown cpu\n");
				patch_cpuid_set_info_32(kernelData, 0, 0);
			}
			else
			{
                msglog(HEADER "Attempting to patch 64bit kernel for unknown cpu\n");
				patch_cpuid_set_info_64(kernelData, 0, 0);
			}
            
			break;
	}
}

void patch_cpuid_set_info_64(void* kernelData, UInt32 impersonateFamily, UInt8 impersonateModel)
{
	section_t* txt = lookup_section("__TEXT","__text");
	if(!txt)
	{
		printf(HEADER "Unable to locate __TEXT,__text\n");
		return;
	}
	
    int i = 0;
	UInt8* bytes = (UInt8*)kernelData;
	
	symbolList_t *symbol = lookup_kernel_symbol("_cpuid_set_info");
	
	UInt32 patchLocation = symbol ? symbol->addr - txt->address + txt->offset: 0; //	(kernelSymbolAddresses[SYMBOL_CPUID_SET_INFO] - txt->address + txt->offset);
	patchLocation -= (UInt32)kernelData;	// Remove offset
	
	/** Remove the PANIC for unknown cpu **/
    
    symbol = lookup_kernel_symbol("_panic");
    UInt32 panicAddr = symbol ? symbol->addr - txt->address: 0; //kernelSymbolAddresses[SYMBOL_PANIC] - txt->address;
    if(symbol == 0 || symbol->addr == 0)
    {
        msglog(HEADER "Unable to locate _panic\n");
        return;
    }
    panicAddr -= (UInt32)kernelData;
    
    
    
    //TODO: don't assume it'll always work (Look for *next* function address in symtab and fail once it's been reached)
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
    
    patchLocation -= 0x0d; // remove jump over panic call
    for(i = 0; i < 0x0d; i++, patchLocation++) bytes[patchLocation] = 0x90;
    
    //printf("AAA");
    
    // NOP the call to _panic
    bytes[patchLocation + 0] = 0x90;
    bytes[patchLocation + 1] = 0x90;
    bytes[patchLocation + 2] = 0x90;
    bytes[patchLocation + 3] = 0x90;
    bytes[patchLocation + 4] = 0x90;

	msglog(HEADER "Panic call removed.\n", __FILE__);
    
    
    /** NOTE: Panic has now been replaced **/ 
    // replace cpuid
#if 0
    UInt32 actual_addr = get_cpuid_family_addr_64(kernelData) + 1;
    printf("cpuid familt add is 0x%0.4X\n", actual_addr);
    if(actual_addr != 1 && impersonateModel && impersonateFamily)
    {
        /*
        bytes[patchLocation - 11] = 0xC7;
        bytes[patchLocation - 10] = 0x05;
        
        // cpufamily
        bytes[patchLocation -  9] = (impersonateFamily & 0x000000FF) >> 0;
        bytes[patchLocation -  8] = (impersonateFamily & 0x0000FF00) >> 8;
        bytes[patchLocation -  7] = (impersonateFamily & 0x00FF0000) >> 16;	
        bytes[patchLocation -  6] = (impersonateFamily & 0xFF000000) >> 24;
        */

        
        bytes[patchLocation - 5] = 0xC7;
        bytes[patchLocation - 4] = 0x05;
        
        
        bytes[patchLocation - 3] = actual_addr;
        bytes[patchLocation - 2] = actual_addr >> 8;	
        bytes[patchLocation - 1] = actual_addr >> 16;
        bytes[patchLocation - 0] = actual_addr >> 24;
        
        // Note: I could have just copied the 8bit cpuid_model in and saved about 4 bytes
        // so if this function need a different patch it's still possible. Also, about ten bytes previous can be freed.
        bytes[patchLocation + 1] = impersonateModel;	// cpuid_model
        bytes[patchLocation + 2] = 0x01;	// cpuid_extmodel
        bytes[patchLocation + 3] = 0x00;	// cpuid_extfamily
        bytes[patchLocation + 4] = 0x02;	// cpuid_stepping
         
    }

#endif
    
    
    
    
    
}





void patch_cpuid_set_info_32(void* kernelData, UInt32 impersonateFamily, UInt8 impersonateModel)
{    
	section_t* txt = lookup_section("__TEXT","__text");
	if(!txt)
	{
		printf(HEADER "Unable to locate __TEXT,__text\n");
		return;
	}
	
	UInt8* bytes = (UInt8*)kernelData;
	
	symbolList_t *symbol = lookup_kernel_symbol("_cpuid_set_info");
    
	UInt32 patchLocation = symbol ? symbol->addr - txt->address + txt->offset: 0; //	(kernelSymbolAddresses[SYMBOL_CPUID_SET_INFO] - txt->address + txt->offset);
	patchLocation -= (UInt32)kernelData;	// Remove offset
	
    
	UInt32 jumpLocation = 0;
	
	
	if(symbol == 0 || symbol->addr == 0)
	{
		msglog(HEADER "Unable to locate _cpuid_set_info\n");
		return;
		
	}
    
    msglog(HEADER "Attempting to patch kernel\n");
    
    
    symbol = lookup_kernel_symbol("_panic");
    UInt32 panicAddr = symbol ? symbol->addr - txt->address: 0; //kernelSymbolAddresses[SYMBOL_PANIC] - txt->address;
    if(symbol == 0 || symbol->addr == 0)
    {
        msglog(HEADER "Unable to locate _panic\n");
        return;
    }
    panicAddr -= (UInt32)kernelData;
    
    
    
    //TODO: don't assume it'll always work (Look for *next* function address in symtab and fail once it's been reached)
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
    
    // Remove panic call, just in case the following patch routines fail
    bytes[patchLocation + 0] = 0x90;
    bytes[patchLocation + 1] = 0x90;
    bytes[patchLocation + 2] = 0x90;
    bytes[patchLocation + 3] = 0x90;
    bytes[patchLocation + 4] = 0x90;
	printf(HEADER "Panic call removed.\n", __FILE__);

    
    // Locate the jump call, so that 10 bytes can be reclamed.
    // NOTE: This will *NOT* be located on pre 10.6.2 kernels
    jumpLocation = patchLocation - 15;
    while((bytes[jumpLocation - 1] != 0x77 ||
           bytes[jumpLocation] != (patchLocation - jumpLocation - 18)) &&
          (patchLocation - jumpLocation) < 0xF0)
    {
        jumpLocation--;
    }
    // If found... AND we want to impersonate a specific cpumodel / family...
    if(impersonateFamily && impersonateModel  &&
       ((patchLocation - jumpLocation) < 0xF0))
    {
        msglog(HEADER "Attempting 10.2.0+ kernel patch\n");
        
        
        //bytes[jumpLocation] -= 10;		// sizeof(movl	$0x6b5a4cd2,0x00872eb4) = 10bytes
        
        //int i = 0;
        //for(i = bytes[jumpLocation] + jumpLocation + 1; i < (patchLocation-17); i++) bytes[i] = 0x90;   // reclaim with nops
        
        /* 
         * Inpersonate the specified CPU FAMILY and CPU Model
         */
        //									cpuid_cpufamily_addr, impersonateFamily									cpuid_model_addr	  impersonateModel
        //char new_bytes[] = {0xC7, 0x05, 0x__, 0x__, 0x__, 0x__, 0x__, 0x__, 0x__, 0x__, 0x90, 0x90, 0xC7, 0x05, 0x__, 0x__, 0x__, 0x__, 0x__, 0x01, 0x00, 0x02};
        // bytes[patchLocation - 17] = 0xC7;	// already here... not needed to be done
        // bytes[patchLocation - 16] = 0x05;	// see above
        //UInt32 cpuid_cpufamily_addr =	bytes[patchLocation - 15] << 0  |
        //								bytes[patchLocation - 14] << 8  |
        //								bytes[patchLocation - 13] << 16 |
        //								bytes[patchLocation - 12] << 24;
        
        // NOTE: may change, determined based on cpuid_info struct: TODO: read from binary
        //UInt32 cpuid_model_addr = cpuid_cpufamily_addr - 295; 
        
//        bytes[patchLocation - 17] = 0xC7;
//        bytes[patchLocation - 16] = 0x05;
        
        // cpufamily
        bytes[patchLocation - 11] = (impersonateFamily & 0x000000FF) >> 0;
        bytes[patchLocation - 10] = (impersonateFamily & 0x0000FF00) >> 8;
        bytes[patchLocation -  9] = (impersonateFamily & 0x00FF0000) >> 16;	
        bytes[patchLocation -  8] = (impersonateFamily & 0xFF000000) >> 24;
        
        // NOPS, just in case if the jmp call wasn't patched, we'll jump to a
        // nop and continue with the rest of the patch
        // Yay two free bytes :), 10 more can be reclamed if needed, as well as a few
        // from the above code (only cpuid_model needs to be set.
        bytes[patchLocation - 7] = 0x90;
        bytes[patchLocation - 6] = 0x90;
        
        UInt32 actual_addr = get_cpuid_family_addr(kernelData) + 1;
        
        if(actual_addr != 1)
        {
            bytes[patchLocation - 5] = 0xC7;
            bytes[patchLocation - 4] = 0x05;
            
            
            bytes[patchLocation - 3] = actual_addr;
            bytes[patchLocation - 2] = actual_addr >> 8;	
            bytes[patchLocation - 1] = actual_addr >> 16;
            bytes[patchLocation - 0] = actual_addr >> 24;
            
            // Note: I could have just copied the 8bit cpuid_model in and saved about 4 bytes
            // so if this function need a different patch it's still possible. Also, about ten bytes previous can be freed.
            bytes[patchLocation + 1] = impersonateModel;	// cpuid_model
            bytes[patchLocation + 2] = 0x01;	// cpuid_extmodel
            bytes[patchLocation + 3] = 0x00;	// cpuid_extfamily
            bytes[patchLocation + 4] = 0x02;	// cpuid_stepping
			
			msglog(HEADER "CPU impersonation patch applied.\n");

        }
        else
        {
            msglog(HEADER "Unable to patch cpu model\n");
            // nop
            bytes[patchLocation - 5] = 0x90;
            bytes[patchLocation - 4] = 0x90;
            bytes[patchLocation - 3] = 0x90;
            bytes[patchLocation - 2] = 0x90;
            bytes[patchLocation - 1] = 0x90;
            
            
        }
    }
    else if(impersonateFamily && impersonateModel)
    {
        msglog(HEADER "Attempting 10.0.0 kernel patch\n");
        
        // pre 10.6.2 kernel
        // Locate the jump to directly *after* the panic call,
        jumpLocation = patchLocation - 4;
        while((bytes[jumpLocation - 1] != 0x77 ||
               bytes[jumpLocation] != (patchLocation - jumpLocation + 4)) &&
              (patchLocation - jumpLocation) < 0x20)
        {
            jumpLocation--;
        }
        // NOTE above isn't needed (I was going to use it, but I'm not, so instead,
        // I'll just leave it to verify the binary stucture.
        
        // NOTE: the cpumodel_familt data is not set in _cpuid_set_info
        // so we don't need to set it here, I'll get set later based on the model
        // we set now.
        
        if((patchLocation - jumpLocation) < 0x20)
        {
            UInt32 cpuid_model_addr =	(bytes[patchLocation - 14] << 0  |
                                         bytes[patchLocation - 13] << 8  |
                                         bytes[patchLocation - 12] << 16 |
                                         bytes[patchLocation - 11] << 24);
            // Remove jump
            bytes[patchLocation - 9] = 0x90;		///  Was a jump if supported cpu
            bytes[patchLocation - 8] = 0x90;		// jumped past the panic call, we want to override the panic
            
            bytes[patchLocation - 7] = 0x90;
            bytes[patchLocation - 6] = 0x90;
            
            bytes[patchLocation - 5] = 0xC7;
            bytes[patchLocation - 4] = 0x05;
            bytes[patchLocation - 3] = (cpuid_model_addr & 0x000000FF) >> 0;
            bytes[patchLocation - 2] = (cpuid_model_addr & 0x0000FF00) >> 8;	
            bytes[patchLocation - 1] = (cpuid_model_addr & 0x00FF0000) >> 16;
            bytes[patchLocation - 0] = (cpuid_model_addr & 0xFF000000) >> 24;
            
            // Note: I could have just copied the 8bit cpuid_model in and saved about 4 bytes
            // so if this function need a different patch it's still possible. Also, about ten bytes previous can be freed.
            bytes[patchLocation + 1] = impersonateModel;	// cpuid_model
            bytes[patchLocation + 2] = 0x01;	// cpuid_extmodel
            bytes[patchLocation + 3] = 0x00;	// cpuid_extfamily
            bytes[patchLocation + 4] = 0x02;	// cpuid_stepping
            
            
            
            //patchLocation = jumpLocation;
            // We now have 14 bytes available for a patch
			
			msglog(HEADER "CPU impersonation patch applied.\n");

        }
        else 
        {
            msglog(HEADER "10.0.0 kernel patch failed\n");
            // Patching failed, using NOP replacement done initialy
        }
    }
    else 
    {
        msglog(HEADER "Unknown kernel format, not addiotnal cpuid patches applied.\n");
        
        // Either We were unable to change the jump call due to the function's sctructure
        // changing, or the user did not request a patch. As such, resort to just 
        // removing the panic call (using NOP replacement above). Note that the
        // IntelCPUPM kext may still panic due to the cpu's Model ID not being patched
    }
}


UInt32 get_cpuid_cpu_info_addr(void* kernelData)
{
	section_t* txt = lookup_section("__TEXT","__text");
	if(!txt)
	{
		printf(HEADER "Unable to locate __TEXT,__text\n");
		return 0;
	}
	
    UInt32 cpuid_cpu_info = 0;
	symbolList_t *symbol = lookup_kernel_symbol("_cpuid_family");
    
    
	UInt32 functionStart = symbol ? symbol->addr - txt->address + txt->offset : 0;

    functionStart += 6; // movl	0x00872d24,%edx instruction, 0x00872d24 is a pointer
    if(*(UInt8*)functionStart == 0xa1) functionStart++; // move eax
    else functionStart += 2;                            // move reg
    
    UInt32 cpuid_cpu_infop = *((UInt32*)functionStart); // pointer to the cpuid_cpu_info struct, mem location will be zero initialy, we want the actual struct
//    printf("&cpuid_cpu_infop = 0x%X\n", cpuid_cpu_infop);
    
    // Look for a mov $cpuid_cpu_infop, cpuid_cpu_info instruction
    int i = 0;
    for(i = 0; i < 0xFF; i++)
    {
        ++functionStart;
        if(*(UInt32*)functionStart == cpuid_cpu_infop)
        {
            functionStart += 4;
            cpuid_cpu_info =  *(UInt32*)functionStart; 
            //printf("Located mem at 0x%X\n", cpuid_cpu_info);
            break;
        }
    }
    
    if(!cpuid_cpu_info)
    {
        printf("Unable to locate cpuid_cpu_info\n");   
        pause();
    }
    
    return cpuid_cpu_info;/* - txt->address + txt->offset;*/    // TODO: verify for kernel version 
}

UInt32 get_cpuid_family_addr(void* kernelData)
{
	section_t* txt = lookup_section("__TEXT","__text");
	if(!txt)
	{
		printf(HEADER "Unable to locate __TEXT,__text\n");
		return 0;
	}
	
    UInt8* bytes = kernelData;
    
    
    UInt32 cpuid_cpu_info = get_cpuid_cpu_info_addr(kernelData);
    
	symbolList_t *set_info = lookup_kernel_symbol("_cpuid_set_info");
	symbolList_t *symbol = lookup_kernel_symbol("_cpuid_family");
    
    UInt32 functionStart = symbol ? symbol->addr - txt->address + txt->offset: 0;
    
    UInt32 findAddr = set_info ? set_info->addr - txt->address + txt->offset - (UInt32)bytes: 0;

    UInt32 position = functionStart - (UInt32)bytes;
    
    // locate call
    while(  
          (bytes[position -1] != 0xE8) ||
          ( ( (UInt32)(findAddr - position  - 4)) != (UInt32)((bytes[position + 0] << 0  | 
                                                               bytes[position + 1] << 8  | 
                                                               bytes[position + 2] << 16 |
                                                               bytes[position + 3] << 24)))
          )
    {
        position++;
    }

    // Locate offset for cpuid_info->family
    position += 22;
    UInt8 cpufamily = bytes[position];
    //printf("Located offset at 0x%X\n", cpufamily);
    //printf("get_cpuid_family_addr: 0x%X\n", cpuid_cpu_info + txt->address -  txt->offset);
    return cpuid_cpu_info + cpufamily;    // TODO: verify for kernel version 
}


UInt32 get_cpuid_cpu_info_addr_64(void* kernelData)
{
	section_t* txt = lookup_section("__TEXT","__text");
	if(!txt)
	{
		printf(HEADER "Unable to locate __TEXT,__text\n");
		return 0;
	}
	
    UInt32 cpuid_cpu_info = 0;
	symbolList_t *symbol = lookup_kernel_symbol("_cpuid_family");
    
    
	UInt32 functionStart = symbol ? symbol->addr - txt->address + txt->offset : 0;
    //printf("Located functionStart at 0x%0.4X\n", functionStart);
    
    functionStart +=6; // movl	0x00872d24,%edx instruction, 0x00872d24 is a pointer
    if(*(UInt8*)functionStart == 0xa1) functionStart++; // move eax
    else functionStart += 2;                            // move reg
    
    UInt32 cpuid_cpu_infop = *((UInt32*)functionStart); // pointer to the cpuid_cpu_info struct, mem location will be zero initialy, we want the actual struct
    printf("&cpuid_cpu_infop = 0x%X\n", cpuid_cpu_infop);
    
    // Look for a mov $cpuid_cpu_infop, cpuid_cpu_info instruction
    int i = 0;
    for(i = 0; i < 0xFF; i++)
    {
        ++functionStart;
        if(*(UInt32*)functionStart == cpuid_cpu_infop)
        {
            functionStart += 4;
            cpuid_cpu_info =  *(UInt32*)functionStart; 
            //printf("Located mem at 0x%X\n", cpuid_cpu_info);
            break;
        }
    }
    
    if(!cpuid_cpu_info)
    {
        printf("Unable to locate cpuid_cpu_info\n");   
        pause();
    }
    
    return cpuid_cpu_info;/* - txt->address + txt->offset;*/    // TODO: verify for kernel version 
}


UInt32 get_cpuid_family_addr_64(void* kernelData)
{
	section_t* txt = lookup_section("__TEXT","__text");
	if(!txt)
	{
		printf(HEADER "Unable to locate __TEXT,__text\n");
		return 0;
	}
	
    UInt8* bytes = kernelData;
    
    
    UInt32 cpuid_cpu_info = get_cpuid_cpu_info_addr_64(kernelData);
    
	symbolList_t *set_info = lookup_kernel_symbol("_cpuid_set_info");
	symbolList_t *symbol = lookup_kernel_symbol("_cpuid_family");
    
    UInt32 functionStart = symbol ? symbol->addr - txt->address + txt->offset: 0;
    
    UInt32 findAddr = set_info ? set_info->addr - txt->address + txt->offset - (UInt32)bytes: 0;
    
    UInt32 position = functionStart - (UInt32)bytes;
    
    // locate call
    while(  
          (bytes[position -1] != 0xE8) ||
          ( ( (UInt32)(findAddr - position  - 4)) != (UInt32)((bytes[position + 0] << 0  | 
                                                               bytes[position + 1] << 8  | 
                                                               bytes[position + 2] << 16 |
                                                               bytes[position + 3] << 24)))
          )
    {
        position++;
    }
    
    // Locate offset for cpuid_info->family
    position += 16;
    UInt8 cpufamily = bytes[position];
    //printf("Located offset at 0x%X\n", cpufamily);
    printf("cpuid_cpu_info = 0x%x\n", cpuid_cpu_info);
    //printf("get_cpuid_family_addr: 0x%X\n", cpuid_cpu_info + txt->address -  txt->offset);
    return cpuid_cpu_info + cpufamily;    // TODO: verify for kernel version 
}