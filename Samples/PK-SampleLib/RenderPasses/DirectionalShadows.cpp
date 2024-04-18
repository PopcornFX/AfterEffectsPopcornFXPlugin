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

#include "DirectionalShadows.h"

#include <PK-SampleLib/ShaderDefinitions/UnitTestsShaderDefinitions.h>
#include <PK-SampleLib/ShaderDefinitions/BasicSceneShaderDefinitions.h>

#include <pk_rhi/include/AllInterfaces.h>
#include <pk_rhi/include/PixelFormatFallbacks.h>
#include <pk_geometrics/include/ge_coordinate_frame.h>

#define	MESHSHADOW_VERTEX_SHADER_PATH		"./Shaders/SolidMeshShadow.vert"
#define	MESHSHADOW_FRAGMENT_SHADER_PATH		"./Shaders/SolidMeshShadow.frag"

#define	POSTFX_VERTEX_SHADER_PATH	"./Shaders/FullScreenQuad.vert"
#define	POSTFX_FRAGMENT_SHADER_PATH	"./Shaders/GaussianBlur.frag"

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

static CFloat4x4 CreateOrtho(float b, float t, float l, float r, float n, float f) 
{
	CFloat4x4	ortho = CFloat4x4::IDENTITY;

	ortho.XAxis().x() = 2.0f / (r - l);
	ortho.YAxis().y() = 2.0f / (t - b);
	ortho.ZAxis().z() = -1.0f / (n - f);
	ortho.WAxis() = CFloat4(-((r + l) / (r - l)), -((t + b) / (t - b)), -(n / (n - f)), 1.0f);
	ortho.Transpose();
	return ortho;
} 

static	float	UnlinearizeDepth(float linearDepth, float n, float f)
{
	return (n * f / linearDepth - f) / (n - f);
}

static	float	LinearizeDepth(float logDepth, float n, float f)
{
    return n * f / (f + logDepth * (n - f));
}
//----------------------------------------------------------------------------

CDirectionalShadows::CDirectionalShadows()
:	m_ApiManager(null)
,	m_WorldCameraFrustumBBox(CAABB::DEGENERATED)
,	m_BackdropShadowRenderState(null)
,	m_BlurHRenderState(null)
,	m_BlurVRenderState(null)
,	m_RenderPass(null)
,	m_SamplerRT(null)
,	m_FullScreenQuadBuffer(null)
,	m_LightDir(CFloat3::ZERO)
,	m_DrawCoordFrame(__MaxCoordinateFrames)
,	m_BackdropCoordFrame(__MaxCoordinateFrames)
,	m_MeshToDrawFrame(CFloat4x4::IDENTITY)
,	m_LightTransform(CFloat4x4::IDENTITY)
,	m_CasterLightAlignedBBox(CAABB::DEGENERATED)
,	m_ReceiverViewAlignedBBox(CAABB::DEGENERATED)
,	m_ReceiverWorldAlignedBBox(CAABB::DEGENERATED)
,	m_CasterViewAlignedBBox(CAABB::DEGENERATED)
,	m_CasterWorldAlignedBBox(CAABB::DEGENERATED)
,	m_ShadowBias(0)
,	m_ShadowVariancePower(1.2)
,	m_EnableShadows(true)
,	m_EnableVariance(true)
,	m_DebugShadows(false)
,	m_IdealDepthFormat(RHI::EPixelFormat::FormatUnorm16Depth)
{
}

//----------------------------------------------------------------------------

CDirectionalShadows::~CDirectionalShadows()
{
}

//----------------------------------------------------------------------------

bool	CDirectionalShadows::Init(	const RHI::PApiManager &apiManager,
									const RHI::SConstantSetLayout &sceneInfoLayout,
									const RHI::PConstantSampler &samplerRT,
									const RHI::SConstantSetLayout &samplerRTLayout,
									const RHI::PGpuBuffer &fullScreenQuadVbo)
{
	m_ApiManager = apiManager;
	m_SceneInfoLayout = sceneInfoLayout;
	m_SamplerRT = samplerRT;
	m_SamplerRTLayout = samplerRTLayout;
	m_FullScreenQuadBuffer = fullScreenQuadVbo;

	m_DrawCoordFrame = __MaxCoordinateFrames;
	m_BackdropCoordFrame = __MaxCoordinateFrames;

	const RHI::SGPUCaps &caps = m_ApiManager->GetApiContext()->m_GPUCaps;
	m_IdealDepthFormat = RHI::PixelFormatFallbacks::FindClosestSupportedDepthStencilFormat(caps, RHI::FormatUnorm16Depth);

	if (caps.IsPixelFormatSupported(IDEAL_SHADOW_DEPTH_FORMAT, RHI::FormatUsage_RenderTarget | RHI::FormatUsage_ShaderSampling))
		m_IdealShadowFormat = IDEAL_SHADOW_DEPTH_FORMAT;
	else
		m_IdealShadowFormat = RHI::FormatFloat16RG;

	CreateSimpleSamplerConstSetLayouts(m_BlurConstSetLayout, false);
	return _CreateRenderPass();
}

//----------------------------------------------------------------------------

