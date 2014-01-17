/*
 * Copyright (c) 2009-2011 Evan Lojewski. All rights reserved.
 *
 *
 * This work is licensed under the
 *  Creative Commons Attribution-NonCommercial 3.0 Unported License.
 *  To view a copy of this license, visit http://creativecommons.org/licenses/by-nc/3.0/.
 */

#include "libsaio.h"
#include "kernel_patcher.h"
#include "platform.h"
#include "modules.h"
#include "xml.h"
#include "sl.h"
#include "bootstruct.h"

#define DBG     printf
//fixme ^^^

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#define HEADER      __FILE__ "[" TOSTRING(__LINE__) "]: "

#ifndef UNKNOWN_ADDRESS
#define UNKNOWN_ADDRESS		((unsigned long)(-1))
#endif


void* preDecodedKernAddr = NULL;
void loadKernelPatcherKexts(void* kernelBinary, void* arg2, void* arg3, void *arg4);

void section_handler(char* section, char* segment, void* cmd, UInt64 offset, UInt64 address);

extern symbolList_t* moduleSymbols;
extern PlatformInfo_t    Platform;

patchRoutine_t* patches = NULL;
section_t* kernelSections = NULL;

//kernSymbols_t* kernelSymbols = NULL;
symbolList_t*   kernelSymbols = NULL;


/*
 * Return the symbol specified by the index.
 */
symbolList_t* lookup_symbol_id(UInt32 index)
{
	symbolList_t* entry = moduleSymbols;
    while(index && entry)
    {
        entry = entry->next;
        index--;
    }
    return entry;
}


typedef struct sectionList_t
{
	bool is64;
    union {
		struct section* section; 
		struct section_64* section_64; 
	} data;
	
    struct sectionList_t* next;
} sectionList_t;

sectionList_t* kextSections = NULL;

void KernelPatcher_start()
{
	register_kernel_patch(patch_cpuid_set_info_all,     KERNEL_ANY, CPUID_MODEL_UNKNOWN); 
    
	register_kernel_patch(patch_commpage_stuff_routine, KERNEL_ANY, CPUID_MODEL_ANY);
	register_kernel_patch(patch_lapic_init,             KERNEL_ANY, CPUID_MODEL_ANY);
	register_kernel_patch(patch_lapic_interrupt,             KERNEL_ANY, CPUID_MODEL_ANY);
    
	// NOTE: following is currently 32bit only
	register_kernel_patch(patch_lapic_configure,        KERNEL_32, CPUID_MODEL_ANY);
    
	register_kernel_patch(patch_readStartupExtensions,  KERNEL_ANY, CPUID_MODEL_ANY);
    
    register_kernel_patch(patch_pmKextRegister,        KERNEL_ANY, CPUID_MODEL_ANY);
    //register_kernel_patch(patch_pmCPUExitHaltToOff,        KERNEL_ANY, CPUID_MODEL_ANY);

    register_kernel_patch(patch_kexts,        KERNEL_ANY, CPUID_MODEL_ANY);

    
    register_section("__KLD", "__text");
    register_section("__TEXT","__text");
    register_section("__PRELINK_TEXT","__text");
    register_section("__PRELINK_INFO","__info");
    
	register_hook_callback("DecodeKernel", &patch_kernel); 
    register_hook_callback("DecodedKernel", &loadKernelPatcherKexts);
}

/*
 * Load KPKexts
 */
long long add_symbol_kmod(char* symbol, long long addr, char is64)
{
    symbolList_t* entry;
    symbolList_t* last = moduleSymbols;
    
    entry = malloc(sizeof(symbolList_t));
	entry->addr = (UInt32)addr;
	entry->symbol = symbol;
    entry->next = NULL;
    
    while(last && last->next) last = last->next;
    
    // add symbol to end of list
    if(last == NULL) moduleSymbols = entry; 
    else             last->next = entry;

    return UNKNOWN_ADDRESS;
}

