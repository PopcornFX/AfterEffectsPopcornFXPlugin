#pragma once

//----------------------------------------------------------------------------
// This program is the property of Persistant Studios SARL.
//
// You may not redistribute it and/or modify it under any conditions
// without written permission from Persistant Studios SARL, unless
// otherwise stated in the latest Persistant Studios Code License.
//
// See the Persistant Studios Code License for further details.
//----------------------------------------------------------------------------

#include <PK-SampleLib/PKSample.h>
#include <pk_rhi/include/Enums.h>
#include <pk_rhi/include/interfaces/SShaderBindings.h>

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

struct		SShaderCombination
{
	CString								m_VertexShader;
	CString								m_GeometryShader;
	CString								m_FragmentShader;
	CString								m_ComputeShader;
	TArray<RHI::SShaderDescription>		m_ShaderDescriptions;
};

//----------------------------------------------------------------------------

void		CreateShaderDefinitions(TArray<SShaderCombination> &shaders);
void		GenerateDefinesFromDefinition(TArray<CString> &defines, const RHI::SShaderDescription &desc, RHI::EShaderStage stage);

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