bool	CDirectionalShadows::UpdateSceneInfo(const PKSample::SSceneInfoData &sceneInfoData)
{
	m_SceneInfoData = sceneInfoData;
	m_WorldCameraFrustumBBox = _GetFrustumBBox(m_SceneInfoData.m_InvViewProj, 0.0f, 1.0f);
	return true;
}

//----------------------------------------------------------------------------

bool	CDirectionalShadows::_CreateRenderPass()
{
	m_RenderPass = m_ApiManager->CreateRenderPass(RHI::SRHIResourceInfos("Shadow render pass"));
	if (m_RenderPass == null)
		return false;

	bool						result = true;
	RHI::SSubPassDefinition		solidDepth;
	result &= solidDepth.m_OutputRenderTargets.PushBack(0).Valid();
	solidDepth.m_DepthStencilRenderTarget = 2;
	result &= m_RenderPass->AddRenderSubPass(solidDepth);

	RHI::SSubPassDefinition		renderHBlur;
	result &= renderHBlur.m_InputRenderTargets.PushBack(0).Valid();
	result &= renderHBlur.m_OutputRenderTargets.PushBack(1).Valid();
	result &= m_RenderPass->AddRenderSubPass(renderHBlur);

	RHI::SSubPassDefinition		renderVBlur;
	result &= renderVBlur.m_InputRenderTargets.PushBack(1).Valid();
	result &= renderVBlur.m_OutputRenderTargets.PushBack(0).Valid();
	result &= m_RenderPass->AddRenderSubPass(renderVBlur);

	// Baking
	const RHI::ELoadRTOperation			loadRt[] = { RHI::LoadClear, RHI::LoadClear, RHI::LoadClear };

	m_FrameBufferLayout[0].m_Format = m_IdealShadowFormat;
	m_FrameBufferLayout[1].m_Format = m_IdealShadowFormat;
	m_FrameBufferLayout[2].m_Format = m_IdealDepthFormat;
	
	result &= m_RenderPass->BakeRenderPass(m_FrameBufferLayout, loadRt);
	return result;
}

//----------------------------------------------------------------------------

bool	CDirectionalShadows::_CreateMeshBackdropRenderStates(CShaderLoader &loader, ECoordinateFrame drawFrame, ECoordinateFrame meshFrame)
{
	CShaderLoader::SShadersPaths	shadersPaths;
	shadersPaths.m_Vertex = MESHSHADOW_VERTEX_SHADER_PATH;
	shadersPaths.m_Fragment = MESHSHADOW_FRAGMENT_SHADER_PATH;

	m_BackdropShadowRenderState = m_ApiManager->CreateRenderState(RHI::SRHIResourceInfos("Shadow render state"));

	if (m_BackdropShadowRenderState == null)
		return false;

	// Pipeline state solid color:
	const bool				drawRightHanded = CCoordinateFrame::IsRightHanded(drawFrame);
	const bool				flippedHandedness = CCoordinateFrame::IsRightHanded(meshFrame) != drawRightHanded;

	m_BackdropShadowRenderState->m_RenderState.m_PipelineState.m_DynamicScissor = true;
	m_BackdropShadowRenderState->m_RenderState.m_PipelineState.m_DynamicViewport = true;
	m_BackdropShadowRenderState->m_RenderState.m_PipelineState.m_DepthWrite = true;
	m_BackdropShadowRenderState->m_RenderState.m_PipelineState.m_DepthTest = RHI::Less;
	m_BackdropShadowRenderState->m_RenderState.m_PipelineState.m_CullMode = RHI::CullBackFaces;
	m_BackdropShadowRenderState->m_RenderState.m_PipelineState.m_PolyOrder = (drawRightHanded ^ flippedHandedness) ? RHI::FrontFaceCounterClockWise : RHI::FrontFaceClockWise;

	if (!m_BackdropShadowRenderState->m_RenderState.m_InputVertexBuffers.Resize(1))
		return false;

	m_BackdropShadowRenderState->m_RenderState.m_InputVertexBuffers[0].m_Stride = sizeof(CFloat3);

	FillGBufferShadowShaderBindings(m_BackdropShadowRenderState->m_RenderState.m_ShaderBindings, m_SceneInfoLayout);

	m_BackdropShadowRenderState->m_RenderState.m_ShaderBindings.m_InputAttributes[0].m_BufferIdx = 0;

	if (!loader.LoadShader(m_BackdropShadowRenderState->m_RenderState, shadersPaths, m_ApiManager))
		return false;

	PK_ASSERT(m_CascadedShadows.First().m_FrameBuffer != null);
	if (!m_ApiManager->BakeRenderState(m_BackdropShadowRenderState, m_CascadedShadows.First().m_FrameBuffer->GetLayout(), m_RenderPass, 0))
		return false;

	m_BlurHRenderState = m_ApiManager->CreateRenderState(RHI::SRHIResourceInfos("Shadow Blur Horizontal"));
	m_BlurVRenderState = m_ApiManager->CreateRenderState(RHI::SRHIResourceInfos("Shadow Blur Vertical"));

	if (m_BlurHRenderState == null || m_BlurVRenderState == null)
		return false;

	// Sets the basic viewport / scissor test
	m_BlurHRenderState->m_RenderState.m_PipelineState.m_DynamicScissor = true;
	m_BlurHRenderState->m_RenderState.m_PipelineState.m_DynamicViewport = true;

	// Describe the vertex input layout
	// We have 2 vertex buffers and 2 attributes
	if (!m_BlurHRenderState->m_RenderState.m_InputVertexBuffers.Resize(1))
		return false;

	m_BlurHRenderState->m_RenderState.m_InputVertexBuffers[0].m_Stride = sizeof(CFloat2); // Position for full screen quad

	FillGaussianBlurShaderBindings(	GaussianBlurCombination_9_Tap,
									m_BlurHRenderState->m_RenderState.m_ShaderBindings,
									m_SamplerRTLayout);

	CShaderLoader::SShadersPaths	shadersPathsBlur;
	shadersPathsBlur.m_Vertex = POSTFX_VERTEX_SHADER_PATH;
	shadersPathsBlur.m_Fragment = POSTFX_FRAGMENT_SHADER_PATH;

	if (!loader.LoadShader(m_BlurHRenderState->m_RenderState, shadersPathsBlur, m_ApiManager))
		return false;

	m_BlurVRenderState->m_RenderState = m_BlurHRenderState->m_RenderState;

	if (!m_ApiManager->BakeRenderState(m_BlurHRenderState, m_CascadedShadows.First().m_FrameBuffer->GetLayout(), m_RenderPass, 1))
		return false;
	if (!m_ApiManager->BakeRenderState(m_BlurVRenderState, m_CascadedShadows.First().m_FrameBuffer->GetLayout(), m_RenderPass, 2))
		return false;
	return true;
}