void load32KernelPatcherKexts(void* kernelBinary, void* kernelFinal, void* arg3, void *arg4)
{
    TagPtr plistData;
    TagPtr allSymbols;
    
    // Read in KPDefaultKexts/<kextname>.kext/Contents/Info.plist and determine
    // what functions need to exist in the kernel. If no functions can be found, don't
    // load the kext. If a function does need to be replaced, load the 
    // KPDefaultKexts/<kextname>.kext/Contents/MacOS/<kextname> file and
    // ensure that the file is available for the current architecture (thin if nescicary)
    
    
    char* kextname = malloc(1024);
    char* name;
	long flags;
	long time;
	struct dirstuff* kextsDir = opendir(KPDefaultKexts);
	while(readdir(kextsDir, (const char**)&name, &flags, &time) >= 0)
	{
        strcpy(kextname, name);
        
		if(strcmp(&name[strlen(name) - sizeof("kext")], ".kext") == 0)
		{
            kextname[strlen(name) - sizeof("kext")] = 0;
            
            // Kext found, look for the Infoplist
            char* fullPath = malloc(sizeof(KPDefaultKexts) + strlen(kextname) + strlen(".kext/Contents/Info.plist"));
            char* binPath = malloc(sizeof(KPDefaultKexts) + strlen(kextname) + strlen(".kext/Contents/MacOS/") + strlen(kextname));
            
            sprintf(fullPath, KPDefaultKexts "%s.kext/Contents/Info.plist", kextname);
            sprintf(binPath, KPDefaultKexts "%s.kext/Contents/MacOS/%s", kextname, kextname);
            
            int fh = open(fullPath, 0);
            if(fh >= 0)
            {
                unsigned int plistSize = file_size(fh);
                char* plistBase = (char*) malloc(plistSize);
                
                if (plistSize && read(fh, plistBase, plistSize) == plistSize)
                {
                    int count;
                    //Read in the Info.plist file and look for the KPReplaceFunctions dictionary.
                    //const char *val;
                    //int len;
                    
                    XMLParseFile( plistBase, &plistData );
                    allSymbols = XMLCastDict(XMLGetProperty(plistData, "KPReplaceFunctions"));
                    if(!allSymbols)
                    {
                        printf("%s is missing the KPReplaceFunctions dictionary.\n", kextname);
                    }
                    else
                    {
                        // Load up the kext and add it to the symbol table. Also bind + rebase the kext
                        count = XMLTagCount(allSymbols);
                        symbolList_t* kextSymbols = NULL;
                        struct section* fileSection = NULL; // last file section = text
						
                        long kextBase;
                        if(count)
                        {
                            int fh = open(binPath, 0);
                            if(fh >= 0)
                            {
                                unsigned long kextSize = file_size(fh);
                                void* binary = malloc(kextSize);
                                if (kextSize && read(fh, binary, kextSize) == kextSize)
                                {
                                    
                                    /*long ret = */
                                    ThinFatFile(&binary, &kextSize);
                                    
                                    // Allocate memory for the kext + tell xnu not to use it.
                                    kextBase = AllocateKernelMemory(kextSize);
                                    if(kextBase)
                                    {
                                        AllocateMemoryRange(kextname, kextBase, kextSize, kBootDriverTypeInvalid);
                                        // modify bootArgs->ksize to ensure that it include the kext.
                                        bootArgs->ksize = kextBase - bootArgs->kaddr + kextSize;
                                        bcopy(binary, (void*)kextBase, kextSize); // copy kext into allocated mem
                                        
                                        symbolList_t* oldSymbols = moduleSymbols;
                                        
                                        moduleSymbols = NULL;
                                        kextSections = NULL;
                                        parse_mach((void*)kextBase, NULL, &add_symbol_kmod, &section_handler);
                                        kextSymbols = moduleSymbols;
                                        
                                        while(kextSections)
                                        {  
                                            fileSection = kextSections->data.section;
                                            //printf("nreloc = %d at 0x%X\n", fileSection->nreloc, (UInt32)fileSection->reloff);
                                            //printf("Reloc at 0x%X\n", ((UInt32)kextBase) + ((UInt32)fileSection->reloff));
                                            struct relocation_info* relinfo = (struct relocation_info*)((UInt32)kextBase + (UInt32)fileSection->reloff);
                                            while(fileSection->nreloc)
                                            {
                                                //printf("r_address = 0x%X\n", (UInt32)relinfo->r_address);
                                                //printf("r_symbolnum = %d\n", relinfo->r_symbolnum);
                                                symbolList_t* sym = lookup_symbol_id(relinfo->r_symbolnum);
                                                if(sym)
                                                {                                                       
                                                    moduleSymbols = kernelSymbols;
                                                    UInt32 symAddr = lookup_all_symbols(sym->symbol);
                                                    moduleSymbols = kextSymbols;
                                                    
                                                    if(symAddr != (UInt32)UNKNOWN_ADDRESS)
                                                    {
                                                        symAddr -= (UInt32)preDecodedKernAddr;
                                                        
                                                        printf("Symbol Name: %s found at 0x%X r_pcrel = %d\n", 
                                                               sym->symbol,
                                                               symAddr, relinfo->r_pcrel);
                                                        
                                                        UInt32 *patchLocation = (UInt32*)((UInt32)kextBase +  (UInt32)relinfo->r_address + (UInt32)fileSection->offset);
                                                        //printf("Patch at 0x%X\n", relinfo->r_address);
                                                        if(relinfo->r_pcrel)
                                                            *patchLocation = symAddr - (UInt32)patchLocation + 4;
                                                        else
                                                            *patchLocation = symAddr;
                                                        printf("Sym %s at 0x%X = Value: 0x%X\n", sym->symbol, relinfo->r_address, *patchLocation);                                                               
                                                        //pause();
                                                    }
                                                    else
                                                    {
                                                        //printf("UNK - 0x%X (%d)<><>", relinfo->r_address, relinfo->r_symbolnum);
                                                    }
                                                }
                                                else
                                                {
                                                    //printf("UNKS - 0x%X (%d)<><>", relinfo->r_address, relinfo->r_symbolnum);
                                                }
												
                                                fileSection->nreloc--;
                                                relinfo++;
                                            }
                                            kextSections = kextSections->next;
                                        }
                                        // restore symbols
                                        moduleSymbols = oldSymbols;                                        
                                    }
                                    else
                                    {
                                        printf("Unable to allocate kernel memory for %s.kext\n", kextname);
                                    }
                                }
                                else
                                {
                                    printf("KPKext[%s]: Unable to read in %s\n", binPath);
                                }
                            }
                            else
                            {
                                printf("KPKext[%s]: %s not found.\n", binPath);
                            }
                        }
                        
                        //section_t* txt = lookup_section("__TEXT","__text");
						
                        
                        while(count)
                        {
                            
							
                            const char* oldFunction = XMLCastString(XMLGetKey(allSymbols, count));
                            const char* newFunction = XMLCastString(XMLGetProperty(allSymbols, oldFunction));
							
                            // TODO: fix for 64bit
                            symbolList_t* oldSymbols = moduleSymbols;
							
                            moduleSymbols = kextSymbols;
                            long long newaddr = lookup_all_symbols(newFunction);
							
                            moduleSymbols = kernelSymbols;
                            long long origaddr = lookup_all_symbols(oldFunction);
                            
                            moduleSymbols = oldSymbols;
                            
                            if(origaddr != UNKNOWN_ADDRESS && newaddr != UNKNOWN_ADDRESS)
                            {
                                printf("KPKext[%s]: replacing %s with %s\n", kextname, oldFunction, newFunction);
                                
                                UInt32* jumpPointer = (UInt32*)AllocateKernelMemory(sizeof(UInt32*));	 
                                AllocateMemoryRange("Jump Address", (long)jumpPointer, sizeof(UInt32*), kBootDriverTypeInvalid);
                                
                                char* binary = (char*)(UInt32)origaddr -  (UInt32)preDecodedKernAddr/* + (UInt32)kernelFinal*/;
                                //binary += txt->offset;
                                //binary -= txt->address;
                                //binary -= (UInt32)preDecodedKernAddr;
                                printf("Patch location: 0x%X (binary: 0%X) (kernel: 0x%X)\n", binary, preDecodedKernAddr, kernelFinal);
                                //DBG("Replacing %s to point to 0x%x\n", symbol, newAddress);
                                printf("function start: 0x%X 0x%X 0x%X\n", binary[0], binary[1], binary[2]);
                                *binary++ = 0xFF;	// Jump
                                *binary++ = 0x25;	// Long Jump
                                *((UInt32*)binary) = (UInt32)jumpPointer;
                                
                                *jumpPointer = (UInt32)newaddr + (UInt32)kextBase + (UInt32)fileSection->offset;
								
                                printf("function start: 0x%X 0x%X 0x%X\n", binary[-2], binary[-1], ((UInt32*)binary)[0]);
                                printf("function start: 0x%X 0x%X 0x%X\n",  ((char*)(*jumpPointer))[0],  ((char*)(*jumpPointer))[1],  ((char*)(*jumpPointer))[2]);
								
                                printf("jumpPointer = 0x%X\n", *jumpPointer);
                                
								
                                // This symbol is known, we can patch the kernel
                                
                            }
                            else printf("KPKext[%s]: unable to replace %s with %s\n", kextname, oldFunction, newFunction);
                            
                            count--;
                        }
                        pause();
                    }
                    
                }
                close(fh);
            }
		}
	}
    
    closedir(kextsDir);
    printf(HEADER "Returning...\n");
    pause();
}

