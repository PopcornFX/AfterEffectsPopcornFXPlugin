//----------------------------------------------------------------------------
// This program is the property of Persistant Studios SARL.
//
// You may not redistribute it and/or modify it under any conditions
// without written permission from Persistant Studios SARL, unless
// otherwise stated in the latest Persistant Studios Code License.
//
// See the Persistant Studios Code License for further details.
//----------------------------------------------------------------------------

#include "precompiled.h"

#include "RenderIntegrationRHI/RHIGraphicResources.h"
#include "RenderIntegrationRHI/MaterialToRHI.h"

#include "PK-SampleLib/SampleUtils.h"
#include "pk_render_helpers/include/frame_collector/rh_particle_render_data_factory.h"
#include "pk_render_helpers/include/render_features/rh_features_basic.h"
#include "pk_render_helpers/include/render_features/rh_features_vat_static.h"

#include <pk_kernel/include/kr_resources.h>
#include <pk_geometrics/include/ge_mesh_resource.h>
#include <pk_rhi/include/ShaderConstantBindingGenerator.h>
#include <pk_rhi/include/PixelFormatFallbacks.h>
#include <PK-SampleLib/RenderPasses/GBuffer.h>
#include <PK-SampleLib/RenderPasses/PostFxDistortion.h>

#include <pk_discretizers/include/dc_discretizers.h>
#include <pk_geometrics/include/ge_mesh_vertex_declarations.h>

#include <pk_maths/include/pk_maths_simd.h>

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------
//
//	SPrepareArg
//
//----------------------------------------------------------------------------

SPrepareArg::SPrepareArg(const PCRendererDataBase &renderer, HBO::CContext *context, const RHI::SGPUCaps *gpuCaps)
:	m_ResourceManager(null)
,	m_Renderer(renderer)
{
	if (gpuCaps != null)
		m_GpuCaps = *gpuCaps;

	if (renderer != null)
	{
		PK_ASSERT(context != null);
		m_ResourceManager = context->ResourceManager();

		const CString	&materialPath = renderer->m_Declaration.m_MaterialPath;

		bool			fileWasLoaded = false;
		PBaseObjectFile	materialFile = context->LoadFile(materialPath, false, &fileWasLoaded);
		if (materialFile != null)
		{
			PParticleRendererMaterial	material = materialFile->FindFirstOf<CParticleRendererMaterial>();
			if (PK_VERIFY(material != null) &&
				PK_VERIFY(material->Initialize()))
			{
				// Note(Paul): This should not be in a ctor: cannot properly handle error!
				PK_VERIFY(MaterialToRHI::PopulateSettings(material, renderer->m_RendererType, renderer, &m_FeaturesSettings, &m_MaterialSettings));

				// release refptr here otherwise if kept after file->Unload(), will underflow strong ref if the file has been baked as binary
				// (object memory aliases the loaded file buffer memory, so they get force-destroyed when the file is unloaded even if external refs are lying around)
				material = null;
			}

			if (!fileWasLoaded)
				materialFile->Unload();
			materialFile = null;
		}
		MaterialToRHI::FindParticleRenderPasses(m_RenderPasses, renderer->m_RendererType, FastDelegate<bool(CStringId)>(this, &SPrepareArg::_HasProperty));
	}
}

//----------------------------------------------------------------------------

bool	SPrepareArg::_HasProperty(CStringId materialProperty) const
{
	const SRendererFeaturePropertyValue	*propertyPtr = m_Renderer->m_Declaration.FindProperty(materialProperty);
	return propertyPtr != null && (propertyPtr->m_Type != PropertyType_Feature || propertyPtr->ValueB());
}

//----------------------------------------------------------------------------
//
//	STextureKey
//
//----------------------------------------------------------------------------

bool	STextureKey::UpdateThread_Prepare(const SPrepareArg &args)
{
	(void)args;
	return true;
}

//----------------------------------------------------------------------------

RHI::PTexture	STextureKey::RenderThread_CreateResource(const SCreateArg &args)
{
	if (m_Path.Empty() || args.m_ResourceManagers.Empty())
		return null;

	TResourcePtr<CImage>	image;
	for (auto resourceManager : args.m_ResourceManagers)
	{
		image = resourceManager->Load<CImage>(m_Path, false, SResourceLoadCtl(false, true));
		if ((image != null) && !image->Empty()) break;
	}

	if (image == null || image->Empty())
	{
		CLog::Log(PK_ERROR, "Could not load image \"%s\"", m_Path.Data());
		return null;
	}

	RHI::PTexture	texture = RHI::PixelFormatFallbacks::CreateTextureAndFallbackIFN(args.m_ApiManager, *image, m_LoadAsSRGB, m_Path.Data());

	if (texture == null)
	{
		CLog::Log(PK_ERROR, "Could not create RHI texture for image \"%s\"", m_Path.Data());
		return null;
	}
	return texture;
}

//----------------------------------------------------------------------------

RHI::PTexture	STextureKey::RenderThread_ReloadResource(const SCreateArg &args)
{
	return RenderThread_CreateResource(args);
}

//----------------------------------------------------------------------------

bool	STextureKey::operator == (const STextureKey &oth) const
{
	return m_Path == oth.m_Path;
}

//----------------------------------------------------------------------------

void	STextureKey::SetupDefaultResource()
{
	SPrepareArg		args;
	STextureKey		key;

	key.m_Path = "Textures/default.dds";

	s_DefaultResourceID = CTextureManager::UpdateThread_GetResource(key, args);
}

//----------------------------------------------------------------------------

CTextureManager::CResourceId	STextureKey::s_DefaultResourceID;

//----------------------------------------------------------------------------
//
//	SConstantSamplerKey
//
//----------------------------------------------------------------------------

bool	SConstantSamplerKey::UpdateThread_Prepare(const SPrepareArg &args)
{
	(void)args;
	return true;
}

//----------------------------------------------------------------------------

RHI::PConstantSampler	SConstantSamplerKey::RenderThread_CreateResource(const SCreateArg &args)
{
	RHI::PConstantSampler	sampler = args.m_ApiManager->CreateConstantSampler(RHI::SRHIResourceInfos("PK-RHI Sampler"), m_MagFilter, m_MinFilter, m_WrapU, m_WrapV, m_WrapW, m_MipmapCount);
	if (sampler == null)
	{
		CLog::Log(PK_ERROR, "Could not create constant sampler");
		return null;
	}
	return sampler;
}

//----------------------------------------------------------------------------

bool	SConstantSamplerKey::operator == (const SConstantSamplerKey &other) const
{
	return	m_MagFilter == other.m_MagFilter &&
			m_MinFilter == other.m_MinFilter &&
			m_WrapU == other.m_WrapU &&
			m_WrapV == other.m_WrapV &&
			m_WrapW == other.m_WrapW &&
			m_MipmapCount == other.m_MipmapCount;
}

//----------------------------------------------------------------------------
//
//	SPassDescription
//
//----------------------------------------------------------------------------

const CStringView	s_ParticleRenderPassNames[] =
{
	"ParticlePass_Opaque",
	"ParticlePass_Decal",
	"ParticlePass_Lighting",
	"ParticlePass_Transparent",
	"ParticlePass_Distortion",
	"ParticlePass_Tint",
	"ParticlePass_TransparentPostDisto",
	"ParticlePass_Debug",
	"ParticlePass_Compositing",
	"ParticlePass_OpaqueShadow"
};
PK_STATIC_ASSERT(PK_ARRAY_COUNT(s_ParticleRenderPassNames) == __MaxParticlePass);

TArray<SPassDescription>	SPassDescription::s_PassDescriptions;

//----------------------------------------------------------------------------

namespace
{
	const RHI::EPixelFormat	_OpaqueFormats[] =
	{
		RHI::EPixelFormat::FormatFloat16RGBA,						// GBuffer diffuse HDR
		RHI::EPixelFormat::FormatFloat32R,							// Depth solid
		RHI::EPixelFormat::FormatFloat16RGBA,						// Emissive HDR
		PKSample::CGBuffer::s_NormalRoughMetalBufferFormat,			// GBuffer normal spec
	};

	const RHI::EPixelFormat	_OpaqueFormatsNoDepth[] =
	{
		RHI::EPixelFormat::FormatFloat16RGBA,						// GBuffer diffuse HDR
		RHI::EPixelFormat::FormatFloat16RGBA,						// Emissive HDR
		PKSample::CGBuffer::s_NormalRoughMetalBufferFormat,			// GBuffer normal spec
	};

	const RHI::EPixelFormat	_LightingFormats[] =
	{
		PKSample::CGBuffer::s_LightAccumBufferFormat,				// Light accumulation buffer
	};

	const RHI::EPixelFormat	_DistortionFormats[] =
	{
		PKSample::CPostFxDistortion::s_DistortionBufferFormat,		// Distortion map
	};

	const RHI::EPixelFormat	_PostMergingFormats[] =
	{
		PKSample::CGBuffer::s_MergeBufferFormat,					// Merging buffer format
	};

	const RHI::EPixelFormat	_CompositingFormats[] =
	{
		RHI::FormatUint8RGBA,										// Swapchain format
	};

	const RHI::EPixelFormat	_OpaqueShadowFormats[] = {
		IDEAL_SHADOW_DEPTH_FORMAT,									// Shadow map
	};
}

//----------------------------------------------------------------------------

const TMemoryView<const RHI::EPixelFormat>	SPassDescription::s_PassOutputsFormats[__MaxParticlePass] =
{
	_OpaqueFormats,
	_OpaqueFormatsNoDepth,
	_LightingFormats,
	_PostMergingFormats,	// additive in merging buffer
	_DistortionFormats,
	_PostMergingFormats,
	_PostMergingFormats,
	_PostMergingFormats,	// debug in merging buffer at post bloom
	_CompositingFormats,
	_OpaqueShadowFormats	// variance shadow maps format
};

//----------------------------------------------------------------------------

// GBuffer only formats
PK_STATIC_ASSERT(SPassDescription::__GBufferRTCount == PK_ARRAY_COUNT(_OpaqueFormats) - 1);
const TMemoryView<const RHI::EPixelFormat>	SPassDescription::s_GBufferDefaultFormats = TMemoryView<const RHI::EPixelFormat>(&_OpaqueFormats[0], PK_ARRAY_COUNT(_OpaqueFormats) - 1);

//----------------------------------------------------------------------------
//
//	SRenderStateKey
//
//----------------------------------------------------------------------------

