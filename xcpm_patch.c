#include <kernel_patcher.h>
#include <modules.h>

#define HEADER 

typedef struct {
	UInt32 msr;
	UInt32 unk0;
	UInt32 unk1;
	UInt32 unk2;
	
	UInt32 unk3;
	UInt32 unk4;
	UInt32 data;
	UInt32 unk5;
	
	UInt32 unk6;
	UInt32 unk7;
	UInt32 unk8;
	UInt32 unk9;
} msr_t;

void patch_xcpm_msr(void* kernelData)
{
	int i = 0;
	UInt8* bytes = (UInt8*)kernelData;
	// Location section containing symbol
	section_t* data = lookup_section("__DATA","__data");
	if(!data)
	{
		printf(HEADER "Unable to locate __DATA,__data\n");
		return;
	}
	
	// Locate symbol needing patch:
	symbolList_t* symbol = lookup_kernel_symbol("_xcpm_core_scope_msrs");
	if(!symbol || !symbol->addr)
	{
		printf(HEADER "Unable to locate _xcpm_scope_msrs");
		return;
	}
	UInt32 addr = data->address;
	UInt32 offset = data->offset;
	
	msr_t* msrs = (void*)((UInt32)&bytes[(UInt32)(symbol->addr - data->address + data->offset)] - (UInt32)kernelData);
		 
	// TODO: chose better end condiont (such as end of variable)
	for(i = 0; i < 10; i++)
	{
		//printf("msr[%d] = 0x%X (0x%X)\n", i, msrs[i].msr, msrs[i].data);
		if(msrs[i].msr == 0xe2)
		{
			msrs[i].msr = 0;
			msrs[i].unk0 = 0;
		}
		
	}
	//pause();
}