void load64KernelPatcherKexts(void* kernelBinary, void* kernelFinal, void* arg3, void *arg4)
{
    printf(HEADER "Returning...\n");
    pause();
}





void loadKernelPatcherKexts(void* kernelBinary, void* kernelFinal, void* arg3, void *arg4)

{
	int arch = determineKernelArchitecture(kernelBinary);
	
	if(arch == KERNEL_ERR)
	{
		printf(HEADER "Unable to patch unknown architecture\n");
		return;
	}
	
    
    
	if(arch == KERNEL_32) load32KernelPatcherKexts(kernelBinary, kernelFinal, arg3, arg4);
	if(arch == KERNEL_64) load64KernelPatcherKexts(kernelBinary, kernelFinal, arg3, arg4);
}

	
	/*
 * Register a kerenl patch
 */
void register_kernel_patch(void* patch, int arch, int cpus)
{
	// TODO: only insert valid patches based on current cpuid and architecture
	// AKA, don't add 64bit patches if it's a 32bit only machine
	patchRoutine_t* entry;
	
	// Check to ensure that the patch is valid on this machine
	// If it is not, exit early from this function
	if(cpus != Platform.CPU.Model)
	{
		if(cpus != CPUID_MODEL_ANY)
		{
			if(cpus == CPUID_MODEL_UNKNOWN)
			{
				switch(Platform.CPU.Model)
				{
					case 13:
					case CPUID_MODEL_YONAH:
					case CPUID_MODEL_MEROM:
					case CPUID_MODEL_PENRYN:
					case CPUID_MODEL_NEHALEM:
					case CPUID_MODEL_FIELDS:
					case CPUID_MODEL_DALES:
					case CPUID_MODEL_NEHALEM_EX:
						// Known cpu's we don't want to add the patch
						return;
						break;
                        
					default:
						// CPU not in supported list, so we are going to add
						// The patch will be applied
						break;
						
				}
			}
			else
			{
				// Invalid cpuid for current cpu. Ignoring patch
				return;
			}
            
		}
	}
    
	if(patches == NULL)
	{
		patches = entry = malloc(sizeof(patchRoutine_t));
	}
	else
	{
		entry = patches;
		while(entry->next)
		{
			entry = entry->next;
		}
		
		entry->next = malloc(sizeof(patchRoutine_t));
		entry = entry->next;
	}
	
	entry->next = NULL;
	entry->patchRoutine = patch;
	entry->validArchs = arch;
	entry->validCpu = cpus;
}