PK_NOINLINE bool	SRenderStateKey::UpdateThread_Prepare(const SPrepareArg &args)
{
	// Create the shader bindings:
	if (!MaterialToRHI::MaterialFrontendToShaderBindings(args, m_ShaderBindings, m_InputVertexBuffers, m_NeededConstants, m_RenderStateHash, m_Options))
	{
		CLog::Log(PK_ERROR, "MaterialToRHI::MaterialFrontendToShaderBindings failed");
		return false;
	}
	// We generate dummy bindings for the shaders:
	RHI::ShaderConstantBindingGenerator::GenerateBindingsForApi(RHI::GApi_OpenGL, m_ShaderBindings);
	for (u32 i = 0; i < RHI::ShaderStage_Count; ++i)
		m_ShaderBindings.Hash(static_cast<RHI::EShaderStage>(i));
	// Reset the bindings after the hash:
	RHI::ShaderConstantBindingGenerator::ResetBindings(m_ShaderBindings);

	// Create the pipeline state:
	m_PipelineState.m_DynamicScissor = true;
	m_PipelineState.m_DynamicViewport = true;

	m_PipelineState.m_ColorBlendingEquation = RHI::BlendAdd;
	m_PipelineState.m_AlphaBlendingEquation = RHI::BlendAdd;
	m_PipelineState.m_Blending = true;
	m_PipelineState.m_CullMode = RHI::NoCulling;

	ERendererClass		rendererType = args.m_Renderer->m_RendererType;

	if (rendererType == Renderer_Mesh)
	{
		bool	doubleSided = true; // Keep culling off by default
		if (args.m_Renderer->m_Declaration.IsFeatureEnabled(BasicRendererProperties::SID_Culling()))
			doubleSided = args.m_Renderer->m_Declaration.GetPropertyValue_B(BasicRendererProperties::SID_Culling_DoubleSided(), true);

		// For backward compatibility purposes, if the property is not present on the renderer, act as if the feature is enabled.
		m_PipelineState.m_CullMode = doubleSided ? RHI::NoCulling : RHI::CullBackFaces;
	}

	if (rendererType == Renderer_Light || rendererType == Renderer_Decal || rendererType == Renderer_Mesh || rendererType == Renderer_Triangle)
		m_PipelineState.m_PolyOrder = (CCoordinateFrame::IsRightHanded()) ? RHI::FrontFaceCounterClockWise : RHI::FrontFaceClockWise;

	// Same principle for the lights and the decals, we just want to rasterize the geometry inside the light sphere / decal bbox
	if (rendererType == Renderer_Light || rendererType == Renderer_Decal)
	{
		// We render the backface with a >= depth test (we light everything in front of the backfaces)
		m_PipelineState.m_DepthWrite = false;
		m_PipelineState.m_DepthClamp = true; // We disable the depth clip space to avoid having the light-spheres culled by the far-plane

		m_PipelineState.m_DepthTest = RHI::GreaterOrEqual;
		m_PipelineState.m_CullMode = RHI::CullFrontFaces;
		if (rendererType == Renderer_Decal)
		{
			// Alpha blend for decals:
			m_PipelineState.m_ColorBlendingSrc = RHI::BlendOne;
			m_PipelineState.m_ColorBlendingDst = RHI::BlendOneMinusSrcAlpha;
			m_PipelineState.m_AlphaBlendingSrc = RHI::BlendOne;
			m_PipelineState.m_AlphaBlendingDst = RHI::BlendOneMinusSrcAlpha;
		}
		else
		{
			// Additive blend for light accu buffer:
			m_PipelineState.m_ColorBlendingSrc = RHI::BlendOne;
			m_PipelineState.m_ColorBlendingDst = RHI::BlendOne;
			m_PipelineState.m_AlphaBlendingSrc = RHI::BlendZero;
			m_PipelineState.m_AlphaBlendingDst = RHI::BlendOne;
		}
	}
	else
	{
		m_PipelineState.m_DepthTest = RHI::Less;

		ESampleLibGraphicResources_BlendMode	blendmode = ParticleBlend_AlphaBlendAdditive;
		const SRendererFeaturePropertyValue		*transparentType = args.m_Renderer->m_Declaration.FindProperty(BasicRendererProperties::SID_Transparent_Type());

		if (m_RenderPassIdx == ParticlePass_Distortion)
		{
			blendmode = ParticleBlend_Additive;
		}
		else if (m_RenderPassIdx == ParticlePass_TransparentPostDisto)
		{
			blendmode = ParticleBlend_AlphaBlendAdditive;
		}
		else if (m_RenderPassIdx == ParticlePass_Tint)
		{
			blendmode = ParticleBlend_Multiply;
		}
		else if (	m_RenderPassIdx == ParticlePass_Opaque ||
					m_RenderPassIdx == ParticlePass_OpaqueShadow)
		{
			blendmode = ParticleBlend_Opaque;
		}
		else if (transparentType != null)
		{
			switch (transparentType->ValueI().x())
			{
			default:
			case 0:
				blendmode = ParticleBlend_Additive; break;
			case 1:
				blendmode = ParticleBlend_AdditiveNoAlpha; break;
			case 2:
				blendmode = ParticleBlend_AlphaBlend; break;
			case 3:
				blendmode = ParticleBlend_AlphaBlendAdditive; break;
			}
		}

		switch (blendmode)
		{
		default:
		case ParticleBlend_Additive:
		case ParticleBlend_Distortion:
			m_PipelineState.m_ColorBlendingSrc = RHI::BlendSrcAlpha;
			m_PipelineState.m_ColorBlendingDst = RHI::BlendOne;
			m_PipelineState.m_AlphaBlendingSrc = RHI::BlendOne;
			m_PipelineState.m_AlphaBlendingDst = RHI::BlendOne;
			break;
		case ParticleBlend_AdditiveNoAlpha:
			m_PipelineState.m_ColorBlendingSrc = RHI::BlendOne;
			m_PipelineState.m_ColorBlendingDst = RHI::BlendOne;
			m_PipelineState.m_AlphaBlendingSrc = RHI::BlendZero;
			m_PipelineState.m_AlphaBlendingDst = RHI::BlendOne;
			break;
		case ParticleBlend_AlphaBlend:
			m_PipelineState.m_ColorBlendingSrc = RHI::BlendSrcAlpha;
			m_PipelineState.m_ColorBlendingDst = RHI::BlendOneMinusSrcAlpha;
			m_PipelineState.m_AlphaBlendingSrc = RHI::BlendOne;
			m_PipelineState.m_AlphaBlendingDst = RHI::BlendOne;
			break;
		case ParticleBlend_AlphaBlendAdditive:
			m_PipelineState.m_ColorBlendingSrc = RHI::BlendOne;
			m_PipelineState.m_ColorBlendingDst = RHI::BlendOneMinusSrcAlpha;
			m_PipelineState.m_AlphaBlendingSrc = RHI::BlendOne;
			m_PipelineState.m_AlphaBlendingDst = RHI::BlendOne;
			break;
		case ParticleBlend_Multiply:
			m_PipelineState.m_ColorBlendingSrc = RHI::BlendDstColor;
			m_PipelineState.m_ColorBlendingDst = RHI::BlendZero;
			m_PipelineState.m_AlphaBlendingSrc = RHI::BlendOne;
			m_PipelineState.m_AlphaBlendingDst = RHI::BlendOne;
			break;
		case ParticleBlend_Opaque:
			m_PipelineState.m_DepthWrite = true;
			m_PipelineState.m_Blending = false;
			break;
		}

		if (m_Options & Option_GeomBillboarding)
		{
			m_PipelineState.m_DrawMode = RHI::DrawModePoint;
		}
	}

	// Create the shader program:
	SShaderProgramKey	shaderProgKey;
	shaderProgKey.m_Option = m_Options;
	shaderProgKey.m_RenderPassIdx = m_RenderPassIdx;
	m_ShaderProgram = CShaderProgramManager::UpdateThread_GetResource(shaderProgKey, args);
	if (!m_ShaderProgram.m_ID.Valid())
	{
		CLog::Log(PK_ERROR, "Could not get shader program");
		return false;
	}
	return true;
}

//----------------------------------------------------------------------------

RHI::PRenderState	SRenderStateKey::RenderThread_CreateResource(const SCreateArg &args)
{
	RHI::PRenderState	renderState = args.m_ApiManager->CreateRenderState(RHI::SRHIResourceInfos("PK-RHI Render State"));
	if (renderState != null)
	{
		RHI::SRenderState	&desc = renderState->m_RenderState;
		desc.m_PipelineState = m_PipelineState;
		desc.m_ShaderProgram = CShaderProgramManager::RenderThread_ResolveResource(m_ShaderProgram, args);

#if		defined(PK_DEBUG)
		CString		vtxShaderPath, fragShaderPath, geomShaderPath;

		const SShaderProgramKey &progKey = CShaderProgramManager::RenderThread_GetKey(m_ShaderProgram);

		const SShaderModuleKey	&vtxKey = CShaderModuleManager::RenderThread_GetKey(progKey.m_ShaderModules[RHI::VertexShaderStage]);
		vtxShaderPath = vtxKey.m_Path;

		const SShaderModuleKey	&fragKey = CShaderModuleManager::RenderThread_GetKey(progKey.m_ShaderModules[RHI::FragmentShaderStage]);
		fragShaderPath = fragKey.m_Path;

		if (progKey.m_ShaderModules[RHI::GeometryShaderStage].Valid())
		{
			const SShaderModuleKey	&geomKey = CShaderModuleManager::RenderThread_GetKey(progKey.m_ShaderModules[RHI::GeometryShaderStage]);
			geomShaderPath = geomKey.m_Path;
		}
#endif

		if (desc.m_ShaderProgram == null)
		{
			CLog::Log(PK_ERROR, "Could not create shader program");
			return null;
		}
		desc.m_InputVertexBuffers = m_InputVertexBuffers;
		desc.m_ShaderBindings = m_ShaderBindings;
		// Generate the bindings for the Api:
		RHI::ShaderConstantBindingGenerator::GenerateBindingsForApi(args.m_ApiManager->ApiName(), desc.m_ShaderBindings);
		if (!PK_VERIFY(u32(m_RenderPassIdx) < SPassDescription::s_PassDescriptions.Count()))
		{
			CLog::Log(PK_ERROR, "The render state uses a render pass which is out of bound. It will be ignored.");
			return null;
		}
		SPassDescription	&passDesc = SPassDescription::s_PassDescriptions[m_RenderPassIdx];

		if (passDesc.m_RenderPass == null)
		{
			CLog::Log(PK_ERROR, "The render state uses a render pass which is not in the SPassDescription::s_PassDescriptions array.");
			return null;
		}

		if (!args.m_ApiManager->BakeRenderState(renderState, passDesc.m_FrameBufferLayout, passDesc.m_RenderPass, passDesc.m_SubPassIdx))
		{
#if		defined(PK_DEBUG)
			CLog::Log(PK_ERROR, "Could not bake render state for shaders \"%s\" \"%s\" \"%s\"", vtxShaderPath.Data(), geomShaderPath.Data(), fragShaderPath.Data());
#endif
			return null;
		}
	}
	return renderState;
}

//----------------------------------------------------------------------------

void	SRenderStateKey::UpdateThread_ReleaseDependencies()
{
	if (m_ShaderProgram.Valid())
	{
		CShaderProgramManager::UpdateThread_ReleaseResource(m_ShaderProgram);
	}
}

//----------------------------------------------------------------------------

bool	SRenderStateKey::operator == (const SRenderStateKey &oth) const
{
	return	m_PipelineState == oth.m_PipelineState &&
			m_InputVertexBuffers == oth.m_InputVertexBuffers &&
			m_RenderPassIdx == oth.m_RenderPassIdx &&
			m_ShaderProgram == oth.m_ShaderProgram &&
			m_RenderStateHash == oth.m_RenderStateHash;
}

//----------------------------------------------------------------------------
//
//	SComputeStateKey
//
//----------------------------------------------------------------------------

PK_NOINLINE bool	SComputeStateKey::UpdateThread_Prepare(const SPrepareArg &args)
{
	// Create the shader bindings:
	if (!MaterialToRHI::MaterialFrontendToShaderBindings(m_ShaderBindings, m_Type))
	{
		CLog::Log(PK_ERROR, "MaterialToRHI::MaterialFrontendToShaderBindings failed");
		return false;
	}

	PK_ASSERT(args.m_ApiName != RHI::EGraphicalApi::GApi_Null);
	RHI::ShaderConstantBindingGenerator::GenerateBindingsForApi(args.m_ApiName, m_ShaderBindings);

	// Create the shader program:
	SShaderProgramKey	shaderProgKey = SShaderProgramKey();
	shaderProgKey.m_ComputeShaderType = m_Type;
	shaderProgKey.m_ShaderBindings = m_ShaderBindings;
	m_ShaderProgram = CShaderProgramManager::UpdateThread_GetResource(shaderProgKey, args);

	if (!m_ShaderProgram.m_ID.Valid())
	{
		CLog::Log(PK_ERROR, "Could not get shader program");
		return false;
	}

	return true;
}

//----------------------------------------------------------------------------