//----------------------------------------------------------------------------

bool	CDirectionalShadows::_CreateSliceData(SCascadedShadowSlice &slice)
{
	RHI::PRenderTarget	rts[] =
	{
		slice.m_ShadowMap.m_RenderTarget,
		slice.m_IntermediateRt.m_RenderTarget,
		slice.m_DepthMap.m_RenderTarget
	};
	if (!Utils::CreateFrameBuffer(RHI::SRHIResourceInfos("Shadow frame buffer"), m_ApiManager, rts, slice.m_FrameBuffer, m_RenderPass))
		return false;

	slice.m_BlurHConstantSet = m_ApiManager->CreateConstantSet(RHI::SRHIResourceInfos("Constant Set For Shadow Horizontal Blur"), m_BlurConstSetLayout);
	slice.m_BlurVConstantSet = m_ApiManager->CreateConstantSet(RHI::SRHIResourceInfos("Constant Set For Shadow Vertical Blur"), m_BlurConstSetLayout);

	if (slice.m_BlurHConstantSet == null || slice.m_BlurVConstantSet == null)
		return false;

	bool	success = true;

	success &= slice.m_BlurHConstantSet->SetConstants(m_SamplerRT, slice.m_ShadowMap.m_RenderTarget->GetTexture(), 0);
	success &= slice.m_BlurHConstantSet->UpdateConstantValues();
	success &= slice.m_BlurVConstantSet->SetConstants(m_SamplerRT, slice.m_IntermediateRt.m_RenderTarget->GetTexture(), 0);
	success &= slice.m_BlurVConstantSet->UpdateConstantValues();
	return success;
}

//----------------------------------------------------------------------------

CAABB	CDirectionalShadows::_GetFrustumBBox(CFloat4x4 invViewProj, float nearPlane, float farPlane, const CFloat4x4 &postProjTransform)
{
	const CFloat4	kFrustumCorners[8] =
	{
		CFloat4(-1, -1, nearPlane, 1),
		CFloat4(1, -1, nearPlane, 1),
		CFloat4(-1, 1, nearPlane, 1),
		CFloat4(1, 1, nearPlane, 1),
		CFloat4(-1, -1, farPlane, 1),
		CFloat4(1, -1, farPlane, 1),
		CFloat4(-1, 1, farPlane, 1),
		CFloat4(1, 1, farPlane, 1),
	};

	CAABB	camBBoxFrustum = CAABB::DEGENERATED;
	for (u32 j = 0; j < 8; ++j)
	{
		const CFloat4	unprojectedCorner = invViewProj.TransformVector(kFrustumCorners[j]);
		const CFloat3	cornerPostProj = unprojectedCorner.xyz() / unprojectedCorner.w();
		const CFloat3	cornerTransformed = postProjTransform.TransformVector(cornerPostProj);
		camBBoxFrustum.Add(cornerTransformed);
	}
	return camBBoxFrustum;
}

//----------------------------------------------------------------------------

