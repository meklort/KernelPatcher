#include <mach/mach_types.h>

#include "lapic.h"

extern int	cpu_number(void);
extern int master_cpu;

kern_return_t LAPICFix_start (kmod_info_t * ki, void * d) {
    // NOTE: This hould not be called by XNU. Chameleon should be loading and binding this kext manualy.
    //printf("[LAPICFix] Kext loaded in an usuppoted manner. Please use chameleon with the kernel patcher enabled\n");
    return KERN_FAILURE;
}


kern_return_t LAPICFix_stop (kmod_info_t * ki, void * d) {
    return KERN_SUCCESS;
}



/*
 * Copyright (c) 2008-2009 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 * 
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */
/*
 * @OSF_COPYRIGHT@
 */

static void 
lapic_esr_clear(void)
{
	LAPIC_WRITE(ERROR_STATUS, 0);
	LAPIC_WRITE(ERROR_STATUS, 0);
}


void LAPICFix_lapic_configure(void)
{
	int	value;
    /*
	if (lapic_error_time_threshold == 0 && cpu_number() == 0) {
		nanoseconds_to_absolutetime(NSEC_PER_SEC >> 2, &lapic_error_time_threshold);
		if (!PE_parse_boot_argn("lapic_dont_panic", &lapic_dont_panic, sizeof(lapic_dont_panic))) {
			lapic_dont_panic = FALSE;
		}
	}*/
    
	/* Set flat delivery model, logical processor id */
	LAPIC_WRITE(DFR, LAPIC_DFR_FLAT);
	LAPIC_WRITE(LDR, (cpu_number()) << LAPIC_LDR_SHIFT);
    
	/* Accept all */
	LAPIC_WRITE(TPR, 0);
    
	LAPIC_WRITE(SVR, LAPIC_VECTOR(SPURIOUS) | LAPIC_SVR_ENABLE);
    
	/* ExtINT */
	if (cpu_number() == master_cpu) {
		value = LAPIC_READ(LVT_LINT0);
		value &= ~LAPIC_LVT_MASKED;
		value |= LAPIC_LVT_DM_EXTINT;
		LAPIC_WRITE(LVT_LINT0, value);

	}
    
    /* NMI: ummasked, off course */
    LAPIC_WRITE(LVT_LINT1, LAPIC_LVT_DM_NMI);
    
	/* Timer: unmasked, one-shot */
	LAPIC_WRITE(LVT_TIMER, LAPIC_VECTOR(TIMER));
    
	/* Perfmon: unmasked */
	LAPIC_WRITE(LVT_PERFCNT, LAPIC_VECTOR(PERFCNT));
    
	/* Thermal: unmasked */
	LAPIC_WRITE(LVT_THERMAL, LAPIC_VECTOR(THERMAL));
    
#if CONFIG_MCA
	/* CMCI, if available */
	if (mca_is_cmci_present())
		LAPIC_WRITE(LVT_CMCI, LAPIC_VECTOR(CMCI));
#endif
    
//	if (((cpu_number() == master_cpu)) ||
//		(cpu_number() != master_cpu)) {
		lapic_esr_clear();
		LAPIC_WRITE(LVT_ERROR, LAPIC_VECTOR(ERROR));
//	}
}
