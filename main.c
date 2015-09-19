/*
 * Copyright (c) 2014 xZenue LLC. All rights reserved.
 * Copyright (c) 2011-2013 Evan Lojewski. All rights reserved.
 *
 *
 * This work is licensed under the
 *  Creative Commons Attribution-NonCommercial 3.0 Unported License.
 *  To view a copy of this license, visit http://creativecommons.org/licenses/by-nc/3.0/.
 */

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#define HEADER      __FILE__ "[" TOSTRING(__LINE__) "]: "
#define __DARWIN_ONLY_UNIX_CONFORMANCE	1
#define ERROR_INVALID_PARAMS    -1
#define ERROR_FOPEN             -2

/* Missing definition, added for mach-o/arch.h */
enum NXByteOrder {
    NX_UnknownByteOrder,
    NX_LittleEndian,
    NX_BigEndian
};

#include <stdio.h>
//#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <mach-o/fat.h>
#include <mach-o/arch.h>

#define __BOOT_LIBSA_H /* Ensure libsa.h is not included by boot.h */
#include "include/kernel_patcher.h"
#include "../../boot2/modules.h"
#include "../../boot2/boot.h"

// Internal functions
int show_help(int argc, const char * argv[]);
long DecodeKernel_patcher(void *binary);
static unsigned long Adler32( unsigned char * buffer, long length );

// Variables reveferenced by chameleon objects or by the patcher
boot_args *bootArgs;
cpu_type_t archCpuType=CPU_TYPE_X86_64;
struct multiboot_info *gMI = NULL;

char __data_start;

// external functions not in a header file.
extern u_int8_t *compress_lzss(u_int8_t *dst, u_int32_t dstlen, u_int8_t *src, u_int32_t srcLen);

void KernelPatcher_start();

PlatformInfo_t    Platform;
extern section_t* kernelSections;
extern symbolList_t*   kernelSymbols;

int main (int argc, const char * argv[])
{
	printf("******* XNU kernel patcher by Evan Lojewski                  *******\n");
	printf("******* version 0.3                                          *******\n");
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
//    Platform.CPU.Model = CPUID_MODEL_ATOM;
    
    
    // Determine size of the kernel
    
    fseek(kernel, 0, SEEK_END);
    kernel_size = ftell(kernel);
    fseek(kernel, 0, SEEK_SET);
    printf(HEADER "Reading in %d bytes...\n\n", kernel_size);
    
    
    // read in file
    kernel_data = (char*)malloc(kernel_size);
    
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
                DecodeKernel_patcher(subkernel_data);
				printf("\n");
                
			}
            
		}
		else
        {
			DecodeKernel_patcher(kernel_data);
		}
		
		char* outputFile = (char*)malloc(strlen(argv[1]) + sizeof(".patched"));
		sprintf(outputFile, "%s.patched", argv[1]);
		FILE* outFile = fopen(outputFile, "w");
		if(outFile)
        {
            printf(HEADER "Saving patched kernel to: %s\n", outputFile);
            
            fwrite(kernel_data, sizeof(char), kernel_size, outFile);
            fclose(outFile);
        }
        else
        {
            printf("\n");
            printf(HEADER "Error: Unable to open output file %s\n", outputFile);
        }
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
#if 0
    printf("[%s:%d] Allocateing %d bytes\n", file, line, size);
#endif
    return (void*)malloc(size);
}

int verbose(const char *format, ...)
{
    va_list ap;
	va_start(ap, format);
    vprintf(format, ap);
    va_end(ap);
    return 0;
}

int msglog(const char * format, ...)
{
    va_list ap;
	va_start(ap, format);
    vprintf(format, ap);
    va_end(ap);
	return 0;
}

/* stop is needed by xml code */
void stop(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    vprintf(format, ap);
    va_end(ap);
    exit(-2);
}



int file_size(int fdesc)
{
    // only used by modules.c to read in a new module. Not used by the KernelPatcher module
    return 0;
}

void start_built_in_modules()
{
}

/* copied from dirvers.c */
static unsigned long
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

long AllocateKernelMemory(long inSize)
{
    return (long)malloc(inSize);
}

long AllocateMemoryRange(char * rangeName, long start, long length, long type)
{
    return (long)malloc(length);
}

long ThinFatFile(void **binary, unsigned long *length)
{
    unsigned long nfat, swapped, size = 0;
    struct fat_header *fhp = (struct fat_header *)*binary;
    struct fat_arch   *fap =
    (struct fat_arch *)((unsigned long)*binary + sizeof(struct fat_header));
    cpu_type_t fapcputype;
    uint32_t fapoffset;
    uint32_t fapsize;
    
    if (fhp->magic == FAT_MAGIC) {
        nfat = fhp->nfat_arch;
        swapped = 0;
    } else if (fhp->magic == FAT_CIGAM) {
        nfat = OSSwapInt32(fhp->nfat_arch);
        swapped = 1;
    } else {
        return -1;
    }
    
    for (; nfat > 0; nfat--, fap++) {
        if (swapped) {
            fapcputype = OSSwapInt32(fap->cputype);
            fapoffset = OSSwapInt32(fap->offset);
            fapsize = OSSwapInt32(fap->size);
        }
        else
        {
            fapcputype = fap->cputype;
            fapoffset = fap->offset;
            fapsize = fap->size;
        }
        
        if (fapcputype == archCpuType) {
            *binary = (void *) ((unsigned long)*binary + fapoffset);
            size = fapsize;
            break;
        }
    }
    
    if (length != 0) *length = size;
    
    return 0;
}