void	CDirectionalShadows::_ExtendCasterBBox(CAABB &caster, const CAABB &receiver, const CFloat3 &lightDirection) const
{
	// Projects the shadow caster BBox on the shadow receiver BBox to get the shadow BBox:
	for (u32 i = 0; i < 8; ++i)
	{
		const CFloat3			viewSpaceCorner = caster.ExtractCorner(i);
		const CRay				ray = CRay(viewSpaceCorner, lightDirection);
		// Collide with the 6 planes of the receiver BBox:
		for (u32 j = 0; j < 6; ++j)
		{
			const u32				currentAxis = j % 3;
			CFloat3					normal = CFloat3::ZERO;
			normal.Axis(currentAxis) = j < 3 ? -1.0f : 1.0f;
			const CPlane			plane(normal, j < 3 ? receiver.Min() : receiver.Max());
			TPrimitiveHitReport3D	hit;

			if (Colliders::RayTrace(plane, ray, hit) && hit.t > 0.0f)
			{
				CFloat3		centeredPoint = hit.point;
				centeredPoint.Axis(currentAxis) = receiver.Center().Axis(currentAxis);
				if (receiver.Contains(centeredPoint))
					caster.Add(hit.point);
			}
		}
	}
}

//----------------------------------------------------------------------------

bool	CDirectionalShadows::InitFrameUpdateSceneInfo(const CFloat3 &lightDir, ECoordinateFrame drawFrame, CShaderLoader &loader, const PKSample::SMesh &backdrop, bool castShadows)
{
	if (m_DrawCoordFrame != drawFrame)
		m_DrawCoordFrame = drawFrame;
	m_LightDir = lightDir.Normalized();

	// Compute light rotation matrix:
	const CFloat3		forward = m_LightDir;
	// Safe cross product to find up vector:
	CFloat3				up = PKAbs(CCoordinateFrame::AxisSide().Dot(forward)) < 0.995f ?
								forward.Cross(CCoordinateFrame::AxisSide()).Normalized() :
								forward.Cross(CCoordinateFrame::AxisVertical().Normalized());
	const CFloat3		side = forward.Cross(up).Normalized();
	up = forward.Cross(side);

	// Basic rotation matrix aligned with world axis:
	const CFloat4x4			lightRotation = CFloat4x4(side.xyz0(), up.xyz0(), forward.xyz0(), CFloat4(0, 0, 0, 1));

	PK_ASSERT(lightRotation.Orthonormal());
	m_LightTransform = lightRotation.Inverse();
	m_CasterLightAlignedBBox = CAABB::DEGENERATED;
	m_CasterViewAlignedBBox = CAABB::DEGENERATED;
	m_ReceiverViewAlignedBBox = CAABB::DEGENERATED;
	m_CasterWorldAlignedBBox = CAABB::DEGENERATED;
	m_ReceiverWorldAlignedBBox = CAABB::DEGENERATED;

	for (u32 i = 0; i < m_CascadedShadows.Count(); ++i)
	{
		SCascadedShadowSlice	&slice = m_CascadedShadows[i];
		slice.m_IsValid = true;
		slice.m_InvView = lightRotation;
		slice.m_View = m_LightTransform;
		slice.m_ShadowSliceViewAABB = CAABB::DEGENERATED;
	}

	if (m_BackdropCoordFrame != backdrop.m_MeshBatchCoordinateFrame)
	{
		m_BackdropCoordFrame = backdrop.m_MeshBatchCoordinateFrame;
		_CreateMeshBackdropRenderStates(loader, m_DrawCoordFrame, m_BackdropCoordFrame);
		CCoordinateFrame::BuildTransitionFrame(m_BackdropCoordFrame, m_DrawCoordFrame, m_MeshToDrawFrame);
	}

	// Transform each corner of the meshes AABB in light transform space and recompute AABB:
	for (u32 instanceIdx = 0; instanceIdx < backdrop.m_Transforms.Count(); ++instanceIdx)
	{
		const CFloat4x4	meshTransform = backdrop.m_Transforms[instanceIdx];

		for (u32 meshIdx = 0; meshIdx < backdrop.m_MeshBatches.Count(); ++meshIdx)
		{
			const CAABB		meshBBox = backdrop.m_MeshBatches[meshIdx].m_BBox;
			if (meshBBox.Valid())
			{
				AddBBox(meshBBox, m_MeshToDrawFrame * meshTransform, castShadows);
			}
		}
	}
	return true;
}

//----------------------------------------------------------------------------

