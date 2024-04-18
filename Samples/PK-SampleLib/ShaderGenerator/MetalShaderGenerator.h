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
#include "ShaderGenerator.h"

#if (PK_SAMPLE_LIB_HAS_SHADER_GENERATOR != 0)

#include <pk_rhi/include/interfaces/SShaderBindings.h>

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

class	CMetalShaderGenerator : public CAbstractShaderGenerator
{
public:
	CMetalShaderGenerator() {}
	virtual ~CMetalShaderGenerator() {}

	virtual bool		GatherShaderInfo(const CString &shaderContent, const CString &shaderDir, IFileSystem *fs) override;

private:
	virtual CString		GenDefines(const RHI::SShaderDescription &description) const override final;
	virtual CString		GenHeader(RHI::EShaderStage stage) const override;
	virtual CString		GenVertexInputs(const TMemoryView<const RHI::SVertexAttributeDesc> &vertexInputs) override;
	virtual CString		GenVertexOutputs(const TMemoryView<const RHI::SVertexOutput> &vertexOutputs, bool toFragment) override;
	virtual CString		GenGeometryInputs(const TMemoryView<const RHI::SVertexOutput> &geometryInputs, const RHI::EDrawMode drawMode) override final;
	virtual CString		GenGeometryOutputs(const RHI::SGeometryOutput &geometryOutputs) override final;
	virtual CString		GenFragmentInputs(const TMemoryView<const RHI::SVertexOutput> &fragmentInputs) override;
	virtual CString		GenFragmentOutputs(const TMemoryView<const RHI::SFragmentOutput> &fragmentOutputs) override;
	virtual CString		GenConstantSets(const TMemoryView<const RHI::SConstantSetLayout> &constSet, RHI::EShaderStage stage) override;
	virtual CString		GenPushConstants(const TMemoryView<const RHI::SPushConstantBuffer> &pushConstants, RHI::EShaderStage stage) override;
	virtual CString		GenGroupsharedVariables(const TMemoryView<const RHI::SGroupsharedVariable> &groupsharedVariables) override;

	virtual CString		GenVertexMain(	const TMemoryView<const RHI::SVertexAttributeDesc> &vertexInputs,
										const TMemoryView<const RHI::SVertexOutput> &vertexOutputs,
										const TMemoryView<const CString> &funcToCall,
										bool outputClipspacePosition) override;
	virtual CString		GenGeometryMain(const TMemoryView<const RHI::SVertexOutput> &geometryInputs,
										const RHI::SGeometryOutput &geometryOutputs,
										const TMemoryView<const CString> &funcToCall,
										const RHI::EDrawMode primitiveType) override final;
	virtual CString		GenFragmentMain(const TMemoryView<const RHI::SVertexOutput> &vertexOutputs,
										const TMemoryView<const RHI::SFragmentOutput> &fragmentOutputs,
										const TMemoryView<const CString> &funcToCall) override;
	virtual CString		GenGeometryEmitVertex(const RHI::SGeometryOutput &geometryOutputs, bool outputClipspacePosition) override final;
	virtual CString		GenGeometryEndPrimitive(const RHI::SGeometryOutput &geometryOutputs) override final;
	virtual CString		GenComputeInputs() override final;
	virtual CString		GenComputeMain(	const CUint3						dispatchSize,
										const TMemoryView<const CString>	&funcToCall) override final;

private:
	bool				_FindAllTextureFunctionParameters(const CString &shaderCode);
	bool				_FindMacroParametersInShader(const CString &shaderCode, const CStringView &macro);

	// Textures passed as parameter to functions:
	TArray<CString>		m_TextureParameters;
};

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END

#endif	// (PK_SAMPLE_LIB_HAS_SHADER_GENERATOR != 0)