CStringView	_GetComputeShaderPath(EComputeShaderType computeShaderType)
{
	switch (computeShaderType)
	{
	case ComputeType_ComputeParticleCountPerMesh:
		return "ComputeParticleCountPerMesh.comp";
	case ComputeType_ComputeParticleCountPerMesh_MeshAtlas:
		return "ComputeParticleCountPerMesh.comp";
	case ComputeType_ComputeParticleCountPerMesh_LOD:
		return "ComputeParticleCountPerMesh.comp";
	case ComputeType_ComputeParticleCountPerMesh_LOD_MeshAtlas:
		return "ComputeParticleCountPerMesh.comp";
	case ComputeType_InitIndirectionOffsetsBuffer:
		return "InitIndirectionOffsetsBuffer.comp";
	case ComputeType_InitIndirectionOffsetsBuffer_LODNoAtlas:
		return "InitIndirectionOffsetsBuffer.comp";
	case ComputeType_ComputeMeshIndirectionBuffer:
		return "ComputeMeshIndirectionBuffer.comp";
	case ComputeType_ComputeMeshIndirectionBuffer_MeshAtlas:
		return "ComputeMeshIndirectionBuffer.comp";
	case ComputeType_ComputeMeshIndirectionBuffer_LOD:
		return "ComputeMeshIndirectionBuffer.comp";
	case ComputeType_ComputeMeshIndirectionBuffer_LOD_MeshAtlas:
		return "ComputeMeshIndirectionBuffer.comp";
	case ComputeType_ComputeMeshMatrices:
		return "ComputeMeshMatrices.comp";
	case ComputeType_ComputeSortKeys:
		return "ComputeSortKeys.comp";
	case ComputeType_ComputeSortKeys_CameraDistance:
		return "ComputeSortKeys.comp";
	case ComputeType_ComputeSortKeys_RibbonIndirection:
		return "ComputeSortKeys.comp";
	case ComputeType_ComputeSortKeys_CameraDistance_RibbonIndirection:
		return "ComputeSortKeys.comp";
	case ComputeType_SortUpSweep:
		return "SortUpSweep.comp";
	case ComputeType_SortUpSweep_KeyStride64:
		return "SortUpSweep.comp";
	case ComputeType_SortPrefixSum:
		return "SortPrefixSum.comp";
	case ComputeType_SortDownSweep:
		return "SortDownSweep.comp";
	case ComputeType_SortDownSweep_KeyStride64:
		return "SortDownSweep.comp";
	case ComputeType_ComputeRibbonSortKeys:
		return "ComputeRibbonSortKeys.comp";
	default:
		PK_ASSERT_NOT_REACHED();
	}
	return CStringView();
}

//----------------------------------------------------------------------------

RHI::PComputeState	SComputeStateKey::RenderThread_CreateResource(const SCreateArg &args)
{
	RHI::PComputeState	computeState = args.m_ApiManager->CreateComputeState(RHI::SRHIResourceInfos("PK-RHI Compute State"));
	if (computeState != null)
	{
		RHI::SComputeState	&desc = computeState->m_ComputeState;
		desc.m_ShaderProgram = CShaderProgramManager::RenderThread_ResolveResource(m_ShaderProgram, args);

#if		defined(PK_DEBUG)
		CString		compShaderPath;

		const SShaderProgramKey &progKey = CShaderProgramManager::RenderThread_GetKey(m_ShaderProgram);

		const SShaderModuleKey	&compKey = CShaderModuleManager::RenderThread_GetKey(progKey.m_ShaderModules[RHI::ComputeShaderStage]);
		compShaderPath = compKey.m_Path;
#endif

		if (desc.m_ShaderProgram == null)
		{
			CLog::Log(PK_ERROR, "Could not create shader program");
			return null;
		}
		desc.m_ShaderBindings = m_ShaderBindings;
		// Generate the bindings for the Api:
		RHI::ShaderConstantBindingGenerator::GenerateBindingsForApi(args.m_ApiManager->ApiName(), desc.m_ShaderBindings);

		if (!args.m_ApiManager->BakeComputeState(computeState))
		{
#if		defined(PK_DEBUG)
			CLog::Log(PK_ERROR, "Could not bake compute state for shader \"%s\"", compShaderPath.Data());
#endif
			return null;
		}
	}
	return computeState;
}

//----------------------------------------------------------------------------

void	SComputeStateKey::UpdateThread_ReleaseDependencies()
{
	if (m_ShaderProgram.Valid())
	{
		CShaderProgramManager::UpdateThread_ReleaseResource(m_ShaderProgram);
	}
}

//----------------------------------------------------------------------------

bool	SComputeStateKey::operator == (const SComputeStateKey &oth) const
{
	return m_Type == oth.m_Type &&
		   m_ShaderProgram == oth.m_ShaderProgram;
}

//----------------------------------------------------------------------------
//
//	SShaderModuleKey
//
//----------------------------------------------------------------------------

bool	SShaderModuleKey::UpdateThread_Prepare(const SPrepareArg &args)
{
	(void)args;
	return true;
}

//----------------------------------------------------------------------------

RHI::PShaderModule	SShaderModuleKey::RenderThread_CreateResource(const SCreateArg &args)
{
	if (m_Path.Empty())
		return null;

	TArray<IFileSystem*> controllers;
	if (args.m_ResourceManagers.Empty())
	{
		controllers.PushBack(File::DefaultFileSystem());
	}
	else
	{
		for (u32 i = 0; i < args.m_ResourceManagers.Count(); i++)
		{
			controllers.PushBack (args.m_ResourceManagers[i]->FileController());
		}
	}

	PRefCountedMemoryBuffer	byteCode;
	RHI::EGraphicalApi		graphicalApi = args.m_ApiManager->ApiName();
	const CString			ext = GetShaderExtensionStringFromApi(graphicalApi);
#ifndef PK_RETAIL
	const CString			shaderDebugName = CFilePath::ExtractFilename(m_Path);
#else
	const CString			shaderDebugName;
#endif // ifndef PK_RETAIL
	const CString			apiShaderPath = m_Path + ext;
	bool					precompiledExists = true;

	{
		CLog::Log(PK_INFO, "Loading shader %s", apiShaderPath.Data());
		PFileStream		fileView;
		for (auto controller : controllers)
		{
			precompiledExists = true;
			fileView = controller->OpenStream(apiShaderPath, IFileSystem::Access_Read);
			if (fileView == null)
			{
				precompiledExists = false;

				// Metal: we want to keep the precompiled shader path for static editor shaders and samples but compile from source at runtime in editor.
				if (graphicalApi == RHI::GApi_Metal)
				{
					const CString	apiShaderSourcePath = m_Path + GetShaderExtensionStringFromApi(graphicalApi, true);

					CLog::Log(PK_INFO, "Could not find precompiled shader '%s', will try to fallback on source '%s'", apiShaderPath.Data(), apiShaderSourcePath.Data());

					fileView = controller->OpenStream(apiShaderSourcePath, IFileSystem::Access_Read);
					if (fileView == null)
						fileView = controller->OpenStream(apiShaderSourcePath, IFileSystem::Access_Read, true);
					if (fileView != null)
						CLog::Log(PK_INFO, "Successfully loaded source file \"%s\"", apiShaderSourcePath.Data());
					else
					{
						CLog::Log(PK_ERROR, "Could not load source shader file \"%s\"", apiShaderSourcePath.Data());
						return null;
					}
				}
			}
			if (fileView != null)
				break;
		}

		if (fileView == null)
		{
			CLog::Log(PK_ERROR, "Could not load shader file \"%s\"", apiShaderPath.Data());
			return null;
		}

		byteCode = fileView->BufferizeToRefCountedMemoryBuffer();
		fileView->Close();
	}
	if (byteCode == null)
		return null;

	RHI::PShaderModule	module = args.m_ApiManager->CreateShaderModule(RHI::SRHIResourceInfos(shaderDebugName));
	if (!PK_VERIFY(module != null))
		return null;

	bool	result;
	if (args.m_ApiManager->ApiDesc().m_SupportPrecompiledShader && precompiledExists)
		result = module->LoadFromPrecompiled(byteCode, m_Stage);
	else
		result = module->CompileFromCode(byteCode->Data<char>(), byteCode->DataSizeInBytes(), m_Stage);

	if (!result)
	{
		CLog::Log(PK_ERROR, "Could not create shader program from loaded shader file '%s': '%s'", m_Path.Data(), module->GetShaderModuleCreationInfo());

		if (!args.m_ApiManager->ApiDesc().m_SupportPrecompiledShader)
		{
			CString				shaderCode = CString(byteCode->Data<char>(), byteCode->DataSizeInBytes());
			TArray<CString>		lines;

			shaderCode.Split('\n', lines);

			CLog::Log(PK_ERROR, "Shader code:");
			for (u32 i = 0; i < lines.Count(); ++i)
			{
				CLog::Log(PK_ERROR, "[%u]%s", i, lines[i].Data());
			}
		}
		return null;
	}
	return module;
}

//----------------------------------------------------------------------------

RHI::PShaderModule	SShaderModuleKey::RenderThread_ReloadResource(const SCreateArg &args)
{
	return RenderThread_CreateResource(args);
}

//----------------------------------------------------------------------------

bool	SShaderModuleKey::operator == (const SShaderModuleKey &other) const
{
	return	m_Path == other.m_Path && m_Stage == other.m_Stage;
}

//----------------------------------------------------------------------------

PK_NOINLINE bool	SShaderProgramKey::UpdateThread_Prepare(const SPrepareArg &args)
{
	RHI::EShaderStagePipeline	stagepipeline = ParticleShaderGenerator::GetShaderStagePipeline(m_Option);
	if (m_ComputeShaderType > 0)
		stagepipeline = RHI::EShaderStagePipeline::Cs;

	if (stagepipeline != RHI::EShaderStagePipeline::Cs)
	{
		const CString					&materialName = args.m_MaterialSettings.m_MaterialName;
		const CString					&vertexShader = args.m_MaterialSettings.m_VertexShaderPath;
		const CString					&fragmentShader = args.m_MaterialSettings.m_FragmentShaderPath.Empty() ? CString::Format("%s.frag", materialName.Data()) : args.m_MaterialSettings.m_FragmentShaderPath;

		// Vertex module
		if (stagepipeline & RHI::VertexShaderMask)
		{
			SShaderModuleKey	vertexModuleKey;
			vertexModuleKey.m_Path = MaterialToRHI::RemapShaderPathNoExt(args.m_ShaderFolder, vertexShader, materialName, RHI::VertexShaderStage, args.m_Renderer->m_RendererType, m_Option, args.m_FeaturesSettings.View(), m_RenderPassIdx);
			vertexModuleKey.m_Stage = RHI::VertexShaderStage;
			m_ShaderModules[vertexModuleKey.m_Stage] = CShaderModuleManager::UpdateThread_GetResource(vertexModuleKey, args);
			if (!m_ShaderModules[vertexModuleKey.m_Stage].m_ID.Valid())
			{
				CLog::Log(PK_ERROR, "Could not get vertex shader module");
				return false;
			}
		}

		// Geometry module
		if (stagepipeline & RHI::GeometryShaderMask)
		{
			SShaderModuleKey	geometryModuleKey;
			geometryModuleKey.m_Path = MaterialToRHI::RemapShaderPathNoExt(args.m_ShaderFolder, vertexShader, materialName, RHI::GeometryShaderStage, args.m_Renderer->m_RendererType, m_Option, args.m_FeaturesSettings.View(), m_RenderPassIdx);
			geometryModuleKey.m_Stage = RHI::GeometryShaderStage;
			m_ShaderModules[geometryModuleKey.m_Stage] = CShaderModuleManager::UpdateThread_GetResource(geometryModuleKey, args);
			if (!m_ShaderModules[geometryModuleKey.m_Stage].m_ID.Valid())
			{
				CLog::Log(PK_ERROR, "Could not get geometry shader module");
				return false;
			}
		}

		// Fragment module
		if (stagepipeline & RHI::FragmentShaderMask)
		{
			SShaderModuleKey		fragmentModuleKey;
			fragmentModuleKey.m_Path = MaterialToRHI::RemapShaderPathNoExt(args.m_ShaderFolder, fragmentShader, materialName, RHI::FragmentShaderStage, args.m_Renderer->m_RendererType, m_Option, args.m_FeaturesSettings.View(), m_RenderPassIdx);
			fragmentModuleKey.m_Stage = RHI::FragmentShaderStage;
			m_ShaderModules[fragmentModuleKey.m_Stage] = CShaderModuleManager::UpdateThread_GetResource(fragmentModuleKey, args);
			if (!m_ShaderModules[fragmentModuleKey.m_Stage].m_ID.Valid())
			{
				CLog::Log(PK_ERROR, "Could not get fragment shader module");
				return false;
			}
		}
	}
	else
	{
		// Compute module
		if (stagepipeline & RHI::ComputeShaderMask)
		{
			// Unlike vert/geom/frag, compute shader is compiled offline by shadertool.
			// We build the shader path from compute shader name and hashed binding, as do shadertool.
			// Ultimately, compilation will be dynamic and this will be handled by MaterialToRHI::RemapShaderPathNoExt.
			const CDigestMD5	&computeHash = m_ShaderBindings.Hash(RHI::ComputeShaderStage);
			char				_hashStorage[32 + 1];
			const CString		filePath = "./Shaders/" + _GetComputeShaderPath(m_ComputeShaderType) + "." + RHI::ShaderHashToStringView(computeHash, _hashStorage);
			SShaderModuleKey	computeModuleKey;
			computeModuleKey.m_Path = filePath;
			computeModuleKey.m_Stage = RHI::ComputeShaderStage;
			m_ShaderModules[computeModuleKey.m_Stage] = CShaderModuleManager::UpdateThread_GetResource(computeModuleKey, args);
			if (!m_ShaderModules[computeModuleKey.m_Stage].m_ID.Valid())
			{
				CLog::Log(PK_ERROR, "Could not get compute shader module");
				return false;
			}
		}
	}
	return true;
}