void	CDirectionalShadows::AddBBox(const CAABB &bbox, const CFloat4x4 &transform, bool castShadows)
{
	const float epsilon = 0.001f;
	const CAABB	scaledBBox = bbox.ScaledFromCenter(1.0f + epsilon);

	// We compute a view-space BBox and a world BBox to get the min/max depths:
	CAABB		worldBBox;
	worldBBox.SetupFromOBB(scaledBBox, transform);

	CAABB		viewBBox;
	viewBBox.SetupFromOBB(scaledBBox, transform * m_SceneInfoData.m_View);

	m_ReceiverViewAlignedBBox.Add(viewBBox);
	m_ReceiverWorldAlignedBBox.Add(worldBBox);

	if (!castShadows)
		return;

	m_CasterViewAlignedBBox.Add(viewBBox);
	m_CasterWorldAlignedBBox.Add(worldBBox);

	// We compute a world BBox and a light-space BBox and constraint both depending on the light direction
	// Computing both those BBox helps getting the smallest as possible BBox:
	CAABB		viewSpaceBBox;
	viewSpaceBBox.SetupFromOBB(scaledBBox, transform * m_LightTransform);

	// World space culling with the light direction:
	CAABB					frsutumFittedBBoxWorld = worldBBox;
	for (u32 j = 0; j < 3; ++j)
	{
		if (m_LightDir.Axis(j) <= 0 && worldBBox.Max().Axis(j) > m_WorldCameraFrustumBBox.Min().Axis(j))
			frsutumFittedBBoxWorld.Min().Axis(j) = PKMax(worldBBox.Min().Axis(j), m_WorldCameraFrustumBBox.Min().Axis(j));
		if (m_LightDir.Axis(j) >= 0 && worldBBox.Min().Axis(j) < m_WorldCameraFrustumBBox.Max().Axis(j))
			frsutumFittedBBoxWorld.Max().Axis(j) = PKMin(worldBBox.Max().Axis(j), m_WorldCameraFrustumBBox.Max().Axis(j));
	}
	// Then transform this to light-space
	CAABB		fittedViewSpaceBBox;
	fittedViewSpaceBBox.SetupFromOBB(frsutumFittedBBoxWorld, m_LightTransform);

	if (fittedViewSpaceBBox.Valid())
	{
		fittedViewSpaceBBox.Min() = PKMax(fittedViewSpaceBBox.Min(), viewSpaceBBox.Min());
		fittedViewSpaceBBox.Max() = PKMin(fittedViewSpaceBBox.Max(), viewSpaceBBox.Max());
		m_CasterLightAlignedBBox.Add(fittedViewSpaceBBox);
	}
}

//----------------------------------------------------------------------------

