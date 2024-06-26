#include "AEConfig.h"
#include "AE_EffectVers.h"

#ifndef AE_OS_WIN
	#include <AE_General.r>
#endif

#ifdef AE_OS_WIN
#include "../../AE_Suites/PopcornFX_Define_Version.h"
#endif
#ifdef AE_OS_MAC
#include "PopcornFX_Define_Version.h"
#endif
	
resource 'PiPL' (16000) {
	{	/* array properties: 12 elements */
		/* [1] */
		Kind {
			AEEffect
		},
		/* [2] */
		Name {
			"Attribute Sampler"
		},
		/* [3] */
		Category {
			"PopcornFX"
		},
#ifdef AE_OS_WIN
	#ifdef AE_PROC_INTELx64
		CodeWin64X86 {"EffectMain"},
	#endif
#else
	#ifdef AE_OS_MAC
		CodeMacIntel64 {"EffectMain"},
	#endif
#endif
		/* [6] */
		AE_PiPL_Version {
			2,
			0
		},
		/* [7] */
		AE_Effect_Spec_Version {
			PF_PLUG_IN_VERSION,
			PF_PLUG_IN_SUBVERS
		},
		/* [8] */
		AE_Effect_Version {
			AEPOPCORNFX_PIPL_VERSION
		},
		/* [9] */
		AE_Effect_Info_Flags {
			0
		},
		/* [10] */
		AE_Effect_Global_OutFlags {
			0x06200000
		},
		AE_Effect_Global_OutFlags_2 {
			0x08001403
		},
		/* [11] */
		AE_Effect_Match_Name {
			"ADBE PopcornFX Sampler"
		},
		/* [12] */
		AE_Reserved_Info {
			8
		}
	}
};