//----------------------------------------------------------------------------

RHI::PShaderProgram		SShaderProgramKey::RenderThread_CreateResource(const SCreateArg &args)
{
	RHI::PShaderProgram	program = args.m_ApiManager->CreateShaderProgram(RHI::SRHIResourceInfos("PK-RHI Shader Program"));
	if (program != null)
	{
		RHI::PShaderModule	modules[RHI::ShaderStage_Count];

		for (u32 i = 0; i < RHI::ShaderStage_Count; ++i)
		{
			if (m_ShaderModules[i].Valid())
			{
				modules[i] = CShaderModuleManager::RenderThread_ResolveResource(m_ShaderModules[i], args);
				if (modules[i] == null)
				{
					CLog::Log(PK_ERROR, "Could not create shader module");
					return null;
				}
			}
			else
			{
				modules[i] = null;
			}
		}
		bool	result = true;
		if (m_ComputeShaderType == ComputeType_None)
		{
			result = program->CreateFromShaderModules(modules[RHI::VertexShaderStage], modules[RHI::GeometryShaderStage], modules[RHI::FragmentShaderStage]);
		}
		else
		{
			result = program->CreateFromShaderModule(modules[RHI::ComputeShaderStage]);
		}
		if (!result)
		{
			CLog::Log(PK_ERROR, "Could not create shader program from shader modules");
			return null;
		}
	}
	return program;
}

//----------------------------------------------------------------------------

void	SShaderProgramKey::UpdateThread_ReleaseDependencies()
{
	for (u32 i = 0; i < RHI::ShaderStage_Count; ++i)
	{
		if (m_ShaderModules[i].Valid())
		{
			CShaderModuleManager::UpdateThread_ReleaseResource(m_ShaderModules[i]);
			m_ShaderModules[i].m_ID = CGuid::INVALID;
		}
	}
}

//----------------------------------------------------------------------------

bool	SShaderProgramKey::operator == (const SShaderProgramKey &oth) const
{
	if (m_RenderPassIdx != oth.m_RenderPassIdx)
		return false;
	for (u32 i = 0; i < RHI::ShaderStage_Count; ++i)
	{
		if (m_ShaderModules[i] != oth.m_ShaderModules[i])
			return false;
	}
	return true;
}

//----------------------------------------------------------------------------
//
//	SGeometryKey
//
//----------------------------------------------------------------------------

SGeometryKey::SGeometryKey()
:	m_GeomType(Geom_Mesh)
{
}

//----------------------------------------------------------------------------

PGeometryResource	SGeometryKey::RenderThread_CreateResource(const SCreateArg &args)
{
	PGeometryResource	geom = PK_NEW(CGeometryResource);

	if (geom == null)
	{
		CLog::Log(PK_ERROR, "Could not allocate geometry resource");
		return null;
	}
	if (m_GeomType == Geom_Mesh)
	{
		if (m_Path.Empty() || args.m_ResourceManagers.Empty())
			return null;

		TResourcePtr<CResourceMesh>	sourceMesh;
		for (auto resourceManager : args.m_ResourceManagers)
		{
			sourceMesh = resourceManager->Load<CResourceMesh>(m_Path, false, SResourceLoadCtl(false, true));
			if (sourceMesh != null && !sourceMesh->Empty())
				break;
		}

		if (sourceMesh == null || sourceMesh->Empty())
		{
			CLog::Log(PK_ERROR, "Could not load mesh \"%s\"", m_Path.Data());
			return null;
		}
		const Utils::MeshSemanticFlags	semantics[] =
		{
			Utils::MeshPositions,
			Utils::MeshNormals,
			Utils::MeshTangents,
			Utils::MeshColors,
			Utils::MeshTexcoords,
			Utils::MeshColors1,
			Utils::MeshTexcoords1,
			Utils::MeshBoneIds,
			Utils::MeshBoneWeights,
		};
		if (!Utils::CreateGpuBuffers(args.m_ApiManager, &(*sourceMesh), semantics, geom->m_PerGeometryViews))
		{
			CLog::Log(PK_ERROR, "Could not create the GPU buffers for the geometry resource");
			return null;
		}
	}
	else if (m_GeomType == Geom_Sphere)
	{
		if (!PK_VERIFY(geom->m_PerGeometryViews.PushBack().Valid()))
			return null;
		if (!PK_VERIFY(CreateGeomSphere(args.m_ApiManager, 30, geom->m_PerGeometryViews.Last())))
			return null;
	}
	else if (m_GeomType == Geom_Cube)
	{
		if (!PK_VERIFY(geom->m_PerGeometryViews.PushBack().Valid()))
			return null;
		if (!PK_VERIFY(CreateGeomCube(args.m_ApiManager, geom->m_PerGeometryViews.Last())))
			return null;
	}
	return geom;
}

//----------------------------------------------------------------------------

PGeometryResource	SGeometryKey::RenderThread_ReloadResource(const SCreateArg &args)
{
	return RenderThread_CreateResource(args);
}

//----------------------------------------------------------------------------

bool	SGeometryKey::operator == (const SGeometryKey &other) const
{
	return m_GeomType == other.m_GeomType && m_Path == other.m_Path;
}

//----------------------------------------------------------------------------

bool	SGeometryKey::CreateGeomSphere(const RHI::PApiManager &apiManager, u32 subdivs, Utils::GpuBufferViews &toFill)
{
	float	radiusScale = PrimitiveDiscretizers::GetTangentSphereScale(subdivs);
#if		0
	float		computedEdgeSize = (vtx1 - vtx2).Length();
	// Find longest edge bruteforce:
	float										longestEdgeSq = 0.0f;
	const u32									*idxBuff = static_cast<const u32 *>(batch.m_IStream.RawStream());
	const TStridedMemoryView<const CFloat3>		posBuff = batch.m_VStream.Positions();
	for (u32 i = 0; i < batch.m_IStream.IndexCount(); i += 3)
	{
		u32		idx0 = idxBuff[i + 0];
		u32		idx1 = idxBuff[i + 1];
		u32		idx2 = idxBuff[i + 2];
		CFloat3	edge1 = posBuff[idx0] - posBuff[idx1];
		CFloat3	edge2 = posBuff[idx1] - posBuff[idx2];
		CFloat3	edge3 = posBuff[idx2] - posBuff[idx0];
		float	edge1SqLength = edge1.LengthSquared();
		float	edge2SqLength = edge2.LengthSquared();
		float	edge3SqLength = edge3.LengthSquared();

		if (edge1SqLength > longestEdgeSq)
			longestEdgeSq = edge1SqLength;
		if (edge2SqLength > longestEdgeSq)
			longestEdgeSq = edge2SqLength;
		if (edge3SqLength > longestEdgeSq)
			longestEdgeSq = edge3SqLength;
	}
	CLog::Log(PK_INFO, "longest distance found = %f", sqrtf(longestEdgeSq));
#endif

	// Create the light spheres geometry:
	CMeshTriangleBatch		batch;

	batch.m_IStream.SetPrimitiveType(CMeshIStream::Triangles);
	batch.m_VStream.Reformat(VertexDeclaration::Position3f);
	PrimitiveDiscretizers::BuildSphere(batch, CFloat4x4::IDENTITY, radiusScale, 0.0f, CFloat3::ONE, 1.0f, CFloat4::ONE, subdivs, Frame_RightHand_Y_Up);

	TStridedMemoryView<const CFloat3>	srcPositions = batch.m_VStream.Positions();
	const u32							idxBuffSize = batch.m_IStream.IndexCount() * sizeof(u32);
	const u32							*srcIndices = batch.m_IStream.Stream<u32>();

	RHI::PGpuBuffer						vertexBuff = apiManager->CreateGpuBuffer(RHI::SRHIResourceInfos("Sphere Vertex Buffer"), RHI::VertexBuffer, srcPositions.Count() * sizeof(CFloat3), RHI::UsageStaticDraw);
	RHI::PGpuBuffer						indexBuff = apiManager->CreateGpuBuffer(RHI::SRHIResourceInfos("Sphere Index Buffer"), RHI::IndexBuffer, idxBuffSize, RHI::UsageStaticDraw);

	if (!PK_VERIFY(vertexBuff != null && indexBuff != null))
		return false;

	CFloat3								*mappedPos = static_cast<CFloat3*>(apiManager->MapCpuView(vertexBuff));
	u32									*mappedIdx = static_cast<u32*>(apiManager->MapCpuView(indexBuff));

	if (!PK_VERIFY(mappedPos != null && mappedIdx != null))
		return false;

	for (u32 v = 0; v < srcPositions.Count(); ++v)
	{
		mappedPos[v] = srcPositions[v];
	}

	Mem::Copy(mappedIdx, srcIndices, idxBuffSize);

	apiManager->UnmapCpuView(vertexBuff);
	apiManager->UnmapCpuView(indexBuff);

	if (!PK_VERIFY(toFill.m_VertexBuffers.PushBack(vertexBuff).Valid()))
		return false;
	toFill.m_IndexBuffer = indexBuff;
	toFill.m_IndexBufferSize = RHI::IndexBuffer32Bit;
	return true;
}

//----------------------------------------------------------------------------

bool	SGeometryKey::CreateGeomCube(const RHI::PApiManager &apiManager, Utils::GpuBufferViews &toFill)
{
	// Create the light spheres geometry:
	CMeshTriangleBatch		batch;

	batch.m_IStream.SetPrimitiveType(CMeshIStream::Triangles);
	batch.m_VStream.Reformat(VertexDeclaration::Position3f);
	PrimitiveDiscretizers::BuildBox(batch, CFloat4x4::IDENTITY, CAABB(-CFloat3::ONE, CFloat3::ONE), CFloat4::ONE);

	TStridedMemoryView<const CFloat3>	srcPositions = batch.m_VStream.Positions();
	const u32							idxBuffSize = batch.m_IStream.IndexCount() * sizeof(u32);
	const u32							*srcIndices = batch.m_IStream.Stream<u32>();

	RHI::PGpuBuffer						vertexBuff = apiManager->CreateGpuBuffer(RHI::SRHIResourceInfos("Cube Vertex Buffer"), RHI::VertexBuffer, srcPositions.Count() * sizeof(CFloat3), RHI::UsageStaticDraw);
	RHI::PGpuBuffer						indexBuff = apiManager->CreateGpuBuffer(RHI::SRHIResourceInfos("Cube Index Buffer"), RHI::IndexBuffer, idxBuffSize, RHI::UsageStaticDraw);

	if (!PK_VERIFY(vertexBuff != null && indexBuff != null))
		return false;

	CFloat3								*mappedPos = static_cast<CFloat3*>(apiManager->MapCpuView(vertexBuff));
	u32									*mappedIdx = static_cast<u32*>(apiManager->MapCpuView(indexBuff));

	if (!PK_VERIFY(mappedPos != null && mappedIdx != null))
		return false;

	for (u32 v = 0; v < srcPositions.Count(); ++v)
	{
		mappedPos[v] = srcPositions[v];
	}

	Mem::Copy(mappedIdx, srcIndices, idxBuffSize);

	apiManager->UnmapCpuView(vertexBuff);
	apiManager->UnmapCpuView(indexBuff);

	if (!PK_VERIFY(toFill.m_VertexBuffers.PushBack(vertexBuff).Valid()))
		return false;
	toFill.m_IndexBuffer = indexBuff;
	toFill.m_IndexBufferSize = RHI::IndexBuffer32Bit;
	return true;
}