symbolList_t* lookup_kernel_symbol(const char* name)
{
	symbolList_t *symbol = kernelSymbols;
    
	while(symbol && strcmp(symbol->symbol, name) !=0)
	{
		symbol = symbol->next;
	}
	
	if(!symbol)
	{
		return NULL;
	}
	else
	{
		return symbol;
	}
    
}


section_t* lookup_section(const char* segment, const char* section)
{    
	if(!segment || !section) return NULL;
	
    section_t* sections = kernelSections;
    	
	while(sections && 
		  !(strcmp(sections->segment, segment) == 0 &&
		    strcmp(sections->section, section) == 0))
	{   
		
		sections = sections->next;
	}
	
	return sections;
}

void register_section(const char* segment, const char* section)
{
    if(kernelSections == NULL)
	{
		kernelSections = malloc(sizeof(section_t));
		kernelSections->next = NULL;
        
        kernelSections->segment = segment;
        kernelSections->section = section;
        kernelSections->address = 0;
        kernelSections->offset = 0;
	}
	else {
		section_t *sect = kernelSections;
		while(sect->next != NULL) sect = sect->next;
		
		sect->next = malloc(sizeof(section_t));
		sect = sect->next;
        
        sect->segment = segment;
        sect->section = section;
        sect->address = 0;
        sect->offset = 0;
        
    }
    
    
}


