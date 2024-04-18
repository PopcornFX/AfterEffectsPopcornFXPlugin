//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#pragma once

#ifndef __FX_AECONVERSION_H__
#define __FX_AECONVERSION_H__

#include "AEGP_Define.h"

#include <PopcornFX_Define.h>

#include <pk_rhi/include/FwdInterfaces.h>
#include <pk_particles/include/ps_declaration.h>

#include <AE_EffectPixelFormat.h>
#include <AEFX_SuiteHelper.h>
#include <PopcornFX_Suite.h>
#include <AE_EffectCBSuites.h>
#include <AE_Macros.h>

#include <PK-SampleLib/RHIRenderParticleSceneHelpers.h>
#include <pk_particles/include/ps_samplers_vectorfield.h>

#include <string>

__AEGP_PK_BEGIN

//----------------------------------------------------------------------------

RHI::EPixelFormat		AAEToPK(PF_PixelFormat format);
PF_PixelFormat			PKToAAE(RHI::EPixelFormat format);
u32						GetPixelSizeFromPixelFormat(RHI::EPixelFormat format);

void					AAEToPK(A_Matrix4 &in, CFloat4x4 &out);
void					AAEToPK(A_Matrix4 &in, CFloat4x4 *out);

CFloat3					AAEToPK(A_FloatPoint3 &in);
A_FloatPoint3			PKToAAE(CFloat3 &in);

CFloat3					AngleAAEToPK(A_FloatPoint3 &in);
A_FloatPoint3			AnglePKToAAE(CFloat3 &in);

EAttributeSemantic		AttributePKToAAE(EDataSemantic value);

EAttributeType			AttributePKToAAE(EBaseTypeID value);
EBaseTypeID				AttributeAAEToPK(EAttributeType value);

EAttributeSamplerType	AttributeSamplerPKToAAE(SParticleDeclaration::SSampler::ESamplerType type);

void					AAEToPK(SPostFXBloomDesc &in, PKSample::SParticleSceneOptions::SBloom &out);
void					AAEToPK(SPostFXDistortionDesc &in, PKSample::SParticleSceneOptions::SDistortion &out);
void					AAEToPK(SPostFXToneMappingDesc &in, PKSample::SParticleSceneOptions::SToneMapping &out);
void					AAEToPK(SRenderingDesc &in, PKSample::SParticleSceneOptions &out);

CUbyte3					ConvertSRGBToLinear(CUbyte3 v);
CUbyte3					ConvertLinearToSRGB(CUbyte3 v);

EApiValue				RHIApiToAEApi(RHI::EGraphicalApi value);
RHI::EGraphicalApi		AEApiToRHIApi(EApiValue value);

PKSample::CRHIParticleSceneRenderHelper::ERenderTargetDebug			AAEToPK(ERenderType format);
CParticleSamplerDescriptor_VectorField_Grid::EInterpolation			AAEToPK(EInterpolationType &value);

//----------------------------------------------------------------------------

inline void WCharToCString(aechar_t *input, CString *destination)
{
	CStringUnicode	tmp = CStringUnicode::FromUTF16(input);
	destination->Append(tmp.ToUTF8());
}

//----------------------------------------------------------------------------

__AEGP_PK_END

#endif // !__FX_AECONVERSION_H__