//----------------------------------------------------------------------------

void	SGeometryKey::SetupDefaultResource()
{
	SPrepareArg		args;
	SGeometryKey	key;

	key.m_GeomType = Geom_Mesh;
	key.m_Path = "Meshes/default.pkmm";

	s_DefaultResourceID = CGeometryManager::UpdateThread_GetResource(key, args);
}

//----------------------------------------------------------------------------

CGeometryManager::CResourceId	SGeometryKey::s_DefaultResourceID;

//----------------------------------------------------------------------------
//
//	SConstantAtlasKey
//
//----------------------------------------------------------------------------

PConstantAtlas	SConstantAtlasKey::RenderThread_CreateResource(const SCreateArg &args)
{
	PConstantAtlas		atlas = PK_NEW(CConstantAtlas);
	if (atlas == null)
	{
		CLog::Log(PK_ERROR, "Could not allocate texture atlas data");
		return null;
	}

	const u32	atlasCount = m_SourceAtlas->m_RectsFp32.Count();

	PK_ASSERT(SConstantAtlasKey::GetAtlasConstantSetLayout().m_Constants.Count() == 1);

	RHI::PConstantSet	atlasConstSet = args.m_ApiManager->CreateConstantSet(RHI::SRHIResourceInfos("Atlas Constant Set"), GetAtlasConstantSetLayout());
	const u32			atlasBufferByteSize = 1 * sizeof(u32) + atlasCount * sizeof(CFloat4);
	RHI::PGpuBuffer		atlasBuffer = args.m_ApiManager->CreateGpuBuffer(RHI::SRHIResourceInfos("Atlas Buffer"), RHI::RawBuffer, atlasBufferByteSize, RHI::UsageStaticDraw);

	if (atlasBuffer == null || atlasConstSet == null)
	{
		CLog::Log(PK_ERROR, "Could not create constant buffer for texture atlas");
		return null;
	}
	if (!atlasConstSet->SetConstants(atlasBuffer, 0))
	{
		CLog::Log(PK_ERROR, "Could not set texture atlas data in constant buffer");
		return null;
	}

	{
		void	*ptr = args.m_ApiManager->MapCpuView(atlasBuffer);
		u32		*count = reinterpret_cast<u32*>(Mem::AdvanceRawPointer(ptr, 0));
		*count = m_SourceAtlas->m_RectsFp32.Count();
		CFloat4	*data = reinterpret_cast<CFloat4*>(Mem::AdvanceRawPointer(ptr, sizeof(u32)));
		PK_ASSERT(m_SourceAtlas->m_RectsFp32.Stride() == sizeof(CFloat4));
		Mem::Copy(data, m_SourceAtlas->m_RectsFp32.RawDataPointer(), m_SourceAtlas->m_RectsFp32.CoveredBytes());
		args.m_ApiManager->UnmapCpuView(atlasBuffer);
	}

	atlasConstSet->UpdateConstantValues();
	atlas->m_AtlasConstSet = atlasConstSet;
	atlas->m_AtlasBuffer = atlasBuffer;

	return atlas;
}

//----------------------------------------------------------------------------

PConstantAtlas	SConstantAtlasKey::RenderThread_ReloadResource(const SCreateArg &args)
{
	return RenderThread_CreateResource(args);
}

//----------------------------------------------------------------------------

bool	SConstantAtlasKey::operator == (const SConstantAtlasKey &other) const
{
	if ((m_SourceAtlas == null) != (other.m_SourceAtlas == null))
		return false;
	return m_Path == other.m_Path;
	//return m_SourceAtlas == other.m_SourceAtlas;
}

//----------------------------------------------------------------------------

static const RHI::SConstantSetLayout	*s_AtlasConstantSetLayout = null;

//----------------------------------------------------------------------------

const RHI::SConstantSetLayout	&SConstantAtlasKey::GetAtlasConstantSetLayout()
{
	PK_ASSERT(s_AtlasConstantSetLayout != null);
	return *s_AtlasConstantSetLayout;
}

//----------------------------------------------------------------------------

void	SConstantAtlasKey::SetupConstantSetLayout()
{
	RHI::SConstantSetLayout	*layout = PK_NEW(RHI::SConstantSetLayout(static_cast<RHI::EShaderStageMask>(RHI::VertexShaderMask | RHI::GeometryShaderMask | RHI::FragmentShaderMask)));
	if (layout != null && layout->m_Constants.Empty())
	{
		RHI::SRawBufferDesc			atlas("Atlas");
		if (!layout->AddConstantsLayout(atlas))
		{
			PK_DELETE(layout);
			return;
		}
	}
	s_AtlasConstantSetLayout = layout;
}

//----------------------------------------------------------------------------

void	SConstantAtlasKey::ClearConstantSetLayoutIFN()
{
	RHI::SConstantSetLayout	*layout = const_cast<RHI::SConstantSetLayout*>(s_AtlasConstantSetLayout);
	if (layout != null)
		PK_DELETE(layout);
}

//----------------------------------------------------------------------------
// Constant noise data
//----------------------------------------------------------------------------

static const RHI::SConstantSetLayout	*s_NoiseTextureConstantSetLayout = null;

//----------------------------------------------------------------------------

const RHI::SConstantSetLayout	&SConstantNoiseTextureKey::GetNoiseTextureConstantSetLayout()
{
	PK_ASSERT(s_NoiseTextureConstantSetLayout != null);
	return *s_NoiseTextureConstantSetLayout;
}

//----------------------------------------------------------------------------

void	SConstantNoiseTextureKey::SetupConstantSetLayout()
{
	RHI::SConstantSetLayout	*layout = PK_NEW(RHI::SConstantSetLayout());

	if (layout != null && layout->m_Constants.Empty())
	{
		layout->m_ShaderStagesMask = RHI::FragmentShaderMask;
		if (!layout->AddConstantsLayout(RHI::SConstantSamplerDesc(GetResourceName(), RHI::SamplerTypeSingle)))
		{
			PK_DELETE(layout);
			return;
		}
	}
	s_NoiseTextureConstantSetLayout = layout;
}

//----------------------------------------------------------------------------

void	SConstantNoiseTextureKey::ClearConstantSetLayoutIFN()
{
	RHI::SConstantSetLayout	*layout = const_cast<RHI::SConstantSetLayout*>(s_NoiseTextureConstantSetLayout);
	if (layout != null)
		PK_DELETE(layout);
}

//----------------------------------------------------------------------------
//
//	SConstantDrawRequests
//
//----------------------------------------------------------------------------

static RHI::SConstantSetLayout	*s_DrawRequestsConstantSetLayout_Billboard = null;
static RHI::SConstantSetLayout	*s_DrawRequestsConstantSetLayout_Triangle = null;

//----------------------------------------------------------------------------

const RHI::SConstantSetLayout	&SConstantDrawRequests::GetConstantSetLayout(ERendererClass rendererType)
{
	if (rendererType == Renderer_Triangle)
	{
		PK_ASSERT(s_DrawRequestsConstantSetLayout_Triangle != null);
		return *s_DrawRequestsConstantSetLayout_Triangle;
	}
	else
	{
		PK_ASSERT(s_DrawRequestsConstantSetLayout_Billboard != null);
		return *s_DrawRequestsConstantSetLayout_Billboard;
	}
}

//----------------------------------------------------------------------------

bool	SConstantDrawRequests::GetConstantBufferDesc(RHI::SConstantBufferDesc &outDesc, u32 elementCount, ERendererClass rendererType)
{
	if (rendererType == Renderer_Triangle)
	{
		outDesc.m_Name = "TriangleInfo";

		// (see Drawers::STriangleDrawRequest for more details)
		// Float: NormalsBendingFactor
		return outDesc.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat2, "DrawRequest", elementCount));
	}
	else if (rendererType == Renderer_Mesh)
	{
		outDesc.m_Name = "GPUMeshPushConstants";
		return	outDesc.AddConstant(RHI::SConstantVarDesc(RHI::TypeUint, "DrawRequest")) &&
				outDesc.AddConstant(RHI::SConstantVarDesc(RHI::TypeUint, "IndirectionOffsetsIndex"));
	}
	else
	{
		outDesc.m_Name = "BillboardInfo";

		// (see Drawers::SBillboardDrawRequest for more details)
		// Float0: Flags
		// Float1: NormalsBendingFactor
		// Float2: AspectRatio (TO BE REMOVED)
		// Float3: AxisScale (TO BE REMOVED)
		return outDesc.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat4, "DrawRequest", elementCount));
	}
}

//----------------------------------------------------------------------------

void	SConstantDrawRequests::SetupConstantSetLayouts()
{
	if (s_DrawRequestsConstantSetLayout_Billboard == null)
	{
		RHI::SConstantSetLayout	*layout = PK_NEW(RHI::SConstantSetLayout(static_cast<RHI::EShaderStageMask>(RHI::GeometryShaderMask | RHI::VertexShaderMask)));
		if (layout != null && layout->m_Constants.Empty())
		{
			RHI::SConstantBufferDesc	bufferDesc;

			if (!GetConstantBufferDesc(bufferDesc, 0x100, Renderer_Billboard) ||
				!layout->AddConstantsLayout(bufferDesc))
			{
				PK_DELETE(layout);
				return;
			}
		}
		s_DrawRequestsConstantSetLayout_Billboard = layout;
	}
	if (s_DrawRequestsConstantSetLayout_Triangle == null)
	{
		RHI::SConstantSetLayout	*layout = PK_NEW(RHI::SConstantSetLayout(static_cast<RHI::EShaderStageMask>(RHI::VertexShaderMask)));
		if (layout != null && layout->m_Constants.Empty())
		{
			RHI::SConstantBufferDesc	bufferDesc;

			if (!GetConstantBufferDesc(bufferDesc, 0x100, Renderer_Triangle) ||
				!layout->AddConstantsLayout(bufferDesc))
			{
				PK_DELETE(layout);
				return;
			}
		}
		s_DrawRequestsConstantSetLayout_Triangle = layout;
	}
}

//----------------------------------------------------------------------------

void	SConstantDrawRequests::ClearConstantSetLayoutsIFN()
{
	PK_SAFE_DELETE(s_DrawRequestsConstantSetLayout_Billboard);
	PK_SAFE_DELETE(s_DrawRequestsConstantSetLayout_Triangle);
}

//----------------------------------------------------------------------------
//
//	SConstantVertexBillboarding
//
//----------------------------------------------------------------------------

bool	SConstantVertexBillboarding::GetPushConstantBufferDesc(RHI::SPushConstantBuffer &outDesc, bool gpuStorage)
{
	// If gpu sim, push constant will also contain an index into the draw request stream offsets

	outDesc.m_Name = "GPUBillboardPushConstants";
	outDesc.m_ShaderStagesMask = RHI::VertexShaderMask;

	bool	success = true;

	success &= outDesc.AddConstant(RHI::SConstantVarDesc(RHI::TypeUint, "IndicesOffset"));

	if (gpuStorage)
		success &= outDesc.AddConstant(RHI::SConstantVarDesc(RHI::TypeUint, "StreamOffsetsIndex"));

	return success;
}

//----------------------------------------------------------------------------
//
//	Renderer Cache Manager
//
//----------------------------------------------------------------------------