bool	CDirectionalShadows::FinalizeFrameUpdateSceneInfo()
{
	// Early out if there is no valid shadow caster BBox:
	if (!m_CasterLightAlignedBBox.Valid())
	{
		for (u32 i = 0; i < m_CascadedShadows.Count(); ++i)
		{
			m_CascadedShadows[i].m_IsValid = false;
			m_CascadedShadows[i].m_DepthRangeMin = -1;
			m_CascadedShadows[i].m_DepthRangeMax = -1;
		}
		return true;
	}
	// We start by computing the caster min/max scene depth.
	// To do that we start by projecting the caster BBox corners on the receiver BBox depending on the light direction
	// We do this twice, once in view space and once in world space:
	const CFloat3	viewSpaceLightDirection = m_SceneInfoData.m_View.RotateVector(m_LightDir);
	_ExtendCasterBBox(m_CasterViewAlignedBBox, m_ReceiverViewAlignedBBox, viewSpaceLightDirection);
	// Now we just have to project the caster BBox min/max with the projection matrix:
	// (min and max are inverted because the BBox is in view-space and not inv-view-space)
	CFloat4		projCasterMin = m_SceneInfoData.m_Proj.TransformVector(CFloat4(m_CasterViewAlignedBBox.Max(), 1.0f));
	CFloat4		projCasterMax = m_SceneInfoData.m_Proj.TransformVector(CFloat4(m_CasterViewAlignedBBox.Min(), 1.0f));
	projCasterMin.z() = PKMax(projCasterMin.z(), 0.0f);
	projCasterMax.z() = PKMax(projCasterMax.z(), 0.0f);
	const float	viewSpacecastSceneMinDepth = projCasterMin.z() / projCasterMin.w();
	const float	viewSpacecastSceneMaxDepth = projCasterMax.z() / projCasterMax.w();
	// We do the same thing in world-space to get the smallest possible BBox:
	_ExtendCasterBBox(m_CasterWorldAlignedBBox, m_ReceiverWorldAlignedBBox, m_LightDir);
	// Now we project the 8 corners of the caster world-space BBox with the view/projection matrix:
	float	worldSpacecastSceneMinDepth = 1.0f;
	float	worldSpacecastSceneMaxDepth = 0.0f;
	for (u32 i = 0; i < 8; ++i)
	{
		const CFloat3	worldSpaceCorner = m_CasterWorldAlignedBBox.ExtractCorner(i);
		CFloat4			projCaster = m_SceneInfoData.m_ViewProj.TransformVector(CFloat4(worldSpaceCorner, 1.0f));
		projCaster.z() = PKMax(projCaster.z(), 0);
		float			cornerDepth = projCaster.z() / projCaster.w();
		worldSpacecastSceneMinDepth = PKMin(cornerDepth, worldSpacecastSceneMinDepth);
		worldSpacecastSceneMaxDepth = PKMax(cornerDepth, worldSpacecastSceneMaxDepth);
	}
	// Get the smallest range of the 2 ranges computed:
	const float	castSceneMinDepth = PKMax(viewSpacecastSceneMinDepth, worldSpacecastSceneMinDepth);
	const float	castSceneMaxDepth = PKMin(viewSpacecastSceneMaxDepth, worldSpacecastSceneMaxDepth);
	// Now we can compute the distance for each slice depending on the depths computed above:
	const CFloat2	&nearFar = m_SceneInfoData.m_ZBufferLimits;
	const float		linearSceneDepthMin = LinearizeDepth(castSceneMinDepth, nearFar.x(), nearFar.y());
	const float		linearSceneDepthMax = LinearizeDepth(castSceneMaxDepth, nearFar.x(), nearFar.y());
	const float		linearSceneDepthRange = linearSceneDepthMax - linearSceneDepthMin;
	float			previousMaxDepth = linearSceneDepthMin;

	for (u32 i = 0; i < m_CascadedShadows.Count(); ++i)
	{
		SCascadedShadowSlice	&slice = m_CascadedShadows[i];

		const float linearSliceDepthRangeMin = previousMaxDepth;
		float linearSliceDepthRangeMax = previousMaxDepth + PKMax(slice.m_MinDistance, linearSceneDepthRange * slice.m_SceneRangeRatio);
		linearSliceDepthRangeMax = PKMin(linearSliceDepthRangeMax, linearSceneDepthMax);

		previousMaxDepth = linearSliceDepthRangeMax;

		slice.m_DepthRangeMin = UnlinearizeDepth(linearSliceDepthRangeMin, nearFar.x(), nearFar.y());
		slice.m_DepthRangeMax = UnlinearizeDepth(linearSliceDepthRangeMax, nearFar.x(), nearFar.y());

		slice.m_IsValid &= slice.m_DepthRangeMin < castSceneMaxDepth && slice.m_DepthRangeMax > castSceneMinDepth;
		slice.m_IsValid &= slice.m_DepthRangeMin != slice.m_DepthRangeMax;

		if (!slice.m_IsValid)
		{
			slice.m_DepthRangeMin = -1.0f;
			slice.m_DepthRangeMax = -1.0f;
			continue;
		}

		slice.m_DepthRangeMin = PKMax(castSceneMinDepth, slice.m_DepthRangeMin);
		slice.m_DepthRangeMax = PKMin(castSceneMaxDepth, slice.m_DepthRangeMax);
		
		// We recompute the matrix with the new depth range:
		const CAABB	sceneCameraFrustumBBoxLight = _GetFrustumBBox(m_SceneInfoData.m_InvViewProj, slice.m_DepthRangeMin, slice.m_DepthRangeMax, m_LightTransform);

		slice.m_ShadowSliceViewAABB = m_CasterLightAlignedBBox;

		// Clamp left, right, bottom, up and far to the camera frustum:
		slice.m_ShadowSliceViewAABB.Min().xy() = PKMax(slice.m_ShadowSliceViewAABB.Min().xy(), sceneCameraFrustumBBoxLight.Min().xy());
		slice.m_ShadowSliceViewAABB.Max().xy() = PKMin(slice.m_ShadowSliceViewAABB.Max().xy(), sceneCameraFrustumBBoxLight.Max().xy());
		slice.m_ShadowSliceViewAABB.Max().z() = PKMin(slice.m_ShadowSliceViewAABB.Max().z(), sceneCameraFrustumBBoxLight.Max().z());

		slice.m_AspectRatio = CFloat2(	PKMin(slice.m_ShadowSliceViewAABB.Extent().x() / slice.m_ShadowSliceViewAABB.Extent().y(), 1.0f),
										PKMin(slice.m_ShadowSliceViewAABB.Extent().y() / slice.m_ShadowSliceViewAABB.Extent().x(), 1.0f));

		// Cam position is the center of the BBox in X and Y and the min value of Z:
		CFloat3		camPosition = slice.m_ShadowSliceViewAABB.Center();
		camPosition.z() = slice.m_ShadowSliceViewAABB.Min().z();
		slice.m_ShadowSliceViewAABB -= camPosition;

		// Update scene info with orthogonal view for shadow map:
		slice.m_Proj = CreateOrtho(	slice.m_ShadowSliceViewAABB.Min().y(), slice.m_ShadowSliceViewAABB.Max().y(),
									slice.m_ShadowSliceViewAABB.Min().x(), slice.m_ShadowSliceViewAABB.Max().x(),
									slice.m_ShadowSliceViewAABB.Min().z(), slice.m_ShadowSliceViewAABB.Max().z());
		slice.m_InvProj = slice.m_Proj.Inverse();
		// Transform the inv light space cam position into world space:
		slice.m_InvView.WAxis() = CFloat4(slice.m_InvView.TransformVector(camPosition), 1);
		slice.m_View = slice.m_InvView.Inverse();
		slice.m_WorldToShadow = slice.m_View * slice.m_Proj;

		Utils::SBasicCameraData		basicCam;

		basicCam.m_BillboardingView = slice.m_View;
		basicCam.m_CameraProj = slice.m_Proj;
		basicCam.m_CameraView = slice.m_View;
		basicCam.m_CameraZLimit = CFloat2(slice.m_ShadowSliceViewAABB.Min().z(), slice.m_ShadowSliceViewAABB.Max().z());

		PKSample::SSceneInfoData	cascadeSceneInfo = m_SceneInfoData;

		Utils::SetupSceneInfoData(cascadeSceneInfo, basicCam, m_DrawCoordFrame);

		if (!PK_VERIFY(slice.m_SceneInfoBuffer != null))
			return false;

		// Fill the GPU buffer:
		SSceneInfoData	*sceneInfo = static_cast<SSceneInfoData*>(m_ApiManager->MapCpuView(slice.m_SceneInfoBuffer));
		if (!PK_VERIFY(sceneInfo != null))
			return false;
		*sceneInfo = cascadeSceneInfo;
		if (!PK_VERIFY(m_ApiManager->UnmapCpuView(slice.m_SceneInfoBuffer)))
			return false;
	}
	return true;
}

