/*
 * Copyright (c) 2011 Evan Lojewski. All rights reserved.
 *
 */

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#define HEADER      __FILE__ "[" TOSTRING(__LINE__) "]: "


#define ERROR_INVALID_PARAMS    -1
#define ERROR_FOPEN             -2

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

enum NXByteOrder {
    NX_UnknownByteOrder,
    NX_LittleEndian,
    NX_BigEndian
};


#include <mach-o/fat.h>
#include <mach-o/arch.h>


#include "include/kernel_patcher.h"
#include "../../boot2/modules.h"
#include "../../libsaio/platform.h"

cpu_type_t archCpuType=CPU_TYPE_X86_64;

int show_help(int argc, const char * argv[]);
void KernelPatcher_start();

PlatformInfo_t    Platform;
extern section_t* kernelSections;
extern symbolList_t*   kernelSymbols;
extern symbolList_t*   moduleSymbols;

int main (int argc, const char * argv[])
{
	printf("******* XNU kernel patcher by Evan Lojewski                  *******\n");
	printf("******* version 0.2                                          *******\n");
	printf("******* WARNING: This may cause the kernel to be unbootable. *******\n");

	

    FILE*   kernel;
    char*   kernel_data;
    
    int     kernel_size;
        
    if(argc < 2) return show_help(argc, argv);
    printf(HEADER "Opening file %s\n", argv[1]);
    kernel = fopen(argv[1], "r");
    
    if(!kernel)
    {
        printf(HEADER "Unable to open file %s\n", argv[1]);
        return ERROR_FOPEN;
    }
    
    
    // Populate with platform information used in the modules
    Platform.CPU.Model = CPUID_MODEL_ATOM;

    
    // Determine size of the kernel
    
    fseek(kernel, 0, SEEK_END);
    kernel_size = ftell(kernel);
    fseek(kernel, 0, SEEK_SET);
    printf(HEADER "Reading in %d bytes...\n\n", kernel_size);
    
    
    // read in file
    kernel_data = malloc(kernel_size);

    if(fread(kernel_data, sizeof(char), kernel_size, kernel) == kernel_size)
    {   
		KernelPatcher_start();

		struct fat_header* hdr = (struct fat_header*)kernel_data;
		if(OSSwapBigToHostInt32(hdr->magic) == FAT_MAGIC)
		{
			int i;
			for(i = 0; i < OSSwapBigToHostInt32(hdr->nfat_arch); i++)
			{
				struct fat_arch* kernel_hdr = (struct fat_arch*)(((char*)kernel_data) + sizeof(struct fat_header) + sizeof(struct fat_arch) * i);

				const NXArchInfo* info =  NXGetArchInfoFromCpuType(OSSwapBigToHostInt32(kernel_hdr->cputype), OSSwapBigToHostInt32(kernel_hdr->cpusubtype));

				
				printf(HEADER "Found %s kernel, attempting to patch...\n", info->name);
				
				void* subkernel_data = (char*)kernel_data + OSSwapBigToHostInt32(kernel_hdr->offset);

				execute_hook("DecodeKernel", subkernel_data, NULL, NULL, NULL);
				printf("\n");

			}

		}
		else {
			
			execute_hook("DecodeKernel", kernel_data, NULL, NULL, NULL);

		}
		
		char* outputFile = malloc(strlen(argv[1]) + sizeof(".patched"));
		sprintf(outputFile, "%s.patched", argv[1]);
		FILE* outFile = fopen(outputFile, "w");
		
		fwrite(kernel_data, sizeof(char), kernel_size, outFile);
		fclose(outFile);
		
		
		printf(HEADER "Patched kernel written to: %s\n", outputFile);



        //register_hook_callback("DecodeKernel", &patch_kernel); 
        //patch_kernel(kernel_data, NULL, NULL, NULL);
        
        /*
        // Kernel has been read in, parse it.
        locate_symbols(kernel_data);
        kernelSymbols = moduleSymbols;    // save symbols for future use, if needed

    
        
        
        // Apply cpuid patch
        patch_cpuid_set_info_all(kernel_data);
        //patch_readStartupExtensions(kernel_data);
        
        // write new kernel to file
        
         */
        
    }
    else
    {
        printf(HEADER "Unable to read in %s.\n", argv[1]);
    }

    
    
    fclose(kernel);
    return 0;
}


int show_help(int argc, const char * argv[])
{
	printf(HEADER "Usage:\n");
    printf(HEADER "\t%s mach_kernel\n", argv[0]);
    return ERROR_INVALID_PARAMS;
}

/** functions depended on by chameleon / modules , but not available w/ the stdlib **/

void * safe_malloc(size_t size, const char *file, int line)
{
    return malloc(size);
}

int verbose(const char *format, ...)
{
    va_list ap;
	va_start(ap, format);
    vprintf(format, ap);
    va_end(ap);
    return 0;
}

void msglog(const char * format, ...)
{
    va_list ap;
	va_start(ap, format);
    vprintf(format, ap);
    va_end(ap);
}


int file_size(int fdesc)
{
    // only used by modules.c to read in a new module. Not used by the KernelPatcher module
    return 0;
}

void start_built_in_modules()
{
}