RHI::PRenderState	CRendererCache::GetRenderState(EShaderOptions option, ESampleLibGraphicResources_RenderPass renderPass, u32 *neededConstants)
{
	for (u32 i = 0; i < m_RenderStates.Count(); ++i)
	{
		if (m_RenderStates[i].m_Options == option)
		{
			if (renderPass == __MaxParticlePass || m_RenderStates[i].m_RenderPassIdx == renderPass)
			{
				if (neededConstants != null)
					*neededConstants = m_RenderStates[i].m_NeededConstants;
				if (m_RenderStates[i].m_RenderState == null && !m_RenderStates[i].m_Expected)
				{
					m_RenderStates[i].m_Expected = true;
					CRendererCacheManager::RenderThread_CreateMissingResources(m_LastCreateArgs);
				}
				return m_RenderStates[i].m_RenderState;
			}
		}
	}
	return null;
}

//----------------------------------------------------------------------------

RHI::PComputeState	CRendererCache::GetComputeState(EComputeShaderType type)
{
	for (u32 i = 0; i < m_ComputeStates.Count(); ++i)
	{
		if (m_ComputeStates[i].m_Type == type)
		{
			if (m_ComputeStates[i].m_ComputeState == null && !m_ComputeStates[i].m_Expected)
			{
				m_ComputeStates[i].m_Expected = true;
				CRendererCacheManager::RenderThread_CreateMissingResources(m_LastCreateArgs);
			}
			return m_ComputeStates[i].m_ComputeState;
		}
	}
	return null;
}

//----------------------------------------------------------------------------

bool	CRendererCache::GetGPUStorageConstantSets(EShaderOptions option, const RHI::SConstantSetLayout *&outSimDataConstantSet, const RHI::SConstantSetLayout *&outOffsetsConstantSet) const
{
	for (u32 i = 0; i < m_RenderStates.Count(); ++i)
	{
		if (m_RenderStates[i].m_Options == option)
		{
			outSimDataConstantSet = &m_RenderStates[i].m_GPUStorageSimDataConstantSetLayout;
			outOffsetsConstantSet = &m_RenderStates[i].m_GPUStorageOffsetsConstantSetLayout;
			return true;
		}
	}
	return false;
}

//----------------------------------------------------------------------------

PK_NOINLINE bool	SRendererCacheKey::UpdateThread_Prepare(const SPrepareArg &args)
{
	bool	result = true;

#define		MULT_OPTION(_opt, index) \
	do { \
		const u32	opcount = options.Count() - index; \
		PK_VERIFY(options.Resize(index + opcount * 2)); \
		for (u32 i = index, j = 0; j < opcount; ++i, ++j) \
			options[opcount + i] = options[i] | _opt; \
	} while (0)

	// Gen options
	TStaticCountedArray<EShaderOptions, 128>					options;
	// Needed compute shaders
	TStaticCountedArray<EComputeShaderType, ComputeType_Count>	computeShaderTypes;

	if (args.m_Renderer->m_RendererType == ERendererClass::Renderer_Billboard)
	{
#if !defined(PK_ORBIS)
		if (args.m_GpuCaps.m_SupportsGeometryShaders)
		{
			PK_VERIFY(options.PushBack(Option_GeomBillboarding).Valid());
			PK_VERIFY(options.PushBack(Option_GeomBillboarding | Option_Axis_C1).Valid());
			PK_VERIFY(options.PushBack(Option_GeomBillboarding | Option_Axis_C1 | Option_Capsule).Valid());
			PK_VERIFY(options.PushBack(Option_GeomBillboarding | Option_Axis_C2).Valid());
#if (PK_PARTICLES_UPDATER_USE_GPU != 0)
			MULT_OPTION(Option_GPUStorage, 0);
#endif // (PK_PARTICLES_UPDATER_USE_GPU != 0)
			MULT_OPTION(Option_BillboardSizeFloat2, 0);
		}
#endif
		if (args.m_GpuCaps.m_SupportsShaderResourceViews)
		{
			const u32	optCount = options.Count();
			PK_VERIFY(options.PushBack(Option_VertexBillboarding).Valid());
			PK_VERIFY(options.PushBack(Option_VertexBillboarding | Option_Axis_C1).Valid());
			PK_VERIFY(options.PushBack(Option_VertexBillboarding | Option_Axis_C1 | Option_Capsule).Valid());
			PK_VERIFY(options.PushBack(Option_VertexBillboarding | Option_Axis_C2).Valid());
#if (PK_PARTICLES_UPDATER_USE_GPU != 0)
			MULT_OPTION(Option_GPUStorage, optCount);
			MULT_OPTION(Option_GPUSort, optCount);
			PK_VERIFY(computeShaderTypes.PushBack(ComputeType_ComputeSortKeys).Valid());
			PK_VERIFY(computeShaderTypes.PushBack(ComputeType_ComputeSortKeys_CameraDistance).Valid());
			PK_VERIFY(computeShaderTypes.PushBack(ComputeType_SortUpSweep).Valid());
			PK_VERIFY(computeShaderTypes.PushBack(ComputeType_SortPrefixSum).Valid());
			PK_VERIFY(computeShaderTypes.PushBack(ComputeType_SortDownSweep).Valid());
#endif // (PK_PARTICLES_UPDATER_USE_GPU != 0)
			MULT_OPTION(Option_BillboardSizeFloat2, optCount);
		}
	}
	else if (args.m_Renderer->m_RendererType == ERendererClass::Renderer_Triangle)
	{
		if (args.m_GpuCaps.m_SupportsShaderResourceViews)
		{
			PK_VERIFY(options.PushBack(Option_TriangleVertexBillboarding).Valid());

#if (PK_PARTICLES_UPDATER_USE_GPU != 0)
			// RenderStates
			const u32	optCount = options.Count();
			PK_VERIFY(options.PushBack(Option_TriangleVertexBillboarding | Option_GPUStorage).Valid());
			MULT_OPTION(Option_GPUSort, optCount);
			PK_VERIFY(computeShaderTypes.PushBack(ComputeType_ComputeSortKeys).Valid());
			PK_VERIFY(computeShaderTypes.PushBack(ComputeType_ComputeSortKeys_CameraDistance).Valid());
			PK_VERIFY(computeShaderTypes.PushBack(ComputeType_SortUpSweep).Valid());
			PK_VERIFY(computeShaderTypes.PushBack(ComputeType_SortPrefixSum).Valid());
			PK_VERIFY(computeShaderTypes.PushBack(ComputeType_SortDownSweep).Valid());
#endif // (PK_PARTICLES_UPDATER_USE_GPU != 0)
		}
	}
	else if (args.m_Renderer->m_RendererType == ERendererClass::Renderer_Mesh)
	{
		PK_VERIFY(options.PushBack(Option_GPUMesh).Valid());
#if (PK_PARTICLES_UPDATER_USE_GPU != 0)
		PK_VERIFY(computeShaderTypes.PushBack(ComputeType_ComputeParticleCountPerMesh).Valid());
		PK_VERIFY(computeShaderTypes.PushBack(ComputeType_ComputeParticleCountPerMesh_MeshAtlas).Valid());
		PK_VERIFY(computeShaderTypes.PushBack(ComputeType_ComputeParticleCountPerMesh_LOD).Valid());
		PK_VERIFY(computeShaderTypes.PushBack(ComputeType_ComputeParticleCountPerMesh_LOD_MeshAtlas).Valid());
		PK_VERIFY(computeShaderTypes.PushBack(ComputeType_InitIndirectionOffsetsBuffer).Valid());
		PK_VERIFY(computeShaderTypes.PushBack(ComputeType_InitIndirectionOffsetsBuffer_LODNoAtlas).Valid());
		PK_VERIFY(computeShaderTypes.PushBack(ComputeType_ComputeMeshIndirectionBuffer).Valid());
		PK_VERIFY(computeShaderTypes.PushBack(ComputeType_ComputeMeshIndirectionBuffer_MeshAtlas).Valid());
		PK_VERIFY(computeShaderTypes.PushBack(ComputeType_ComputeMeshIndirectionBuffer_LOD).Valid());
		PK_VERIFY(computeShaderTypes.PushBack(ComputeType_ComputeMeshIndirectionBuffer_LOD_MeshAtlas).Valid());
		PK_VERIFY(computeShaderTypes.PushBack(ComputeType_ComputeMeshMatrices).Valid());
#endif // (PK_PARTICLES_UPDATER_USE_GPU != 0
	}
	else if (args.m_Renderer->m_RendererType == ERendererClass::Renderer_Ribbon)
	{
		if (args.m_GpuCaps.m_SupportsShaderResourceViews)
		{
#if (PK_PARTICLES_UPDATER_USE_GPU != 0)
			// RenderStates
			const u32	optCount = options.Count();
			PK_VERIFY(options.PushBack(Option_RibbonVertexBillboarding | Option_GPUStorage).Valid());
			PK_VERIFY(options.PushBack(Option_RibbonVertexBillboarding | Option_GPUStorage | Option_Axis_C1).Valid());
			MULT_OPTION(Option_GPUSort, optCount);

			// Compute states
			PK_VERIFY(computeShaderTypes.PushBack(ComputeType_ComputeSortKeys_RibbonIndirection).Valid());
			PK_VERIFY(computeShaderTypes.PushBack(ComputeType_ComputeSortKeys_CameraDistance_RibbonIndirection).Valid());
			PK_VERIFY(computeShaderTypes.PushBack(ComputeType_SortUpSweep).Valid());
			PK_VERIFY(computeShaderTypes.PushBack(ComputeType_SortPrefixSum).Valid());
			PK_VERIFY(computeShaderTypes.PushBack(ComputeType_SortDownSweep).Valid());
			PK_VERIFY(computeShaderTypes.PushBack(ComputeType_SortUpSweep_KeyStride64).Valid());
			PK_VERIFY(computeShaderTypes.PushBack(ComputeType_SortDownSweep_KeyStride64).Valid());
			PK_VERIFY(computeShaderTypes.PushBack(ComputeType_ComputeRibbonSortKeys).Valid());
#endif // (PK_PARTICLES_UPDATER_USE_GPU != 0)
		}
	}

	for (u32 i = 0; i < options.Count(); ++i)
		options[i] = options[i] | Option_VertexPassThrough;

	options.PushBack(Option_VertexPassThrough);

#undef		MULT_OPTION

	m_RenderPasses = args.m_RenderPasses;

	for (const auto &option : options)
		result &= PushRenderState(args, option);

	if (!result)
		CLog::Log(PK_ERROR, "Push render state failed");

	for (const auto &computeShaderType : computeShaderTypes)
		result &= PushComputeState(args, computeShaderType);

	if (!result)
		CLog::Log(PK_ERROR, "Push Compute state failed");

	return result;
}

//----------------------------------------------------------------------------

bool	SRendererCacheKey::PushRenderState(const SPrepareArg &args, EShaderOptions options)
{
	for (u32 i = 0; i < m_RenderPasses.Count(); ++i)
	{
		if (!m_RenderStates.PushBack().Valid())
			return false;
		SRendererCacheKey::SRenderStateFeature	&renderState = m_RenderStates.Last();
		SRenderStateKey							renderStateKey;
		renderStateKey.m_Options = options;
		renderStateKey.m_RenderPassIdx = m_RenderPasses[i];
		renderState.m_RenderPassIdx = m_RenderPasses[i];
		renderState.m_RenderState = CRenderStateManager::UpdateThread_GetResource(renderStateKey, args);
		renderState.m_Options = options;
		renderState.m_NeededConstants = renderStateKey.GetNeededConstants();
		if (!m_RenderStates.Last().m_RenderState.Valid())
			return false;
	}
	return true;
}

//----------------------------------------------------------------------------

bool	SRendererCacheKey::PushComputeState(const SPrepareArg &args, EComputeShaderType type)
{
	if (!m_ComputeStates.PushBack().Valid())
		return false;
	SRendererCacheKey::SComputeStateFeature	&computeState = m_ComputeStates.Last();
	SComputeStateKey						computeStateKey;
	computeStateKey.m_Type = type;
	computeState.m_ComputeState = CComputeStateManager::UpdateThread_GetResource(computeStateKey, args);
	computeState.m_Type = type;
	return m_ComputeStates.Last().m_ComputeState.Valid();
}

