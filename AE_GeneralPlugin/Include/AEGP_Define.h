//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#pragma once

#ifndef __FX_AEGP_DEFINE_H__
#define __FX_AEGP_DEFINE_H__

#include <PopcornFX_Define.h>

#include <popcornfx.h>

//----------------------------------------------------------------------------

#define	__AEGP_PK_BEGIN		namespace AEGPPk {
#define	__AEGP_PK_END		}

#define	ASSERT_AE_ERR(result, value)	PK_ASSERT(value != A_Err_NONE); result |= value;

__AEGP_PK_BEGIN

//----------------------------------------------------------------------------

using namespace	PK_NAMESPACE;
using namespace	AAePk;

//----------------------------------------------------------------------------

enum EApiValue //Do not reorder/change values
{
	D3D11 = 1,
	D3D12,
	Metal,
	Size,
};

//----------------------------------------------------------------------------

__AEGP_PK_END

#endif // !__FX_AEGP_DEFINE_H__
