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

#include <pk_rhi/include/interfaces/SShaderBindings.h>
#include <pk_rhi/include/interfaces/SRenderState.h>

#include <pk_particles/include/Renderers/ps_renderer_material.h>
#include <pk_particles/include/Renderers/ps_renderer_base.h>
#include "PK-SampleLib/ShaderGenerator/ShaderGenerator.h"
#include "PK-SampleLib/ShaderGenerator/ParticleShaderGenerator.h"
#include "PK-SampleLib/RenderIntegrationRHI/RHIGraphicResources.h"

#if (PK_SAMPLE_LIB_HAS_SHADER_GENERATOR != 0) && (PK_COMPILER_BUILD_COMPILER != 0)	// No shader compilation without graph compiler (Needs access to export nodes)
#	define	PK_MAT2RHI_CAN_COMPILE	1
#else
#	define	PK_MAT2RHI_CAN_COMPILE	0
#endif

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

#if (PK_MAT2RHI_CAN_COMPILE != 0)
// Wrapper around the fs controller (shaders compilation system issues tons of Timestamps OS requests)
// PK-Editor overrides this, default implementation just forwards the requests
// !Only works with virtual paths
class	CRHIShadersCompilationFileSystem : public CNonCopyable
{
public:
	CRHIShadersCompilationFileSystem(IFileSystem *fsController) : m_FSController(fsController) { PK_ASSERT(m_FSController != null); }

	virtual u64		FileLastModifiedTime(const CString &path) const
	{
		SFileTimes	timestamps;
		if (m_FSController->Timestamps(path, timestamps))
			return timestamps.m_LastWriteTime;
		return 0;
	}

	// note(Paul): Add getter for preprocessing shaders.
	// kind of breaks the whole point of this wrapper...
	IFileSystem		*GetFileSystem() const { return m_FSController; }

protected:
	IFileSystem		*m_FSController;
};
#endif // (PK_MAT2RHI_CAN_COMPILE != 0)

//----------------------------------------------------------------------------

namespace	MaterialToRHI
{
	enum	EFeatureDependantBindings
	{
		NeedSampleDepth				= (1 << 0),
		NeedSampleNormalRoughMetal	= (1 << 1),
		NeedSampleDiffuse			= (1 << 2),
		NeedLightingInfo			= (1 << 3),
		NeedAtlasInfo				= (1 << 4),
		NeedDitheringPattern		= (1 << 5),
	};

#if (PK_MAT2RHI_CAN_COMPILE != 0)
	struct	SCompileArg
	{
		struct	STimestamps
		{
			STimestamps() : m_Vertex(0), m_Fragment(0) { }

			u64	m_Vertex;
			u64	m_Fragment;
		};

		STimestamps								m_SourceTimestamps;
		CString									m_ShaderFolder; // Default shader folder

		TArray<SRendererFeatureFieldDefinition>	m_Fields;
		TArray<SRendererFeaturePropertyValue>	m_Properties;

		TArray<SToggledRenderingFeature>		m_FeaturesSettings;
		SMaterialSettings						m_MaterialSettings;
		ERendererClass							m_RendererType;

		CRenderPassArray						m_RenderPasses;

		TArray<EShaderOptions>					m_OptionsOverride;

		// Only used when this is initialized from a material:
		PCParticleRendererMaterial				m_Material;

		SCompileArg(const SCompileArg &oth);
		SCompileArg(const ERendererClass	rendererType,
					const CString			&shaderFolder);

		// Init from material:
		bool		InitFromRendererFeatures(const PCParticleRendererMaterial &material);
		bool		ComputeSourceShadersTimestamps(const CString &materialPath, const CRHIShadersCompilationFileSystem &fs);
		bool		SetShaderCombination(u32 combination);

		// Init from renderer:
		bool		InitFromRendererData(	const PCRendererDataBase				&renderer,
											HBO::CContext							*context);

		bool		_HasProperty(CStringId materialProperty) const;
		void		_FindParticleRenderPasses();
	};
#endif // (PK_MAT2RHI_CAN_COMPILE != 0)

	// Struct to contains generic data used by intermediate functions
	// can construct from SCompileArg and SPrepareArg
	struct	SGenericArgs
	{
		TMemoryView<const SRendererFeatureFieldDefinition>	m_FieldDefinitions;
		TMemoryView<const SToggledRenderingFeature>			m_FeaturesSettings;
		TMemoryView<const SRendererFeaturePropertyValue>	m_Properties;
		ERendererClass										m_RendererType;

		SGenericArgs(const SPrepareArg &args)
		:	m_FieldDefinitions(args.m_Renderer->m_Declaration.m_AdditionalFields)
		,	m_FeaturesSettings(args.m_FeaturesSettings)
		,	m_Properties(args.m_Renderer->m_Declaration.m_Properties)
		,	m_RendererType(args.m_Renderer->m_RendererType)
		{
		}

#if (PK_MAT2RHI_CAN_COMPILE != 0)
		SGenericArgs(const SCompileArg &args)
		:	m_FieldDefinitions(args.m_Fields)
		,	m_FeaturesSettings(args.m_FeaturesSettings)
		,	m_Properties(args.m_Properties)
		,	m_RendererType(args.m_RendererType)
		{
		}
#endif // (PK_MAT2RHI_CAN_COMPILE != 0)
	};