//----------------------------------------------------------------------------

template<>
bool TGraphicResourceManager<PRendererCache, SRendererCacheKey, SPrepareArg, SCreateArg>::RenderThread_CreateMissingResources(const SCreateArg &args)
{
	bool	success = true;

	PK_SCOPEDLOCK(m_ResourcesLock);
	for (u32 i = 0; i < m_Resources.Count(); ++i)
	{
		if (!m_Resources[i].m_IsSlotEmpty && (m_Resources[i].m_ResourcePtr == null || m_Resources[i].m_ResourceKey.RenderThread_IsPartiallyBuilt(m_Resources[i].m_ResourcePtr)))
		{
			m_Resources[i].m_ResourcePtr = m_Resources[i].m_ResourceKey.RenderThread_CreateResource(args, m_Resources[i].m_ResourcePtr);
			if (m_Resources[i].m_ResourcePtr == null)
				success = false;
		}
	}
	return success;
}

//----------------------------------------------------------------------------

PRendererCache	SRendererCacheKey::RenderThread_CreateResource(const SCreateArg &args, PRendererCache rendererCache)
{
	if (rendererCache == null)
		rendererCache = PK_NEW(CRendererCache);

	if (rendererCache == null ||
		!rendererCache->m_RenderStates.Resize(m_RenderStates.Count()) ||
		!rendererCache->m_ComputeStates.Resize(m_ComputeStates.Count()))
	{
		CLog::Log(PK_ERROR, "Could not create renderer cache");
		return null;
	}

	rendererCache->m_LastCreateArgs = args;
	rendererCache->m_RenderPasses = m_RenderPasses;
	for (u32 i = 0; i < m_RenderStates.Count(); ++i)
	{
		CRendererCache::SRenderStateFeature	&renderState = rendererCache->m_RenderStates[i];
		if (renderState.m_RenderState != null)
			continue;
		renderState.m_RenderPassIdx = m_RenderStates[i].m_RenderPassIdx;
		renderState.m_Options = m_RenderStates[i].m_Options;
		renderState.m_NeededConstants = m_RenderStates[i].m_NeededConstants;

		if (renderState.m_Expected)
		{
			renderState.m_RenderState = CRenderStateManager::RenderThread_ResolveResource(m_RenderStates[i].m_RenderState, args);
			if (renderState.m_RenderState == null)
			{
				CLog::Log(PK_INFO, "Could not create render state");
			}
		}
		const bool	vertexBB =	(renderState.m_Options & Option_VertexBillboarding) ||
								(renderState.m_Options & Option_TriangleVertexBillboarding) ||
								(renderState.m_Options & Option_RibbonVertexBillboarding);
		const bool	gpuStorage = renderState.m_Options & Option_GPUStorage;
		const bool	GPUMesh = renderState.m_Options & Option_GPUMesh;
		if (vertexBB || GPUMesh)
		{
			// Right now, we know the last added constant set layout is the vertex bb one (see MaterialToRHI.cpp)
			const RHI::SShaderBindings	&shaderBindings = CRenderStateManager::RenderThread_GetKey(m_RenderStates[i].m_RenderState).GetGeneratedShaderBindings();
			renderState.m_GPUStorageSimDataConstantSetLayout = shaderBindings.m_ConstantSets.Last(); // copy

																								   // And stream offsets (if any) were added right before
			if (gpuStorage || GPUMesh)
				renderState.m_GPUStorageOffsetsConstantSetLayout = shaderBindings.m_ConstantSets[shaderBindings.m_ConstantSets.Count() - 2]; // copy
		}
	}

	for (u32 i = 0; i < m_ComputeStates.Count(); ++i)
	{
		CRendererCache::SComputeStateFeature	&computeState = rendererCache->m_ComputeStates[i];
		if (computeState.m_ComputeState != null)
			continue;
		computeState.m_Type = m_ComputeStates[i].m_Type;
		if (computeState.m_Expected)
		{
			computeState.m_ComputeState = CComputeStateManager::RenderThread_ResolveResource(m_ComputeStates[i].m_ComputeState, args);
			if (computeState.m_ComputeState == null)
			{
				CLog::Log(PK_INFO, "Could not create compute state");
			}
		}
	}

	return rendererCache;
}

//----------------------------------------------------------------------------

bool	SRendererCacheKey::RenderThread_IsPartiallyBuilt(const PRendererCache &rendererCache) const
{
	if (rendererCache == null)
		return false;
	for (u32 i = 0; i < m_RenderStates.Count(); ++i)
	{
		if (rendererCache->m_RenderStates[i].m_RenderState == null && rendererCache->m_RenderStates[i].m_Expected)
			return true;
	}
	for (u32 i = 0; i < m_ComputeStates.Count(); ++i)
	{
		if (rendererCache->m_ComputeStates[i].m_ComputeState == null && rendererCache->m_ComputeStates[i].m_Expected)
			return true;
	}
	return false;
}

//----------------------------------------------------------------------------

void	SRendererCacheKey::UpdateThread_ReleaseDependencies()
{
	for (const SRenderStateFeature &renderStateFeature : m_RenderStates)
	{
		if (renderStateFeature.m_RenderState.Valid())
			CRenderStateManager::UpdateThread_ReleaseResource(renderStateFeature.m_RenderState);
	}

	for (const SComputeStateFeature &computeStateFeature : m_ComputeStates)
	{
		if (computeStateFeature.m_ComputeState.Valid())
			CComputeStateManager::UpdateThread_ReleaseResource(computeStateFeature.m_ComputeState);
	}
}

//----------------------------------------------------------------------------

bool	SRendererCacheKey::operator == (const SRendererCacheKey &oth) const
{
	if (m_RenderStates.Count() != oth.m_RenderStates.Count())
		return false;
	for (u32 i = 0; i < m_RenderStates.Count(); ++i)
	{
		if (m_RenderStates[i].m_RenderState != oth.m_RenderStates[i].m_RenderState)
			return false;
	}

	if (m_ComputeStates.Count() != oth.m_ComputeStates.Count())
		return false;
	for (u32 i = 0; i < m_ComputeStates.Count(); ++i)
	{
		if (m_ComputeStates[i].m_ComputeState != oth.m_ComputeStates[i].m_ComputeState)
			return false;
	}
	return true;
}

//----------------------------------------------------------------------------
//
//	Renderer Cache Instance Manager
//
//----------------------------------------------------------------------------

PK_NOINLINE bool	SRendererCacheInstanceKey::UpdateThread_Prepare(const SPrepareArg &args)
{
	if (args.m_Renderer->m_RendererType == Renderer_Sound)
		return false;

	//------------------------------------------------------------
	// Create the constant set layout:
	//------------------------------------------------------------
	TArray<MaterialToRHI::STextureProperty>	textureProperties;
	m_ConstantProperties.Clear();
	if (!MaterialToRHI::CreateConstantSetLayout(MaterialToRHI::SGenericArgs(args), "Material", m_ConstSetLayout, &textureProperties, &m_ConstantProperties, &m_ContentHash))
		return false;

	//------------------------------------------------------------
	// Create the renderer cache:
	//------------------------------------------------------------
	SRendererCacheKey	rendererCacheKey;

	m_Cache = CRendererCacheManager::UpdateThread_GetResource(rendererCacheKey, args);
	if (!m_Cache.Valid())
		return false;

	//------------------------------------------------------------
	// Create the textures:
	//------------------------------------------------------------
	for (u32 i = 0; i < textureProperties.Count(); ++i)
	{
		STextureKey						textureKey;
		CTextureManager::CResourceId	textureId;

		textureKey.m_Path = textureProperties[i].m_Property->ValuePath();
		textureKey.m_LoadAsSRGB = textureProperties[i].m_LoadAsSRGB;

		textureId = CTextureManager::UpdateThread_GetResource(textureKey, args);
		if (!textureId.Valid() || !m_Textures.PushBack(textureId).Valid())
			return false;
	}

	//------------------------------------------------------------
	// Create the sampler:
	//------------------------------------------------------------
	if (!m_Samplers.Resize(textureProperties.Count()))
		return false;
	for (u32 i = 0; i < textureProperties.Count(); ++i)
	{
		SConstantSamplerKey		samplerKey;

		samplerKey.m_MagFilter = RHI::SampleLinear;
		samplerKey.m_MinFilter = RHI::SampleLinearMipmapLinear;

		RHI::EWrapMode	wrapMode = RHI::SampleClampToEdge;

		const SRendererFeaturePropertyValue	*textureClamp = args.m_Renderer->m_Declaration.FindProperty(BasicRendererProperties::SID_TextureClamp());
		const SRendererFeaturePropertyValue* textureRepeat = args.m_Renderer->m_Declaration.FindProperty(BasicRendererProperties::SID_TextureRepeat());
		const bool	isLUT = textureProperties[i].m_Property != null &&
							(textureProperties[i].m_Property->m_Name == BasicRendererProperties::SID_AlphaRemap_AlphaMap() ||
							textureProperties[i].m_Property->m_Name == BasicRendererProperties::SID_DiffuseRamp_RampMap());

		const bool	isScrollingTexture = textureProperties[i].m_Property != null &&
										 (textureProperties[i].m_Property->m_Name == BasicRendererProperties::SID_UVDistortions_Distortion1Map() ||
										 textureProperties[i].m_Property->m_Name == BasicRendererProperties::SID_UVDistortions_Distortion2Map() ||
										 textureProperties[i].m_Property->m_Name == BasicRendererProperties::SID_AlphaMasks_Mask1Map() ||
										 textureProperties[i].m_Property->m_Name == BasicRendererProperties::SID_AlphaMasks_Mask2Map());
										

		if (((textureClamp != null && !textureClamp->ValueB()) || (textureRepeat != null && textureRepeat->ValueB())) && !isLUT)
			wrapMode = RHI::SampleRepeat;

		if (isScrollingTexture)
			wrapMode = RHI::SampleRepeat;

#if 1 // Tmp - this should be configurable at texture asset level, and/or at renderer feature property level
		const bool	vatMaterial = args.m_Renderer->m_Declaration.FindProperty(VertexAnimationRendererProperties::SID_VertexAnimation_Fluid()) ||
								  args.m_Renderer->m_Declaration.FindProperty(VertexAnimationRendererProperties::SID_VertexAnimation_Soft()) ||
								  args.m_Renderer->m_Declaration.FindProperty(VertexAnimationRendererProperties::SID_VertexAnimation_Rigid());

		if (vatMaterial)
		{
			if (textureProperties[i].m_Property->m_Name.ToString().Contains("VertexAnimation"))
			{
				samplerKey.m_MagFilter = RHI::SampleNearest;
				samplerKey.m_MinFilter = RHI::SampleNearest;
				wrapMode = RHI::SampleRepeat;
			}
		}
#endif

		samplerKey.m_WrapU = wrapMode;
		samplerKey.m_WrapV = wrapMode;
		samplerKey.m_WrapW = wrapMode;

		samplerKey.m_MipmapCount = 16;
		m_Samplers[i] = CConstantSamplerManager::UpdateThread_GetResource(samplerKey, args);
		if (!m_Samplers[i].Valid())
			return false;
	}

	const SRendererDeclaration	&decl = args.m_Renderer->m_Declaration;
	m_HasAtlas = decl.IsFeatureEnabled(BasicRendererProperties::SID_Atlas());

	m_HasRawUV0 = m_HasAtlas && (args.m_Renderer->m_RendererType == Renderer_Billboard || args.m_Renderer->m_RendererType == Renderer_Ribbon) &&(decl.IsFeatureEnabled(BasicRendererProperties::SID_AlphaMasks()) || decl.IsFeatureEnabled(BasicRendererProperties::SID_UVDistortions()));

	if (args.m_Renderer->m_RendererType == Renderer_Billboard ||
		args.m_Renderer->m_RendererType == Renderer_Ribbon ||
		args.m_Renderer->m_RendererType == Renderer_Decal ||
		args.m_Renderer->m_RendererType == Renderer_Mesh)
	{
		// Create Atlas
		if (m_HasAtlas)
		{
			SConstantAtlasKey							atlasKey;
			const BasicRendererProperties::EAtlasSource	atlasSource = decl.GetPropertyValue_Enum<BasicRendererProperties::EAtlasSource>(BasicRendererProperties::SID_Atlas_Source(), BasicRendererProperties::EAtlasSource::External);
			if (atlasSource == BasicRendererProperties::EAtlasSource::External)
				atlasKey.m_Path = decl.GetPropertyValue_Path(BasicRendererProperties::SID_Atlas_Definition(), CString::EmptyString);
			else // Subdivision count
			{
				const CInt2	&subDivCount = decl.GetPropertyValue_I2(BasicRendererProperties::SID_Atlas_SubDiv(), CInt2::ZERO);

				// Custom name, as we can't rely on comparing CRectangleList strong pointers right now
				atlasKey.m_Path = CString::Format("$_ProceduralAtlas_$/%d.%d", subDivCount.x(), subDivCount.y());
			}

			// Warning: This will create a duplicate atlas for Procedural atlases
			TResourcePtr<CRectangleList>	atlasResource;
			if (LoadRendererAtlas(args.m_Renderer, args.m_ResourceManager, atlasResource))
			{
				atlasKey.m_SourceAtlas = &(*atlasResource); // <--
			}

			m_Atlas = CConstantAtlasManager::UpdateThread_GetResource(atlasKey, args);
		}
	}
	if (args.m_Renderer->m_RendererType == Renderer_Mesh)
	{
		SGeometryKey				geometryKey;
		const CString				&meshPath = decl.GetPropertyValue_Path(BasicRendererProperties::SID_Mesh(), CString::EmptyString);
		TResourcePtr<CResourceMesh>	meshResource = args.m_ResourceManager->Load<CResourceMesh>(meshPath, false, SResourceLoadCtl(false, true));
		if (meshResource != null && !meshResource->Empty())
		{
			geometryKey.m_GeomType = SGeometryKey::Geom_Mesh;
			geometryKey.m_Path = meshPath;
		}

		m_Geometry = CGeometryManager::UpdateThread_GetResource(geometryKey, args);
	}
	else if (args.m_Renderer->m_RendererType == Renderer_Light)
	{
		SGeometryKey	geometryKey;
		geometryKey.m_GeomType = SGeometryKey::Geom_Sphere;
		m_Geometry = CGeometryManager::UpdateThread_GetResource(geometryKey, args);
	}
	if (args.m_Renderer->m_RendererType == Renderer_Decal)
	{
		SGeometryKey	geometryKey;
		geometryKey.m_GeomType = SGeometryKey::Geom_Cube;
		m_Geometry = CGeometryManager::UpdateThread_GetResource(geometryKey, args);
	}
	return true;
}