void patch_kernel(void* kernelData, void* arg2, void* arg3, void *arg4)
{
	int arch = determineKernelArchitecture(kernelData);
	
	if(arch == KERNEL_ERR)
	{
		printf(HEADER "Unable to patch unknown architecture\n");
		return;
	}
	
    preDecodedKernAddr = kernelData;
	
    // Use the symbol handler from the module system. This requires us to backup the moduleSymbols variable and restore it once complete
    symbolList_t* origmoduleSymbols = moduleSymbols;
    
    
    moduleSymbols = NULL;
    locate_symbols(kernelData);
    kernelSymbols = moduleSymbols;    // save symbols for future use, if needed
    
	section_t* txt = lookup_section("__TEXT","__text");
	if(!txt)
	{
		printf(HEADER "Unable to locate __TEXT,__text\n");
		return;
	}
	
    // Determine kernel version and print this out once done.
    symbolList_t *ver_major = lookup_kernel_symbol("_version_major");
	symbolList_t *ver_minor = lookup_kernel_symbol("_version_minor");
    symbolList_t *ver_rev   = lookup_kernel_symbol("_version_revision");

	symbolList_t *version   = lookup_kernel_symbol("_version");

    UInt32* version_major = (UInt32*)(UInt32)(ver_major ? ver_major->addr - txt->address + txt->offset: 0);
    UInt32* version_minor = (UInt32*)(UInt32)(ver_minor ? ver_minor->addr - txt->address + txt->offset: 0);
    UInt32* version_rev   = (UInt32*)(UInt32)(ver_rev   ? ver_rev->addr   - txt->address + txt->offset: 0);
	char*   version_str      = (void*)(UInt32)(version   ? version->addr   - txt->address + txt->offset: 0);
	char* start = 0;
	
	if(version_str && (start = strstr(version_str,   "root:xnu")))
	{
		memcpy(start, "*patched", strlen("*patched"));
	}
    verbose(HEADER "Patching %dbit XNU Kernel %d.%d.%d\n%s\n", 
		   arch == KERNEL_32 ? 32 : 64, 
		   version_major ? *version_major : 0, 
		   version_minor ? *version_minor : 0, 
		   version_rev   ? *version_rev : 0, version_str);
    

	patchRoutine_t* kernelPatch = patches;
    while(kernelPatch)
    {
        if(kernelPatch->validArchs == KERNEL_ANY || arch == kernelPatch->validArchs)
        {
            if(kernelPatch->patchRoutine) kernelPatch->patchRoutine(kernelData);
        }
        kernelPatch = kernelPatch->next;
    }

    // Notify any clients that the kext patcher is about to exit, sending along the list of symbols and arch of the kernel.
    execute_hook("Kernel Patched", (void*)kernelData, (void*)kernelSymbols, (void*)arch, NULL);
    
    
    moduleSymbols = origmoduleSymbols; // restore orig pointer;
}

int determineKernelArchitecture(void* kernelData)
{	
	if(((struct mach_header*)kernelData)->magic == MH_MAGIC)
	{
		return KERNEL_32;
	}
	if(((struct mach_header*)kernelData)->magic == MH_MAGIC_64)
	{
		return KERNEL_64;
	}
	else
	{
		return KERNEL_ERR;
	}
}


/**
 **		This functions located the requested symbols and segments in the mach-o file
 **/
inline int locate_symbols(void* kernelData)
{
	parse_mach(kernelData, NULL, &add_symbol, &section_handler);
	return 1;
}

void section_handler(char* section, char* segment, void* cmd, UInt64 offset, UInt64 address)
{
//    printf("segment: %s,%s\t\toffset: 0x%lX, 0x%lX\n", segment, section, address, offset);
    //    printf("segment: %s,%s offset: 0x%X, 0x%X\n", segment, section, address, offset);
    //    printf("segment: %s,%s offset: 0x%X, 0x%X\n", segment, section, offset, address);
    
    if(archCpuType == CPU_TYPE_I386)
    {
        struct section* fileSection = (void*)(UInt32)cmd;
        if(fileSection->nreloc)
        {
            sectionList_t* first = malloc(sizeof(sectionList_t));
            first->next = kextSections;
            kextSections = first;
            kextSections->data.section = fileSection;
			kextSections->is64 = false;
        }
    }

    if(archCpuType == CPU_TYPE_X86_64)
    {
        struct section_64* fileSection = (void*)(UInt32)cmd;
        if(fileSection->nreloc)
        {
            sectionList_t* first = malloc(sizeof(sectionList_t));
            first->next = kextSections;
            kextSections = first;
            kextSections->data.section_64 = fileSection;
			kextSections->is64 = true;
        }
    }

    // Locate the section in the list, if it exists, update it's address
	section_t *kernelSection = lookup_section(segment, section);
	
	if(kernelSection)
    {
        //printf("%s,%s located at 0x%lX\n", kernelSection->segment, kernelSection->section, kernelSection->address - kernelSection->offset);
        //printf("%s,%s located at 0x%X\n", kernelSection->segment, kernelSection->section, address - offset);
		kernelSection->address = address;
        kernelSection->offset = offset;
    }
}


// checkOSVersion