//----------------------------------------------------------------------------

bool		CDirectionalShadows::IsAnySliceValidForDraw() const
{
	for (u32 i = 0; i < m_CascadedShadows.Count(); ++i)
	{
		if (IsSliceValidForDraw(i))
			return true;
	}
	return false;
}

//----------------------------------------------------------------------------

bool	CDirectionalShadows::BeginDrawShadowRenderPass(const RHI::PCommandBuffer &cmdBuff, u32 cascadeIdx, const PKSample::SMesh *backdrop)
{
	// Begin render pass:
	const RHI::SFrameBufferClearValue	clearValues[3] =
	{
		RHI::SFrameBufferClearValue(1.0f, 1.0f, 1.0f, 1.0f),
		RHI::SFrameBufferClearValue(1.0f, 1.0f, 1.0f, 1.0f),
		RHI::SFrameBufferClearValue(1.0f, 0)
	};

	const SCascadedShadowSlice	&slice = m_CascadedShadows[cascadeIdx];

	if (!PK_VERIFY(slice.m_IsValid))
		return false;

	const CUint2	viewportSize = m_ShadowMapResolution * slice.m_AspectRatio;

	cmdBuff->BeginRenderPass(m_RenderPass, slice.m_FrameBuffer, clearValues);

	cmdBuff->SetViewport(CInt2(0, 0), viewportSize, CFloat2(0, 1));
	cmdBuff->SetScissor(CInt2(0), viewportSize);

	if (backdrop == null)
		return true;

	// Draw the meshes:
	cmdBuff->BindRenderState(m_BackdropShadowRenderState);

	const RHI::PConstantSet		constSets[] =
	{
		slice.m_SceneInfoConstantSet,
	};

	cmdBuff->BindConstantSets(constSets);

	// Push matrices as constants
	CFloat4x4	meshVertexConstant;

	const u32	instanceCount = backdrop->m_Transforms.Count();
	for (u32 iInstance = 0; iInstance < instanceCount; ++iInstance)
	{
		// Model matrix
		const CFloat4x4	meshTransforms = m_MeshToDrawFrame * backdrop->m_Transforms[iInstance];
		meshVertexConstant = meshTransforms;

		cmdBuff->PushConstant(&meshVertexConstant, 0);
		for (u32 j = 0; j < backdrop->m_MeshBatches.Count(); ++j)
		{
			const PKSample::SMesh::SMeshBatch	&curBatch = backdrop->m_MeshBatches[j];
			RHI::PGpuBuffer						vBuffer = null;

			// curBatch can be either static or skinned batch, most of the time it won't be animated
			PK_ASSERT(curBatch.m_Instances.Empty() || iInstance < curBatch.m_Instances.Count());
			// If this instance has valid skinned data, render with those vb, otherwise render the bindpose
			if (curBatch.m_Instances.Empty() || !curBatch.m_Instances[iInstance].m_HasValidSkinnedData)
				vBuffer = curBatch.m_BindPoseVertexBuffers;
			else
				vBuffer = curBatch.m_Instances[iInstance].m_SkinnedVertexBuffers;

			if (!PK_VERIFY(vBuffer != null && curBatch.m_BindPoseVertexBuffers != null))
				continue;

			const RHI::PGpuBuffer		vertexBuffers[] = { vBuffer };
			const u32					offsets[] = { curBatch.m_PositionsOffset };

			cmdBuff->BindVertexBuffers(vertexBuffers, offsets);
			cmdBuff->BindIndexBuffer(curBatch.m_IndexBuffer, 0, curBatch.m_IndexSize);

			cmdBuff->DrawIndexed(0, 0, curBatch.m_IndexCount);
		}
	}
	return true;
}