long
DecodeKernel_patcher(void *binary)
{
	compressed_kernel_header * kernel_header = (compressed_kernel_header *) binary;
	u_int32_t uncompressed_size, size;
	void *buffer;
    u_int8_t* dstEnd = NULL;
	
#if 0
	printf("kernel header:\n");
	printf("signature: 0x%x\n", kernel_header->signature);
	printf("compress_type: 0x%x\n", kernel_header->compress_type);
	printf("adler32: 0x%x\n", OSSwapBigToHostInt32(kernel_header->adler32));
	printf("uncompressed_size: 0x%x\n", OSSwapBigToHostInt32(kernel_header->uncompressed_size));
	printf("compressed_size: 0x%x\n", OSSwapBigToHostInt32(kernel_header->compressed_size));
#endif
	
	if (kernel_header->signature == OSSwapBigToHostConstInt32('comp'))
	{
		union {
			u_int32_t i;
			char	  c[4];
		} convert;
		convert.i = kernel_header->compress_type;
        printf(HEADER "Found compressed kernel (type '%c%c%c%c'), decompressing...\n", convert.c[0], convert.c[1], convert.c[2], convert.c[3]);
		if (kernel_header->compress_type == OSSwapBigToHostConstInt32('lzss'))
		{
			uncompressed_size = OSSwapBigToHostInt32(kernel_header->uncompressed_size);
			binary = buffer = (char*)malloc(uncompressed_size);
			
			size = decompress_lzss((u_int8_t *) binary, &kernel_header->data[0],
								   OSSwapBigToHostInt32(kernel_header->compressed_size));
			if (uncompressed_size != size) {
				printf(HEADER "size mismatch from lzss: %x\n", size);
				return -1;
			}
			
			if (OSSwapBigToHostInt32(kernel_header->adler32) !=
				Adler32(binary, uncompressed_size))
			{
				printf(HEADER "adler mismatch\n");
				return -1;
			}
		}
		else if (kernel_header->compress_type == OSSwapBigToHostConstInt32('lzvn'))
		{
			uncompressed_size = OSSwapBigToHostInt32(kernel_header->uncompressed_size);
			binary = buffer = (char*)malloc(uncompressed_size);
			
			size = lzvn_decode(binary, uncompressed_size, &kernel_header->data[0], OSSwapBigToHostInt32(kernel_header->compressed_size));

			if (uncompressed_size != size) {
				printf(HEADER "size mismatch from lzvn: %x\n", size);
				return -1;
			}
			
			if (OSSwapBigToHostInt32(kernel_header->adler32) !=
				Adler32(binary, uncompressed_size))
			{
				printf(HEADER "adler mismatch\n");
				return -1;
			}
		}
        else
		{
			printf(HEADER "kernel compression is bad\n");
			return -1;
		}
	}
	
    // Notify modules that the kernel has been decompressed, thinned and is about to be decoded
	execute_hook("DecodeKernel", (void*)binary, NULL, NULL, NULL);
    
    if (kernel_header->signature == OSSwapBigToHostConstInt32('comp'))
    {
		if (kernel_header->compress_type == OSSwapBigToHostConstInt32('lzss'))
		{
			// Kernel was compressed, recompress patched kernel.
			printf(HEADER "Compressing patched kernel\n");
			dstEnd =
			compress_lzss(&kernel_header->data[0],
						  OSSwapBigToHostInt32(kernel_header->compressed_size),
						  (u_int8_t*)binary,
						  OSSwapBigToHostInt32(kernel_header->uncompressed_size));
		}
		
        if(!dstEnd)
        {
            printf("\n");
            printf(HEADER "Error: Unable to recompress kernel.");
        }
        else
        {
            printf(HEADER "Uncompress kernel is %d bytes\n", (int)((void*)dstEnd - (void*)&kernel_header->data[0]), dstEnd - &kernel_header->data[0]);
            int compressed_size = dstEnd - &kernel_header->data[0];
            if(compressed_size > OSSwapBigToHostInt32(kernel_header->compressed_size))
            {
                printf("\n");
                printf(HEADER "ERROR: Unable to recompress image to same size as before, resulting binary will be invalid");
            }
            else
            {
                kernel_header->compressed_size = OSSwapHostToBigInt32(compressed_size);
                kernel_header->adler32 = OSSwapHostToBigInt32(Adler32(binary, uncompressed_size));
            }
        }
    }
    return 0;
}
