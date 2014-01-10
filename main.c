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

/*static*/ unsigned long Adler32( unsigned char * buffer, long length );


/*
 * lzss.c
 */
extern int decompress_lzss(u_int8_t *dst, u_int8_t *src, u_int32_t srclen);
extern u_int8_t *compress_lzss(u_int8_t *dst, u_int32_t dstlen, u_int8_t *src, u_int32_t srcLen);

struct compressed_kernel_header {
    u_int32_t signature;
    u_int32_t compress_type;
    u_int32_t adler32;
    u_int32_t uncompressed_size;
    u_int32_t compressed_size;
    u_int32_t reserved[11];
    char      platform_name[64];
    char      root_path[256];
    u_int8_t  data[0];
};
typedef struct compressed_kernel_header compressed_kernel_header;

/*static*/ unsigned long
Adler32( unsigned char * buffer, long length )
{
    long          cnt;
    unsigned long result, lowHalf, highHalf;
    
    lowHalf  = 1;
    highHalf = 0;
    
	for ( cnt = 0; cnt < length; cnt++ )
    {
        if ((cnt % 5000) == 0)
        {
            lowHalf  %= 65521L;
            highHalf %= 65521L;
        }
        
        lowHalf  += buffer[cnt];
        highHalf += lowHalf;
    }
    
	lowHalf  %= 65521L;
	highHalf %= 65521L;
    
	result = (highHalf << 16) | lowHalf;
    
	return result;
}

long
DecodeKernel(void *binary)
{
	compressed_kernel_header * kernel_header = (compressed_kernel_header *) binary;
	u_int32_t uncompressed_size, size;
	void *buffer;
    u_int8_t* dstEnd;
	
#if 1
	printf("kernel header:\n");
	printf("signature: 0x%x\n", kernel_header->signature);
	printf("compress_type: 0x%x\n", kernel_header->compress_type);
	printf("adler32: 0x%x\n", OSSwapBigToHostInt32(kernel_header->adler32));
	printf("uncompressed_size: 0x%x\n", OSSwapBigToHostInt32(kernel_header->uncompressed_size));
	printf("compressed_size: 0x%x\n", OSSwapBigToHostInt32(kernel_header->compressed_size));
#endif
	
	if (kernel_header->signature == OSSwapBigToHostConstInt32('comp'))
	{
        printf("Found compressed kernel, won't recompress for now..\n");
		if (kernel_header->compress_type != OSSwapBigToHostConstInt32('lzss'))
		{
			printf("kernel compression is bad\n");
			return -1;
		}
        
#if NOTDEF
		if (kernel_header->platform_name[0] && strcmp(gPlatformName, kernel_header->platform_name))
			return -1;
		if (kernel_header->root_path[0] && strcmp(gBootFile, kernel_header->root_path))
			return -1;
#endif
		uncompressed_size = OSSwapBigToHostInt32(kernel_header->uncompressed_size);
		binary = buffer = malloc(uncompressed_size);
		
		size = decompress_lzss((u_int8_t *) binary, &kernel_header->data[0],
							   OSSwapBigToHostInt32(kernel_header->compressed_size));
		if (uncompressed_size != size) {
			printf("size mismatch from lzss: %x\n", size);
			return -1;
		}
		
		if (OSSwapBigToHostInt32(kernel_header->adler32) !=
			Adler32(binary, uncompressed_size))
		{
			printf("adler mismatch\n");
			return -1;
		}
	}
	
    // Notify modules that the kernel has been decompressed, thinned and is about to be decoded
	execute_hook("DecodeKernel", (void*)binary, NULL, NULL, NULL);
    
    //extern u_int8_t *compress_lzss(u_int8_t *dst, u_int32_t dstlen, u_int8_t *src, u_int32_t srcLen);
    dstEnd =
    compress_lzss(&kernel_header->data[0],
                  OSSwapBigToHostInt32(kernel_header->compressed_size),
                  (u_int8_t*)binary,
                  OSSwapBigToHostInt32(kernel_header->uncompressed_size));
    if(!dstEnd)
    {
        printf("Error: Unable to recompress kernel.");
    }
    else
    {
        printf("Total size = %d bytes %x\n", (int)((void*)dstEnd - (void*)&kernel_header->data[0]), dstEnd - &kernel_header->data[0]);
        int compressed_size = dstEnd - &kernel_header->data[0];
        if(compressed_size > OSSwapBigToHostInt32(kernel_header->compressed_size))
        {
            printf("ERROR: Unable to recompress image to same size as before, need to remake binary.");
        }
        else
        {
            kernel_header->compressed_size = OSSwapHostToBigInt32(compressed_size);
            kernel_header->adler32 = OSSwapHostToBigInt32(Adler32(binary, uncompressed_size));
        }
    }
    //TODO: compress_lzss if it was compressed previously
    return 0;
}

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

				//execute_hook("DecodeKernel", subkernel_data, NULL, NULL, NULL);
                DecodeKernel(subkernel_data);
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