//----------------------------------------------------------------------------

PRendererCacheInstance	SRendererCacheInstanceKey::RenderThread_CreateResource(const SCreateArg &args)
{
	PRendererCacheInstance	rendererCacheInstance = PK_NEW(CRendererCacheInstance);
	if (rendererCacheInstance == null)
	{
		CLog::Log(PK_ERROR, "Could not create renderer instance");
		return null;
	}

	rendererCacheInstance->m_HasAtlas = m_HasAtlas;
	rendererCacheInstance->m_HasRawUV0 = m_HasRawUV0;

	// Base renderer cache:
	if (m_Cache.Valid())
	{
		rendererCacheInstance->m_Cache = CRendererCacheManager::RenderThread_ResolveResource(m_Cache, args);
		if (rendererCacheInstance->m_Cache == null)
		{
			CLog::Log(PK_ERROR, "Could not create renderer cache");
			return null;
		}
	}

	// Constant buffer:
	rendererCacheInstance->m_ConstValues = null;
	if (!m_ConstSetLayout.m_Constants.Empty())
	{
		const RHI::SConstantSetLayout::SConstantDesc	&constDesc = m_ConstSetLayout.m_Constants.Last();

		if (constDesc.m_Type == RHI::TypeConstantBuffer)
		{
			const RHI::SConstantBufferDesc	&bufferDesc = constDesc.m_ConstantBuffer;

			rendererCacheInstance->m_ConstValues = args.m_ApiManager->CreateGpuBuffer(RHI::SRHIResourceInfos("Material Constant Values Buffer"), RHI::ConstantBuffer, bufferDesc.m_ConstantBufferSize, RHI::UsageStaticDraw);
			if (rendererCacheInstance->m_ConstValues == null)
			{
				CLog::Log(PK_ERROR, "Could not create GPU buffer for material constant values");
				return null;
			}

			// Fill the constant buffer
			void	*buffer = args.m_ApiManager->MapCpuView(rendererCacheInstance->m_ConstValues);
			for (u32 i = 0; i < m_ConstantProperties.Count(); ++i)
			{
				const RHI::SConstantVarDesc	&varDesc = bufferDesc.m_Constants[i];
				const SRendererFeaturePropertyValue	&prop = m_ConstantProperties[i];

				const u32	datasize = varDesc.RawSizeInBytes();
				void		*dataPtr = Mem::AdvanceRawPointer(buffer, varDesc.m_OffsetInBuffer);

				switch (prop.m_Type)
				{
				case PropertyType_Bool1:
				case PropertyType_Feature:
				{
					PK_ASSERT(datasize == sizeof(CInt1));
					const CInt1 valueI( (prop.m_ValueB) ? 1 : 0 );
					Mem::Copy_Uncached(dataPtr, &valueI, datasize);
					break;
				}
				case PropertyType_Int1:
				case PropertyType_Int2:
				case PropertyType_Int3:
				case PropertyType_Int4:
				case PropertyType_Enum:
					PK_ASSERT(datasize <= sizeof(CInt4));
					Mem::Copy_Uncached(dataPtr, prop.m_ValueI, datasize);
					break;
				case PropertyType_Float1:
				case PropertyType_Float2:
				case PropertyType_Float3:
				case PropertyType_Float4:
				case PropertyType_Orientation:
					PK_ASSERT(datasize <= sizeof(CFloat4));
					Mem::Copy_Uncached(dataPtr, prop.m_ValueF, datasize);
					break;
				default:
					PK_ASSERT_NOT_REACHED();
					Mem::Clear_Uncached(dataPtr, datasize);
					break;
				}
			}
			args.m_ApiManager->UnmapCpuView(rendererCacheInstance->m_ConstValues);
		}
	}

	// Constant textures:
	if (!rendererCacheInstance->m_ConstTextures.Resize(m_Textures.Count()))
	{
		CLog::Log(PK_ERROR, "Could not allocate texture set for renderer cache");
		return null;
	}
	for (u32 i = 0; i < m_Textures.Count(); ++i)
	{
		rendererCacheInstance->m_ConstTextures[i] = CTextureManager::RenderThread_ResolveResource(m_Textures[i], args);
		if (rendererCacheInstance->m_ConstTextures[i] == null)
		{
			CLog::Log(PK_ERROR, "Could not create textures for renderer cache");
			rendererCacheInstance->m_ConstTextures[i] = CTextureManager::RenderThread_ResolveResource(STextureKey::s_DefaultResourceID, args);
			if (rendererCacheInstance->m_ConstTextures[i] == null)
				return null;
		}
	}

	// Constant sampler:
	if (!rendererCacheInstance->m_ConstSamplers.Resize(m_Samplers.Count()))
	{
		CLog::Log(PK_ERROR, "Could not allocate samplers for renderer cache");
		return null;
	}
	for (u32 i = 0; i < m_Samplers.Count(); ++i)
	{
		rendererCacheInstance->m_ConstSamplers[i] = CConstantSamplerManager::RenderThread_ResolveResource(m_Samplers[i], args);
		if (rendererCacheInstance->m_ConstSamplers[i] == null)
		{
			CLog::Log(PK_ERROR, "Could not create samplers for renderer cache");
			return null;
		}
	}

	// Constant set:
	rendererCacheInstance->m_ConstSet = null;
	if (!m_ConstSetLayout.m_Constants.Empty())
	{
		u32		textureIdx = 0;
		bool	success = true;

		rendererCacheInstance->m_ConstSet = args.m_ApiManager->CreateConstantSet(RHI::SRHIResourceInfos("Material Constant Set"), m_ConstSetLayout);
		if (rendererCacheInstance->m_ConstSet == null)
		{
			CLog::Log(PK_ERROR, "Could not create constant set for renderer cache");
			return null;
		}
		for (u32 i = 0; i < m_ConstSetLayout.m_Constants.Count(); ++i)
		{
			if (m_ConstSetLayout.m_Constants[i].m_Type == RHI::TypeConstantSampler)
			{
				PK_ASSERT_MESSAGE(textureIdx == i, "All the textures should be the first constants in the constant set");
				success &= rendererCacheInstance->m_ConstSet->SetConstants(	rendererCacheInstance->m_ConstSamplers[textureIdx],
																			rendererCacheInstance->m_ConstTextures[textureIdx],
																			i);
				++textureIdx;
			}
			else if (m_ConstSetLayout.m_Constants[i].m_Type == RHI::TypeConstantBuffer)
			{
				PK_ASSERT_MESSAGE(i == m_ConstSetLayout.m_Constants.Count() - 1, "The GPU buffer should be the last thing in the constant set");
				success &= rendererCacheInstance->m_ConstSet->SetConstants(rendererCacheInstance->m_ConstValues, i);
			}
		}
		success &= rendererCacheInstance->m_ConstSet->UpdateConstantValues();
		if (!success)
		{
			CLog::Log(PK_ERROR, "Could not update constant set for renderer cache");
			return null;
		}
	}

	// Specific geometry
	if (m_Geometry.Valid())
	{
		rendererCacheInstance->m_AdditionalGeometry = CGeometryManager::RenderThread_ResolveResource(m_Geometry, args);
		if (rendererCacheInstance->m_AdditionalGeometry == null ||
			rendererCacheInstance->m_AdditionalGeometry->m_PerGeometryViews.Count() == 0)
		{
			CLog::Log(PK_ERROR, "Could not create the mesh for the renderer cache");
			rendererCacheInstance->m_AdditionalGeometry = CGeometryManager::RenderThread_ResolveResource(SGeometryKey::s_DefaultResourceID, args);
			rendererCacheInstance->m_Cache = null; // fallback
		}
	}

	// Atlas
	if (m_Atlas.Valid())
		rendererCacheInstance->m_Atlas = CConstantAtlasManager::RenderThread_ResolveResource(m_Atlas, args);

	return rendererCacheInstance;
}

//----------------------------------------------------------------------------

void	SRendererCacheInstanceKey::UpdateThread_ReleaseDependencies()
{
	if (m_Cache.Valid())
		CRendererCacheManager::UpdateThread_ReleaseResource(m_Cache);
	for (u32 i = 0; i < m_Textures.Count(); ++i)
	{
		if (m_Textures[i].Valid())
		{
			CTextureManager::UpdateThread_ReleaseResource(m_Textures[i]);
		}
		if (m_Samplers[i].Valid())
			CConstantSamplerManager::UpdateThread_ReleaseResource(m_Samplers[i]);
	}
	if (m_Geometry.Valid())
		CGeometryManager::UpdateThread_ReleaseResource(m_Geometry);
}

//----------------------------------------------------------------------------

bool	SRendererCacheInstanceKey::operator == (const SRendererCacheInstanceKey &oth) const
{
	return (m_Atlas == oth.m_Atlas &&
			m_HasAtlas == oth.m_HasAtlas &&
			m_HasRawUV0 == oth.m_HasRawUV0 &&
			m_Geometry == oth.m_Geometry &&
			m_Samplers == oth.m_Samplers &&
			m_Cache == oth.m_Cache &&
			m_ContentHash == oth.m_ContentHash &&
			m_Textures == oth.m_Textures &&
			m_ConstSetLayout == oth.m_ConstSetLayout);
}

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
