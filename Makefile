MODULE_NAME = KernelPatcher
MODULE_AUTHOR = xZenue LLC.
MODULE_DESCRIPTION = Kernel patcher
MODULE_VERSION = "1.2.0"
MODULE_COMPAT_VERSION = "1.0.0"
MODULE_START = $(MODULE_NAME)_start
MODULE_DEPENDENCIES = 

DIR = KernelPatcher

MODULE_OBJS   = kernel_patcher.x86.mach.o commpage_patch.x86.mach.o cpuid_patch.x86.mach.o lapic_patch.x86.mach.o power_managment_patch.x86.mach.o bootstrap_patch.x86.mach.o kext_patch.x86.mach.o

PATCHER_OBJS := main.o ${MODULE_OBJS}


include ../MakeInc.dir


#all: ${SYMROOT}/xnu_patcher

xnu_patcher: ${OBJROOT} $(addprefix $(OBJROOT)/, $(PATCHER_OBJS)) $(OBJROOT)/../../boot2/modules.o $(OBJROOT)/../../boot2/lzss.o $(OBJROOT)/../../libsaio/xml.o $(OBJROOT)/../../libsaio/base64-decode.o
	@echo "\t[LD] $@"
	@$(CC) -arch i386 $(filter %.o,$^) -o $@
	@rm $(OBJROOT)/main.o

