/*
 * Copyright (c) 2009-2012 Evan Lojewski. All rights reserved.
 *
 */
#ifndef __BOOT2_KERNEL_PATCHER_H
#define __BOOT2_KERNEL_PATCHER_H

#define KPDefaultKexts      "/Extra/KPKexts/"
#define KPDirKey            "KPDir"

#include <libkern/OSTypes.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <mach-o/reloc.h>

#include "modules.h"
#include "cpu.h"
#define CPUID_MODEL_ANY		0x00
#define CPUID_MODEL_UNKNOWN	0x01


#define KERNEL_ANY	0x00
#define KERNEL_64	0x01
#define KERNEL_32	0x02
#define KERNEL_ERR	0xFF

typedef struct patchRoutine_t
{
	void(*patchRoutine)(void*);
	int validArchs;
	int validCpu;
	struct patchRoutine_t* next;
} patchRoutine_t;


typedef struct kernSymbols_t
{
	char* symbol;
	UInt64 addr;
	struct kernSymbols_t* next;
} kernSymbols_t;

typedef struct section_t
{
	const char* segment;
    const char* section;
    UInt64 address;
    UInt64 offset;
	struct section_t* next;
} section_t;




symbolList_t* lookup_kernel_symbol(const char* name);
void register_kernel_symbol(int kernelType, const char* name);

section_t* lookup_section(const char* segment, const char* section);
void register_section(const char* segment, const char* section);


long long symbol_handler(char* symbolName, long long addr, char is64);
void patch_kernel(void* kernelData, void* arg2, void* arg3, void *arg4);
void register_kernel_patch(void* patch, int arch, int cpus);

int locate_symbols(void* kernelData);

int determineKernelArchitecture(void* kernelData);

/*
 * Internal patches provided by this module.
 */
void patch_cpuid_set_info_all(void* kernelData);
void patch_cpuid_set_info_32(void* kernelData, UInt32 impersonateFamily, UInt8 impersonateModel);
void patch_cpuid_set_info_64(void* kernelData, UInt32 impersonateFamily, UInt8 impersonateModel);

void patch_pmCPUExitHaltToOff(void* kernelData);
void patch_pmKextRegister(void* kernelData);
void patch_lapic_init(void* kernelData);
void patch_commpage_stuff_routine(void* kernelData);
void patch_lapic_configure(void* kernelData);
void patch_lapic_interrupt(void* kernelData);

void patch_readStartupExtensions(void* kernelData);

#endif /* !__BOOT2_KERNEL_PATCHER_H */