bool	CDirectionalShadows::EndDrawShadowRenderPass(const RHI::PCommandBuffer &cmdBuff, u32 cascadeIdx)
{
	if (m_EnableVariance)
	{
		const SCascadedShadowSlice	&slice = m_CascadedShadows[cascadeIdx];
		const CUint2		viewportSize = m_ShadowMapResolution * slice.m_AspectRatio;
		const SBlurInfo		horizontal(viewportSize, CFloat2(1.0f, 0.0f), CFloat4(0.0f, 0.0f, slice.m_AspectRatio));
		const SBlurInfo		vertical(viewportSize, CFloat2(0.0f, 1.0f), CFloat4(0.0f, 0.0f, slice.m_AspectRatio));

		cmdBuff->SetViewport(CInt2(0, 0), viewportSize, CFloat2(0, 1));
		cmdBuff->SetScissor(CInt2(0), viewportSize);

		cmdBuff->NextRenderSubPass();
		cmdBuff->BindRenderState(m_BlurHRenderState);

		cmdBuff->BindVertexBuffers(TMemoryView<const RHI::PGpuBuffer>(m_FullScreenQuadBuffer));
		cmdBuff->BindConstantSets(TMemoryView<const RHI::PConstantSet>(slice.m_BlurHConstantSet));
		cmdBuff->PushConstant(&horizontal, 0);
		cmdBuff->Draw(0, 6);

		cmdBuff->NextRenderSubPass();
		cmdBuff->BindRenderState(m_BlurVRenderState);

		cmdBuff->BindVertexBuffers(TMemoryView<const RHI::PGpuBuffer>(m_FullScreenQuadBuffer));
		cmdBuff->BindConstantSets(TMemoryView<const RHI::PConstantSet>(slice.m_BlurVConstantSet));
		cmdBuff->PushConstant(&vertical, 0);
		cmdBuff->Draw(0, 6);
	}
	else
	{
		cmdBuff->NextRenderSubPass();
		cmdBuff->NextRenderSubPass();
	}
	bool	success = true;
	success &= cmdBuff->EndRenderPass();
	return success;
}

//----------------------------------------------------------------------------

void	CDirectionalShadows::SetShadowsInfo(float bias, float variancePower, bool enableShadows, bool enableVariance, bool debugShadows, const CUint2 &resolution)
{
	m_ShadowBias = bias;
	m_ShadowVariancePower = variancePower;
	m_EnableShadows = enableShadows;
	m_EnableVariance = enableVariance;
	m_DebugShadows = debugShadows;
	m_ShadowMapResolution = resolution;
}

//----------------------------------------------------------------------------

void	CDirectionalShadows::SetCascadeShadowsSettings(u32 cascadeIdx, float sceneRangeRatio, float minDistance)
{
	m_CascadedShadows[cascadeIdx].m_SceneRangeRatio = sceneRangeRatio;
	m_CascadedShadows[cascadeIdx].m_MinDistance = minDistance;
}

//----------------------------------------------------------------------------

bool	CDirectionalShadows::UpdateShadowsSettings()
{
	// Normalize the range ratios:
	float	rangeSum = 0.0f;
	for (u32 i = 0; i < m_CascadedShadows.Count(); ++i)
		rangeSum += m_CascadedShadows[i].m_SceneRangeRatio;

	for (u32 i = 0; i < m_CascadedShadows.Count(); ++i)
	{
		SCascadedShadowSlice	&slice = m_CascadedShadows[i];

		slice.m_SceneRangeRatio /= rangeSum;
		// Create render target:
		if (!slice.m_ShadowMap.CreateRenderTarget(	RHI::SRHIResourceInfos("Shadow render target RG"),
													m_ApiManager, m_SamplerRT, m_IdealShadowFormat, m_ShadowMapResolution, m_SamplerRTLayout))
			return false;
		// Create tmp render target:
		if (!slice.m_IntermediateRt.CreateRenderTarget(	RHI::SRHIResourceInfos("Shadow blur intermediate render target"),
														m_ApiManager, m_SamplerRT, m_IdealShadowFormat, m_ShadowMapResolution, m_SamplerRTLayout))
			return false;
		// Create depth:
		if (!slice.m_DepthMap.CreateRenderTarget(	RHI::SRHIResourceInfos("Shadow depth render target"),
													m_ApiManager, m_SamplerRT, m_IdealDepthFormat, m_ShadowMapResolution, m_SamplerRTLayout))
			return false;
	
		// Scene info:
		const RHI::SConstantBufferDesc	&sceneInfoBufferDesc = m_SceneInfoLayout.m_Constants.First().m_ConstantBuffer;
	
		slice.m_SceneInfoConstantSet = m_ApiManager->CreateConstantSet(RHI::SRHIResourceInfos("Shadow scene info layout"), m_SceneInfoLayout);
		if (!PK_VERIFY(slice.m_SceneInfoConstantSet != null))
			return false;
	
		const u32	constantBufferSize = sceneInfoBufferDesc.m_ConstantBufferSize; // We could also create the new one based on the default one created
		PK_ASSERT(constantBufferSize == sizeof(PKSample::SSceneInfoData));
		slice.m_SceneInfoBuffer = m_ApiManager->CreateGpuBuffer(RHI::SRHIResourceInfos("Shadow scene info buffer"), RHI::ConstantBuffer, constantBufferSize);
		if (!PK_VERIFY(slice.m_SceneInfoBuffer != null))
			return false;
		if (!PK_VERIFY(slice.m_SceneInfoConstantSet->SetConstants(slice.m_SceneInfoBuffer, 0)))
			return false;
		if (!PK_VERIFY(slice.m_SceneInfoConstantSet->UpdateConstantValues()))
			return false;
		if (!_CreateSliceData(slice))
			return false;
	}
	return true;
}

//----------------------------------------------------------------------------

TMemoryView<const RHI::SRenderTargetDesc>	CDirectionalShadows::GetFrameBufferLayout() const
{
	return TMemoryView<const RHI::SRenderTargetDesc>(m_FrameBufferLayout);
}

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
