//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#pragma once

#include <ae_precompiled.h>
#include <A.h>

#include <PopcornFX_Suite.h>

__AAEPK_BEGIN

//----------------------------------------------------------------------------

enum EPKParams
{
	ATTRIBUTE_INPUT = 0,
	ATTRIBUTE_PARAM,
	ATTRIBUTE_NUM_PARAMS
};

//----------------------------------------------------------------------------

enum	EStrIDType
{
	StrID_NONE = 0,
	StrID_Name,
	StrID_Description,
	StrID_Generic_Bool1,
	StrID_Generic_Bool2,
	StrID_Generic_Bool3,
	StrID_Generic_Bool4,
	StrID_Generic_Int1,
	StrID_Generic_Int2,
	StrID_Generic_Int3,
	StrID_Generic_Int4,
	StrID_Generic_Float1,
	StrID_Generic_Float2,
	StrID_Generic_Float3,
	StrID_Generic_Float4,
	StrID_Generic_Quaternion,

	StrID_Generic_Infernal_Uuid,
	StrID_Scale_Checkbox,
	StrID_Generic_Infernal_Name,

	StrID_Generic_Color_RGB,
	StrID_Generic_Color_A,

	StrID_Parameters_Reset,
	StrID_Parameters_Reset_Button,

	StrID_NUMTYPES
};

//----------------------------------------------------------------------------

__AAEPK_END
