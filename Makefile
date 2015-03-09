MODULE_NAME = KernelPatcher
MODULE_AUTHOR = xZenue LLC.
MODULE_DESCRIPTION = Kernel patcher
MODULE_VERSION = "1.2.0"
MODULE_COMPAT_VERSION = "1.0.0"
MODULE_START = $(MODULE_NAME)_start
MODULE_DEPENDENCIES = Chameleon

DIR = KernelPatcher

MODULE_OBJS   = kernel_patcher.x86.mach.o commpage_patch.x86.mach.o cpuid_patch.x86.mach.o lapic_patch.x86.mach.o power_managment_patch.x86.mach.o bootstrap_patch.x86.mach.o kext_patch.x86.mach.o xcpm_patch.x86.mach.o

PATCHER_OBJS := main.o kernel_patcher.o commpage_patch.o cpuid_patch.o lapic_patch.o power_managment_patch.o bootstrap_patch.o kext_patch.o xcpm_patch.o

ifeq (${MAKECMDGOALS},xnu_patcher)
override CONFIG_COVERAGE=n
endif

include ../MakeInc.dir

ifeq (${MAKECMDGOALS},xnu_patcher)
override CFLAGS = -m32 -D__LITTLE_ENDIAN__ -Wno-multichar -Wno-int-to-pointer-cast
#override INC := ${filter-out -I$(SRCROOT)/i386/include/,${INC}}
endif

#all: ${SYMROOT}/xnu_patcher

xnu_patcher: 	${OBJROOT} $(addprefix $(OBJROOT)/, $(PATCHER_OBJS)) $(OBJROOT)/../../boot2/macho.o $(OBJROOT)/../../boot2/modules.o \
		$(OBJROOT)/../../boot2/hooks.o \
		$(OBJROOT)/../Chameleon/lzss.o $(OBJROOT)/../Chameleon/libsaio/xml.o $(OBJROOT)/../Chameleon/libsaio/base64-decode.o
	@echo "	[LD] $@"
	${PRINT}$(CC) -m32 $(filter %.o,$^) -o $@
	@rm $(OBJROOT)/main.o


override INC = -I$(SRCROOT)/i386/modules/include/ -I${abspath include/} -I../Chameleon/trunk/i386/libsaio/ -I../Chameleon/trunk/i386/libsa/ \
        -I../Chameleon/trunk/i386/include/ -I${SRCROOT}/i386/boot2/ $(MODULE_INCLUDES)