	struct	STextureProperty
	{
		const SRendererFeaturePropertyValue		*m_Property;
		bool									m_LoadAsSRGB;
	};

	// Find particle render passes depending on enabled features:
	void			FindParticleRenderPasses(CRenderPassArray &renderPasses, ERendererClass renderer, const FastDelegate<bool(CStringId)> &hasProperty);
	// Generic function to populate array of settings (offline: renderer = null, combination = X...) (runtime: renderer = 0x..., combination = ~0)
	bool			PopulateSettings(	const PCParticleRendererMaterial	&material,
										const ERendererClass				rendererType,
										const PCRendererDataBase			renderer,
										TArray<SToggledRenderingFeature>	*featuresSettings = null,
										SMaterialSettings					*materialSettings = null);

	// Generic function to generate constant set layout
	bool			CreateConstantSetLayout(const SGenericArgs								&args,
											const CString									&constantBuffName,
											RHI::SConstantSetLayout							&outConstSetLayout,
											TArray<STextureProperty>						*outTextureArray = null,
											TArray<SRendererFeaturePropertyValue>			*outconstantArra = null,
											CDigestMD5										*contentHash = null);

	// Generate shader bindings from PrepareArg
	bool			MaterialFrontendToShaderBindings(	const SPrepareArg					&args,
														RHI::SShaderBindings				&outShaderBindings,
														TArray<RHI::SVertexInputBufferDesc>	&outVertexInputBuffer,
														u32									&outNeededConstants,
														CDigestMD5							&outHash,
														EShaderOptions						options);

	bool			MaterialFrontendToShaderBindings(	RHI::SShaderBindings				&outShaderBindings,
														EComputeShaderType					computeShaderType);

	CString			GetHashedShaderName(const char											*pathPrefix,
										const char											*path,
										const CString										&materialName,
										const char											*specialization,
										const TMemoryView<const SToggledRenderingFeature>	&features,
										const CStringView									&permsSuffix);

	CString			RemapShaderPathNoExt(	const CString										&shaderRoot,
											const CString										&shaderPath,
											const CString										&materialName,
											RHI::EShaderStage									stage,
											ERendererClass										rendererClass,
											EShaderOptions										options,
											const TMemoryView<const SToggledRenderingFeature>	&features,
											ESampleLibGraphicResources_RenderPass				renderPass,
											CString												*outShaderPermutation = null);

#if (PK_MAT2RHI_CAN_COMPILE != 0)
	// Fill the current output for a specific render pass:
	bool			SetOutputs(	ESampleLibGraphicResources_RenderPass	renderPass,
								TArray<RHI::SFragmentOutput>			&shaderDescOutputs);

	// Generate shader description from CompileArg
	bool			MaterialFrontendToShaderDesc(	const SCompileArg						&args,
													RHI::SShaderDescription					&outShaderDesc,
													EShaderOptions							options,
													ESampleLibGraphicResources_RenderPass	renderPass = __MaxParticlePass);

	// Generate shaders from CompileArg
	bool			CompileParticleShadersFromMaterial(	const SCompileArg						&args,
														const SShaderCompilationSettings		&settings,
														HBO::CContext							*context);

	// Material Compilation
	class	CMaterialCompilation : public CRefCountedObject
	{
	private:
		const SCompileArg					&m_Args;
		const SShaderCompilationSettings	&m_Settings;
		IFileSystem							*m_FSController;

		struct	SFileCompilation
		{
			CString									m_InputPath;
			CString									m_OutputPath;
			CString									m_ShaderPermutation;
			RHI::EShaderStage						m_Stage;
			EShaderOptions							m_Option;
			ESampleLibGraphicResources_RenderPass	m_RenderPass;
			RHI::EGraphicalApi						m_Api;
			CString									m_PrecompileError;
		};
		TArray<SFileCompilation>	m_ShaderFiles;

		bool				_AddShaderFileForCompilation(	const CString							&inputPath,
															const CString							&outputPath,
															const CString							&shaderPermutation,
															RHI::EShaderStage						stage,
															ESampleLibGraphicResources_RenderPass	renderPass,
															EShaderOptions							option,
															RHI::EGraphicalApi						api);
		bool				_FileRequiresCompilation(const CString &compiledFilePath, u64 srcFileTimestamp);
		bool				_BuildShaderFilesListForOptions(TMemoryView<const EShaderOptions> options, ESampleLibGraphicResources_RenderPass renderPass);

	public:
		CMaterialCompilation(	const SCompileArg					&args,
								const SShaderCompilationSettings	&settings,
								HBO::CContext						*context);
		~CMaterialCompilation();

		bool				BuildShaderFilesList(); // Returns true if there are shader files to build
		bool				Exec() const;

		// Number of shader to compile (api independent)
		u32					ShaderCount() const { return m_ShaderFiles.Count(); }
		bool				Partial(u32 taskNb) const;
		const CString		&GetShaderName(u32 taskNb) const { return m_ShaderFiles[taskNb].m_OutputPath; }
		RHI::EShaderStage	GetShaderStage(u32 taskNb) const { return m_ShaderFiles[taskNb].m_Stage; }
		RHI::EGraphicalApi	GetApiShader(u32 taskNb) const { return m_ShaderFiles[taskNb].m_Api; }
	};
	PK_DECLARE_REFPTRCLASS(MaterialCompilation);
#endif // (PK_MAT2RHI_CAN_COMPILE != 0)
}

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
