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
#include <PK-SampleLib/ShaderGenerator/ParticleShaderGenerator.h>
#include <pk_rhi/include/interfaces/SShaderBindings.h>
#include <pk_base_object/include/hbo_object.h>

#if (PK_SAMPLE_LIB_HAS_SHADER_GENERATOR != 0)

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

struct	SShaderCompilationSettings;

//----------------------------------------------------------------------------

class	CAbstractShaderGenerator
{
public:
	CAbstractShaderGenerator() {}
	virtual ~CAbstractShaderGenerator() {}

	virtual bool	GatherShaderInfo(const CString &shaderContent, const CString &shaderDir, IFileSystem *fs) { (void)shaderContent; (void)shaderDir; (void)fs; return true; }
	CString			GenerateShader(	const CString &fileContent,
									RHI::EShaderStage stage,
									const RHI::SShaderDescription &description,
									EShaderOptions options);

private:
	virtual CString				GenDefines(const RHI::SShaderDescription &description) const = 0;
	virtual CString				GenHeader(RHI::EShaderStage stage) const = 0;
	virtual CString				GenVertexInputs(const TMemoryView<const RHI::SVertexAttributeDesc> &vertexInputs) = 0;
	virtual CString				GenVertexOutputs(const TMemoryView<const RHI::SVertexOutput> &vertexOutputs, bool toFragment) = 0;
	virtual CString				GenGeometryInputs(const TMemoryView<const RHI::SVertexOutput> &geometryInputs, const RHI::EDrawMode drawMode) = 0;
	virtual CString				GenGeometryOutputs(const RHI::SGeometryOutput &geometryOutputs) = 0;
	virtual CString				GenFragmentInputs(const TMemoryView<const RHI::SVertexOutput> &fragmentInputs) = 0;
	virtual CString				GenFragmentOutputs(const TMemoryView<const RHI::SFragmentOutput> &fragmentOutputs) = 0;
	virtual CString				GenConstantSets(const TMemoryView<const RHI::SConstantSetLayout> &constSet, RHI::EShaderStage stage) = 0;
	virtual CString				GenPushConstants(const TMemoryView<const RHI::SPushConstantBuffer> &pushConstants, RHI::EShaderStage stage) = 0;
	virtual CString				GenGroupsharedVariables(const TMemoryView<const RHI::SGroupsharedVariable> &groupsharedVariables) = 0;
	virtual CString				GenVertexMain(	const TMemoryView<const RHI::SVertexAttributeDesc> &vertexInputs,
												const TMemoryView<const RHI::SVertexOutput> &vertexOutputs,
												const TMemoryView<const CString> &funcToCall,
												bool outputClipspacePosition) = 0;
	virtual CString				GenGeometryMain(const TMemoryView<const RHI::SVertexOutput> &geometryInputs,
												const RHI::SGeometryOutput &geometryOutputs,
												const TMemoryView<const CString> &funcToCall,
												const RHI::EDrawMode primitiveType) = 0;
	virtual CString				GenFragmentMain(const TMemoryView<const RHI::SVertexOutput> &vertexOutputs,
												const TMemoryView<const RHI::SFragmentOutput> &fragmentOutputs,
												const TMemoryView<const CString> &funcToCall) = 0;

	virtual	CString				GenGeometryEmitVertex(const RHI::SGeometryOutput &geometryOutputs, bool outputClipspacePosition) = 0;
	virtual	CString				GenGeometryEndPrimitive(const RHI::SGeometryOutput &geometryOutputs) = 0;
	virtual CString				GenComputeInputs() = 0;
	virtual CString				GenComputeMain(	const CUint3						dispatchSize,
												const TMemoryView<const CString> &funcToCall) = 0;
};

//----------------------------------------------------------------------------

struct	CShaderCompilation : public CNonCopyable
{
private:
	CString								m_InputPath;			// Original shader path
	CString								m_OutputPath;			// Final shader path (after compilation)
	CString								m_ShaderPermutation;	// Every options contributing to the final hashed shader name
	RHI::EShaderStage					m_ShaderStage;
	EShaderOptions						m_ParticleOptions;
	RHI::SShaderDescription				&m_ShaderDescription;

	CString								m_ShaderMetaFilePath;
	bool								m_GenerateMetaShaderFile;

	const SShaderCompilationSettings	&m_CompilationSettings;

public:
	enum	EStage
	{
		GenerationStage,
		CompilationStage,
	};

	bool	Partial(RHI::EGraphicalApi api, EStage compilationStage, IFileSystem *controller);
	bool	Exec(RHI::EGraphicalApi api, IFileSystem *controller);
	void	Clear();

	CShaderCompilation(	const CString						&inputPath,
						const CString						&outputPath,
						const CString						&shaderPermutation,
						RHI::EShaderStage					shaderStage,
						EShaderOptions						shaderOptions,
						RHI::SShaderDescription				&shaderDesc,
						const SShaderCompilationSettings	&compilatons);
	~CShaderCompilation();

private:
	bool				GenerateShaderFile(RHI::EGraphicalApi apiName, const CString &inputPath, const CString &outputPath, IFileSystem *controller) const;
	bool				CompileShaderFile(const CString &cmdLine, const CString &inputPath, const CString &outputPath, IFileSystem *controller) const;
};

//----------------------------------------------------------------------------

struct	SShaderCompilationTargetSettings
{
	RHI::EGraphicalApi	m_Target;
	CString				m_CompilerCmdLine;
};

//----------------------------------------------------------------------------

struct	SShaderCompilationSettings
{
	TArray<SShaderCompilationTargetSettings>	m_Targets;
	CString										m_OutputFolder;
	bool										m_KeepTmpFile;
	bool										m_GenerateGeometryBBShaders;
	bool										m_GenerateVertexBBShaders;
	bool										m_ForceRecreate;

	SShaderCompilationSettings()
	:	m_KeepTmpFile(false)
	,	m_GenerateGeometryBBShaders(false)
	,	m_GenerateVertexBBShaders(false)
	,	m_ForceRecreate(false)
	{}
};

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END

#endif // (PK_SAMPLE_LIB_HAS_SHADER_GENERATOR != 0)
