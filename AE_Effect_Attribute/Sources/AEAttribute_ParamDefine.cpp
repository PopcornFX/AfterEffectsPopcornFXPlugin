//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#include "ae_precompiled.h"

#include "AEAttribute_ParamDefine.h"

__AAEPK_BEGIN

//----------------------------------------------------------------------------

struct	STableString
{
	A_u_long	index;
	A_char		str[256];
} ;

//----------------------------------------------------------------------------

STableString		g_strs[StrID_NUMTYPES] =
{
	{ StrID_NONE,							"" },
	{ StrID_Name,							"Attribute" },
	{ StrID_Description,					"PopcornFX Plugin." },
	{ StrID_Generic_Bool1,					"Attribute bool" },
	{ StrID_Generic_Bool2,					"Attribute bool 2" },
	{ StrID_Generic_Bool3,					"Attribute bool 3" },
	{ StrID_Generic_Bool4,					"Attribute bool 4" },
	{ StrID_Generic_Int1,					"Attribute int" },
	{ StrID_Generic_Int2,					"Attribute int 2" },
	{ StrID_Generic_Int3,					"Attribute int 3" },
	{ StrID_Generic_Int4,					"Attribute int 4" },
	{ StrID_Generic_Float1,					"Attribute float" },
	{ StrID_Generic_Float2,					"Attribute float 2" },
	{ StrID_Generic_Float3,					"Attribute float 3" },
	{ StrID_Generic_Float4,					"Attribute float 4" },
	{ StrID_Generic_Quaternion,				"Attribute Quaternion" },
	{ StrID_Generic_Infernal_Uuid,			"AttributeKey" },
	{ StrID_Scale_Checkbox,					"Affected by scale" },
	{ StrID_Generic_Infernal_Name,			"AttributeName" },
	{ StrID_Generic_Color_RGB,				"RGB" },
	{ StrID_Generic_Color_A,				"Alpha" },
	{ StrID_Parameters_Reset,				"" },
	{ StrID_Parameters_Reset_Button,		"Reset Value" },
};

//----------------------------------------------------------------------------

__AAEPK_END

//----------------------------------------------------------------------------

#ifdef 	__cplusplus
extern "C"
{
#endif

A_char	*GetStringPtr(int strNum)
{
	return AAePk::g_strs[strNum].str;
}

#ifdef 	__cplusplus
}
#endif

