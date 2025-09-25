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
#include "RHIBillboardingBatch_GPUsim.h"

#include "pk_render_helpers/include/render_features/rh_features_basic.h"

#include <pk_rhi/include/interfaces/IApiManager.h>
#include <pk_rhi/include/interfaces/IGpuBuffer.h>

#if	(PK_PARTICLES_UPDATER_USE_D3D11 != 0)
//#	include <pk_rhi/include/D3D11/D3D11RHI.h>
#	include <pk_particles/include/Storage/D3D11/storage_d3d11.h>
#	include <pk_rhi/include/D3D11/D3D11ApiManager.h>
#endif
#if	(PK_PARTICLES_UPDATER_USE_D3D12 != 0) || (PK_PARTICLES_UPDATER_USE_D3D12U != 0)
//#	include <pk_rhi/include/D3D12/D3D12RHI.h>
#	include <pk_rhi/include/D3D12/D3D12ApiManager.h>
#endif
#if	(PK_PARTICLES_UPDATER_USE_D3D12 != 0)
#	include <pk_particles/include/Storage/D3D12/storage_d3d12.h>
#endif
#if	(PK_PARTICLES_UPDATER_USE_D3D12U != 0)
#	include <pk_particles/include/Storage/D3D12U/storage_d3d12U.h>
#endif
#if	(PK_PARTICLES_UPDATER_USE_UNKNOWN2 != 0)
//#	include <pk_rhi/include/UNKNOWN2/UNKNOWN2RHI.h>
#	include <pk_particles/include/Storage/UNKNOWN2/storage_UNKNOWN2.h>
#	include <pk_rhi/include/UNKNOWN2/UNKNOWN2ApiManager.h>
#endif

#include "PK-SampleLib/ShaderDefinitions/SampleLibShaderDefinitions.h"
#include "PK-SampleLib/RenderIntegrationRHI/RendererCache.h"

#include "RHIRenderIntegrationConfig.h"

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

// Maximum number of draw requests batched in a single billboarding batch policy (1 draw request = 1 particle renderer being drawn)
static u32	kMaxDrawRequestCount = 0x100;

// PK-SampleLib implementation of renderer batches, for the GPU simulated particles:
// - using RHI (abstraction API for graphics APIs)
// - full integration of all features (supported by the editor)

// Each renderer-bach gathers renderers (that are compatible each others).
// Particle data resides on gpu memory directly, no copy task occur on CPU, we "retrieve" GPU buffers which basically create a PK-RHI "handle" on the graphics API native object (SRV/Buffer/..)
// A single buffer contains all particles for a given particle layer (might be shared by several draw requests), with particle streams offset in that buffer.
// As PK-RHI doesn't currently support structured buffers (only raw buffers), we create one gpu buffer per stream, containing offsets for that stream in all draw requests, but you can hook that as you wish
// Sorting is implemented for vertex billboarded GPU particles using a base 2 parallel radix sort on 16 bits keys.

// This implementation is both used by samples and v2 editor

//----------------------------------------------------------------------------
//
//	Helpers
//
//----------------------------------------------------------------------------

static bool	AreBillboardingBatchable(const PCRendererCacheBase &firstCache, const PCRendererCacheBase &secondCache)
{
	// Return true if firstCache and secondCache can be batched together (same draw call)
	// Simplest approach here is to break batching when those two materials are incompatible (varying uniforms, mismatching textures, ..)
	return	firstCache == secondCache ||
			*checked_cast<const CRendererCacheInstance_UpdateThread *>(firstCache.Get()) ==
			*checked_cast<const CRendererCacheInstance_UpdateThread *>(secondCache.Get());
}

//----------------------------------------------------------------------------

static RHI::PGpuBuffer	_RetrieveStorageBuffer(const RHI::PApiManager &manager, const CParticleStreamToRender *streams, CGuid streamIdx, u32 &storageOffset)
{
	PK_ASSERT(streams != null);

	(void)manager; (void)streams; (void)storageOffset;
	if (!PK_VERIFY(streamIdx.Valid()))
		return null;
#if	(PK_PARTICLES_UPDATER_USE_D3D11 != 0)
	// We retrieve the vertex buffer from the GPU storages:
	if (streams->StorageClass() == CParticleStorageManager_D3D11::DefaultStorageClass())
	{
		const CParticleStreamToRender_D3D11	*streamsToRender = static_cast<const CParticleStreamToRender_D3D11*>(streams);
		const SBuffer_D3D11					&stream = streamsToRender->StreamBuffer();
		if (stream.Empty())
			return null;
		storageOffset = streamsToRender->StreamOffset(streamIdx);
		return CastD3D11(manager)->D3D11GetGpuBufferFromExisting(RHI::SRHIResourceInfos("Stream Data Buffer"), stream.m_Buffer, RHI::RawVertexBuffer);
	}
#endif
#if	(PK_PARTICLES_UPDATER_USE_D3D12 != 0)
	// We retrieve the vertex buffer from the GPU storages:
	if (streams->StorageClass() == CParticleStorageManager_D3D12::DefaultStorageClass())
	{
		const CParticleStreamToRender_D3D12	*streamsToRender = static_cast<const CParticleStreamToRender_D3D12*>(streams);
		const SBuffer_D3D12					&stream = streamsToRender->StreamBuffer();
		if (stream.Empty())
			return null;
		storageOffset = streamsToRender->StreamOffset(streamIdx);
		return CastD3D12(manager)->D3D12GetGpuBufferFromExisting(RHI::SRHIResourceInfos("Stream Data Buffer"), stream.m_Resource, RHI::RawVertexBuffer, stream.m_State);
	}
#endif
#if	(PK_PARTICLES_UPDATER_USE_D3D12U != 0)
	// We retrieve the vertex buffer from the GPU storages:
	if (streams->StorageClass() == CParticleStorageManager_D3D12U::DefaultStorageClass())
	{
		const CParticleStreamToRender_D3D12	*streamsToRender = static_cast<const CParticleStreamToRender_D3D12*>(streams);
		const SBuffer_D3D12					&stream = streamsToRender->StreamBuffer();
		if (stream.Empty())
			return null;
		storageOffset = streamsToRender->StreamOffset(streamIdx);
		return CastD3D12(manager)->D3D12GetGpuBufferFromExisting(RHI::SRHIResourceInfos("Stream Data Buffer"), stream.m_Resource, RHI::RawVertexBuffer, stream.m_State);
	}
#endif
#if	(PK_PARTICLES_UPDATER_USE_UNKNOWN2 != 0)
	// We retrieve the vertex buffer from the GPU storages:
	if (streams->StorageClass() == CParticleStorageManager_UNKNOWN2::DefaultStorageClass())
	{
		const CParticleStreamToRender_UNKNOWN2*	streamsToRender = static_cast<const CParticleStreamToRender_UNKNOWN2*>(streams);
		const SBuffer_UNKNOWN2&					stream = streamsToRender->StreamBuffer();
		if (stream.Empty())
			return null;
		storageOffset = streamsToRender->StreamOffset(streamIdx);
		return CastUNKNOWN2(manager)->UNKNOWN2GetGpuBufferFromExisting(RHI::SRHIResourceInfos("Stream Data Buffer"), stream.m_Resource, stream.m_ByteSize, RHI::RawBuffer);
	}
#endif
	return null;
}

//----------------------------------------------------------------------------

static RHI::PGpuBuffer	_RetrieveParticleInfoBuffer(const RHI::PApiManager &manager, const CParticleStreamToRender *streams)
{
	(void)manager; (void)streams;
#if	(PK_PARTICLES_UPDATER_USE_D3D11 != 0)
	// We retrieve the stream size buffer from the GPU storages:
	if (streams->StorageClass() == CParticleStorageManager_D3D11::DefaultStorageClass())
	{
		const CParticleStreamToRender_D3D11	*streamsToRender = static_cast<const CParticleStreamToRender_D3D11*>(streams);
		const SBuffer_D3D11					&stream = streamsToRender->StreamSizeBuf();
		if (stream.Empty())
			return null;
		return CastD3D11(manager)->D3D11GetGpuBufferFromExisting(RHI::SRHIResourceInfos("Stream Infos Buffer"), stream.m_Buffer, RHI::RawVertexBuffer);
	}
#endif
#if	(PK_PARTICLES_UPDATER_USE_D3D12U != 0)
	// We retrieve the stream size buffer from the GPU storages:
	if (streams->StorageClass() == CParticleStorageManager_D3D12U::DefaultStorageClass())
	{
		const CParticleStreamToRender_D3D12	*streamsToRender = static_cast<const CParticleStreamToRender_D3D12*>(streams);
		const SBuffer_D3D12					&stream = streamsToRender->StreamSizeBuf();
		if (stream.Empty())
			return null;
		return CastD3D12(manager)->D3D12GetGpuBufferFromExisting(RHI::SRHIResourceInfos("Stream Infos Buffer"), stream.m_Resource, RHI::RawVertexBuffer, stream.m_State);
	}
#endif
#if	(PK_PARTICLES_UPDATER_USE_D3D12 != 0)
	// We retrieve the stream size buffer from the GPU storages:
	if (streams->StorageClass() == CParticleStorageManager_D3D12::DefaultStorageClass())
	{
		const CParticleStreamToRender_D3D12	*streamsToRender = static_cast<const CParticleStreamToRender_D3D12*>(streams);
		const SBuffer_D3D12					&stream = streamsToRender->StreamSizeBuf();
		if (stream.Empty())
			return null;
		return CastD3D12(manager)->D3D12GetGpuBufferFromExisting(RHI::SRHIResourceInfos("Stream Infos Buffer"), stream.m_Resource, RHI::RawVertexBuffer, stream.m_State);
	}
#endif
#if	(PK_PARTICLES_UPDATER_USE_UNKNOWN2 != 0)
	// We retrieve the stream size buffer from the GPU storages:
	if (streams->StorageClass() == CParticleStorageManager_UNKNOWN2::DefaultStorageClass())
	{
		const CParticleStreamToRender_UNKNOWN2	*streamsToRender = static_cast<const CParticleStreamToRender_UNKNOWN2*>(streams);
		const SBuffer_UNKNOWN2					&stream = streamsToRender->StreamSizeBuf();
		if (stream.Empty())
			return null;
		return CastUNKNOWN2(manager)->UNKNOWN2GetGpuBufferFromExisting(RHI::SRHIResourceInfos("Stream Infos Buffer"), stream.m_Resource, stream.m_ByteSize, RHI::RawVertexBuffer);
	}
#endif
	return null;
}

//----------------------------------------------------------------------------

static u32	_GetGeomBillboardShaderOptions(const Drawers::SBillboard_BillboardingRequest &bbRequest)
{
	u32	shaderOptions = Option_GeomBillboarding;

	// Here, we set some flags to know which shader we should use to billboard those particles.
	// We need that so PK-SampleLib's render loop avoids re-creating the shaders each time the billboarding mode changes:
	// The renderer cache contains the geometry shaders for ALL billboarding mode and we choose between those depending on "m_ShaderOptions"
	switch (bbRequest.m_Mode)
	{
	case BillboardMode_ScreenAligned:
	case BillboardMode_ViewposAligned:
		break;
	case BillboardMode_AxisAligned:
	case BillboardMode_AxisAlignedSpheroid:
		shaderOptions |= Option_Axis_C1;
		break;
	case BillboardMode_AxisAlignedCapsule:
		shaderOptions |= Option_Axis_C1 | Option_Capsule;
		break;
	case BillboardMode_PlaneAligned:
		shaderOptions |= Option_Axis_C2;
		break;
	default:
		PK_ASSERT_NOT_REACHED();
		return 0;
		break;
	}
	if (bbRequest.m_SizeFloat2)
		shaderOptions |= Option_BillboardSizeFloat2;
	return shaderOptions;
}

//----------------------------------------------------------------------------

static bool	_CreateOrResizeGpuBufferIf(const RHI::SRHIResourceInfos &infos, bool condition, const RHI::PApiManager &manager, SGpuBuffer &buffer, RHI::EBufferType type, u32 sizeToAlloc, u32 requiredSize)
{
	PK_ASSERT(sizeToAlloc >= requiredSize);
	if (!condition)
		return true;
	if (buffer.m_Buffer == null || buffer.m_Buffer->GetByteSize() < requiredSize)
	{
		RHI::PGpuBuffer	gpuBuffer = manager->CreateGpuBuffer(infos, type, sizeToAlloc);
		buffer.SetGpuBuffer(gpuBuffer);
		if (!PK_VERIFY(gpuBuffer != null) ||
			!PK_VERIFY(buffer.Used()))
			return false;
	}
	else
		buffer.Use();
	return true;
}

//----------------------------------------------------------------------------

static u32	_GetVertexBillboardShaderOptions(const Drawers::SBillboard_BillboardingRequest &bbRequest, bool needGPUSort)
{
	u32	shaderOptions = Option_VertexBillboarding;

	// Here, we set some flags to know which shader we should use to billboard those particles.
	// We need that so PK-SampleLib's render loop avoids re-creating the shaders each time the billboarding mode changes:
	// The renderer cache contains the geometry shaders for ALL billboarding mode and we choose between those depending on "m_ShaderOptions"
	switch (bbRequest.m_Mode)
	{
	case	BillboardMode_ScreenAligned:
	case	BillboardMode_ViewposAligned:
		break;
	case	BillboardMode_AxisAligned:
	case	BillboardMode_AxisAlignedSpheroid:
		shaderOptions |= Option_Axis_C1;
		break;
	case	BillboardMode_AxisAlignedCapsule:
		shaderOptions |= Option_Axis_C1 | Option_Capsule;
		break;
	case	BillboardMode_PlaneAligned:
		shaderOptions |= Option_Axis_C2;
		break;
	default:
		PK_ASSERT_NOT_REACHED();
		return 0;
		break;
	}
	if (bbRequest.m_SizeFloat2)
		shaderOptions |= Option_BillboardSizeFloat2;
	if (needGPUSort)
		shaderOptions |= Option_GPUSort;
	return shaderOptions;
}

//----------------------------------------------------------------------------

static u32	_GetVertexRibbonShaderOptions(const Drawers::SRibbon_BillboardingRequest &bbRequest, bool needGPUSort)
{
	u32	shaderOptions = Option_VertexPassThrough | Option_RibbonVertexBillboarding;

	// Here, we set some flags to know which shader we should use to billboard those particles.
	// We need that so PK-SampleLib's render loop avoids re-creating the shaders each time the billboarding mode changes:
	// The renderer cache contains the geometry shaders for ALL billboarding mode and we choose between those depending on "m_ShaderOptions"

	switch (bbRequest.m_Mode)
	{
	case	RibbonMode_ViewposAligned:
		break;
	case	RibbonMode_NormalAxisAligned:
	case	RibbonMode_SideAxisAligned:
	case	RibbonMode_SideAxisAlignedTube: // TODO: implem shader options for tube & multi-planes like capsules.
	case	RibbonMode_SideAxisAlignedMultiPlane:
		shaderOptions |= Option_Axis_C1;
		break;
	default:
		PK_ASSERT_NOT_REACHED();
		return 0;
		break;
	}
	if (needGPUSort)
		shaderOptions |= Option_GPUSort;
	return shaderOptions;
}

//----------------------------------------------------------------------------

CGuid	_GetDrawDebugColorIndex(const SRHIAdditionalFieldBatchGPU &bufferBatch, const SGeneratedInputs &toGenerate)
{
	CGuid	ret;
	for (u32 j = 0; j < bufferBatch.m_Fields.Count(); ++j)
	{
		const u32	i = bufferBatch.m_Fields[j].m_AdditionalInputIndex;
		if (toGenerate.m_AdditionalGeneratedInputs[i].m_Type == PopcornFX::EBaseTypeID::BaseType_Float4 &&
			(toGenerate.m_AdditionalGeneratedInputs[i].m_Name == BasicRendererProperties::SID_Diffuse_Color() ||
			toGenerate.m_AdditionalGeneratedInputs[i].m_Name == BasicRendererProperties::SID_Diffuse_DiffuseColor()))
			ret = j;
		else if (!ret.Valid() &&
				toGenerate.m_AdditionalGeneratedInputs[i].m_Type == PopcornFX::EBaseTypeID::BaseType_Float4 &&
				(toGenerate.m_AdditionalGeneratedInputs[i].m_Name == BasicRendererProperties::SID_Emissive_EmissiveColor() ||
					toGenerate.m_AdditionalGeneratedInputs[i].m_Name == BasicRendererProperties::SID_Distortion_Color()))
			ret = j;
	}
	return ret;
}

//----------------------------------------------------------------------------
//
// SRHIAdditionalFieldBatchGPU
//
//----------------------------------------------------------------------------

bool	SRHIAdditionalFieldBatchGPU::AllocBuffers(u32 vCount, RHI::PApiManager manager)
{
	if (!m_MappedFields.Resize(m_Fields.Count()))
		return false;
	const u32	vCountAligned = Mem::Align<0x100>(vCount);
	for (u32 i = 0; i < m_Fields.Count(); ++i)
	{
		// GPU buffer holding offsets into the GPU sim storage (1 element per draw request)
		// CPU writable, we know the offsets on the CPU
		SAdditionalInputs	&decl = m_Fields[i];
		if (!PK_VERIFY(_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("AdditionalInputs Constant Buffer"), true, manager, decl.m_Buffer, RHI::RawBuffer, vCountAligned * sizeof(u32), vCount * sizeof(u32))))
			return false;
	}
	return true;
}

//----------------------------------------------------------------------------

bool	SRHIAdditionalFieldBatchGPU::MapBuffers(u32 vCount, RHI::PApiManager manager)
{
	PK_ASSERT(m_Fields.Count() == m_MappedFields.Count());
	for (u32 i = 0; i < m_Fields.Count(); ++i)
	{
		if (m_Fields[i].m_Buffer.Used())
		{
			void	*mappedValue = manager->MapCpuView(m_Fields[i].m_Buffer.m_Buffer, 0, vCount * sizeof(u32));
			if (!PK_VERIFY(mappedValue != null))
				return false;
			m_MappedFields[i].m_AdditionalInputIndex = m_Fields[i].m_AdditionalInputIndex;
			m_MappedFields[i].m_Storage.m_Count = vCount;
			m_MappedFields[i].m_Storage.m_RawDataPtr = static_cast<u8*>(mappedValue);
			m_MappedFields[i].m_Storage.m_Stride = sizeof(u32);
		}
	}
	return true;
}

//----------------------------------------------------------------------------

void	SRHIAdditionalFieldBatchGPU::UnmapBuffers(RHI::PApiManager manager)
{
	for (u32 i = 0; i < m_Fields.Count(); ++i)
	{
		m_Fields[i].m_Buffer.UnmapIFN(manager);
		m_MappedFields[i] = Drawers::SCopyFieldDesc();
	}
}

//----------------------------------------------------------------------------
//
// CRHIRendererBatch_BillboardGPU_GeomBB
//
//----------------------------------------------------------------------------

bool	CRHIRendererBatch_BillboardGPU_GeomBB::AreRenderersCompatible(const CRendererDataBase *rendererA, const CRendererDataBase *rendererB) const
{
	return	CRendererBatchJobs_Billboard_GPUBB::AreRenderersCompatible(rendererA, rendererB) &&
			AreBillboardingBatchable(rendererA->m_RendererCache, rendererB->m_RendererCache);
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_BillboardGPU_GeomBB::AllocBuffers(SRenderContext &ctx)
{
	(void)ctx;

	const u32	drCount = m_DrawPass->m_DrawRequests.Count();
	const u32	drCountAligned = Mem::Align<0x10>(drCount);

	if (m_DrawPass->m_DrawRequests.First()->BaseBillboardingRequest().m_Flags.m_NeedSort)
	{
		PK_ASSERT_NOT_IMPLEMENTED_MESSAGE("CRHIRendererBatch_BillboardGPU_GeomBB used with sort needed, but not implemented! Sorting is ignored.");
	}

	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Billboard DrawRequests Buffer"), true, m_ApiManager, m_IndirectDraw, RHI::IndirectDrawBuffer, drCountAligned * sizeof(RHI::SDrawIndirectArgs), drCount * sizeof(RHI::SDrawIndirectArgs)))
		return false;

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_BillboardGPU_GeomBB::MapBuffers(SRenderContext &ctx)
{
	(void)ctx;

	m_MappedIndirectBuffer = static_cast<RHI::SDrawIndirectArgs*>(m_ApiManager->MapCpuView(m_IndirectDraw.m_Buffer));
	return PK_VERIFY(m_MappedIndirectBuffer != null);
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_BillboardGPU_GeomBB::LaunchCustomTasks(SRenderContext &ctx)
{
	(void)ctx;
	// This is processed inline
	if (!PK_VERIFY(m_MappedIndirectBuffer != null))
		return false;
	for (u32 i = 0; i < m_DrawPass->m_DrawRequests.Count(); ++i)
	{
		m_MappedIndirectBuffer[i].m_InstanceCount = 1;
		m_MappedIndirectBuffer[i].m_InstanceOffset = 0;
		m_MappedIndirectBuffer[i].m_VertexOffset = 0;
		m_MappedIndirectBuffer[i].m_VertexCount = m_DrawPass->m_DrawRequests[i]->RenderedParticleCount(); // over-estimation, that will be overwriten by the GPU-side value.
	}
	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_BillboardGPU_GeomBB::UnmapBuffers(SRenderContext &ctx)
{
	(void)ctx;
	m_IndirectDraw.UnmapIFN(m_ApiManager);
	m_MappedIndirectBuffer = null;
	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_BillboardGPU_GeomBB::EmitDrawCall(SRenderContext &ctx, const SDrawCallDesc &toEmit)
{
	SRHIRenderContext	&renderContext = static_cast<SRHIRenderContext&>(ctx);

	// !Currently, there is no batching for gpu particles!
	// So we'll emit one draw call per draw request
	// Some data are indexed by the draw-requests. So "EmitDrawCall" shoudl be called once, with all draw-requests.
	PK_ASSERT(toEmit.m_DrawRequests.Count() == m_DrawPass->m_DrawRequests.Count());

	// No need to iterate on all draw requests, just take the first as reference as they wouldn't have been batched if not compatible
	CRendererCacheInstance_UpdateThread		*renderCacheInstance = static_cast<CRendererCacheInstance_UpdateThread*>(toEmit.m_RendererCaches.First().Get());
	if (!PK_VERIFY(renderCacheInstance != null))
	{
		CLog::Log(PK_ERROR, "Invalid renderer cache instance");
		return false;
	}
	PKSample::PCRendererCacheInstance	rCacheInstance = renderCacheInstance->RenderThread_GetCacheInstance();
	if (!PK_VERIFY(rCacheInstance != null))
		return false;

	// Generate the draw-calls
	const u32	drCount = toEmit.m_DrawRequests.Count();
	for (u32 dri = 0; dri < drCount; ++dri)
	{
		PK_ASSERT(toEmit.m_DrawRequests[dri] != null);
		const Drawers::SBillboard_DrawRequest			*dr = static_cast<const Drawers::SBillboard_DrawRequest*>(toEmit.m_DrawRequests[dri]);
		const Drawers::SBillboard_BillboardingRequest	*bbRequest = static_cast<const Drawers::SBillboard_BillboardingRequest*>(&dr->BaseBillboardingRequest());
		const CParticleStreamToRender					*streamToRender = &dr->StreamToRender();
		if (!PK_VERIFY(streamToRender != null))
			continue;

		if (!PK_VERIFY(renderContext.m_DrawOutputs.m_DrawCalls.PushBack().Valid()))
		{
			CLog::Log(PK_ERROR, "Failed to create a draw-call");
			return false;
		}
		SRHIDrawCall	&outDrawCall = renderContext.m_DrawOutputs.m_DrawCalls.Last();

		outDrawCall.m_Batch = this;
		outDrawCall.m_RendererCacheInstance = renderCacheInstance;
		outDrawCall.m_Type = SRHIDrawCall::DrawCall_InstancedIndirect;
		outDrawCall.m_ShaderOptions = PKSample::Option_VertexPassThrough | Option_GPUStorage | _GetGeomBillboardShaderOptions(*bbRequest);
		outDrawCall.m_RendererType = Renderer_Billboard;

		// Some meta-data (the Editor uses them)
		{
			outDrawCall.m_BBox = toEmit.m_BBox;
			outDrawCall.m_TotalBBox = m_DrawPass->m_TotalBBox;
			outDrawCall.m_SlicedDC = toEmit.m_TotalParticleCount != m_DrawPass->m_TotalParticleCount;

			outDrawCall.m_Valid =	rCacheInstance->m_Cache != null &&
									rCacheInstance->m_Cache->GetRenderState(static_cast<PKSample::EShaderOptions>(outDrawCall.m_ShaderOptions)) != null;
		}

		// Retrieve the particle streams in GPU storage:
		static const u32	kBillboardingInputs = 5; // Positions/Sizes/Rotations/Axis0s/Axis1s
		const u32			maxBufferCount = bbRequest->m_AdditionalInputs.Count() + kBillboardingInputs;
		if (!PK_VERIFY(outDrawCall.m_VertexBuffers.Reserve(maxBufferCount)))
			return false;

		// Mandatory streams
		u32					enabledOffset = 0;
		u32					posOffset = 0;
		u32					sizeOffset = 0;
		RHI::PGpuBuffer		enableds = _RetrieveStorageBuffer(m_ApiManager, streamToRender, bbRequest->m_EnabledStreamId, enabledOffset);
		RHI::PGpuBuffer		positions = _RetrieveStorageBuffer(m_ApiManager, streamToRender, bbRequest->m_PositionStreamId, posOffset);
		RHI::PGpuBuffer		sizes = _RetrieveStorageBuffer(m_ApiManager, streamToRender, bbRequest->m_SizeStreamId, sizeOffset);

		if (!PK_VERIFY(positions != null) ||
			!PK_VERIFY(sizes != null) ||
			!PK_VERIFY(enableds != null))
			return false;

		// Optional streams
		const u32			geomInputs = bbRequest->GetGeomGeneratedVertexInputsFlags();
		u32					rotOffset = 0;
		u32					axis0Offset = 0;
		u32					axis1Offset = 0;
		RHI::PGpuBuffer		rotations = null;
		RHI::PGpuBuffer		axis0s = null;
		RHI::PGpuBuffer		axis1s = null;

		if (geomInputs & Drawers::GenInput_ParticleRotation)
		{
			rotations = _RetrieveStorageBuffer(m_ApiManager, streamToRender, bbRequest->m_RotationStreamId, rotOffset);
			PK_ASSERT(rotations != null);
		}
		if (geomInputs & Drawers::GenInput_ParticleAxis0)
		{
			axis0s = _RetrieveStorageBuffer(m_ApiManager, streamToRender, bbRequest->m_Axis0StreamId, axis0Offset);
			PK_ASSERT(axis0s != null);
		}
		if (geomInputs & Drawers::GenInput_ParticleAxis1)
		{
			axis1s = _RetrieveStorageBuffer(m_ApiManager, streamToRender, bbRequest->m_Axis1StreamId, axis1Offset);
			PK_ASSERT(axis1s != null);
		}

		RHI::PGpuBuffer		bufferIsSelected = null;
#if	(PK_HAS_PARTICLES_SELECTION != 0)
		PKSample::SRHIRenderContext	&ctxEditor = *static_cast<PKSample::SRHIRenderContext*>(&ctx);
		bufferIsSelected = ctxEditor.Selection().HasGPUParticlesSelected() ? GetIsSelectedBuffer(ctxEditor.Selection(), *dr) : null;
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)

		outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Position] = positions;
		outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Size] = sizes;
		outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Rotation] = rotations;
		outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Axis0] = axis0s;
		outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Axis1] = axis1s;
		outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Enabled] = enableds;
		outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_IsParticleSelected] = bufferIsSelected;

		outDrawCall.m_DebugDrawGPUBufferOffsets[SRHIDrawCall::DebugDrawGPUBuffer_Position] = posOffset;
		outDrawCall.m_DebugDrawGPUBufferOffsets[SRHIDrawCall::DebugDrawGPUBuffer_Size] = sizeOffset;
		outDrawCall.m_DebugDrawGPUBufferOffsets[SRHIDrawCall::DebugDrawGPUBuffer_Rotation] = rotOffset;
		outDrawCall.m_DebugDrawGPUBufferOffsets[SRHIDrawCall::DebugDrawGPUBuffer_Axis0] = axis0Offset;
		outDrawCall.m_DebugDrawGPUBufferOffsets[SRHIDrawCall::DebugDrawGPUBuffer_Axis1] = axis1Offset;
		outDrawCall.m_DebugDrawGPUBufferOffsets[SRHIDrawCall::DebugDrawGPUBuffer_Enabled] = enabledOffset;
		outDrawCall.m_DebugDrawGPUBufferOffsets[SRHIDrawCall::DebugDrawGPUBuffer_IsParticleSelected] = 0;

		if (!PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(positions).Valid()) ||
			!PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(sizes).Valid()) ||
			!PK_VERIFY(outDrawCall.m_VertexOffsets.PushBack(posOffset).Valid()) ||
			!PK_VERIFY(outDrawCall.m_VertexOffsets.PushBack(sizeOffset).Valid()))
			return false;
		if (rotations != null)
		{
			if (!PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(rotations).Valid()) ||
				!PK_VERIFY(outDrawCall.m_VertexOffsets.PushBack(rotOffset).Valid()))
				return false;
		}
		if (axis0s != null)
		{
			if (!PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(axis0s).Valid()) ||
				!PK_VERIFY(outDrawCall.m_VertexOffsets.PushBack(axis0Offset).Valid()))
				return false;
		}
		if (axis1s != null)
		{
			if (!PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(axis1s).Valid()) ||
				!PK_VERIFY(outDrawCall.m_VertexOffsets.PushBack(axis1Offset).Valid()))
				return false;
		}
		if (!PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(enableds).Valid()) ||
			!PK_VERIFY(outDrawCall.m_VertexOffsets.PushBack(enabledOffset).Valid()))
			return false;

		// Additional fields
		const u32	additionalInputCount = bbRequest->m_AdditionalInputs.Count();
		for (u32 iInput = 0; iInput < additionalInputCount; ++iInput)
		{
			u32					offset = 0;
			RHI::PGpuBuffer		buffer = _RetrieveStorageBuffer(m_ApiManager, streamToRender, bbRequest->m_AdditionalInputs[iInput].m_StreamId, offset);
			if (!PK_VERIFY(buffer != null) ||
				!PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(buffer).Valid()) ||
				!PK_VERIFY(outDrawCall.m_VertexOffsets.PushBack(offset).Valid()))
				return false;

			// Semantic for the RHI draw-call (used by the editor)
			if (bbRequest->m_AdditionalInputs[iInput].m_Type == PopcornFX::EBaseTypeID::BaseType_Float4 &&
				(bbRequest->m_AdditionalInputs[iInput].m_Name == BasicRendererProperties::SID_Diffuse_Color() ||
				bbRequest->m_AdditionalInputs[iInput].m_Name == BasicRendererProperties::SID_Diffuse_DiffuseColor()))
			{
				outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Color] = buffer;
				outDrawCall.m_DebugDrawGPUBufferOffsets[SRHIDrawCall::DebugDrawGPUBuffer_Color] = offset;
			}
			else if (outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Color] == null &&
					bbRequest->m_AdditionalInputs[iInput].m_Type == PopcornFX::EBaseTypeID::BaseType_Float4 &&
					(bbRequest->m_AdditionalInputs[iInput].m_Name == BasicRendererProperties::SID_Emissive_EmissiveColor() ||
					bbRequest->m_AdditionalInputs[iInput].m_Name == BasicRendererProperties::SID_Distortion_Color()))
			{
				outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Color] = buffer;
				outDrawCall.m_DebugDrawGPUBufferOffsets[SRHIDrawCall::DebugDrawGPUBuffer_Color] = offset;
			}
		}

		outDrawCall.m_IndirectBuffer = m_IndirectDraw.m_Buffer;
		outDrawCall.m_IndirectBufferOffset = dri * sizeof(RHI::SDrawIndirectArgs);
		outDrawCall.m_EstimatedParticleCount = dr->RenderedParticleCount();

		// Now we need to create the indirect buffer to store the draw informations
		// We do not want to read-back the exact particle count from the GPU, so we are creating an
		// indirect buffer and send a copy command to copy the exact particle count to this buffer:
		RHI::PGpuBuffer		particleSimInfo = _RetrieveParticleInfoBuffer(m_ApiManager, streamToRender);
		if (!PK_VERIFY(particleSimInfo != null))
			return false;

		if (!PK_VERIFY(renderContext.m_DrawOutputs.m_CopyCommands.PushBack().Valid()))
			return false;
		SRHICopyCommand		&copyCommand = renderContext.m_DrawOutputs.m_CopyCommands.Last();

		// We retrieve the particles info buffer:
		copyCommand.m_SrcBuffer = particleSimInfo;
		copyCommand.m_SrcOffset = 0;
		copyCommand.m_DstBuffer = m_IndirectDraw.m_Buffer;
		copyCommand.m_DstOffset = dri * sizeof(RHI::SDrawIndirectArgs) + PK_MEMBER_OFFSET(RHI::SDrawIndirectArgs, m_VertexCount);
		copyCommand.m_SizeToCopy = sizeof(u32);

		if (!PK_VERIFY(outDrawCall.m_PushConstants.PushBack().Valid()))
			return false;
		PK_STATIC_ASSERT(sizeof(Drawers::SBillboardDrawRequest) == sizeof(CFloat4));
		Drawers::SBillboardDrawRequest	&desc = *reinterpret_cast<Drawers::SBillboardDrawRequest*>(&outDrawCall.m_PushConstants.Last());
		desc.Setup(*bbRequest);
	}
	return true;
}

//----------------------------------------------------------------------------
//
// CRHIRendererBatch_BillboardGPU_VertexBB
//
//----------------------------------------------------------------------------

bool	CRHIRendererBatch_BillboardGPU_VertexBB::Setup(const CRendererDataBase *renderer, const CParticleRenderMedium *owner, const CFrameCollector *fc, const CStringId &storageClass)
{
	if (!CRendererBatchJobs_Billboard_GPUBB::Setup(renderer, owner, fc, storageClass))
		return false;

	// Quad or Capsule
	const CRendererDataBillboard			*billboardRenderer = static_cast<const CRendererDataBillboard*>(renderer);
	const SBillboardRendererDeclaration		&decl = billboardRenderer->m_RendererDeclaration;
	const EBillboardMode					billboardingMode = decl.GetPropertyValue_Enum<EBillboardMode>(BasicRendererProperties::SID_BillboardingMode(), BillboardMode_ScreenAligned);
	m_CapsulesDC = billboardingMode == BillboardMode_AxisAlignedCapsule; // Right now, capsules are not batched with other billboarding modes

	// Setup additional fields:
	// The additional fields are supposed to be the same for all renderers in a batch.
	// If not, then you can recompute then on the "Bind()" method.

	const auto		&toGenerate = m_DrawPass->m_ToGenerate;
	const u32		additionalFieldsCount = toGenerate.m_AdditionalGeneratedInputs.Count();

	if (!PK_VERIFY(m_AdditionalFieldsSimStreamOffsets.m_Fields.Reserve(additionalFieldsCount)))
		return false;

	for (u32 i = 0; i < additionalFieldsCount; ++i)
	{
		// no ignored field

		m_AdditionalFieldsSimStreamOffsets.m_Fields.PushBackUnsafe(SAdditionalInputs(sizeof(u32) /* it contains the sim buffer offsets */, i));
	}

	m_ColorStreamIdx = _GetDrawDebugColorIndex(m_AdditionalFieldsSimStreamOffsets, toGenerate);

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_BillboardGPU_VertexBB::AreRenderersCompatible(const CRendererDataBase *rendererA, const CRendererDataBase *rendererB) const
{
	return	CRendererBatchJobs_Billboard_GPUBB::AreRenderersCompatible(rendererA, rendererB) &&
			AreBillboardingBatchable(rendererA->m_RendererCache, rendererB->m_RendererCache);
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_BillboardGPU_VertexBB::AllocBuffers(SRenderContext &ctx)
{
	(void)ctx;

	const u32	drCount = m_DrawPass->m_DrawRequests.Count();
	const u32	drCountAligned = Mem::Align<0x10>(drCount);

	m_NeedGPUSort = m_DrawPass->m_DrawRequests.First()->BaseBillboardingRequest().m_Flags.m_NeedSort;
	m_SortByCameraDistance = m_DrawPass->m_DrawRequests.First()->BaseBillboardingRequest().m_SortByCameraDistance;

	// GPU particles are rendered using DrawIndexedInstancedIndirect
	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Indirect Draw Args Buffer"), true, m_ApiManager, m_IndirectDraw, RHI::IndirectDrawBuffer, drCountAligned *  sizeof(RHI::SDrawIndexedIndirectArgs), drCount * sizeof(RHI::SDrawIndexedIndirectArgs)))
		return false;

	const u32	viewIndependentInputs = m_DrawPass->m_ToGenerate.m_GeneratedInputs;

	// Camera sort
	if (m_NeedGPUSort)
	{
		// For each draw request, we have indirection and sort key buffers
		// and a GPU sorter object, handling intermediates work buffers
		if (!PK_VERIFY(m_CameraSortIndirection.Resize(drCount)) ||
			!PK_VERIFY(m_CameraSortKeys.Resize(drCount)) ||
			!PK_VERIFY(m_CameraGPUSorters.Resize(drCount)))
			return false;
		for (u32 i = 0; i < drCount; i++)
		{
			if (!PK_VERIFY(m_DrawPass->m_DrawRequests[i]->RenderedParticleCount() > 0))
				continue;
			const u32	alignedParticleCount = Mem::Align<PK_GPU_SORT_NUM_KEY_PER_THREAD * PK_RH_GPU_THREADGROUP_SIZE>(m_DrawPass->m_DrawRequests[i]->RenderedParticleCount());
			if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Camera Sort Indirection Buffer"), true, m_ApiManager, m_CameraSortIndirection[i], RHI::RawBuffer, alignedParticleCount * sizeof(u32), alignedParticleCount * sizeof(u32)) ||
				!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Camera Sort Keys Buffer"), true, m_ApiManager, m_CameraSortKeys[i], RHI::RawBuffer, alignedParticleCount * sizeof(u32), alignedParticleCount * sizeof(u32)))
				return false;
			// 16 bits key
			const u32	sortKeySizeInBits = 16;
			// Init sorter if needed and allocate buffers
			if (!PK_VERIFY(m_CameraGPUSorters[i].Init(sortKeySizeInBits, m_ApiManager)) ||
				!PK_VERIFY(m_CameraGPUSorters[i].AllocateBuffers(m_DrawPass->m_DrawRequests[i]->RenderedParticleCount(), m_ApiManager)))
				return false;
		}
		// Create compute sort key constant set
		if (!PK_VERIFY(m_ComputeCameraSortKeysConstantSets.Reserve(drCount)))
			return false;
		RHI::SConstantSetLayout	layout;
		PKSample::CreateComputeSortKeysConstantSetLayout(layout, m_SortByCameraDistance, false);
		while (m_ComputeCameraSortKeysConstantSets.Count() < drCount)
		{
			RHI::PConstantSet	cs = m_ApiManager->CreateConstantSet(RHI::SRHIResourceInfos("Camera Sort Keys Constant Set"), layout);
			m_ComputeCameraSortKeysConstantSets.PushBackUnsafe(cs);
		}
	}

	// Allocate once, max number of draw requests, indexed by DC from push constant
	const u32	offsetsSizeInBytes = kMaxDrawRequestCount * sizeof(u32); // u32 offsets

	// Particle positions stream
	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Offsets_Enableds Buffer"), (viewIndependentInputs & Drawers::GenInput_ParticlePosition) != 0, m_ApiManager, m_SimStreamOffsets_Enableds, RHI::RawBuffer, offsetsSizeInBytes, offsetsSizeInBytes) ||
		!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Offsets_Positions Buffer"), (viewIndependentInputs & Drawers::GenInput_ParticlePosition) != 0, m_ApiManager, m_SimStreamOffsets_Positions, RHI::RawBuffer, offsetsSizeInBytes, offsetsSizeInBytes))
		return false;
	// GPU sort related offset buffers
	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Offsets_CustomSortKeys Buffer"), m_NeedGPUSort && !m_SortByCameraDistance, m_ApiManager, m_CustomSortKeysOffsets, RHI::RawBuffer, offsetsSizeInBytes, offsetsSizeInBytes))
		return false;
	// Size Size2 (can be either one)
	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Offsets_Sizes Buffer"), (viewIndependentInputs & Drawers::GenInput_ParticleSize) != 0, m_ApiManager, m_SimStreamOffsets_Sizes, RHI::RawBuffer, offsetsSizeInBytes, offsetsSizeInBytes))
		return false;
	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Offsets_Size2s Buffer"), (viewIndependentInputs & Drawers::GenInput_ParticleSize2) != 0, m_ApiManager, m_SimStreamOffsets_Size2s, RHI::RawBuffer, offsetsSizeInBytes, offsetsSizeInBytes))
		return false;
	// Rotation particle stream
	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Offsets_Rotations Buffer"), (viewIndependentInputs & Drawers::GenInput_ParticleRotation) != 0, m_ApiManager, m_SimStreamOffsets_Rotations, RHI::RawBuffer, offsetsSizeInBytes, offsetsSizeInBytes))
		return false;
	// First billboarding axis
	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Offsets_Axis0s Buffer"), (viewIndependentInputs & Drawers::GenInput_ParticleAxis0) != 0, m_ApiManager, m_SimStreamOffsets_Axis0s, RHI::RawBuffer, offsetsSizeInBytes, offsetsSizeInBytes))
		return false;
	// Second billboarding axis
	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Offsets_Axis1s Buffer"), (viewIndependentInputs & Drawers::GenInput_ParticleAxis1) != 0, m_ApiManager, m_SimStreamOffsets_Axis1s, RHI::RawBuffer, offsetsSizeInBytes, offsetsSizeInBytes))
		return false;

	// Additional fields simStreamOffsets:
	if (!m_AdditionalFieldsSimStreamOffsets.AllocBuffers(kMaxDrawRequestCount, m_ApiManager))
		return false;

	if (!m_Initialized)
		m_Initialized = _InitStaticBuffers(); // Important: after alloctation of the Offsets_XXX buffers
	if (!m_Initialized)
		return false;

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_BillboardGPU_VertexBB::MapBuffers(SRenderContext &ctx)
{
	(void)ctx;
	const u32	drCount = m_DrawPass->m_DrawRequests.Count();

	m_MappedIndirectBuffer = static_cast<RHI::SDrawIndexedIndirectArgs*>(m_ApiManager->MapCpuView(m_IndirectDraw.m_Buffer));

	// Mandatory streams
	m_MappedSimStreamOffsets[0] = static_cast<u32*>(m_ApiManager->MapCpuView(m_SimStreamOffsets_Enableds.m_Buffer));
	m_MappedSimStreamOffsets[1] = static_cast<u32*>(m_ApiManager->MapCpuView(m_SimStreamOffsets_Positions.m_Buffer));
	if (m_SimStreamOffsets_Sizes.Used())
		m_MappedSimStreamOffsets[2] = static_cast<u32*>(m_ApiManager->MapCpuView(m_SimStreamOffsets_Sizes.m_Buffer));
	else if (m_SimStreamOffsets_Size2s.Used())
		m_MappedSimStreamOffsets[2] = static_cast<u32*>(m_ApiManager->MapCpuView(m_SimStreamOffsets_Size2s.m_Buffer));

	// Optional streams
	m_MappedSimStreamOffsets[3] = m_SimStreamOffsets_Rotations.Used() ? static_cast<u32*>(m_ApiManager->MapCpuView(m_SimStreamOffsets_Rotations.m_Buffer)) : null;
	m_MappedSimStreamOffsets[4] = m_SimStreamOffsets_Axis0s.Used() ? static_cast<u32*>(m_ApiManager->MapCpuView(m_SimStreamOffsets_Axis0s.m_Buffer)) : null;
	m_MappedSimStreamOffsets[5] = m_SimStreamOffsets_Axis1s.Used() ? static_cast<u32*>(m_ApiManager->MapCpuView(m_SimStreamOffsets_Axis1s.m_Buffer)) : null;

	// Additional streams
	if (!m_AdditionalFieldsSimStreamOffsets.MapBuffers(drCount, m_ApiManager))
		return false;

	// Camera sort optional stream offsets
	if (m_NeedGPUSort && !m_SortByCameraDistance)
	{
		m_MappedCustomSortKeysOffsets = static_cast<u32*>(m_ApiManager->MapCpuView(m_CustomSortKeysOffsets.m_Buffer));
		if (!PK_VERIFY(m_MappedCustomSortKeysOffsets != null))
			return false;
	}

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_BillboardGPU_VertexBB::LaunchCustomTasks(SRenderContext &ctx)
{
	(void)ctx;
	const u32	drCount = m_DrawPass->m_DrawRequests.Count();

	PK_ASSERT(m_MappedIndirectBuffer != null);
	for (u32 dri = 0; dri < drCount; ++dri)
	{
		// Indirect buffer
		m_MappedIndirectBuffer[dri].m_InstanceCount = m_DrawPass->m_DrawRequests[dri]->RenderedParticleCount(); // will be overwritten by a GPU command.
		m_MappedIndirectBuffer[dri].m_IndexOffset = 0;
		m_MappedIndirectBuffer[dri].m_IndexCount = m_CapsulesDC ? 12 : 6;
		m_MappedIndirectBuffer[dri].m_VertexOffset = 0;
		m_MappedIndirectBuffer[dri].m_InstanceOffset = 0;
	}

#if (PK_PARTICLES_UPDATER_USE_GPU != 0)
	const u32	streamMaxCount = 6 + (m_NeedGPUSort && !m_SortByCameraDistance ? 1 : 0) + m_AdditionalFieldsSimStreamOffsets.m_MappedFields.Count();

	// Store offsets in a stack view ([PosOffsetDr0][PosOffsetDr1][SizeOffsetDr0][SizeOffsetDr1]..)
	PK_STACKALIGNEDMEMORYVIEW(u32, streamsOffsets, drCount * streamMaxCount, 0x10);

	for (u32 iDr = 0; iDr < drCount; ++iDr)
	{
		// Fill in stream offsets for each draw request
		const CParticleStreamToRender_GPU				*streamToRender = m_DrawPass->m_DrawRequests[iDr]->StreamToRender_GPU();
		if (!PK_VERIFY(streamToRender != null))
			continue;
		const Drawers::SBase_DrawRequest			*dr = m_DrawPass->m_DrawRequests[iDr];
		const Drawers::SBase_BillboardingRequest	baseBr = dr->BaseBillboardingRequest();

		u32		offset = 0;

		// Mandatory streams
		streamsOffsets[offset++ * drCount + iDr] = streamToRender->StreamOffset(baseBr.m_EnabledStreamId);
		streamsOffsets[offset++ * drCount + iDr] = streamToRender->StreamOffset(baseBr.m_PositionStreamId);

		// Billboard streams
		{
			const Drawers::SBillboard_DrawRequest	*billboardDr = static_cast<const Drawers::SBillboard_DrawRequest *>(dr);
			// Mandatory
			streamsOffsets[offset++ * drCount + iDr] = streamToRender->StreamOffset(billboardDr->m_BB.m_SizeStreamId);
			// Optional
			if (m_SimStreamOffsets_Rotations.Used())
				streamsOffsets[offset++ * drCount + iDr] = streamToRender->StreamOffset(billboardDr->m_BB.m_RotationStreamId);
			if (m_SimStreamOffsets_Axis0s.Used())
				streamsOffsets[offset++ * drCount + iDr] = streamToRender->StreamOffset(billboardDr->m_BB.m_Axis0StreamId);
			if (m_SimStreamOffsets_Axis1s.Used())
				streamsOffsets[offset++ * drCount + iDr] = streamToRender->StreamOffset(billboardDr->m_BB.m_Axis1StreamId);
		}

		// Add all non-virtual stream additional inputs
		for (auto &addField : m_AdditionalFieldsSimStreamOffsets.m_Fields)
			streamsOffsets[offset++ * drCount + iDr] = streamToRender->StreamOffset(baseBr.m_AdditionalInputs[addField.m_AdditionalInputIndex].m_StreamId);

		if (m_NeedGPUSort && !m_SortByCameraDistance)
			streamsOffsets[offset++ * drCount + iDr] = streamToRender->StreamOffset(baseBr.m_SortKeyStreamId);

		PK_ASSERT(offset <= streamMaxCount);
	}

	// Non temporal writes to gpu mem, aligned and contiguous
	u32	streamOffset = 0;
	Mem::Copy_Uncached(m_MappedSimStreamOffsets[0], &streamsOffsets[streamOffset++ * drCount], sizeof(u32) * drCount);
	Mem::Copy_Uncached(m_MappedSimStreamOffsets[1], &streamsOffsets[streamOffset++ * drCount], sizeof(u32) * drCount);

	if (m_SimStreamOffsets_Size2s.Used() || m_SimStreamOffsets_Sizes.Used())
		Mem::Copy_Uncached(m_MappedSimStreamOffsets[2], &streamsOffsets[streamOffset++ * drCount], sizeof(u32) * drCount);

	if (m_MappedSimStreamOffsets[3] != null)
		Mem::Copy_Uncached(m_MappedSimStreamOffsets[3], &streamsOffsets[streamOffset++ * drCount], sizeof(u32) * drCount);
	if (m_MappedSimStreamOffsets[4] != null)
		Mem::Copy_Uncached(m_MappedSimStreamOffsets[4], &streamsOffsets[streamOffset++ * drCount], sizeof(u32) * drCount);
	if (m_MappedSimStreamOffsets[5] != null)
		Mem::Copy_Uncached(m_MappedSimStreamOffsets[5], &streamsOffsets[streamOffset++ * drCount], sizeof(u32) * drCount);

	for (u32 iInput = 0; iInput < m_AdditionalFieldsSimStreamOffsets.m_MappedFields.Count(); ++iInput)
		Mem::Copy_Uncached(m_AdditionalFieldsSimStreamOffsets.m_MappedFields[iInput].m_Storage.m_RawDataPtr, &streamsOffsets[streamOffset++ * drCount], sizeof(u32) * drCount);

	if (m_MappedCustomSortKeysOffsets != null)
		Mem::Copy_Uncached(m_MappedCustomSortKeysOffsets, &streamsOffsets[streamOffset++ * drCount], sizeof(u32) * drCount);

#endif // (PK_PARTICLES_UPDATER_USE_GPU != 0)

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_BillboardGPU_VertexBB::UnmapBuffers(SRenderContext &ctx)
{
	(void)ctx;

	m_IndirectDraw.UnmapIFN(m_ApiManager);
	m_MappedIndirectBuffer = null;

	m_CustomSortKeysOffsets.UnmapIFN(m_ApiManager);
	m_MappedCustomSortKeysOffsets = null;

	// GPU stream offsets
	m_SimStreamOffsets_Enableds.UnmapIFN(m_ApiManager);
	m_SimStreamOffsets_Positions.UnmapIFN(m_ApiManager);
	m_SimStreamOffsets_Sizes.UnmapIFN(m_ApiManager);
	m_SimStreamOffsets_Size2s.UnmapIFN(m_ApiManager);
	m_SimStreamOffsets_Rotations.UnmapIFN(m_ApiManager);
	m_SimStreamOffsets_Axis0s.UnmapIFN(m_ApiManager);
	m_SimStreamOffsets_Axis1s.UnmapIFN(m_ApiManager);
	Mem::Clear(m_MappedSimStreamOffsets);

	m_AdditionalFieldsSimStreamOffsets.UnmapBuffers(m_ApiManager);

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_BillboardGPU_VertexBB::EmitDrawCall(SRenderContext &ctx, const SDrawCallDesc &toEmit)
{
	SRHIRenderContext	&renderContext = static_cast<SRHIRenderContext&>(ctx);

	// !Currently, there is no batching for gpu particles!
	// So we'll emit one draw call per draw request
	// Some data are indexed by the draw-requests. So "EmitDrawCall" shoudl be called once, with all draw-requests.
	PK_ASSERT(toEmit.m_DrawRequests.Count() == m_DrawPass->m_DrawRequests.Count());
	const u32	drCount = m_DrawPass->m_DrawRequests.Count();

	// No need to iterate on all draw requests, just take the first as reference as they wouldn't have been batched if not compatible
	CRendererCacheInstance_UpdateThread		*renderCacheInstance = static_cast<CRendererCacheInstance_UpdateThread*>(toEmit.m_RendererCaches.First().Get());
	if (!PK_VERIFY(renderCacheInstance != null))
	{
		CLog::Log(PK_ERROR, "Invalid renderer cache instance");
		return false;
	}
	PKSample::PCRendererCacheInstance	rCacheInstance = renderCacheInstance->RenderThread_GetCacheInstance();
	if (!PK_VERIFY(rCacheInstance != null))
		return false;

	for (u32 dri = 0; dri < drCount; ++dri)
	{
		PK_ASSERT(toEmit.m_DrawRequests[dri] != null);
		const Drawers::SBillboard_DrawRequest			*dr = static_cast<const Drawers::SBillboard_DrawRequest*>(toEmit.m_DrawRequests[dri]);
		const Drawers::SBillboard_BillboardingRequest	&bbRequest = dr->m_BB;
		const CParticleStreamToRender					*streamToRender = &dr->StreamToRender();
		if (!PK_VERIFY(streamToRender != null))
			continue;

		u32		dummyOffset = 0;
		RHI::PGpuBuffer		drStreamBuffer = _RetrieveStorageBuffer(m_ApiManager, streamToRender, bbRequest.m_PositionStreamId, dummyOffset);
		RHI::PGpuBuffer		drStreamSizeBuffer = _RetrieveParticleInfoBuffer(m_ApiManager, streamToRender);

		if (!PK_VERIFY(drStreamBuffer != null) ||
			!PK_VERIFY(drStreamSizeBuffer != null))
			return false;

		// Camera GPU sort
		if (m_NeedGPUSort)
		{
			// Compute: Compute sort keys and init indirection buffer
			{
				// For radix sort, a thread computes PK_GPU_SORT_NUM_KEY_PER_THREAD keys (see shader definition and RHIGPUSorter.cpp),
				// so a group computes (PK_GPU_SORT_NUM_KEY_PER_THREAD * PK_RH_GPU_THREADGROUP_SIZE) keys

				const u32	sortGroupCount = (dr->RenderedParticleCount() + PK_GPU_SORT_NUM_KEY_PER_THREAD * PK_RH_GPU_THREADGROUP_SIZE - 1) / (PK_GPU_SORT_NUM_KEY_PER_THREAD * PK_RH_GPU_THREADGROUP_SIZE);
				PKSample::SRHIComputeDispatchs	computeDispatch = PKSample::SRHIComputeDispatchs();

				// Constant set
				computeDispatch.m_NeedSceneInfoConstantSet = m_SortByCameraDistance;
				RHI::PConstantSet	computeConstantSet = m_ComputeCameraSortKeysConstantSets[dri];
				computeConstantSet->SetConstants(drStreamSizeBuffer, 0);
				computeConstantSet->SetConstants(drStreamBuffer, 1);
				computeConstantSet->SetConstants(m_SortByCameraDistance ? m_SimStreamOffsets_Positions.m_Buffer : m_CustomSortKeysOffsets.m_Buffer, 2);
				computeConstantSet->SetConstants(m_CameraSortKeys[dri].m_Buffer, 3);
				computeConstantSet->SetConstants(m_CameraSortIndirection[dri].m_Buffer, 4);

				computeConstantSet->UpdateConstantValues();
				computeDispatch.m_ConstantSet = computeConstantSet;

				// Push constant
				if (!PK_VERIFY(computeDispatch.m_PushConstants.PushBack().Valid()))
					return false;
				u32	*drPushConstant = reinterpret_cast<u32*>(&computeDispatch.m_PushConstants.Last());
				drPushConstant[0] = dri;

				// State
				const PKSample::EComputeShaderType	type = m_SortByCameraDistance ? ComputeType_ComputeSortKeys_CameraDistance : ComputeType_ComputeSortKeys;
				computeDispatch.m_State = rCacheInstance->m_Cache->GetComputeState(type);
				if (!PK_VERIFY(computeDispatch.m_State != null))
					return false;

				// Dispatch args
				// For the sort key computation dispatch, we need exactly twice the sort group count
				// so that every thread associated key is initialized. Overflowing keys are initialized
				// to 0xFFFF, sorted last (and ultimately not rendered).
				computeDispatch.m_ThreadGroups = CInt3(sortGroupCount * PK_GPU_SORT_NUM_KEY_PER_THREAD, 1, 1);

				renderContext.m_DrawOutputs.m_ComputeDispatchs.PushBack(computeDispatch);
			}

			// Compute: Sort computes
			{
				m_CameraGPUSorters[dri].SetInOutBuffers(m_CameraSortKeys[dri].m_Buffer, m_CameraSortIndirection[dri].m_Buffer);
				m_CameraGPUSorters[dri].AppendDispatchs(rCacheInstance, renderContext.m_DrawOutputs.m_ComputeDispatchs);
			}
		}

		// Emit draw call
		{
			const u32				shaderOptions = PKSample::Option_VertexPassThrough | PKSample::Option_GPUStorage | _GetVertexBillboardShaderOptions(bbRequest, m_NeedGPUSort);
			const RHI::PGpuBuffer	sortIndirectionBuffer = m_NeedGPUSort ? m_CameraSortIndirection[dri].m_Buffer : null;
			RHI::PConstantSet		vertexBBSimDataConstantSet;
			// create the local constant-set
			{
				const RHI::SConstantSetLayout	*simDataConstantSetLayout = null;
				const RHI::SConstantSetLayout	*offsetsConstantSetLayout = null;
				if (!PK_VERIFY(rCacheInstance->m_Cache->GetGPUStorageConstantSets(static_cast<PKSample::EShaderOptions>(shaderOptions), simDataConstantSetLayout, offsetsConstantSetLayout)) ||
					!PK_VERIFY(simDataConstantSetLayout != null) ||
					!PK_VERIFY(offsetsConstantSetLayout != null) ||
					simDataConstantSetLayout->m_Constants.Empty() ||
					offsetsConstantSetLayout->m_Constants.Empty())
					return false;

				vertexBBSimDataConstantSet = m_ApiManager->CreateConstantSet(RHI::SRHIResourceInfos("Sim Data Constant Set"), *simDataConstantSetLayout);
				if (!PK_VERIFY(vertexBBSimDataConstantSet != null))
					return false;

				PK_ASSERT(drStreamBuffer != null);
				// Sim data constant set are:
				// raw stream + camera sort indirection buffer if needed
				PK_ASSERT(vertexBBSimDataConstantSet->GetConstantValues().Count() == (1u + (m_NeedGPUSort ? 1u : 0u)));
				u32	constantSetLocation = 0;
				if (!PK_VERIFY(vertexBBSimDataConstantSet->SetConstants(drStreamBuffer, constantSetLocation++)))
					return false;
				if (m_NeedGPUSort)
				{
					if (!PK_VERIFY(sortIndirectionBuffer != null) ||
						!PK_VERIFY(vertexBBSimDataConstantSet->SetConstants(sortIndirectionBuffer, constantSetLocation++)))
						return false;
				}
				vertexBBSimDataConstantSet->UpdateConstantValues();
			}

			if (!PK_VERIFY(renderContext.m_DrawOutputs.m_DrawCalls.PushBack().Valid()))
			{
				CLog::Log(PK_ERROR, "Failed to create a draw-call");
				return false;
			}
			SRHIDrawCall		&outDrawCall = renderContext.m_DrawOutputs.m_DrawCalls.Last();

			outDrawCall.m_Batch = this;
			outDrawCall.m_RendererCacheInstance = renderCacheInstance;
			outDrawCall.m_Type = SRHIDrawCall::DrawCall_IndexedInstancedIndirect;
			outDrawCall.m_ShaderOptions = shaderOptions;
			outDrawCall.m_RendererType = Renderer_Billboard;
			outDrawCall.m_GPUStorageSimDataConstantSet = vertexBBSimDataConstantSet;

			// Some meta-data (the Editor uses them)
			{
				outDrawCall.m_BBox = toEmit.m_BBox;
				outDrawCall.m_TotalBBox = m_DrawPass->m_TotalBBox;
				outDrawCall.m_SlicedDC = toEmit.m_TotalParticleCount != m_DrawPass->m_TotalParticleCount;

				outDrawCall.m_Valid =	rCacheInstance->m_Cache != null &&
										rCacheInstance->m_Cache->GetRenderState(static_cast<PKSample::EShaderOptions>(outDrawCall.m_ShaderOptions)) != null;
			}

			outDrawCall.m_GPUStorageOffsetsConstantSet = m_VertexBBOffsetsConstantSet;

			// A single vertex buffer is used for the instanced draw: the texcoords buffer, contains the direction in which vertices should be expanded
			PK_ASSERT(m_TexCoords.Used());
			if (!PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(m_TexCoords.m_Buffer).Valid()))
				return false;

			{
#if	(PK_HAS_PARTICLES_SELECTION != 0)
				PKSample::SRHIRenderContext	&ctxEditor = *static_cast<PKSample::SRHIRenderContext*>(&ctx);
				RHI::PGpuBuffer		bufferIsSelected = ctxEditor.Selection().HasGPUParticlesSelected() ? GetIsSelectedBuffer(ctxEditor.Selection(), *dr) : null;
				if (bufferIsSelected != null)
				{
					RHI::SConstantSetLayout	selectionSetLayout(RHI::VertexShaderMask);
					selectionSetLayout.AddConstantsLayout(RHI::SRawBufferDesc("Selections"));
					RHI::PConstantSet	selectionConstantSet = m_ApiManager->CreateConstantSet(RHI::SRHIResourceInfos("Selection Constant Set"), selectionSetLayout);
					if (PK_VERIFY(selectionConstantSet != null) && PK_VERIFY(selectionConstantSet->SetConstants(bufferIsSelected, 0)))
					{
						selectionConstantSet->UpdateConstantValues();
						outDrawCall.m_SelectionConstantSet = selectionConstantSet;
					}
				}
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)
			}

			// Fill the semantics for the debug draws:
			outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Enabled] = m_SimStreamOffsets_Enableds.m_Buffer;
			outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Position] = m_SimStreamOffsets_Positions.m_Buffer;
			outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Size] = m_SimStreamOffsets_Sizes.Used() ? m_SimStreamOffsets_Sizes.m_Buffer : m_SimStreamOffsets_Size2s.m_Buffer;
			outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Rotation] = m_SimStreamOffsets_Rotations.m_Buffer;
			outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Axis0] = m_SimStreamOffsets_Axis0s.m_Buffer;
			outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Axis1] = m_SimStreamOffsets_Axis1s.m_Buffer;
			outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Texcoords] = m_TexCoords.m_Buffer;
			if (m_ColorStreamIdx.Valid())
				outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Color] = m_AdditionalFieldsSimStreamOffsets.m_Fields[m_ColorStreamIdx].m_Buffer.m_Buffer;

			outDrawCall.m_IndexOffset = 0;
			outDrawCall.m_IndexSize = RHI::IndexBuffer16Bit;
			outDrawCall.m_IndexBuffer = m_DrawIndices.m_Buffer;

			outDrawCall.m_IndirectBuffer = m_IndirectDraw.m_Buffer;
			outDrawCall.m_IndirectBufferOffset = dri * sizeof(RHI::SDrawIndexedIndirectArgs);
			outDrawCall.m_EstimatedParticleCount = dr->RenderedParticleCount();

			// Now we need to create the indirect buffer to store the draw informations
			// We do not want to read-back the exact particle count from the GPU, so we are creating an
			// indirect buffer and send a copy command to copy the exact particle count to this buffer:
			if (!PK_VERIFY(renderContext.m_DrawOutputs.m_CopyCommands.PushBack().Valid()))
				return false;

			SRHICopyCommand		&copyCommand = renderContext.m_DrawOutputs.m_CopyCommands.Last();

			// We retrieve the particles info buffer:
			copyCommand.m_SrcBuffer = drStreamSizeBuffer;
			copyCommand.m_SrcOffset = 0;
			copyCommand.m_DstBuffer = m_IndirectDraw.m_Buffer;
			copyCommand.m_DstOffset = dri * sizeof(RHI::SDrawIndexedIndirectArgs) + PK_MEMBER_OFFSET(RHI::SDrawIndexedIndirectArgs, m_InstanceCount);
			copyCommand.m_SizeToCopy = sizeof(u32);

			if (!PK_VERIFY(outDrawCall.m_PushConstants.PushBack().Valid()))
				return false;
			PK_STATIC_ASSERT(sizeof(Drawers::SBillboardDrawRequest) == sizeof(CFloat4));
			Drawers::SBillboardDrawRequest	&desc = *reinterpret_cast<Drawers::SBillboardDrawRequest*>(&outDrawCall.m_PushConstants.Last());
			desc.Setup(bbRequest);

			// GPUBillboardPushConstants
			if (!PK_VERIFY(outDrawCall.m_PushConstants.PushBack().Valid()))
				return false;
			SVertexBillboardingConstants	&indices = *reinterpret_cast<SVertexBillboardingConstants*>(&outDrawCall.m_PushConstants.Last());
			indices.m_IndicesOffset = toEmit.m_IndexOffset;
			indices.m_StreamOffsetsIndex = dri;
		}
	}

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_BillboardGPU_VertexBB::_InitStaticBuffers()
{
	// Index buffer
	// RHI currently doesn't support DrawInstanced (without indices)
	{
		static const u32	indexCount = 12;
		if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("VertexBB Draw Indices Buffer"), true, m_ApiManager, m_DrawIndices, RHI::IndexBuffer, indexCount * sizeof(u16), indexCount * sizeof(u16)))
			return false;
		volatile u16	*indices = static_cast<u16*>(m_ApiManager->MapCpuView(m_DrawIndices.m_Buffer, 0, indexCount * sizeof(u16)));
		if (!PK_VERIFY(indices != null))
			return false;
		indices[0] = 1;
		indices[1] = 3;
		indices[2] = 0;
		indices[3] = 2;
		indices[4] = 3;
		indices[5] = 1;
		indices[6] = 3;
		indices[7] = 5;
		indices[8] = 0;
		indices[9] = 1;
		indices[10] = 4;
		indices[11] = 2;
		m_DrawIndices.Unmap(m_ApiManager);
	}

	// TexCoords buffer
	{
		static const u32	uvCount = 6;
		if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("VertexBB Texcoords Buffer"), true, m_ApiManager, m_TexCoords, RHI::VertexBuffer, uvCount * sizeof(CFloat2), uvCount * sizeof(CFloat2)))
			return false;
		volatile float	*texCoords = static_cast<float*>(m_ApiManager->MapCpuView(m_TexCoords.m_Buffer, 0, uvCount * sizeof(CFloat2)));
		if (!PK_VERIFY(texCoords != null))
			return false;
		texCoords[0] = -1.0f; // Lower left corner
		texCoords[1] = -1.0f;
		texCoords[2] = -1.0f; // Upper left corner
		texCoords[3] = 1.0f;
		texCoords[4] = 1.0f; // Upper right corner
		texCoords[5] = 1.0f;
		texCoords[6] = 1.0f; // Lower right corner
		texCoords[7] = -1.0f;
		texCoords[8] = 0.0f; // Capsule up
		texCoords[9] = 2.0f;
		texCoords[10] = 0.0f; // Capsule down
		texCoords[11] = -2.0f;
		m_TexCoords.Unmap(m_ApiManager);
	}

	// Constant-Set
	{
		CRendererCacheInstance_UpdateThread		*renderCacheInstance = static_cast<CRendererCacheInstance_UpdateThread*>(m_DrawPass->m_RendererCaches.First().Get());
		PK_ASSERT(renderCacheInstance != null);
		const PCRendererCacheInstance	cacheInstance = renderCacheInstance->RenderThread_GetCacheInstance();
		if (cacheInstance == null)
			return false;

		const Drawers::SBillboard_DrawRequest			*dr = static_cast<const Drawers::SBillboard_DrawRequest*>(m_DrawPass->m_DrawRequests.First());
		const Drawers::SBillboard_BillboardingRequest	&bbRequest = dr->m_BB;
		const u32	shaderOptions = PKSample::Option_VertexPassThrough | PKSample::Option_GPUStorage | _GetVertexBillboardShaderOptions(bbRequest, m_NeedGPUSort);

		const RHI::SConstantSetLayout	*simDataConstantSetLayout = null;
		const RHI::SConstantSetLayout	*offsetsConstantSetLayout = null;
		if (!PK_VERIFY(cacheInstance->m_Cache->GetGPUStorageConstantSets(static_cast<PKSample::EShaderOptions>(shaderOptions), simDataConstantSetLayout, offsetsConstantSetLayout)) ||
			!PK_VERIFY(offsetsConstantSetLayout != null) ||
			offsetsConstantSetLayout->m_Constants.Empty())
			return false;

		m_VertexBBOffsetsConstantSet = m_ApiManager->CreateConstantSet(RHI::SRHIResourceInfos("Offsets Constant Set"), *offsetsConstantSetLayout);
		if (!PK_VERIFY(m_VertexBBOffsetsConstantSet != null))
			return false;

		// Fill offsets constant sets.
		u32 i = 0;
		{
			if (!PK_VERIFY(m_VertexBBOffsetsConstantSet->SetConstants(m_SimStreamOffsets_Enableds.m_Buffer, i++)))
				return false;
			if (!PK_VERIFY(m_VertexBBOffsetsConstantSet->SetConstants(m_SimStreamOffsets_Positions.m_Buffer, i++)))
				return false;
			if (m_SimStreamOffsets_Sizes.Used())
			{
				if (!PK_VERIFY(m_VertexBBOffsetsConstantSet->SetConstants(m_SimStreamOffsets_Sizes.m_Buffer, i++)))
					return false;
			}
			else if (m_SimStreamOffsets_Size2s.Used())
			{
				if (!PK_VERIFY(m_VertexBBOffsetsConstantSet->SetConstants(m_SimStreamOffsets_Size2s.m_Buffer, i++)))
					return false;
			}
			if (m_SimStreamOffsets_Rotations.Used())
			{
				if (!PK_VERIFY(m_VertexBBOffsetsConstantSet->SetConstants(m_SimStreamOffsets_Rotations.m_Buffer, i++)))
					return false;
			}
			if (m_SimStreamOffsets_Axis0s.Used())
			{
				if (!PK_VERIFY(m_VertexBBOffsetsConstantSet->SetConstants(m_SimStreamOffsets_Axis0s.m_Buffer, i++)))
					return false;
			}
			if (m_SimStreamOffsets_Axis1s.Used())
			{
				if (!PK_VERIFY(m_VertexBBOffsetsConstantSet->SetConstants(m_SimStreamOffsets_Axis1s.m_Buffer, i++)))
					return false;
			}
			for (auto &additionalSimStreamOffset : m_AdditionalFieldsSimStreamOffsets.m_Fields)
			{
				if (!PK_VERIFY(m_VertexBBOffsetsConstantSet->SetConstants(additionalSimStreamOffset.m_Buffer.m_Buffer, i++)))
					return false;
			}
		}
		PK_ASSERT(m_VertexBBOffsetsConstantSet->GetConstantValues().Count() == i);
		m_VertexBBOffsetsConstantSet->UpdateConstantValues();
	}

	return true;
}

//----------------------------------------------------------------------------
//
// CRHIRendererBatch_Ribbon_GPU
//
//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Ribbon_GPU::Setup(const CRendererDataBase *renderer, const CParticleRenderMedium *owner, const CFrameCollector *fc, const CStringId &storageClass)
{
	if (!CRendererBatchJobs_Ribbon_GPUBB::Setup(renderer, owner, fc, storageClass))
			return false;

	const CRendererDataRibbon				*ribbonRenderer = static_cast<const CRendererDataRibbon*>(renderer);
	const SRibbonRendererDeclaration		&decl = ribbonRenderer->m_RendererDeclaration;

	// Right now, tubes & multi-plane ribbons are not batched with other billboarding modes
	const ERibbonMode		bbMode = decl.GetPropertyValue_Enum<ERibbonMode>(BasicRendererProperties::SID_BillboardingMode(), RibbonMode_ViewposAligned);
	m_MultiPlanesDC = bbMode == PopcornFX::RibbonMode_SideAxisAlignedMultiPlane;
	m_TubesDC = bbMode == PopcornFX::RibbonMode_SideAxisAlignedTube;
	if (bbMode == RibbonMode_SideAxisAlignedTube)
	{
		m_ParticleQuadCount = decl.GetPropertyValue_I1(BasicRendererProperties::SID_GeometryRibbon_SegmentCount(), 8);
	}
	else if (bbMode == RibbonMode_SideAxisAlignedMultiPlane)
	{
		m_ParticleQuadCount = decl.GetPropertyValue_I1(BasicRendererProperties::SID_GeometryRibbon_PlaneCount(), 2);
	}
	else // 2D ribbons, only one quad.
	{
		m_ParticleQuadCount = 1;
	}

	// Setup additional fields:
	// The additional fields are supposed to be the same for all renderers in a batch.
	// If not, then you can recompute then on the "Bind()" method.

	const auto		&toGenerate = m_DrawPass->m_ToGenerate;
	const u32		additionalFieldsCount = toGenerate.m_AdditionalGeneratedInputs.Count();

	if (!PK_VERIFY(m_AdditionalFieldsSimStreamOffsets.m_Fields.Reserve(additionalFieldsCount)))
		return false;

	for (u32 i = 0; i < additionalFieldsCount; ++i)
	{
		// no ignored fields
		m_AdditionalFieldsSimStreamOffsets.m_Fields.PushBackUnsafe(SAdditionalInputs(sizeof(u32) /* it contains the sim buffer offsets */, i));
	}

	m_ColorStreamIdx = _GetDrawDebugColorIndex(m_AdditionalFieldsSimStreamOffsets, toGenerate);

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Ribbon_GPU::AreRenderersCompatible(const CRendererDataBase *rendererA, const CRendererDataBase *rendererB) const
{
	return	CRendererBatchJobs_Ribbon_GPUBB::AreRenderersCompatible(rendererA, rendererB) &&
			AreBillboardingBatchable(rendererA->m_RendererCache, rendererB->m_RendererCache);
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Ribbon_GPU::AllocBuffers(SRenderContext &ctx)
{
	(void)ctx;

	const u32	drCount = m_DrawPass->m_DrawRequests.Count();
	const u32	drCountAligned = Mem::Align<0x10>(drCount);

	m_NeedGPUSort = m_DrawPass->m_DrawRequests.First()->BaseBillboardingRequest().m_Flags.m_NeedSort;
	m_SortByCameraDistance = m_DrawPass->m_DrawRequests.First()->BaseBillboardingRequest().m_SortByCameraDistance;

	// GPU particles are rendered using DrawIndexedInstancedIndirect
	// For ribbon, this must be a raw indirect buffer to allow compute write (count is set from compute ribbon sort key pass instead of a copy command)
	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Indirect Draw Args Buffer"), true, m_ApiManager, m_IndirectDraw, RHI::RawIndirectDrawBuffer, drCountAligned *  sizeof(RHI::SDrawIndexedIndirectArgs), drCount * sizeof(RHI::SDrawIndexedIndirectArgs)))
		return false;

	const u32	viewIndependentInputs = m_DrawPass->m_ToGenerate.m_GeneratedInputs;

	// Camera sort
	if (m_NeedGPUSort)
	{
		// For each draw request, we have indirection and sort key buffers
		// and a GPU sorter object, handling intermediates work buffers
		if (!PK_VERIFY(m_CameraSortIndirection.Resize(drCount)) ||
			!PK_VERIFY(m_CameraSortKeys.Resize(drCount)) ||
			!PK_VERIFY(m_CameraGPUSorters.Resize(drCount)))
			return false;
		for (u32 i = 0; i < drCount; i++)
		{
			if (!PK_VERIFY(m_DrawPass->m_DrawRequests[i]->RenderedParticleCount() > 0))
				continue;
			const u32	alignedParticleCount = Mem::Align<PK_GPU_SORT_NUM_KEY_PER_THREAD * PK_RH_GPU_THREADGROUP_SIZE>(m_DrawPass->m_DrawRequests[i]->RenderedParticleCount());
			if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Camera Sort Indirection Buffer"), true, m_ApiManager, m_CameraSortIndirection[i], RHI::RawBuffer, alignedParticleCount * sizeof(u32), alignedParticleCount * sizeof(u32)) ||
				!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Camera Sort Keys Buffer"), true, m_ApiManager, m_CameraSortKeys[i], RHI::RawBuffer, alignedParticleCount * sizeof(u32), alignedParticleCount * sizeof(u32)))
				return false;
			// 16 bits key
			const u32	sortKeySizeInBits = 16;
			// Init sorter if needed and allocate buffers
			if (!PK_VERIFY(m_CameraGPUSorters[i].Init(sortKeySizeInBits, m_ApiManager)) ||
				!PK_VERIFY(m_CameraGPUSorters[i].AllocateBuffers(m_DrawPass->m_DrawRequests[i]->RenderedParticleCount(), m_ApiManager)))
				return false;
		}
		// Create compute sort key constant set
		if (!PK_VERIFY(m_ComputeCameraSortKeysConstantSets.Reserve(drCount)))
			return false;
		RHI::SConstantSetLayout	layout;
		PKSample::CreateComputeSortKeysConstantSetLayout(layout, m_SortByCameraDistance, true);
		while (m_ComputeCameraSortKeysConstantSets.Count() < drCount)
		{
			RHI::PConstantSet	cs = m_ApiManager->CreateConstantSet(RHI::SRHIResourceInfos("Camera Sort Keys Constant Set"), layout);
			m_ComputeCameraSortKeysConstantSets.PushBackUnsafe(cs);
		}
	}

	// Ribbon sort
	// For each draw request, we have indirection and sort key buffers
	// and a GPU sorter object, handling intermediates work buffers
	if (!PK_VERIFY(m_RibbonSortIndirection.Resize(drCount)) ||
		!PK_VERIFY(m_RibbonSortKeys.Resize(drCount)) ||
		!PK_VERIFY(m_RibbonGPUSorters.Resize(drCount)))
		return false;
	for (u32 i = 0; i < drCount; i++)
	{
		if (!PK_VERIFY(m_DrawPass->m_DrawRequests[i]->RenderedParticleCount() > 0))
			continue;
		const u32	alignedParticleCount = Mem::Align<PK_GPU_SORT_NUM_KEY_PER_THREAD * PK_RH_GPU_THREADGROUP_SIZE>(m_DrawPass->m_DrawRequests[i]->RenderedParticleCount());
		// Like for CPU ribbon, we have a 44 bits sort key from the self ID (26 bits) and the parentID (18 bits)
		const u32	sortKeySizeInBits = 44;
		const u32	sortKeyStrideInBytes = 2 * sizeof(u32);
		if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Ribbon Sort Indirection Buffer"), true, m_ApiManager, m_RibbonSortIndirection[i], RHI::RawBuffer, alignedParticleCount * sizeof(u32), alignedParticleCount * sizeof(u32)) ||
			!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Ribbon Sort Keys Buffer"), true, m_ApiManager, m_RibbonSortKeys[i], RHI::RawBuffer, alignedParticleCount * sortKeyStrideInBytes, alignedParticleCount * sortKeyStrideInBytes))
			return false;
		// Init sorter if needed and allocate buffers
		if (!PK_VERIFY(m_RibbonGPUSorters[i].Init(sortKeySizeInBits, m_ApiManager)) ||
			!PK_VERIFY(m_RibbonGPUSorters[i].AllocateBuffers(m_DrawPass->m_DrawRequests[i]->RenderedParticleCount(), m_ApiManager)))
			return false;
	}
	// Create compute sort key constant set
	if (!PK_VERIFY(m_ComputeRibbonSortKeysConstantSets.Reserve(drCount)))
		return false;
	RHI::SConstantSetLayout	layout;
	PKSample::CreateComputeRibbonSortKeysConstantSetLayout(layout);
	while (m_ComputeRibbonSortKeysConstantSets.Count() < drCount)
	{
		RHI::PConstantSet	cs = m_ApiManager->CreateConstantSet(RHI::SRHIResourceInfos("Ribbon Sort Keys Constant Set"), layout);
		m_ComputeRibbonSortKeysConstantSets.PushBackUnsafe(cs);
	}

	// Allocate once, max number of draw requests, indexed by DC from push constant
	const u32	offsetsSizeInBytes = kMaxDrawRequestCount * sizeof(u32); // u32 offsets

	// Particle positions stream
	PK_ASSERT((viewIndependentInputs & Drawers::GenInput_ParticlePosition) != 0);
	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Offsets_Enableds Buffer"), true, m_ApiManager, m_SimStreamOffsets_Enableds, RHI::RawBuffer, offsetsSizeInBytes, offsetsSizeInBytes) ||
		!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Offsets_Positions Buffer"), true, m_ApiManager, m_SimStreamOffsets_Positions, RHI::RawBuffer, offsetsSizeInBytes, offsetsSizeInBytes))
		return false;

	// Particle parent IDs and self IDs for ribbon sort.
	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Offsets_ParentIDs Buffer"), true, m_ApiManager, m_SimStreamOffsets_ParentIDs, RHI::RawBuffer, offsetsSizeInBytes, offsetsSizeInBytes) ||
		!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Offsets_SelfIDs Buffer"), true, m_ApiManager, m_SimStreamOffsets_SelfIDs, RHI::RawBuffer, offsetsSizeInBytes, offsetsSizeInBytes))
		return false;

	// GPU sort related offset buffers
	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Offsets_CustomSortKeys Buffer"), m_NeedGPUSort && !m_SortByCameraDistance, m_ApiManager, m_CustomSortKeysOffsets, RHI::RawBuffer, offsetsSizeInBytes, offsetsSizeInBytes))
		return false;

	// Size
	PK_ASSERT((viewIndependentInputs & Drawers::GenInput_ParticleSize) != 0);
	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Offsets_Sizes Buffer"), true, m_ApiManager, m_SimStreamOffsets_Sizes, RHI::RawBuffer, offsetsSizeInBytes, offsetsSizeInBytes))
		return false;
	// Billboarding axis
	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Offsets_Axis0s Buffer"), (viewIndependentInputs & Drawers::GenInput_ParticleAxis0) != 0, m_ApiManager, m_SimStreamOffsets_Axis0s, RHI::RawBuffer, offsetsSizeInBytes, offsetsSizeInBytes))
		return false;

	// Additional fields simStreamOffsets:
	if (!m_AdditionalFieldsSimStreamOffsets.AllocBuffers(kMaxDrawRequestCount, m_ApiManager))
		return false;

	if (!m_Initialized)
		m_Initialized = _InitStaticBuffers(); // Important: after alloctation of the Offsets_XXX buffers
	if (!m_Initialized)
		return false;

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Ribbon_GPU::MapBuffers(SRenderContext &ctx)
{
	(void)ctx;
	const u32	drCount = m_DrawPass->m_DrawRequests.Count();

	m_MappedIndirectBuffer = static_cast<RHI::SDrawIndexedIndirectArgs*>(m_ApiManager->MapCpuView(m_IndirectDraw.m_Buffer));

	m_MappedSimStreamOffsets[0] = static_cast<u32*>(m_ApiManager->MapCpuView(m_SimStreamOffsets_Enableds.m_Buffer));
	m_MappedSimStreamOffsets[1] = static_cast<u32*>(m_ApiManager->MapCpuView(m_SimStreamOffsets_Positions.m_Buffer));
	m_MappedSimStreamOffsets[2] = static_cast<u32*>(m_ApiManager->MapCpuView(m_SimStreamOffsets_Sizes.m_Buffer));
	m_MappedSimStreamOffsets[3] = static_cast<u32*>(m_ApiManager->MapCpuView(m_SimStreamOffsets_ParentIDs.m_Buffer));
	m_MappedSimStreamOffsets[4] = static_cast<u32*>(m_ApiManager->MapCpuView(m_SimStreamOffsets_SelfIDs.m_Buffer));
	m_MappedSimStreamOffsets[5] = m_SimStreamOffsets_Axis0s.Used() ? static_cast<u32*>(m_ApiManager->MapCpuView(m_SimStreamOffsets_Axis0s.m_Buffer)) : null;

	// Additional streams
	if (!m_AdditionalFieldsSimStreamOffsets.MapBuffers(drCount, m_ApiManager))
		return false;

	// Camera sort optional stream offsets
	if (m_NeedGPUSort && !m_SortByCameraDistance)
	{
		m_MappedCustomSortKeysOffsets = static_cast<u32*>(m_ApiManager->MapCpuView(m_CustomSortKeysOffsets.m_Buffer));
		if (!PK_VERIFY(m_MappedCustomSortKeysOffsets != null))
			return false;
	}

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Ribbon_GPU::LaunchCustomTasks(SRenderContext &ctx)
{
	(void)ctx;
	const u32	drCount = m_DrawPass->m_DrawRequests.Count();

	PK_ASSERT(m_MappedIndirectBuffer != null);
	for (u32 dri = 0; dri < drCount; ++dri)
	{
		// Indirect buffer
		m_MappedIndirectBuffer[dri].m_InstanceCount = m_DrawPass->m_DrawRequests[dri]->RenderedParticleCount(); // will be overwritten by the GPU command.
		m_MappedIndirectBuffer[dri].m_IndexOffset = 0;
		m_MappedIndirectBuffer[dri].m_VertexOffset = 0;
		m_MappedIndirectBuffer[dri].m_InstanceOffset = 0;
		m_MappedIndirectBuffer[dri].m_IndexCount = m_ParticleQuadCount * 6;
	}

#if (PK_PARTICLES_UPDATER_USE_GPU != 0)
	const u32	streamMaxCount = 6 + (m_NeedGPUSort && !m_SortByCameraDistance ? 1 : 0) + m_AdditionalFieldsSimStreamOffsets.m_MappedFields.Count();

	// Store offsets in a stack view ([PosOffsetDr0][PosOffsetDr1][SizeOffsetDr0][SizeOffsetDr1]..)
	PK_STACKALIGNEDMEMORYVIEW(u32, streamsOffsets, drCount * streamMaxCount, 0x10);
	
	for (u32 iDr = 0; iDr < drCount; ++iDr)
	{
		// Fill in stream offsets for each draw request
		const CParticleStreamToRender_GPU				*streamToRender = m_DrawPass->m_DrawRequests[iDr]->StreamToRender_GPU();
		if (!PK_VERIFY(streamToRender != null))
			continue;
		const Drawers::SBase_DrawRequest			*dr = m_DrawPass->m_DrawRequests[iDr];
		const Drawers::SBase_BillboardingRequest	baseBr = dr->BaseBillboardingRequest();

		u32		offset = 0;

		// Mandatory streams
		streamsOffsets[offset++ * drCount + iDr] = streamToRender->StreamOffset(baseBr.m_EnabledStreamId);
		streamsOffsets[offset++ * drCount + iDr] = streamToRender->StreamOffset(baseBr.m_PositionStreamId);

		// Billboard streams
		{
			const Drawers::SRibbon_DrawRequest	*ribbonDr = static_cast<const Drawers::SRibbon_DrawRequest *>(dr);

			// Mandatory
			streamsOffsets[offset++ * drCount + iDr] = streamToRender->StreamOffset(ribbonDr->m_BB.m_WidthStreamId); // Size
			streamsOffsets[offset++ * drCount + iDr] = streamToRender->StreamOffset(ribbonDr->m_BB.m_ParentIDStreamId);
			streamsOffsets[offset++ * drCount + iDr] = streamToRender->StreamOffset(ribbonDr->m_BB.m_SelfIDStreamId);

			// Optional
			if (m_SimStreamOffsets_Axis0s.Used())
				streamsOffsets[offset++ * drCount + iDr] = streamToRender->StreamOffset(ribbonDr->m_BB.m_AxisStreamId);
		}

		// Add all non-virtual stream additional inputs
		for (auto &addField : m_AdditionalFieldsSimStreamOffsets.m_Fields)
			streamsOffsets[offset++ * drCount + iDr] = streamToRender->StreamOffset(baseBr.m_AdditionalInputs[addField.m_AdditionalInputIndex].m_StreamId);

		if (m_NeedGPUSort && !m_SortByCameraDistance)
			streamsOffsets[offset++ * drCount + iDr] = streamToRender->StreamOffset(baseBr.m_SortKeyStreamId);

		PK_ASSERT(offset <= streamMaxCount);
	}

	// Non temporal writes to gpu mem, aligned and contiguous
	u32	streamOffset = 0;
	Mem::Copy_Uncached(m_MappedSimStreamOffsets[0], &streamsOffsets[streamOffset++ * drCount], sizeof(u32) * drCount);
	Mem::Copy_Uncached(m_MappedSimStreamOffsets[1], &streamsOffsets[streamOffset++ * drCount], sizeof(u32) * drCount);
	Mem::Copy_Uncached(m_MappedSimStreamOffsets[2], &streamsOffsets[streamOffset++ * drCount], sizeof(u32) * drCount);
	Mem::Copy_Uncached(m_MappedSimStreamOffsets[3], &streamsOffsets[streamOffset++ * drCount], sizeof(u32) * drCount);
	Mem::Copy_Uncached(m_MappedSimStreamOffsets[4], &streamsOffsets[streamOffset++ * drCount], sizeof(u32) * drCount);
	if (m_MappedSimStreamOffsets[5] != null)
		Mem::Copy_Uncached(m_MappedSimStreamOffsets[5], &streamsOffsets[streamOffset++ * drCount], sizeof(u32) * drCount);

	for (u32 iInput = 0; iInput < m_AdditionalFieldsSimStreamOffsets.m_MappedFields.Count(); ++iInput)
		Mem::Copy_Uncached(m_AdditionalFieldsSimStreamOffsets.m_MappedFields[iInput].m_Storage.m_RawDataPtr, &streamsOffsets[streamOffset++ * drCount], sizeof(u32) * drCount);

	if (m_MappedCustomSortKeysOffsets != null && !m_SortByCameraDistance)
		Mem::Copy_Uncached(m_MappedCustomSortKeysOffsets, &streamsOffsets[streamOffset++ * drCount], sizeof(u32) * drCount);

#endif // (PK_PARTICLES_UPDATER_USE_GPU != 0)

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Ribbon_GPU::UnmapBuffers(SRenderContext &ctx)
{
	(void)ctx;

	m_IndirectDraw.UnmapIFN(m_ApiManager);
	m_MappedIndirectBuffer = null;

	m_CustomSortKeysOffsets.UnmapIFN(m_ApiManager);
	m_MappedCustomSortKeysOffsets = null;

	// GPU stream offsets
	m_SimStreamOffsets_Enableds.UnmapIFN(m_ApiManager);
	m_SimStreamOffsets_Positions.UnmapIFN(m_ApiManager);
	m_SimStreamOffsets_Sizes.UnmapIFN(m_ApiManager);
	m_SimStreamOffsets_Axis0s.UnmapIFN(m_ApiManager);
	m_SimStreamOffsets_ParentIDs.UnmapIFN(m_ApiManager);
	m_SimStreamOffsets_SelfIDs.UnmapIFN(m_ApiManager);
	Mem::Clear(m_MappedSimStreamOffsets);

	m_AdditionalFieldsSimStreamOffsets.UnmapBuffers(m_ApiManager);

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Ribbon_GPU::EmitDrawCall(SRenderContext &ctx, const SDrawCallDesc &toEmit)
{
	SRHIRenderContext	&renderContext = static_cast<SRHIRenderContext&>(ctx);

	// !Currently, there is no batching for gpu particles!
	// So we'll emit one draw call per draw request
	// Some data are indexed by the draw-requests. So "EmitDrawCall" shoudl be called once, with all draw-requests.
	PK_ASSERT(toEmit.m_DrawRequests.Count() == m_DrawPass->m_DrawRequests.Count());
	const u32	drCount = m_DrawPass->m_DrawRequests.Count();

	// No need to iterate on all draw requests, just take the first as reference as they wouldn't have been batched if not compatible
	CRendererCacheInstance_UpdateThread		*renderCacheInstance = static_cast<CRendererCacheInstance_UpdateThread*>(toEmit.m_RendererCaches.First().Get());
	if (!PK_VERIFY(renderCacheInstance != null))
	{
		CLog::Log(PK_ERROR, "Invalid renderer cache instance");
		return false;
	}
	PKSample::PCRendererCacheInstance	rCacheInstance = renderCacheInstance->RenderThread_GetCacheInstance();
	if (!PK_VERIFY(rCacheInstance != null))
		return false;

	for (u32 dri = 0; dri < drCount; ++dri)
	{
		PK_ASSERT(toEmit.m_DrawRequests[dri] != null);
		const Drawers::SRibbon_DrawRequest			*dr = static_cast<const Drawers::SRibbon_DrawRequest*>(toEmit.m_DrawRequests[dri]);
		const Drawers::SRibbon_BillboardingRequest	&bbRequest = dr->m_BB;
		const CParticleStreamToRender				*streamToRender = &dr->StreamToRender();
		if (!PK_VERIFY(streamToRender != null))
			continue;

		PK_ASSERT(bbRequest.m_ParticleQuadCount == m_ParticleQuadCount);

		u32		dummyOffset = 0;
		RHI::PGpuBuffer		drStreamBuffer = _RetrieveStorageBuffer(m_ApiManager, streamToRender, bbRequest.m_PositionStreamId, dummyOffset);
		RHI::PGpuBuffer		drStreamSizeBuffer = _RetrieveParticleInfoBuffer(m_ApiManager, streamToRender);

		if (!PK_VERIFY(drStreamBuffer != null) ||
			!PK_VERIFY(drStreamSizeBuffer != null))
			return false;

		// Ribbon GPU sort
		{
			// Compute: Compute sort keys and init indirection buffer
			{
				// For radix sort, a thread computes 2 keys (see shader definition and RHIGPUSorter.cpp),
				// so a group computes (PK_GPU_SORT_NUM_KEY_PER_THREAD * PK_RH_GPU_THREADGROUP_SIZE) keys
				// It also counts enabled particles and writes the indirectDraw.

				const u32	sortGroupCount = (dr->RenderedParticleCount() + PK_GPU_SORT_NUM_KEY_PER_THREAD * PK_RH_GPU_THREADGROUP_SIZE - 1) / (PK_GPU_SORT_NUM_KEY_PER_THREAD * PK_RH_GPU_THREADGROUP_SIZE);
				PKSample::SRHIComputeDispatchs	computeDispatch = PKSample::SRHIComputeDispatchs();

				// Constant set
				RHI::PConstantSet	computeConstantSet = m_ComputeRibbonSortKeysConstantSets[dri];
				computeConstantSet->SetConstants(drStreamSizeBuffer, 0);
				computeConstantSet->SetConstants(drStreamBuffer, 1);
				computeConstantSet->SetConstants(m_SimStreamOffsets_SelfIDs.m_Buffer, 2);
				computeConstantSet->SetConstants(m_SimStreamOffsets_ParentIDs.m_Buffer, 3);
				computeConstantSet->SetConstants(m_SimStreamOffsets_Enableds.m_Buffer, 4);
				computeConstantSet->SetConstants(m_RibbonSortKeys[dri].m_Buffer, 5);
				computeConstantSet->SetConstants(m_RibbonSortIndirection[dri].m_Buffer, 6);
				computeConstantSet->SetConstants(m_IndirectDraw.m_Buffer, 7);

				computeConstantSet->UpdateConstantValues();
				computeDispatch.m_ConstantSet = computeConstantSet;

				// Push constant
				if (!PK_VERIFY(computeDispatch.m_PushConstants.PushBack().Valid()))
					return false;
				u32	*drPushConstant = reinterpret_cast<u32*>(&computeDispatch.m_PushConstants.Last());
				drPushConstant[0] = dri;

				// State
				const PKSample::EComputeShaderType	type = ComputeType_ComputeRibbonSortKeys;
				computeDispatch.m_State = rCacheInstance->m_Cache->GetComputeState(type);
				if (!PK_VERIFY(computeDispatch.m_State != null))
					return false;

				// Dispatch args
				// For the sort key computation dispatch, we need exactly PK_GPU_SORT_NUM_KEY_PER_THREAD the sort group count
				// so that every thread associated key is initialized. Overflowing keys are initialized
				// to 0xFFFF, sorted last (and ultimately not rendered).
				computeDispatch.m_ThreadGroups = CInt3(sortGroupCount * PK_GPU_SORT_NUM_KEY_PER_THREAD, 1, 1);

				if (!PK_VERIFY(renderContext.m_DrawOutputs.m_ComputeDispatchs.PushBack(computeDispatch).Valid()))
					return false;
			}

			// Compute: Sort computes
			{
				m_RibbonGPUSorters[dri].SetInOutBuffers(m_RibbonSortKeys[dri].m_Buffer, m_RibbonSortIndirection[dri].m_Buffer);
				m_RibbonGPUSorters[dri].AppendDispatchs(rCacheInstance, renderContext.m_DrawOutputs.m_ComputeDispatchs);
			}
		}

		// Camera GPU sort (must be done after the ribbon GPU sort)
		if (m_NeedGPUSort)
		{
			// Compute: Compute sort keys and init indirection buffer
			{
				// For radix sort, a thread computes PK_GPU_SORT_NUM_KEY_PER_THREAD keys (see shader definition and RHIGPUSorter.cpp),
				// so a group computes (PK_GPU_SORT_NUM_KEY_PER_THREAD * PK_RH_GPU_THREADGROUP_SIZE) keys
				const u32	sortGroupCount = (dr->RenderedParticleCount() + PK_GPU_SORT_NUM_KEY_PER_THREAD * PK_RH_GPU_THREADGROUP_SIZE - 1) / (PK_GPU_SORT_NUM_KEY_PER_THREAD * PK_RH_GPU_THREADGROUP_SIZE);
				PKSample::SRHIComputeDispatchs	computeDispatch = PKSample::SRHIComputeDispatchs();

				// Constant set
				computeDispatch.m_NeedSceneInfoConstantSet = m_SortByCameraDistance;
				RHI::PConstantSet	computeConstantSet = m_ComputeCameraSortKeysConstantSets[dri];
				computeConstantSet->SetConstants(drStreamSizeBuffer, 0);
				computeConstantSet->SetConstants(drStreamBuffer, 1);
				computeConstantSet->SetConstants(m_SortByCameraDistance ? m_SimStreamOffsets_Positions.m_Buffer : m_CustomSortKeysOffsets.m_Buffer, 2);
				computeConstantSet->SetConstants(m_RibbonSortIndirection[dri].m_Buffer, 3);
				computeConstantSet->SetConstants(m_IndirectDraw.m_Buffer, 4);
				computeConstantSet->SetConstants(m_CameraSortKeys[dri].m_Buffer, 5);
				computeConstantSet->SetConstants(m_CameraSortIndirection[dri].m_Buffer, 6);

				computeConstantSet->UpdateConstantValues();
				computeDispatch.m_ConstantSet = computeConstantSet;

				// Push constant
				if (!PK_VERIFY(computeDispatch.m_PushConstants.PushBack().Valid()))
					return false;
				u32	*drPushConstant = reinterpret_cast<u32*>(&computeDispatch.m_PushConstants.Last());
				drPushConstant[0] = dri;

				// State
				const PKSample::EComputeShaderType	type = m_SortByCameraDistance ? ComputeType_ComputeSortKeys_CameraDistance_RibbonIndirection : ComputeType_ComputeSortKeys_RibbonIndirection;
				computeDispatch.m_State = rCacheInstance->m_Cache->GetComputeState(type);
				if (!PK_VERIFY(computeDispatch.m_State != null))
					return false;

				// Dispatch args
				// For the sort key computation dispatch, we need exactly PK_GPU_SORT_NUM_KEY_PER_THREAD the sort group count
				// so that every thread associated key is initialized. Overflowing keys are initialized
				// to 0xFFFF, sorted last (and ultimately not rendered).
				computeDispatch.m_ThreadGroups = CInt3(sortGroupCount * PK_GPU_SORT_NUM_KEY_PER_THREAD, 1, 1);

				renderContext.m_DrawOutputs.m_ComputeDispatchs.PushBack(computeDispatch);
			}

			// Compute: Sort computes
			{
				m_CameraGPUSorters[dri].SetInOutBuffers(m_CameraSortKeys[dri].m_Buffer, m_CameraSortIndirection[dri].m_Buffer);
				m_CameraGPUSorters[dri].AppendDispatchs(rCacheInstance, renderContext.m_DrawOutputs.m_ComputeDispatchs);
			}
		}

		// Emit draw call
		{
			const u32				shaderOptions = Option_GPUStorage | _GetVertexRibbonShaderOptions(bbRequest, m_NeedGPUSort);
			const RHI::PGpuBuffer	ribbonSortIndirection = m_RibbonSortIndirection[dri].m_Buffer;
			const RHI::PGpuBuffer	cameraSortIndirection = m_NeedGPUSort ? m_CameraSortIndirection[dri].m_Buffer : null;
			RHI::PConstantSet		vertexBBSimDataConstantSet;
			// create the local constant-set
			{
				const RHI::SConstantSetLayout	*simDataConstantSetLayout = null;
				const RHI::SConstantSetLayout	*offsetsConstantSetLayout = null;
				if (!PK_VERIFY(rCacheInstance->m_Cache->GetGPUStorageConstantSets(static_cast<PKSample::EShaderOptions>(shaderOptions), simDataConstantSetLayout, offsetsConstantSetLayout)) ||
					!PK_VERIFY(simDataConstantSetLayout != null) ||
					!PK_VERIFY(offsetsConstantSetLayout != null) ||
					simDataConstantSetLayout->m_Constants.Empty() ||
					offsetsConstantSetLayout->m_Constants.Empty())
					return false;

				vertexBBSimDataConstantSet = m_ApiManager->CreateConstantSet(RHI::SRHIResourceInfos("Sim Data Constant Set"), *simDataConstantSetLayout);
				if (!PK_VERIFY(vertexBBSimDataConstantSet != null))
					return false;

				PK_ASSERT(drStreamBuffer != null);
				// Sim data constant set are:
				// raw stream + camera sort indirection buffer if needed + ribbon sort indirection and indirect draw buffers if needed
				PK_ASSERT(vertexBBSimDataConstantSet->GetConstantValues().Count() == (1u + (m_NeedGPUSort ? 1u : 0u) + 2u));
				u32	constantSetLocation = 0;
				if (!PK_VERIFY(vertexBBSimDataConstantSet->SetConstants(drStreamBuffer, constantSetLocation++)))
					return false;
				if (m_NeedGPUSort)
				{
					if (!PK_VERIFY(cameraSortIndirection != null) ||
						!PK_VERIFY(vertexBBSimDataConstantSet->SetConstants(cameraSortIndirection, constantSetLocation++)))
						return false;
				}
				if (!PK_VERIFY(vertexBBSimDataConstantSet->SetConstants(ribbonSortIndirection, constantSetLocation++)) ||
					!PK_VERIFY(vertexBBSimDataConstantSet->SetConstants(m_IndirectDraw.m_Buffer, constantSetLocation++)))
					return false;
				vertexBBSimDataConstantSet->UpdateConstantValues();
			}

			if (!PK_VERIFY(renderContext.m_DrawOutputs.m_DrawCalls.PushBack().Valid()))
			{
				CLog::Log(PK_ERROR, "Failed to create a draw-call");
				return false;
			}
			SRHIDrawCall		&outDrawCall = renderContext.m_DrawOutputs.m_DrawCalls.Last();

			outDrawCall.m_Batch = this;
			outDrawCall.m_RendererCacheInstance = renderCacheInstance;
			outDrawCall.m_Type = SRHIDrawCall::DrawCall_IndexedInstancedIndirect;
			outDrawCall.m_ShaderOptions = shaderOptions;
			outDrawCall.m_RendererType = Renderer_Ribbon;
			outDrawCall.m_GPUStorageSimDataConstantSet = vertexBBSimDataConstantSet;

			// Some meta-data (the Editor uses them)
			{
				outDrawCall.m_BBox = toEmit.m_BBox;
				outDrawCall.m_TotalBBox = m_DrawPass->m_TotalBBox;
				outDrawCall.m_SlicedDC = toEmit.m_TotalParticleCount != m_DrawPass->m_TotalParticleCount;

				outDrawCall.m_Valid =	rCacheInstance->m_Cache != null &&
										rCacheInstance->m_Cache->GetRenderState(static_cast<PKSample::EShaderOptions>(outDrawCall.m_ShaderOptions)) != null;
			}

			outDrawCall.m_GPUStorageOffsetsConstantSet = m_VertexBBOffsetsConstantSet;

#if	(PK_HAS_PARTICLES_SELECTION != 0)
			{
				PKSample::SRHIRenderContext	&ctxEditor = *static_cast<PKSample::SRHIRenderContext*>(&ctx);
				RHI::PGpuBuffer		bufferIsSelected = ctxEditor.Selection().HasGPUParticlesSelected() ? GetIsSelectedBuffer(ctxEditor.Selection(), *dr) : null;
				if (bufferIsSelected != null)
				{
					RHI::SConstantSetLayout	selectionSetLayout(RHI::VertexShaderMask);
					selectionSetLayout.AddConstantsLayout(RHI::SRawBufferDesc("Selections"));
					RHI::PConstantSet	selectionConstantSet = m_ApiManager->CreateConstantSet(RHI::SRHIResourceInfos("Selection Constant Set"), selectionSetLayout);
					if (PK_VERIFY(selectionConstantSet != null) && PK_VERIFY(selectionConstantSet->SetConstants(bufferIsSelected, 0)))
					{
						selectionConstantSet->UpdateConstantValues();
						outDrawCall.m_SelectionConstantSet = selectionConstantSet;
					}
				}
			}
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)

			// A single vertex buffer is used for the instanced draw: the texcoords buffer, contains the direction in which vertices should be expanded
			PK_ASSERT(m_TexCoords.Used());
			if (!PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(m_TexCoords.m_Buffer).Valid()))
				return false;

			// Fill the semantics for the debug draws:
			outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Enabled] = m_SimStreamOffsets_Enableds.m_Buffer;
			outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Position] = m_SimStreamOffsets_Positions.m_Buffer;
			outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Size] = m_SimStreamOffsets_Sizes.m_Buffer;
			outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Axis0] = m_SimStreamOffsets_Axis0s.m_Buffer;
			outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Texcoords] = m_TexCoords.m_Buffer;
			if (m_ColorStreamIdx.Valid())
				outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Color] = m_AdditionalFieldsSimStreamOffsets.m_Fields[m_ColorStreamIdx].m_Buffer.m_Buffer;

			outDrawCall.m_IndexOffset = 0;
			outDrawCall.m_IndexSize = RHI::IndexBuffer16Bit;
			outDrawCall.m_IndexBuffer = m_DrawIndices.m_Buffer;

			outDrawCall.m_IndirectBuffer = m_IndirectDraw.m_Buffer;
			outDrawCall.m_IndirectBufferOffset = dri * sizeof(RHI::SDrawIndexedIndirectArgs);
			outDrawCall.m_EstimatedParticleCount = dr->RenderedParticleCount();

			if (!PK_VERIFY(renderContext.m_DrawOutputs.m_CopyCommands.PushBack().Valid()))
				return false;

			SRHICopyCommand		&copyCommand = renderContext.m_DrawOutputs.m_CopyCommands.Last();

			// We retrieve the particles info buffer:
			copyCommand.m_SrcBuffer = drStreamSizeBuffer;
			copyCommand.m_SrcOffset = 0;
			copyCommand.m_DstBuffer = m_IndirectDraw.m_Buffer;
			copyCommand.m_DstOffset = dri * sizeof(RHI::SDrawIndexedIndirectArgs) + PK_MEMBER_OFFSET(RHI::SDrawIndexedIndirectArgs, m_InstanceCount);
			copyCommand.m_SizeToCopy = sizeof(u32);

			// Ribbon GPU billboarding info: constains billboarding mode and normal bending factor.
			// No batching with GPU storage, so here this is constant within a drawcall (uses push constant).
			if (!PK_VERIFY(outDrawCall.m_PushConstants.PushBack().Valid()))
				return false;
			PK_STATIC_ASSERT(sizeof(Drawers::SRibbonDrawRequest) == sizeof(CFloat4));
			Drawers::SRibbonDrawRequest	&desc = *reinterpret_cast<Drawers::SRibbonDrawRequest*>(&outDrawCall.m_PushConstants.Last());
			desc.Setup(bbRequest);

			// GPUBillboardPushConstants
			if (!PK_VERIFY(outDrawCall.m_PushConstants.PushBack().Valid()))
				return false;
			SVertexBillboardingConstants	&indices = *reinterpret_cast<SVertexBillboardingConstants*>(&outDrawCall.m_PushConstants.Last());
			indices.m_IndicesOffset = toEmit.m_IndexOffset;
			indices.m_StreamOffsetsIndex = dri;
		}
	}

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Ribbon_GPU::_InitStaticBuffers()
{
	// Index buffer
	// RHI currently doesn't support DrawInstanced (without indices)
	{
		const u16	indexCount = m_ParticleQuadCount * 6;
		if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("VertexBB Draw Indices Buffer"), true, m_ApiManager, m_DrawIndices, RHI::IndexBuffer, indexCount * sizeof(u16), indexCount * sizeof(u16)))
			return false;
		volatile u16	*indices = static_cast<u16*>(m_ApiManager->MapCpuView(m_DrawIndices.m_Buffer, 0, indexCount * sizeof(u16)));
		if (!PK_VERIFY(indices != null))
			return false;
		if (m_TubesDC)
		{
			// Filling tube indices.
			const u16	segmentCount = m_ParticleQuadCount;
			// This is the index pattern of a tube single quad.
			const u16	pattern[6] = { 0, 1, (u16)(segmentCount+1), (u16)(segmentCount+1), 1, (u16)(segmentCount+2) };

			for (u16 i = 0; i < segmentCount; ++i)
			{
				indices[0] = pattern[0] + i;
				indices[1] = pattern[1] + i;
				indices[2] = pattern[2] + i;
				indices[3] = pattern[3] + i;
				indices[4] = pattern[4] + i;
				indices[5] = pattern[5] + i;
				indices += 6;
			}
		}
		else if (m_MultiPlanesDC)
		{
			// Filling multi-plane indices.
			const u16	planeCount = m_ParticleQuadCount;
			// This is the index pattern of a plane quad.
			const u16	pattern[6] = { 1 , (u16)(planeCount*2+1), 0, (u16)(planeCount*2), (u16)(planeCount*2+1), 1 };

			for (u16 i = 0; i < planeCount * 2; i += 2)
			{
				indices[0] = pattern[0] + i;
				indices[1] = pattern[1] + i;
				indices[2] = pattern[2] + i;
				indices[3] = pattern[3] + i;
				indices[4] = pattern[4] + i;
				indices[5] = pattern[5] + i;
				indices += 6;
			}
		}
		else
		{
			indices[0] = 1;
			indices[1] = 3;
			indices[2] = 0;
			indices[3] = 2;
			indices[4] = 3;
			indices[5] = 1;
		}
		m_DrawIndices.Unmap(m_ApiManager);
	}

	// TexCoords buffer
	{
		u32	uvCount = 4;
		if (m_TubesDC) // Tube vertex count
			uvCount = (m_ParticleQuadCount + 1) * 2;
		else if (m_MultiPlanesDC) // Multi-Plane vertex count
			uvCount = m_ParticleQuadCount * 4;
		if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("VertexBB Texcoords Buffer"), true, m_ApiManager, m_TexCoords, RHI::VertexBuffer, uvCount * sizeof(CFloat2), uvCount * sizeof(CFloat2)))
			return false;
		volatile float	*texCoords = static_cast<float*>(m_ApiManager->MapCpuView(m_TexCoords.m_Buffer, 0, uvCount * sizeof(CFloat2)));
		if (!PK_VERIFY(texCoords != null))
			return false;
		if (m_MultiPlanesDC)
		{
			for (u32 planeId = 1; planeId <= m_ParticleQuadCount; ++planeId)
			{
				texCoords[0] = -1.0f * planeId;	// Lower left corner
				texCoords[1] = -1.0f;
				texCoords[2] = -1.0f * planeId;	// Lower right corner
				texCoords[3] = 1.0f;
				texCoords += 4;
			}

			for (u32 planeId = 1; planeId <= m_ParticleQuadCount; ++planeId)
			{
				texCoords[0] = 1.0f * planeId;	// Upper right corner
				texCoords[1] = 1.0f;
				texCoords[2] = 1.0f * planeId;	// Upper left corner
				texCoords[3] = -1.0f;
				texCoords += 4;
			}
		}
		if (m_TubesDC)
		{
			float		v = -1.0f;
			const float	vStep = 2.0f / m_ParticleQuadCount;
			for (u32 segmentId = 1; segmentId <= m_ParticleQuadCount + 1; ++segmentId)
			{
				texCoords[0] = -1.0f * segmentId;	// Lower corner
				texCoords[1] = v;
				texCoords += 2;
				v += vStep;
			}
			v = -1.0f;
			for (u32 segmentId = 1; segmentId <= m_ParticleQuadCount + 1; ++segmentId)
			{
				texCoords[0] = 1.0f * segmentId;	// Upper corner
				texCoords[1] = v;
				texCoords += 2;
				v += vStep;
			}
		}
		else
		{
			texCoords[0] = -1.0f; // Lower left corner
			texCoords[1] = -1.0f;
			texCoords[2] = -1.0f; // Upper left corner
			texCoords[3] = 1.0f;
			texCoords[4] = 1.0f; // Upper right corner
			texCoords[5] = 1.0f;
			texCoords[6] = 1.0f; // Lower right corner
			texCoords[7] = -1.0f;
		}
		m_TexCoords.Unmap(m_ApiManager);
	}

	// Constant-Set
	{
		CRendererCacheInstance_UpdateThread		*renderCacheInstance = static_cast<CRendererCacheInstance_UpdateThread*>(m_DrawPass->m_RendererCaches.First().Get());
		PK_ASSERT(renderCacheInstance != null);
		const PCRendererCacheInstance	cacheInstance = renderCacheInstance->RenderThread_GetCacheInstance();
		if (cacheInstance == null)
			return false;

		const Drawers::SRibbon_DrawRequest			*dr = static_cast<const Drawers::SRibbon_DrawRequest*>(m_DrawPass->m_DrawRequests.First());
		const Drawers::SRibbon_BillboardingRequest	&bbRequest = dr->m_BB;
		const u32				shaderOptions = Option_GPUStorage | _GetVertexRibbonShaderOptions(bbRequest, m_NeedGPUSort);

		const RHI::SConstantSetLayout	*simDataConstantSetLayout = null;
		const RHI::SConstantSetLayout	*offsetsConstantSetLayout = null;
		if (!PK_VERIFY(cacheInstance->m_Cache->GetGPUStorageConstantSets(static_cast<PKSample::EShaderOptions>(shaderOptions), simDataConstantSetLayout, offsetsConstantSetLayout)) ||
			!PK_VERIFY(offsetsConstantSetLayout != null) ||
			offsetsConstantSetLayout->m_Constants.Empty())
			return false;

		m_VertexBBOffsetsConstantSet = m_ApiManager->CreateConstantSet(RHI::SRHIResourceInfos("Offsets Constant Set"), *offsetsConstantSetLayout);
		if (!PK_VERIFY(m_VertexBBOffsetsConstantSet != null))
			return false;

		// Fill offsets constant sets.
		u32 i = 0;
		{
			if (!PK_VERIFY(m_VertexBBOffsetsConstantSet->SetConstants(m_SimStreamOffsets_Enableds.m_Buffer, i++)))
				return false;
			if (!PK_VERIFY(m_VertexBBOffsetsConstantSet->SetConstants(m_SimStreamOffsets_Positions.m_Buffer, i++)))
				return false;
			if (!PK_VERIFY(m_VertexBBOffsetsConstantSet->SetConstants(m_SimStreamOffsets_ParentIDs.m_Buffer, i++)))
				return false;
			if (!PK_VERIFY(m_VertexBBOffsetsConstantSet->SetConstants(m_SimStreamOffsets_Sizes.m_Buffer, i++)))
				return false;
			if (m_SimStreamOffsets_Axis0s.Used())
			{
				if (!PK_VERIFY(m_VertexBBOffsetsConstantSet->SetConstants(m_SimStreamOffsets_Axis0s.m_Buffer, i++)))
					return false;
			}
			for (auto &additionalSimStreamOffset : m_AdditionalFieldsSimStreamOffsets.m_Fields)
			{
				if (!PK_VERIFY(m_VertexBBOffsetsConstantSet->SetConstants(additionalSimStreamOffset.m_Buffer.m_Buffer, i++)))
					return false;
			}
		}
		PK_ASSERT(m_VertexBBOffsetsConstantSet->GetConstantValues().Count() == i);
		m_VertexBBOffsetsConstantSet->UpdateConstantValues();
	}

	return true;
}

//----------------------------------------------------------------------------
//
// CRHIRendererBatch_Triangle_GPU
//
//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Triangle_GPU::Setup(const CRendererDataBase *renderer, const CParticleRenderMedium *owner, const CFrameCollector *fc, const CStringId &storageClass)
{
	if (!CRendererBatchJobs_Triangle_GPUBB::Setup(renderer, owner, fc, storageClass))
			return false;

	// Setup additional fields:
	// The additional fields are supposed to be the same for all renderers in a batch.
	// If not, then you can recompute then on the "Bind()" method.

	const auto		&toGenerate = m_DrawPass->m_ToGenerate;
	const u32		additionalFieldsCount = toGenerate.m_AdditionalGeneratedInputs.Count();

	if (!PK_VERIFY(m_AdditionalFieldsSimStreamOffsets.m_Fields.Reserve(additionalFieldsCount)))
		return false;

	for (u32 i = 0; i < additionalFieldsCount; ++i)
	{
		// no ignored fields
		m_AdditionalFieldsSimStreamOffsets.m_Fields.PushBackUnsafe(SAdditionalInputs(sizeof(u32) /* it contains the sim buffer offsets */, i));
	}

	m_ColorStreamIdx = _GetDrawDebugColorIndex(m_AdditionalFieldsSimStreamOffsets, toGenerate);

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Triangle_GPU::AreRenderersCompatible(const CRendererDataBase *rendererA, const CRendererDataBase *rendererB) const
{
	return	CRendererBatchJobs_Triangle_GPUBB::AreRenderersCompatible(rendererA, rendererB) &&
			AreBillboardingBatchable(rendererA->m_RendererCache, rendererB->m_RendererCache);
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Triangle_GPU::AllocBuffers(SRenderContext &ctx)
{
	(void)ctx;

	const u32	drCount = m_DrawPass->m_DrawRequests.Count();
	const u32	drCountAligned = Mem::Align<0x10>(drCount);

	m_NeedGPUSort = m_DrawPass->m_DrawRequests.First()->BaseBillboardingRequest().m_Flags.m_NeedSort;
	m_SortByCameraDistance = m_DrawPass->m_DrawRequests.First()->BaseBillboardingRequest().m_SortByCameraDistance;

	// GPU particles are rendered using DrawIndexedInstancedIndirect
	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Indirect Draw Args Buffer"), true, m_ApiManager, m_IndirectDraw, RHI::IndirectDrawBuffer, drCountAligned *  sizeof(RHI::SDrawIndexedIndirectArgs), drCount * sizeof(RHI::SDrawIndexedIndirectArgs)))
		return false;

	// Camera sort
	if (m_NeedGPUSort)
	{
		// For each draw request, we have indirection and sort key buffers
		// and a GPU sorter object, handling intermediates work buffers
		if (!PK_VERIFY(m_CameraSortIndirection.Resize(drCount)) ||
			!PK_VERIFY(m_CameraSortKeys.Resize(drCount)) ||
			!PK_VERIFY(m_CameraGPUSorters.Resize(drCount)))
			return false;
		for (u32 i = 0; i < drCount; i++)
		{
			if (!PK_VERIFY(m_DrawPass->m_DrawRequests[i]->RenderedParticleCount() > 0))
				continue;
			const u32	alignedParticleCount = Mem::Align<PK_GPU_SORT_NUM_KEY_PER_THREAD * PK_RH_GPU_THREADGROUP_SIZE>(m_DrawPass->m_DrawRequests[i]->RenderedParticleCount());
			if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Camera Sort Indirection Buffer"), true, m_ApiManager, m_CameraSortIndirection[i], RHI::RawBuffer, alignedParticleCount * sizeof(u32), alignedParticleCount * sizeof(u32)) ||
				!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Camera Sort Keys Buffer"), true, m_ApiManager, m_CameraSortKeys[i], RHI::RawBuffer, alignedParticleCount * sizeof(u32), alignedParticleCount * sizeof(u32)))
				return false;
			// 16 bits key
			const u32	sortKeySizeInBits = 16;
			// Init sorter if needed and allocate buffers
			if (!PK_VERIFY(m_CameraGPUSorters[i].Init(sortKeySizeInBits, m_ApiManager)) ||
				!PK_VERIFY(m_CameraGPUSorters[i].AllocateBuffers(m_DrawPass->m_DrawRequests[i]->RenderedParticleCount(), m_ApiManager)))
				return false;
		}
		// Create compute sort key constant set
		if (!PK_VERIFY(m_ComputeCameraSortKeysConstantSets.Reserve(drCount)))
			return false;
		RHI::SConstantSetLayout	layout;
		PKSample::CreateComputeSortKeysConstantSetLayout(layout, m_SortByCameraDistance, false);
		while (m_ComputeCameraSortKeysConstantSets.Count() < drCount)
		{
			RHI::PConstantSet	cs = m_ApiManager->CreateConstantSet(RHI::SRHIResourceInfos("Camera Sort Keys Constant Set"), layout);
			m_ComputeCameraSortKeysConstantSets.PushBackUnsafe(cs);
		}
	}

	// Allocate once, max number of draw requests, indexed by DC from push constant
	const u32	offsetsSizeInBytes = kMaxDrawRequestCount * sizeof(u32); // u32 offsets

	// Particle positions stream
	PK_ASSERT((m_DrawPass->m_ToGenerate.m_GeneratedInputs & Drawers::GenInput_ParticlePosition012) != 0);
	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Offsets_Enableds Buffer"), true, m_ApiManager, m_SimStreamOffsets_Enableds, RHI::RawBuffer, offsetsSizeInBytes, offsetsSizeInBytes) ||
		!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Offsets_Position0s Buffer"), true, m_ApiManager, m_SimStreamOffsets_Positions0, RHI::RawBuffer, offsetsSizeInBytes, offsetsSizeInBytes) ||
		!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Offsets_Position1s Buffer"), true, m_ApiManager, m_SimStreamOffsets_Positions1, RHI::RawBuffer, offsetsSizeInBytes, offsetsSizeInBytes) ||
		!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Offsets_Position2s Buffer"), true, m_ApiManager, m_SimStreamOffsets_Positions2, RHI::RawBuffer, offsetsSizeInBytes, offsetsSizeInBytes))
		return false;

	// GPU sort related offset buffers
	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Offsets_CustomSortKeys Buffer"), m_NeedGPUSort && !m_SortByCameraDistance, m_ApiManager, m_CustomSortKeysOffsets, RHI::RawBuffer, offsetsSizeInBytes, offsetsSizeInBytes))
		return false;

	// (custom) Normals -> additional field

	// (custom) UVs -> additional field

	// Additional fields simStreamOffsets:
	if (!m_AdditionalFieldsSimStreamOffsets.AllocBuffers(kMaxDrawRequestCount, m_ApiManager))
		return false;

	if (!m_Initialized)
		m_Initialized = _InitStaticBuffers(); // Important: after alloctation of the Offsets_XXX buffers
	if (!m_Initialized)
		return false;

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Triangle_GPU::MapBuffers(SRenderContext &ctx)
{
	(void)ctx;
	const u32	drCount = m_DrawPass->m_DrawRequests.Count();

	m_MappedIndirectBuffer = static_cast<RHI::SDrawIndexedIndirectArgs*>(m_ApiManager->MapCpuView(m_IndirectDraw.m_Buffer));

	m_MappedSimStreamOffsets[0] = static_cast<u32*>(m_ApiManager->MapCpuView(m_SimStreamOffsets_Enableds.m_Buffer));
	m_MappedSimStreamOffsets[1] = static_cast<u32*>(m_ApiManager->MapCpuView(m_SimStreamOffsets_Positions0.m_Buffer));
	m_MappedSimStreamOffsets[2] = static_cast<u32*>(m_ApiManager->MapCpuView(m_SimStreamOffsets_Positions1.m_Buffer));
	m_MappedSimStreamOffsets[3] = static_cast<u32*>(m_ApiManager->MapCpuView(m_SimStreamOffsets_Positions2.m_Buffer));

	// Additional streams
	if (!m_AdditionalFieldsSimStreamOffsets.MapBuffers(drCount, m_ApiManager))
		return false;

	// Camera sort optional stream offsets
	if (m_NeedGPUSort && !m_SortByCameraDistance)
	{
		m_MappedCustomSortKeysOffsets = static_cast<u32*>(m_ApiManager->MapCpuView(m_CustomSortKeysOffsets.m_Buffer));
		if (!PK_VERIFY(m_MappedCustomSortKeysOffsets != null))
			return false;
	}

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Triangle_GPU::LaunchCustomTasks(SRenderContext &ctx)
{
	(void)ctx;
	const u32	drCount = m_DrawPass->m_DrawRequests.Count();

	PK_ASSERT(m_MappedIndirectBuffer != null);
	for (u32 dri = 0; dri < drCount; ++dri)
	{
		// Indirect buffer
		m_MappedIndirectBuffer[dri].m_InstanceCount = m_DrawPass->m_DrawRequests[dri]->RenderedParticleCount(); // will be overwritten by the GPU command.
		m_MappedIndirectBuffer[dri].m_IndexOffset = 0;
		m_MappedIndirectBuffer[dri].m_VertexOffset = 0;
		m_MappedIndirectBuffer[dri].m_InstanceOffset = 0;
		m_MappedIndirectBuffer[dri].m_IndexCount = 3;
	}

#if (PK_PARTICLES_UPDATER_USE_GPU != 0)
	const u32	streamMaxCount = 4 + (m_NeedGPUSort && !m_SortByCameraDistance ? 1 : 0) + m_AdditionalFieldsSimStreamOffsets.m_MappedFields.Count();

	// Store offsets in a stack view ([PosOffsetDr0][PosOffsetDr1][SizeOffsetDr0][SizeOffsetDr1]..)
	PK_STACKALIGNEDMEMORYVIEW(u32, streamsOffsets, drCount * streamMaxCount, 0x10);

	for (u32 iDr = 0; iDr < drCount; ++iDr)
	{
		// Fill in stream offsets for each draw request
		const Drawers::SBase_DrawRequest			*dr = m_DrawPass->m_DrawRequests[iDr];
		const Drawers::SBase_BillboardingRequest	baseBr = dr->BaseBillboardingRequest();
		const CParticleStreamToRender_GPU			*streamToRender = m_DrawPass->m_DrawRequests[iDr]->StreamToRender_GPU();
		if (!PK_VERIFY(streamToRender != null))
			continue;

		u32		offset = 0;

		// Mandatory streams
		streamsOffsets[offset++ * drCount + iDr] = streamToRender->StreamOffset(baseBr.m_EnabledStreamId);

		const Drawers::STriangle_DrawRequest	*triDr = static_cast<const Drawers::STriangle_DrawRequest *>(dr);
		streamsOffsets[offset++ * drCount + iDr] = streamToRender->StreamOffset(triDr->m_BB.m_PositionStreamId);
		streamsOffsets[offset++ * drCount + iDr] = streamToRender->StreamOffset(triDr->m_BB.m_Position2StreamId);
		streamsOffsets[offset++ * drCount + iDr] = streamToRender->StreamOffset(triDr->m_BB.m_Position3StreamId);

		// Add all non-virtual stream additional inputs
		for (auto &addField : m_AdditionalFieldsSimStreamOffsets.m_Fields)
			streamsOffsets[offset++ * drCount + iDr] = streamToRender->StreamOffset(baseBr.m_AdditionalInputs[addField.m_AdditionalInputIndex].m_StreamId);

		if (m_NeedGPUSort && !m_SortByCameraDistance)
			streamsOffsets[offset++ * drCount + iDr] = streamToRender->StreamOffset(baseBr.m_SortKeyStreamId);

		PK_ASSERT(offset <= streamMaxCount);
	}

	// Non temporal writes to gpu mem, aligned and contiguous
	u32	streamOffset = 0;
	Mem::Copy_Uncached(m_MappedSimStreamOffsets[0], &streamsOffsets[streamOffset++ * drCount], sizeof(u32) * drCount);
	Mem::Copy_Uncached(m_MappedSimStreamOffsets[1], &streamsOffsets[streamOffset++ * drCount], sizeof(u32) * drCount);
	Mem::Copy_Uncached(m_MappedSimStreamOffsets[2], &streamsOffsets[streamOffset++ * drCount], sizeof(u32) * drCount);
	Mem::Copy_Uncached(m_MappedSimStreamOffsets[3], &streamsOffsets[streamOffset++ * drCount], sizeof(u32) * drCount);

	for (u32 iInput = 0; iInput < m_AdditionalFieldsSimStreamOffsets.m_MappedFields.Count(); ++iInput)
		Mem::Copy_Uncached(m_AdditionalFieldsSimStreamOffsets.m_MappedFields[iInput].m_Storage.m_RawDataPtr, &streamsOffsets[streamOffset++ * drCount], sizeof(u32) * drCount);

	if (m_MappedCustomSortKeysOffsets != null && !m_SortByCameraDistance)
		Mem::Copy_Uncached(m_MappedCustomSortKeysOffsets, &streamsOffsets[streamOffset++ * drCount], sizeof(u32) * drCount);

#endif // (PK_PARTICLES_UPDATER_USE_GPU != 0)

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Triangle_GPU::UnmapBuffers(SRenderContext &ctx)
{
	(void)ctx;

	m_IndirectDraw.UnmapIFN(m_ApiManager);
	m_MappedIndirectBuffer = null;

	m_CustomSortKeysOffsets.UnmapIFN(m_ApiManager);
	m_MappedCustomSortKeysOffsets = null;

	// GPU stream offsets
	m_SimStreamOffsets_Enableds.UnmapIFN(m_ApiManager);
	m_SimStreamOffsets_Positions0.UnmapIFN(m_ApiManager);
	m_SimStreamOffsets_Positions1.UnmapIFN(m_ApiManager);
	m_SimStreamOffsets_Positions2.UnmapIFN(m_ApiManager);
	Mem::Clear(m_MappedSimStreamOffsets);

	m_AdditionalFieldsSimStreamOffsets.UnmapBuffers(m_ApiManager);

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Triangle_GPU::EmitDrawCall(SRenderContext &ctx, const SDrawCallDesc &toEmit)
{
	SRHIRenderContext	&renderContext = static_cast<SRHIRenderContext&>(ctx);

	// !Currently, there is no batching for gpu particles!
	// So we'll emit one draw call per draw request
	// Some data are indexed by the draw-requests. So "EmitDrawCall" shoudl be called once, with all draw-requests.
	PK_ASSERT(toEmit.m_DrawRequests.Count() == m_DrawPass->m_DrawRequests.Count());
	const u32	drCount = m_DrawPass->m_DrawRequests.Count();

	// No need to iterate on all draw requests, just take the first as reference as they wouldn't have been batched if not compatible
	CRendererCacheInstance_UpdateThread		*renderCacheInstance = static_cast<CRendererCacheInstance_UpdateThread*>(toEmit.m_RendererCaches.First().Get());
	if (!PK_VERIFY(renderCacheInstance != null))
	{
		CLog::Log(PK_ERROR, "Invalid renderer cache instance");
		return false;
	}
	PKSample::PCRendererCacheInstance	rCacheInstance = renderCacheInstance->RenderThread_GetCacheInstance();
	if (!PK_VERIFY(rCacheInstance != null))
		return false;

	for (u32 dri = 0; dri < drCount; ++dri)
	{
		PK_ASSERT(toEmit.m_DrawRequests[dri] != null);
		const Drawers::STriangle_DrawRequest			*dr = static_cast<const Drawers::STriangle_DrawRequest*>(toEmit.m_DrawRequests[dri]);
		const Drawers::STriangle_BillboardingRequest	&bbRequest = dr->m_BB;
		const CParticleStreamToRender					*streamToRender = &dr->StreamToRender();
		if (!PK_VERIFY(streamToRender != null))
			continue;

		u32		dummyOffset = 0;
		RHI::PGpuBuffer		drStreamBuffer = _RetrieveStorageBuffer(m_ApiManager, streamToRender, bbRequest.m_PositionStreamId, dummyOffset);
		RHI::PGpuBuffer		drStreamSizeBuffer = _RetrieveParticleInfoBuffer(m_ApiManager, streamToRender);

		if (!PK_VERIFY(drStreamBuffer != null) ||
			!PK_VERIFY(drStreamSizeBuffer != null))
			return false;

		// Camera GPU sort (must be done after the ribbon GPU sort)
		if (m_NeedGPUSort)
		{
			// Compute: Compute sort keys and init indirection buffer
			{
				// For radix sort, a thread computes PK_GPU_SORT_NUM_KEY_PER_THREAD keys (see shader definition and RHIGPUSorter.cpp),
				// so a group computes (PK_GPU_SORT_NUM_KEY_PER_THREAD * PK_RH_GPU_THREADGROUP_SIZE) keys
				const u32	sortGroupCount = (dr->RenderedParticleCount() + PK_GPU_SORT_NUM_KEY_PER_THREAD * PK_RH_GPU_THREADGROUP_SIZE - 1) / (PK_GPU_SORT_NUM_KEY_PER_THREAD * PK_RH_GPU_THREADGROUP_SIZE);
				PKSample::SRHIComputeDispatchs	computeDispatch = PKSample::SRHIComputeDispatchs();

				// Constant set
				computeDispatch.m_NeedSceneInfoConstantSet = m_SortByCameraDistance;
				RHI::PConstantSet	computeConstantSet = m_ComputeCameraSortKeysConstantSets[dri];
				computeConstantSet->SetConstants(drStreamSizeBuffer, 0);
				computeConstantSet->SetConstants(drStreamBuffer, 1);
				computeConstantSet->SetConstants(m_SortByCameraDistance ? m_SimStreamOffsets_Positions0.m_Buffer : m_CustomSortKeysOffsets.m_Buffer, 2);
				computeConstantSet->SetConstants(m_CameraSortKeys[dri].m_Buffer, 3);
				computeConstantSet->SetConstants(m_CameraSortIndirection[dri].m_Buffer, 4);

				computeConstantSet->UpdateConstantValues();
				computeDispatch.m_ConstantSet = computeConstantSet;

				// Push constant
				if (!PK_VERIFY(computeDispatch.m_PushConstants.PushBack().Valid()))
					return false;
				u32	*drPushConstant = reinterpret_cast<u32*>(&computeDispatch.m_PushConstants.Last());
				drPushConstant[0] = dri;

				// State
				const PKSample::EComputeShaderType	type = m_SortByCameraDistance ? ComputeType_ComputeSortKeys_CameraDistance : ComputeType_ComputeSortKeys;
				computeDispatch.m_State = rCacheInstance->m_Cache->GetComputeState(type);
				if (!PK_VERIFY(computeDispatch.m_State != null))
					return false;

				// Dispatch args
				// For the sort key computation dispatch, we need exactly twice the sort group count
				// so that every thread associated key is initialized. Overflowing keys are initialized
				// to 0xFFFF, sorted last (and ultimately not rendered).
				computeDispatch.m_ThreadGroups = CInt3(sortGroupCount * PK_GPU_SORT_NUM_KEY_PER_THREAD, 1, 1);

				renderContext.m_DrawOutputs.m_ComputeDispatchs.PushBack(computeDispatch);
			}

			// Compute: Sort computes
			{
				m_CameraGPUSorters[dri].SetInOutBuffers(m_CameraSortKeys[dri].m_Buffer, m_CameraSortIndirection[dri].m_Buffer);
				m_CameraGPUSorters[dri].AppendDispatchs(rCacheInstance, renderContext.m_DrawOutputs.m_ComputeDispatchs);
			}
		}

		// Emit draw call
		{
			const u32				shaderOptions = Option_GPUStorage | Option_VertexPassThrough | Option_TriangleVertexBillboarding | (m_NeedGPUSort ? Option_GPUSort : 0);
			const RHI::PGpuBuffer	cameraSortIndirection = m_NeedGPUSort ? m_CameraSortIndirection[dri].m_Buffer : null;
			RHI::PConstantSet		vertexBBSimDataConstantSet;
			// create the local constant-set
			{
				const RHI::SConstantSetLayout	*simDataConstantSetLayout = null;
				const RHI::SConstantSetLayout	*offsetsConstantSetLayout = null;
				if (!PK_VERIFY(rCacheInstance->m_Cache->GetGPUStorageConstantSets(static_cast<PKSample::EShaderOptions>(shaderOptions), simDataConstantSetLayout, offsetsConstantSetLayout)) ||
					!PK_VERIFY(simDataConstantSetLayout != null) ||
					!PK_VERIFY(offsetsConstantSetLayout != null) ||
					simDataConstantSetLayout->m_Constants.Empty() ||
					offsetsConstantSetLayout->m_Constants.Empty())
					return false;

				vertexBBSimDataConstantSet = m_ApiManager->CreateConstantSet(RHI::SRHIResourceInfos("Sim Data Constant Set"), *simDataConstantSetLayout);
				if (!PK_VERIFY(vertexBBSimDataConstantSet != null))
					return false;

				PK_ASSERT(drStreamBuffer != null);
				// Sim data constant set are:
				// raw stream + camera sort indirection buffer if needed
				PK_ASSERT(vertexBBSimDataConstantSet->GetConstantValues().Count() == (1u + (m_NeedGPUSort ? 1u : 0u)));
				u32	constantSetLocation = 0;
				if (!PK_VERIFY(vertexBBSimDataConstantSet->SetConstants(drStreamBuffer, constantSetLocation++)))
					return false;
				if (m_NeedGPUSort)
				{
					if (!PK_VERIFY(cameraSortIndirection != null) ||
						!PK_VERIFY(vertexBBSimDataConstantSet->SetConstants(cameraSortIndirection, constantSetLocation++)))
						return false;
				}
				vertexBBSimDataConstantSet->UpdateConstantValues();
			}

			if (!PK_VERIFY(renderContext.m_DrawOutputs.m_DrawCalls.PushBack().Valid()))
			{
				CLog::Log(PK_ERROR, "Failed to create a draw-call");
				return false;
			}
			SRHIDrawCall		&outDrawCall = renderContext.m_DrawOutputs.m_DrawCalls.Last();

			outDrawCall.m_Batch = this;
			outDrawCall.m_RendererCacheInstance = renderCacheInstance;
			outDrawCall.m_Type = SRHIDrawCall::DrawCall_IndexedInstancedIndirect;
			outDrawCall.m_ShaderOptions = shaderOptions;
			outDrawCall.m_RendererType = Renderer_Triangle;
			outDrawCall.m_GPUStorageSimDataConstantSet = vertexBBSimDataConstantSet;

			// Some meta-data (the Editor uses them)
			{
				outDrawCall.m_BBox = toEmit.m_BBox;
				outDrawCall.m_TotalBBox = m_DrawPass->m_TotalBBox;
				outDrawCall.m_SlicedDC = toEmit.m_TotalParticleCount != m_DrawPass->m_TotalParticleCount;

				outDrawCall.m_Valid =	rCacheInstance->m_Cache != null &&
										rCacheInstance->m_Cache->GetRenderState(static_cast<PKSample::EShaderOptions>(outDrawCall.m_ShaderOptions)) != null;
			}

			outDrawCall.m_GPUStorageOffsetsConstantSet = m_VertexBBOffsetsConstantSet;

			{
#if	(PK_HAS_PARTICLES_SELECTION != 0)
				PKSample::SRHIRenderContext	&ctxEditor = *static_cast<PKSample::SRHIRenderContext*>(&ctx);
				RHI::PGpuBuffer		bufferIsSelected = ctxEditor.Selection().HasGPUParticlesSelected() ? GetIsSelectedBuffer(ctxEditor.Selection(), *dr) : null;
				if (bufferIsSelected != null)
				{
					RHI::SConstantSetLayout	selectionSetLayout(RHI::VertexShaderMask);
					selectionSetLayout.AddConstantsLayout(RHI::SRawBufferDesc("Selections"));
					RHI::PConstantSet	selectionConstantSet = m_ApiManager->CreateConstantSet(RHI::SRHIResourceInfos("Selection Constant Set"), selectionSetLayout);
					if (PK_VERIFY(selectionConstantSet != null) && PK_VERIFY(selectionConstantSet->SetConstants(bufferIsSelected, 0)))
					{
						selectionConstantSet->UpdateConstantValues();
						outDrawCall.m_SelectionConstantSet = selectionConstantSet;
					}
				}
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)
			}

			// Fill the semantics for the debug draws:
			outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Enabled] = m_SimStreamOffsets_Enableds.m_Buffer;
			outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_VertexPosition0] = m_SimStreamOffsets_Positions0.m_Buffer;
			outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_VertexPosition1] = m_SimStreamOffsets_Positions1.m_Buffer;
			outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_VertexPosition2] = m_SimStreamOffsets_Positions2.m_Buffer;
			if (m_ColorStreamIdx.Valid())
				outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Color] = m_AdditionalFieldsSimStreamOffsets.m_Fields[m_ColorStreamIdx].m_Buffer.m_Buffer;

			outDrawCall.m_IndexOffset = 0;
			outDrawCall.m_IndexSize = RHI::IndexBuffer16Bit;
			outDrawCall.m_IndexBuffer = m_DrawIndices.m_Buffer;

			outDrawCall.m_IndirectBuffer = m_IndirectDraw.m_Buffer;
			outDrawCall.m_IndirectBufferOffset = dri * sizeof(RHI::SDrawIndexedIndirectArgs);
			outDrawCall.m_EstimatedParticleCount = dr->RenderedParticleCount();

			if (!PK_VERIFY(renderContext.m_DrawOutputs.m_CopyCommands.PushBack().Valid()))
				return false;

			SRHICopyCommand		&copyCommand = renderContext.m_DrawOutputs.m_CopyCommands.Last();

			// We retrieve the particles info buffer:
			copyCommand.m_SrcBuffer = drStreamSizeBuffer;
			copyCommand.m_SrcOffset = 0;
			copyCommand.m_DstBuffer = m_IndirectDraw.m_Buffer;
			copyCommand.m_DstOffset = dri * sizeof(RHI::SDrawIndexedIndirectArgs) + PK_MEMBER_OFFSET(RHI::SDrawIndexedIndirectArgs, m_InstanceCount);
			copyCommand.m_SizeToCopy = sizeof(u32);

			// No batching with GPU storage, so here this is constant within a drawcall (uses push constant).
			if (!PK_VERIFY(outDrawCall.m_PushConstants.PushBack().Valid()))
				return false;
			PK_STATIC_ASSERT(sizeof(Drawers::STriangleDrawRequest) == 8);
			Drawers::STriangleDrawRequest	&desc = *reinterpret_cast<Drawers::STriangleDrawRequest*>(&outDrawCall.m_PushConstants.Last());
			desc.Setup(bbRequest);

			// GPUBillboardPushConstants
			if (!PK_VERIFY(outDrawCall.m_PushConstants.PushBack().Valid()))
				return false;
			SVertexBillboardingConstants	&indices = *reinterpret_cast<SVertexBillboardingConstants*>(&outDrawCall.m_PushConstants.Last());
			indices.m_IndicesOffset = toEmit.m_IndexOffset;
			indices.m_StreamOffsetsIndex = dri;
		}
	}

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Triangle_GPU::_InitStaticBuffers()
{
	// Index buffer
	// RHI currently doesn't support DrawInstanced (without indices)
	{
		if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("VertexBB Draw Indices Buffer"), true, m_ApiManager, m_DrawIndices, RHI::IndexBuffer, 8 * sizeof(u16), 3 * sizeof(u16)))
			return false;
		volatile u16	*indices = static_cast<u16*>(m_ApiManager->MapCpuView(m_DrawIndices.m_Buffer, 0, 3 * sizeof(u16)));
		if (!PK_VERIFY(indices != null))
			return false;
		indices[0] = 0;
		indices[1] = 1;
		indices[2] = 2;
		m_DrawIndices.Unmap(m_ApiManager);
	}

	// Constant-Set
	{
		CRendererCacheInstance_UpdateThread		*renderCacheInstance = static_cast<CRendererCacheInstance_UpdateThread*>(m_DrawPass->m_RendererCaches.First().Get());
		PK_ASSERT(renderCacheInstance != null);
		const PCRendererCacheInstance	cacheInstance = renderCacheInstance->RenderThread_GetCacheInstance();
		if (cacheInstance == null)
			return false;

		const u32	shaderOptions = Option_GPUStorage | Option_VertexPassThrough | Option_TriangleVertexBillboarding | (m_NeedGPUSort ? Option_GPUSort : 0);

		const RHI::SConstantSetLayout	*simDataConstantSetLayout = null;
		const RHI::SConstantSetLayout	*offsetsConstantSetLayout = null;
		if (!PK_VERIFY(cacheInstance->m_Cache->GetGPUStorageConstantSets(static_cast<PKSample::EShaderOptions>(shaderOptions), simDataConstantSetLayout, offsetsConstantSetLayout)) ||
			!PK_VERIFY(offsetsConstantSetLayout != null) ||
			offsetsConstantSetLayout->m_Constants.Empty())
			return false;

		m_VertexBBOffsetsConstantSet = m_ApiManager->CreateConstantSet(RHI::SRHIResourceInfos("Offsets Constant Set"), *offsetsConstantSetLayout);
		if (!PK_VERIFY(m_VertexBBOffsetsConstantSet != null))
			return false;

		// Fill offsets constant sets.
		u32 i = 0;
		{
			if (!PK_VERIFY(m_VertexBBOffsetsConstantSet->SetConstants(m_SimStreamOffsets_Enableds.m_Buffer, i++)))
				return false;
			if (!PK_VERIFY(m_VertexBBOffsetsConstantSet->SetConstants(m_SimStreamOffsets_Positions0.m_Buffer, i++)))
				return false;
			if (!PK_VERIFY(m_VertexBBOffsetsConstantSet->SetConstants(m_SimStreamOffsets_Positions1.m_Buffer, i++)))
				return false;
			if (!PK_VERIFY(m_VertexBBOffsetsConstantSet->SetConstants(m_SimStreamOffsets_Positions2.m_Buffer, i++)))
				return false;

			for (auto &additionalSimStreamOffset : m_AdditionalFieldsSimStreamOffsets.m_Fields)
			{
				if (!PK_VERIFY(m_VertexBBOffsetsConstantSet->SetConstants(additionalSimStreamOffset.m_Buffer.m_Buffer, i++)))
					return false;
			}
		}
		PK_ASSERT(m_VertexBBOffsetsConstantSet->GetConstantValues().Count() == i);
		m_VertexBBOffsetsConstantSet->UpdateConstantValues();
	}

	return true;
}

//----------------------------------------------------------------------------
//
// CRHIRendererBatch_Mesh_GPU
//
//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Mesh_GPU::Setup(const CRendererDataBase *renderer, const CParticleRenderMedium *owner, const CFrameCollector *fc, const CStringId &storageClass)
{
	if (!CRendererBatchJobs_Mesh_GPUBB::Setup(renderer, owner, fc, storageClass))
		return false;

	// Compute mesh info
	m_MeshCount = 0;
	m_MeshLODCount = renderer->m_RendererCache->LODCount();
	for (u32 i = 0; i < m_MeshLODCount; ++i)
		m_MeshCount += renderer->m_RendererCache->m_PerLODMeshCount[i];

	if (!PK_VERIFY(m_PerMeshIndexCount.Resize(m_MeshCount)))
		return false;

	// Setup additional fields:
	// The additional fields are supposed to be the same for all renderers in a batch.
	// If not, then you can recompute then on the "Bind()" method.

	const auto		&toGenerate = m_DrawPass->m_ToGenerate;
	const u32		additionalFieldsCount = toGenerate.m_AdditionalGeneratedInputs.Count();

	if (!PK_VERIFY(m_AdditionalFieldsSimStreamOffsets.m_Fields.Reserve(additionalFieldsCount)))
		return false;

	for (u32 i = 0; i < additionalFieldsCount; ++i)
	{
		if (toGenerate.m_AdditionalGeneratedInputs[i].m_Name == BasicRendererProperties::SID_MeshLOD_LOD())
			continue; // ignored field

		m_AdditionalFieldsSimStreamOffsets.m_Fields.PushBackUnsafe(SAdditionalInputs(sizeof(u32) /* it contains the sim buffer offsets */, i));
	}

	m_ColorStreamIdx = _GetDrawDebugColorIndex(m_AdditionalFieldsSimStreamOffsets, toGenerate);

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Mesh_GPU::AreRenderersCompatible(const CRendererDataBase *rendererA, const CRendererDataBase *rendererB) const
{
	return	CRendererBatchJobs_Mesh_GPUBB::AreRenderersCompatible(rendererA, rendererB) &&
			AreBillboardingBatchable(rendererA->m_RendererCache, rendererB->m_RendererCache);
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Mesh_GPU::AllocBuffers(SRenderContext &ctx)
{
	(void)ctx;

	const u32	drCount = m_DrawPass->m_DrawRequests.Count();
	const u32	drCountAligned = Mem::Align<0x10>(drCount);

	const u32	totalParticleCount = m_DrawPass->m_TotalParticleCount;
	const u32	totalParticleCountAligned = Mem::Align<0x100>(totalParticleCount);

	// Indirect draw buffer
	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Indirect Draw Args Buffer"), true, m_ApiManager, m_IndirectDraw, RHI::RawIndirectDrawBuffer, sizeof(RHI::SDrawIndexedIndirectArgs) * m_MeshCount * drCountAligned, sizeof(RHI::SDrawIndexedIndirectArgs) * m_MeshCount * drCount))
		return false;

	// Indirection buffer
	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Indirection Buffer"), true, m_ApiManager, m_Indirection, RHI::RawBuffer, totalParticleCountAligned * sizeof(u32), totalParticleCount * sizeof(u32)))
		return false;

	// Indirection offsets buffer:
	// * Mesh atlas enabled: one u32 per sub mesh (sub mesh array is "flat" when LODs are enabled, total sub meshes is the total number of sub meshes per LODs)
	// * LOD enabled: one u32 per lod level
	// * No mesh atlas/LOD: one u32 per draw request
	// times 2, the first part of the buffer is used as a scratch buffer for computing the per-particle indirections
	const u32	indOffsetElementCount = 2 * drCount * sizeof(u32) * (m_HasMeshIDs ? m_MeshCount : (m_HasMeshLODs ? m_MeshLODCount : 1));
	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Indirection Offsets Buffer"), true, m_ApiManager, m_IndirectionOffsets, RHI::RawBuffer, indOffsetElementCount * sizeof(u32), indOffsetElementCount * sizeof(u32)))
		return false;

	// Allocate once, max number of draw requests, indexed by DC from push constant
	const u32	offsetsSizeInBytes = kMaxDrawRequestCount * sizeof(u32); // u32 offsets
	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Offsets_Positions Buffer"), true, m_ApiManager, m_SimStreamOffsets_Positions, RHI::RawBuffer, offsetsSizeInBytes, offsetsSizeInBytes) ||
		!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Offsets_Scales Buffer"), true, m_ApiManager, m_SimStreamOffsets_Scales, RHI::RawBuffer, offsetsSizeInBytes, offsetsSizeInBytes) ||
		!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Offsets_Orientations Buffer"), true, m_ApiManager, m_SimStreamOffsets_Orientations, RHI::RawBuffer, offsetsSizeInBytes, offsetsSizeInBytes) ||
		!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Offsets_Enableds Buffer"), true, m_ApiManager, m_SimStreamOffsets_Enableds, RHI::RawBuffer, offsetsSizeInBytes, offsetsSizeInBytes) ||
		!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Offsets_MeshIDs Buffer"), m_HasMeshIDs, m_ApiManager, m_SimStreamOffsets_MeshIDs, RHI::RawBuffer, offsetsSizeInBytes, offsetsSizeInBytes) ||
		!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Offsets_MeshLODs Buffer"), m_HasMeshLODs, m_ApiManager, m_SimStreamOffsets_MeshLODs, RHI::RawBuffer, offsetsSizeInBytes, offsetsSizeInBytes) ||
		!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Offsets_Matrices Buffer"), true, m_ApiManager, m_MatricesOffsets, RHI::RawBuffer, offsetsSizeInBytes, offsetsSizeInBytes))
		return false;

	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Matrices Buffer"), true, m_ApiManager, m_Matrices, RHI::RawBuffer, totalParticleCountAligned * sizeof(CFloat4x4), totalParticleCount * sizeof(CFloat4x4)))
		return false;

	// Constant buffer containing an array of 16 uints for per lod level submesh count (and a second array of the same size + 1, if we have per-particle LODs)
	PK_ASSERT(!m_HasMeshLODs || m_MeshLODCount <= 0x10u);  // We limit the max number of mesh LODs.
	const u32	cbSizeInBytes = (sizeof(u32) * 0x10 + (m_HasMeshLODs ? sizeof(u32) * (0x10 + 1) : 0)) * 4; // * 4: as all uint are 16bytes aligned.
	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("LODOffsets Constant Buffer"), true, m_ApiManager, m_LODsConstantBuffer, RHI::ConstantBuffer, cbSizeInBytes, cbSizeInBytes))
		return false;

	{
		u32	*mappedLODOffsets = static_cast<u32*>(m_ApiManager->MapCpuView(m_LODsConstantBuffer.m_Buffer, 0, cbSizeInBytes));
		if (!PK_VERIFY(mappedLODOffsets != null))
			return false;
		// Clear everything, just in case.
		Mem::Clear_Uncached(mappedLODOffsets, cbSizeInBytes);

		// Write PerMeshLODCount
		for (u32 i = 0; i < PKMin(m_MeshLODCount, 0x10u); ++i)
			*Mem::AdvanceRawPointer(mappedLODOffsets, i * 0x10) = m_DrawPass->m_RendererCaches.First()->m_PerLODMeshCount[i];
		mappedLODOffsets = Mem::AdvanceRawPointer(mappedLODOffsets, 0x100);

		// Accumulate PerMeshLODCount into the second constant buffer member
		if (m_HasMeshLODs)
		{
			u32	submeshCount = 0;
			for (u32 i = 0; i < PKMin(m_MeshLODCount, 0x10u); ++i)
			{
				*Mem::AdvanceRawPointer(mappedLODOffsets, i * 0x10) = submeshCount;
				submeshCount += m_DrawPass->m_RendererCaches.First()->m_PerLODMeshCount[i];
			}
			// Write at last LOD level + 1 the total number of submeshes, to simplify the shader a bit.
			*Mem::AdvanceRawPointer(mappedLODOffsets, PKMin(m_MeshLODCount, 0x10u) * 0x10) = submeshCount;
		}
		m_ApiManager->UnmapCpuView(m_LODsConstantBuffer.m_Buffer);
	}

	{
		CRendererCacheInstance_UpdateThread		*refCacheInstance = static_cast<CRendererCacheInstance_UpdateThread*>(m_DrawPass->m_RendererCaches.First().Get());
		PKSample::PCRendererCacheInstance		rCacheInstance = refCacheInstance->RenderThread_GetCacheInstance();
		bool	hasNoMesh = rCacheInstance == null || rCacheInstance->m_AdditionalGeometry == null;
		for (u32 i = 0; i < m_PerMeshIndexCount.Count(); i++)
		{
			if (hasNoMesh)
			{
				m_PerMeshIndexCount[i] = 0;
			}
			else
			{
				const Utils::GpuBufferViews	&bufferView = rCacheInstance->m_AdditionalGeometry->m_PerGeometryViews[i];

				// Get index count
				u32	indexCount = 0;
				if (bufferView.m_IndexBufferSize == RHI::IndexBuffer16Bit)
					indexCount = bufferView.m_IndexBuffer->GetByteSize() / sizeof(u16);
				else if (bufferView.m_IndexBufferSize == RHI::IndexBuffer32Bit)
					indexCount = bufferView.m_IndexBuffer->GetByteSize() / sizeof(u32);
				else
				{
					PK_ASSERT_NOT_REACHED();
					return false;
				}
				m_PerMeshIndexCount[i] = indexCount;
			}
		}
	}

	// Additional fields simStreamOffsets:
	if (!m_AdditionalFieldsSimStreamOffsets.AllocBuffers(kMaxDrawRequestCount, m_ApiManager))
		return false;

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Mesh_GPU::MapBuffers(SRenderContext &ctx)
{
	(void)ctx;

	// Indirect draw buffer
	m_MappedIndexedIndirectBuffer = static_cast<RHI::SDrawIndexedIndirectArgs*>(m_ApiManager->MapCpuView(m_IndirectDraw.m_Buffer));
	if (!PK_VERIFY(m_MappedIndexedIndirectBuffer != null))
		return false;

	// Sim stream offsets
	m_MappedSimStreamOffsets[0] = static_cast<u32*>(m_ApiManager->MapCpuView(m_SimStreamOffsets_Positions.m_Buffer));
	if (!PK_VERIFY(m_MappedSimStreamOffsets[0] != null))
		return false;
	m_MappedSimStreamOffsets[1] = static_cast<u32*>(m_ApiManager->MapCpuView(m_SimStreamOffsets_Orientations.m_Buffer));
	if (!PK_VERIFY(m_MappedSimStreamOffsets[1] != null))
		return false;
	m_MappedSimStreamOffsets[2] = static_cast<u32*>(m_ApiManager->MapCpuView(m_SimStreamOffsets_Scales.m_Buffer));
	if (!PK_VERIFY(m_MappedSimStreamOffsets[2] != null))
		return false;
	m_MappedSimStreamOffsets[3] = static_cast<u32*>(m_ApiManager->MapCpuView(m_SimStreamOffsets_Enableds.m_Buffer));
	if (!PK_VERIFY(m_MappedSimStreamOffsets[3] != null))
		return false;
	if (m_HasMeshIDs)
	{
		m_MappedSimStreamOffsets[4] = static_cast<u32*>(m_ApiManager->MapCpuView(m_SimStreamOffsets_MeshIDs.m_Buffer));
		if (!PK_VERIFY(m_MappedSimStreamOffsets[4] != null))
			return false;
	}
	if (m_HasMeshLODs)
	{
		m_MappedSimStreamOffsets[5] = static_cast<u32*>(m_ApiManager->MapCpuView(m_SimStreamOffsets_MeshLODs.m_Buffer));
		if (!PK_VERIFY(m_MappedSimStreamOffsets[5] != null))
			return false;
	}
	if (!m_AdditionalFieldsSimStreamOffsets.MapBuffers(kMaxDrawRequestCount, m_ApiManager))
		return false;

	// Matrices offsets
	m_MappedMatricesOffsets = static_cast<u32*>(m_ApiManager->MapCpuView(m_MatricesOffsets.m_Buffer));
	if (!PK_VERIFY(m_MappedMatricesOffsets != null))
		return false;

	// Indirection offsets
	m_MappedIndirectionOffsets = static_cast<u32*>(m_ApiManager->MapCpuView(m_IndirectionOffsets.m_Buffer));
	if (!PK_VERIFY(m_MappedIndirectionOffsets != null))
		return false;


	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Mesh_GPU::LaunchCustomTasks(SRenderContext &ctx)
{
	(void)ctx;
	// Done inline.
#if (PK_PARTICLES_UPDATER_USE_GPU != 0)
	const u32	drCount = m_DrawPass->m_DrawRequests.Count();

	if (m_MappedIndexedIndirectBuffer != null)
	{
		for (u32 dri = 0; dri < drCount; dri++)
		{
			for (u32 meshi = 0; meshi < m_MeshCount; meshi++)
			{
				m_MappedIndexedIndirectBuffer[m_MeshCount * dri + meshi].m_IndexCount = m_PerMeshIndexCount[meshi];
				m_MappedIndexedIndirectBuffer[m_MeshCount * dri + meshi].m_InstanceCount = 0; // instance count will be set by compute shader
				m_MappedIndexedIndirectBuffer[m_MeshCount * dri + meshi].m_IndexOffset = 0;
				m_MappedIndexedIndirectBuffer[m_MeshCount * dri + meshi].m_VertexOffset = 0;
				m_MappedIndexedIndirectBuffer[m_MeshCount * dri + meshi].m_InstanceOffset = 0;
			}
		}
	}

	// Setup stream offsets
	//	Pos/Scale/Orientation/Enabled [ + MeshIds ] [ + MeshLODs ] [ + Additional inputs ]
	const u32	streamCount = 4 + (m_HasMeshIDs ? 1 : 0) + (m_HasMeshLODs ? 1 : 0) + m_AdditionalFieldsSimStreamOffsets.m_MappedFields.Count();
	PK_STACKALIGNEDMEMORYVIEW(u32, streamsOffsets, drCount * streamCount, 0x10);
	for (u32 dri = 0; dri < drCount; dri++)
	{
		// Fill in stream offsets for each draw request
		const Drawers::SMesh_DrawRequest			*dr = static_cast<const Drawers::SMesh_DrawRequest*>(m_DrawPass->m_DrawRequests[dri]);
		const Drawers::SMesh_BillboardingRequest	*bbRequest = static_cast<const Drawers::SMesh_BillboardingRequest*>(&dr->BaseBillboardingRequest());
		const CParticleStreamToRender_GPU			*streamToRender = dr->StreamToRender_GPU();
		if (!PK_VERIFY(streamToRender != null))
			continue;

		u32		offset = 0;
		streamsOffsets[offset++ * drCount + dri] = streamToRender->StreamOffset(bbRequest->m_PositionStreamId);
		streamsOffsets[offset++ * drCount + dri] = streamToRender->StreamOffset(bbRequest->m_OrientationStreamId);
		streamsOffsets[offset++ * drCount + dri] = streamToRender->StreamOffset(bbRequest->m_ScaleStreamId);
		streamsOffsets[offset++ * drCount + dri] = streamToRender->StreamOffset(bbRequest->m_EnabledStreamId);
		if (m_HasMeshIDs)
			streamsOffsets[offset++ * drCount + dri] = streamToRender->StreamOffset(bbRequest->m_MeshIDStreamId);
		if (m_HasMeshLODs)
			streamsOffsets[offset++ * drCount + dri] = streamToRender->StreamOffset(bbRequest->m_MeshLODStreamId);

		for (const auto &addField : m_AdditionalFieldsSimStreamOffsets.m_MappedFields)
		{
			const u32	additionalInputId = addField.m_AdditionalInputIndex;
			streamsOffsets[offset++ * drCount + dri] = streamToRender->StreamOffset(bbRequest->m_AdditionalInputs[additionalInputId].m_StreamId);
		}
		PK_ASSERT(offset == streamCount);
	}
	// Non temporal writes to gpu mem, aligned and contiguous
	u32	streamOffset = 0;
	Mem::Copy_Uncached(m_MappedSimStreamOffsets[0], &streamsOffsets[streamOffset++ * drCount], sizeof(u32) * drCount);
	Mem::Copy_Uncached(m_MappedSimStreamOffsets[1], &streamsOffsets[streamOffset++ * drCount], sizeof(u32) * drCount);
	Mem::Copy_Uncached(m_MappedSimStreamOffsets[2], &streamsOffsets[streamOffset++ * drCount], sizeof(u32) * drCount);
	Mem::Copy_Uncached(m_MappedSimStreamOffsets[3], &streamsOffsets[streamOffset++ * drCount], sizeof(u32) * drCount);
	if (m_HasMeshIDs)
		Mem::Copy_Uncached(m_MappedSimStreamOffsets[4], &streamsOffsets[streamOffset++ * drCount], sizeof(u32) * drCount);
	if (m_HasMeshLODs)
		Mem::Copy_Uncached(m_MappedSimStreamOffsets[5], &streamsOffsets[streamOffset++ * drCount], sizeof(u32) * drCount);
	for (const auto &addField : m_AdditionalFieldsSimStreamOffsets.m_MappedFields)
		Mem::Copy_Uncached(addField.m_Storage.m_RawDataPtr, &streamsOffsets[streamOffset++ * drCount], sizeof(u32) * drCount);

	// Init indirection offsets
	const u32	indirectionOffsetsSizeInBytes = 2 * drCount * sizeof(u32) * (m_HasMeshIDs ? m_MeshCount : (m_HasMeshLODs ? m_MeshLODCount : 1));
	Mem::Clear_Uncached(m_MappedIndirectionOffsets, indirectionOffsetsSizeInBytes);

	// Setup matrices offset
	u32		matricesOffset = 0u;
	for (u32 dri = 0; dri < drCount; dri++)
	{
		m_MappedMatricesOffsets[dri] = matricesOffset;
		const Drawers::SMesh_DrawRequest	*dr = static_cast<const Drawers::SMesh_DrawRequest*>(m_DrawPass->m_DrawRequests[dri]);
		matricesOffset += dr->InputParticleCount() * sizeof(CFloat4x4);
	}

	return true;
#else
	return false;
#endif // (PK_PARTICLES_UPDATER_USE_GPU != 0)
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Mesh_GPU::UnmapBuffers(SRenderContext &ctx)
{
	(void)ctx;
	RHI::PApiManager	manager = m_ApiManager;

	m_IndirectDraw.UnmapIFN(m_ApiManager);

	// GPU stream offsets
	m_SimStreamOffsets_Positions.UnmapIFN(m_ApiManager);
	m_SimStreamOffsets_Scales.UnmapIFN(m_ApiManager);
	m_SimStreamOffsets_Orientations.UnmapIFN(m_ApiManager);
	m_SimStreamOffsets_Enableds.UnmapIFN(m_ApiManager);
	m_SimStreamOffsets_MeshIDs.UnmapIFN(m_ApiManager);
	m_SimStreamOffsets_MeshLODs.UnmapIFN(m_ApiManager);
	Mem::Clear(m_MappedSimStreamOffsets);

	m_AdditionalFieldsSimStreamOffsets.UnmapBuffers(m_ApiManager);

	m_IndirectionOffsets.UnmapIFN(manager);
	m_MatricesOffsets.UnmapIFN(manager);

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Mesh_GPU::EmitDrawCall(SRenderContext &ctx, const SDrawCallDesc &toEmit)
{
#if (PK_PARTICLES_UPDATER_USE_GPU != 0)
	SRHIRenderContext	&renderContext = static_cast<SRHIRenderContext&>(ctx);

	// !Currently, there is no batching for gpu particles!
	// So we'll emit one draw call per draw request
	// Some data are indexed by the draw-requests. So "EmitDrawCall" shoudl be called once, with all draw-requests.
	PK_ASSERT(toEmit.m_DrawRequests.Count() == m_DrawPass->m_DrawRequests.Count());

	// Get renderer cache instance
	CRendererCacheInstance_UpdateThread			*refCacheInstance = static_cast<CRendererCacheInstance_UpdateThread*>(toEmit.m_RendererCaches.First().Get());
	if (!PK_VERIFY(refCacheInstance != null))
		return false;
	PKSample::PCRendererCacheInstance		rCacheInstance = refCacheInstance->RenderThread_GetCacheInstance();

	if (rCacheInstance == null || rCacheInstance->m_Cache == null)
		return false;

	// Mesh not resolved, shouldn't assert.
	if (rCacheInstance->m_AdditionalGeometry == null)
		return true;

	// Will search the attribute description in render state, because renderer features has been checked at renderer cache build in MaterialToRHI.cpp:256 (_SetGeneratedInputs)
	const u32			shaderOptions = PKSample::Option_VertexPassThrough | PKSample::Option_GPUMesh;

	TMemoryView<const RHI::SVertexAttributeDesc>	vertexBuffDescView;
	RHI::PCRenderState	renderState = rCacheInstance->m_Cache->GetRenderState(static_cast<PKSample::EShaderOptions>(shaderOptions));
	if (renderState != null)
	{
		vertexBuffDescView = renderState->m_RenderState.m_ShaderBindings.m_InputAttributes.View();
		// Vertex attribute should only hold mesh data since particle additionnal fields are bound using raw buffer
		PK_ONLY_IF_ASSERTS({
			const u32	vertexBufferMaxCount = Utils::__MaxMeshSemantics + 1;
		PK_ASSERT(vertexBuffDescView.Count() <= vertexBufferMaxCount);
		});
	}

	const u32	subMeshCount = rCacheInstance->m_AdditionalGeometry->m_PerGeometryViews.Count();
	if (subMeshCount == 0)
		return false;

	// We will fill those compute dispatch descriptions arrays and add them to draw call afterwards since order matters.
	TArray<SRHIComputeDispatchs>	computeParticleCountPerMesh;
	TArray<SRHIComputeDispatchs>	computeMeshMatrices;
	TArray<SRHIComputeDispatchs>	computeIndirection;

	// Get simData and offsets constant sets layouts
	const RHI::SConstantSetLayout	*simDataConstantSetLayout = null;
	const RHI::SConstantSetLayout	*offsetsConstantSetLayout = null;
	if (!PK_VERIFY(rCacheInstance->m_Cache->GetGPUStorageConstantSets(static_cast<PKSample::EShaderOptions>(shaderOptions), simDataConstantSetLayout, offsetsConstantSetLayout)) ||
		!PK_VERIFY(simDataConstantSetLayout != null) ||
		!PK_VERIFY(offsetsConstantSetLayout != null) ||
		simDataConstantSetLayout->m_Constants.Empty() ||
		offsetsConstantSetLayout->m_Constants.Empty())
		return false;

	const u32	drCount = toEmit.m_DrawRequests.Count();
	const u32	lodCount = refCacheInstance->m_PerLODMeshCount.Count();
	u32			drawCallCurrentOffset = 0;

	// Iterate on draw requests
	for (u32 dri = 0; dri < drCount; ++dri)
	{
		const Drawers::SMesh_DrawRequest				*dr = static_cast<const Drawers::SMesh_DrawRequest*>(toEmit.m_DrawRequests[dri]);
		const Drawers::SMesh_BillboardingRequest		*bbRequest = static_cast<const Drawers::SMesh_BillboardingRequest*>(&dr->BaseBillboardingRequest());
		const CParticleStreamToRender_GPU				*streamToRender = dr->StreamToRender_GPU();
		if (!PK_VERIFY(streamToRender != null))
			continue;

		// Get particle sim info GPU buffer
		RHI::PGpuBuffer									particleSimInfo = _RetrieveParticleInfoBuffer(m_ApiManager, streamToRender);
		if (!PK_VERIFY(particleSimInfo != null))
			return false;

		// Get particle stream GPU buffer
		u32					offsetDummy = 0;
		RHI::PGpuBuffer		streamBufferGPU = _RetrieveStorageBuffer(m_ApiManager, streamToRender, bbRequest->m_PositionStreamId, offsetDummy);
		PK_ASSERT(streamBufferGPU != null);

		// Get particle count estimated for dispatch
		const u32										drParticleCountEst = dr->InputParticleCount();

		// Update simData constant set
		m_SimDataConstantSet = m_ApiManager->CreateConstantSet(RHI::SRHIResourceInfos("Sim Data Constant Set"), *simDataConstantSetLayout);
		if (!PK_VERIFY(m_SimDataConstantSet != null))
			return false;
		PK_ASSERT(m_SimDataConstantSet->GetConstantValues().Count() == 3); // SimBuffer, transforms (from computes), indirection (from computes)
		if (!PK_VERIFY(m_SimDataConstantSet->SetConstants(streamBufferGPU, 0)) ||
			!PK_VERIFY(m_SimDataConstantSet->SetConstants(m_Matrices.m_Buffer, 1)) ||
			!PK_VERIFY(m_SimDataConstantSet->SetConstants(m_Indirection.m_Buffer, 2)))
			return false;
		m_SimDataConstantSet->UpdateConstantValues();

		// Update offsets constant set
		m_OffsetsConstantSet = m_ApiManager->CreateConstantSet(RHI::SRHIResourceInfos("Offsets Constant Set"), *offsetsConstantSetLayout);
		if (!PK_VERIFY(m_OffsetsConstantSet != null))
			return false;
		PK_ASSERT(m_OffsetsConstantSet->GetConstantValues().Count() == m_AdditionalFieldsSimStreamOffsets.m_Fields.Count() + 2); // transforms offsets, indirection offsets, additional inputs
		if (!PK_VERIFY(m_OffsetsConstantSet->SetConstants(m_MatricesOffsets.m_Buffer, 0)) ||
			!PK_VERIFY(m_OffsetsConstantSet->SetConstants(m_IndirectionOffsets.m_Buffer, 1)))
			return false;
		for (u32 i = 2; i < m_OffsetsConstantSet->GetConstantValues().Count(); ++i)
		{
			if (!PK_VERIFY(m_OffsetsConstantSet->SetConstants(m_AdditionalFieldsSimStreamOffsets.m_Fields[i - 2].m_Buffer.m_Buffer, i)))
				return false;
		}
		m_OffsetsConstantSet->UpdateConstantValues();

#if	(PK_HAS_PARTICLES_SELECTION != 0)
		PKSample::SRHIRenderContext	&ctxEditor = *static_cast<PKSample::SRHIRenderContext*>(&ctx);
		RHI::PGpuBuffer		bufferIsSelected = ctxEditor.Selection().HasGPUParticlesSelected() ? GetIsSelectedBuffer(ctxEditor.Selection(), *dr) : null;
		RHI::PConstantSet	selectionConstantSet;
		if (bufferIsSelected != null)
		{
			RHI::SConstantSetLayout	selectionSetLayout(RHI::VertexShaderMask);
			selectionSetLayout.AddConstantsLayout(RHI::SRawBufferDesc("IsSelected"));
			selectionConstantSet = m_ApiManager->CreateConstantSet(RHI::SRHIResourceInfos("IsSelected Constant Set"), selectionSetLayout);
			if (PK_VERIFY(selectionConstantSet != null) && PK_VERIFY(selectionConstantSet->SetConstants(bufferIsSelected, 0)))
			{
				selectionConstantSet->UpdateConstantValues();
			}
		}
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)

		// For each mesh, setup draw call description
		for (u32 iSubMesh = 0; iSubMesh < subMeshCount; ++iSubMesh)
		{

			if (!PK_VERIFY(renderContext.m_DrawOutputs.m_DrawCalls.PushBack().Valid()))
			{
				CLog::Log(PK_ERROR, "Failed to create a draw-call");
				return false;
			}
			SRHIDrawCall	&outDrawCall = renderContext.m_DrawOutputs.m_DrawCalls.Last();

			outDrawCall.m_Batch = this;
			outDrawCall.m_RendererCacheInstance = refCacheInstance;
			outDrawCall.m_Type = SRHIDrawCall::DrawCall_IndexedInstancedIndirect;
			outDrawCall.m_ShaderOptions = shaderOptions;
			outDrawCall.m_RendererType = Renderer_Mesh;

			// Some meta-data (the Editor uses them)
			{
				outDrawCall.m_BBox = toEmit.m_BBox;
				outDrawCall.m_TotalBBox = m_DrawPass->m_TotalBBox;
				outDrawCall.m_SlicedDC = toEmit.m_TotalParticleCount != m_DrawPass->m_TotalParticleCount;

				outDrawCall.m_Valid =	rCacheInstance->m_Cache != null &&
										rCacheInstance->m_Cache->GetRenderState(static_cast<PKSample::EShaderOptions>(outDrawCall.m_ShaderOptions)) != null;
			}

			// Try gathering mesh from renderer cache instance
			bool	success = true;

			// Push mesh buffers:
			const Utils::GpuBufferViews	&bufferView = rCacheInstance->m_AdditionalGeometry->m_PerGeometryViews[iSubMesh];

			if (!PK_VERIFY(outDrawCall.m_VertexBuffers.Reserve(vertexBuffDescView.Count())))
				return false;

			{
#define		_PUSH_VERTEX_BUFFER_AND_OFFSET(_condition, _name) \
				if (_condition) \
				{ \
					success &= outDrawCall.m_VertexBuffers.PushBack(bufferView.m_VertexBuffers[Utils::PK_GLUE(Mesh, _name)]).Valid(); \
					success &= outDrawCall.m_VertexOffsets.PushBack(0).Valid(); \
					if (bufferView.m_VertexBuffers[Utils::PK_GLUE(Mesh, _name)] == null) \
					{ \
						success = false; \
						CLog::Log(PK_ERROR, "Submesh %d doesn't have stream %s in its geometry.", iSubMesh, PK_STRINGIFY(_name)); \
					} \
				}

				for (u32 aidx = 0; aidx < vertexBuffDescView.Count(); ++aidx)
				{
					_PUSH_VERTEX_BUFFER_AND_OFFSET(vertexBuffDescView[aidx].m_Name == "Position", Positions);
					_PUSH_VERTEX_BUFFER_AND_OFFSET(vertexBuffDescView[aidx].m_Name == "Normal", Normals);
					_PUSH_VERTEX_BUFFER_AND_OFFSET(vertexBuffDescView[aidx].m_Name == "Tangent", Tangents);
					_PUSH_VERTEX_BUFFER_AND_OFFSET(vertexBuffDescView[aidx].m_Name == "Color0", Colors);
					_PUSH_VERTEX_BUFFER_AND_OFFSET(vertexBuffDescView[aidx].m_Name == "UV0", Texcoords);
					_PUSH_VERTEX_BUFFER_AND_OFFSET(vertexBuffDescView[aidx].m_Name == "Color1", Colors1);
					_PUSH_VERTEX_BUFFER_AND_OFFSET(vertexBuffDescView[aidx].m_Name == "UV1", Texcoords1);
					_PUSH_VERTEX_BUFFER_AND_OFFSET(vertexBuffDescView[aidx].m_Name == "BoneIds", BoneIds);
					_PUSH_VERTEX_BUFFER_AND_OFFSET(vertexBuffDescView[aidx].m_Name == "BoneWeights", BoneWeights);
				}
#undef		_PUSH_VERTEX_BUFFER_AND_OFFSET
			}

			if (!success)
			{
				CLog::Log(PK_ERROR, "Could not add all the mesh vertex data");
				return false;
			}

			outDrawCall.m_GPUStorageSimDataConstantSet = m_SimDataConstantSet;
			outDrawCall.m_GPUStorageOffsetsConstantSet = m_OffsetsConstantSet;
#if	(PK_HAS_PARTICLES_SELECTION != 0)
			outDrawCall.m_SelectionConstantSet = selectionConstantSet;
			outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_IsParticleSelected] = bufferIsSelected;
#endif
			const u32	currentLOD = _SubmeshIDToLOD(*refCacheInstance, lodCount, iSubMesh);

			// Draw call push constants
			if (!PK_VERIFY(outDrawCall.m_PushConstants.PushBack().Valid()))
				return false;
			u32	*drPushConstant = reinterpret_cast<u32*>(&outDrawCall.m_PushConstants.Last());
			drPushConstant[0] = dri; // Draw request ID (used to get streams offsets from stream offsets buffers)
			drPushConstant[1] =		m_HasMeshIDs ?
									m_MeshCount * drCount + m_MeshCount * dri + iSubMesh :
									(m_HasMeshLODs ? lodCount * drCount + lodCount * dri + currentLOD : drCount + dri); // Used to get the indirection offset from indirection offsets buffer

			// Set index buffer
			outDrawCall.m_IndexBuffer = bufferView.m_IndexBuffer;
			outDrawCall.m_IndexSize = bufferView.m_IndexBufferSize;

			// Set indirect buffer (will be updated by computeParticleCountPerMesh compute shader)
			outDrawCall.m_IndirectBuffer = m_IndirectDraw.m_Buffer;
			outDrawCall.m_IndirectBufferOffset = drawCallCurrentOffset;
			outDrawCall.m_EstimatedParticleCount = dr->RenderedParticleCount();

			// Fill the semantics for the debug draws:
			outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Position] = bufferView.m_VertexBuffers[Utils::MeshPositions];
			outDrawCall.m_DebugDrawGPUBufferOffsets[SRHIDrawCall::DebugDrawGPUBuffer_Position] = 0;
			outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_TransformsOffsets] = m_MatricesOffsets.m_Buffer;
			outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_IndirectionOffsets] = m_IndirectionOffsets.m_Buffer;
			outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_ColorsOffsets] = m_ColorStreamIdx.Valid() ? m_AdditionalFieldsSimStreamOffsets.m_Fields[m_ColorStreamIdx].m_Buffer.m_Buffer : null;

			drawCallCurrentOffset += sizeof(RHI::SDrawIndexedIndirectArgs);
		} // end of draw call description setup

		// Compute : Count particles per mesh
		{
			PKSample::SRHIComputeDispatchs	computeDispatch = PKSample::SRHIComputeDispatchs();

			// Constant set
			RHI::SConstantSetLayout	layout;
			PKSample::CreateComputeParticleCountPerMeshConstantSetLayout(layout, m_HasMeshIDs, m_HasMeshLODs);
			RHI::PConstantSet	computeConstantSet = m_ApiManager->CreateConstantSet(RHI::SRHIResourceInfos("Compute Mesh PCount Constant Set"), layout);
			u32					constantId = 0;
			computeConstantSet->SetConstants(particleSimInfo, constantId++);
			computeConstantSet->SetConstants(streamBufferGPU, constantId++);
			computeConstantSet->SetConstants(m_SimStreamOffsets_Enableds.m_Buffer, constantId++);
			computeConstantSet->SetConstants(m_IndirectDraw.m_Buffer, constantId++);
			if (m_HasMeshIDs)
				computeConstantSet->SetConstants(m_SimStreamOffsets_MeshIDs.m_Buffer, constantId++);
			if (m_HasMeshLODs)
				computeConstantSet->SetConstants(m_SimStreamOffsets_MeshLODs.m_Buffer, constantId++);
			computeConstantSet->SetConstants(m_LODsConstantBuffer.m_Buffer, constantId++);
			computeConstantSet->UpdateConstantValues();
			computeDispatch.m_ConstantSet = computeConstantSet;

			// Push constant
			if (!PK_VERIFY(computeDispatch.m_PushConstants.PushBack().Valid()))
				return false;
			u32	*drPushConstant = reinterpret_cast<u32*>(&computeDispatch.m_PushConstants.Last());
			drPushConstant[0] = dri;
			drPushConstant[1] = m_MeshCount;
			if (m_HasMeshLODs)
				drPushConstant[2] = lodCount;

			// State
			const PKSample::EComputeShaderType	type =	m_HasMeshIDs ?
														(m_HasMeshLODs ? ComputeType_ComputeParticleCountPerMesh_LOD_MeshAtlas : ComputeType_ComputeParticleCountPerMesh_MeshAtlas) :
														(m_HasMeshLODs ? ComputeType_ComputeParticleCountPerMesh_LOD : ComputeType_ComputeParticleCountPerMesh);
			computeDispatch.m_State = rCacheInstance->m_Cache->GetComputeState(type);

			// Dispatch args
			computeDispatch.m_ThreadGroups = CInt3((Mem::Align(drParticleCountEst, PK_RH_GPU_THREADGROUP_SIZE) / PK_RH_GPU_THREADGROUP_SIZE), 1, 1);

			computeParticleCountPerMesh.PushBack(computeDispatch);
		}

		// Compute : compute indirection
		{
			PKSample::SRHIComputeDispatchs	computeDispatch = PKSample::SRHIComputeDispatchs();

			// Constant set
			RHI::SConstantSetLayout	layout;
			PKSample::CreateComputeMeshIndirectionBufferConstantSetLayout(layout, m_HasMeshIDs, m_HasMeshLODs);
			RHI::PConstantSet	computeConstantSet = m_ApiManager->CreateConstantSet(RHI::SRHIResourceInfos("Compute Indirection Constant Set"), layout);
			u32					constantId = 0;
			computeConstantSet->SetConstants(particleSimInfo, constantId++);
			computeConstantSet->SetConstants(streamBufferGPU, constantId++);
			computeConstantSet->SetConstants(m_SimStreamOffsets_Enableds.m_Buffer, constantId++);
			computeConstantSet->SetConstants(m_Indirection.m_Buffer, constantId++);
			computeConstantSet->SetConstants(m_IndirectionOffsets.m_Buffer, constantId++);
			if (m_HasMeshIDs)
				computeConstantSet->SetConstants(m_SimStreamOffsets_MeshIDs.m_Buffer, constantId++);
			if (m_HasMeshLODs)
				computeConstantSet->SetConstants(m_SimStreamOffsets_MeshLODs.m_Buffer, constantId++);
			computeConstantSet->SetConstants(m_LODsConstantBuffer.m_Buffer, constantId++);

			computeConstantSet->UpdateConstantValues();
			computeDispatch.m_ConstantSet = computeConstantSet;

			// Push constant
			if (!PK_VERIFY(computeDispatch.m_PushConstants.PushBack().Valid()))
				return false;
			u32	*drPushConstant = reinterpret_cast<u32*>(&computeDispatch.m_PushConstants.Last());
			drPushConstant[0] = dri;
			drPushConstant[1] = m_MeshCount;
			if (m_HasMeshLODs)
				drPushConstant[2] = lodCount;

			// State
			const PKSample::EComputeShaderType	type =	m_HasMeshIDs ?
														(m_HasMeshLODs ? ComputeType_ComputeMeshIndirectionBuffer_LOD_MeshAtlas : ComputeType_ComputeMeshIndirectionBuffer_MeshAtlas) :
														(m_HasMeshLODs ? ComputeType_ComputeMeshIndirectionBuffer_LOD : ComputeType_ComputeMeshIndirectionBuffer);

			computeDispatch.m_State = rCacheInstance->m_Cache->GetComputeState(type);

			// Dispatch args
			computeDispatch.m_ThreadGroups = CInt3((Mem::Align(drParticleCountEst, PK_RH_GPU_THREADGROUP_SIZE) / PK_RH_GPU_THREADGROUP_SIZE), 1, 1);

			computeIndirection.PushBack(computeDispatch);
		}

		// Compute : build matrices from stream
		{
			PKSample::SRHIComputeDispatchs	computeDispatch = PKSample::SRHIComputeDispatchs();

			// Constant set
			RHI::SConstantSetLayout	layout;
			PKSample::CreateComputeMeshMatricesConstantSetLayout(layout);
			RHI::PConstantSet	computeConstantSet = m_ApiManager->CreateConstantSet(RHI::SRHIResourceInfos("Build Matrices Constant Set"), layout);
			u32					constantId = 0;
			computeConstantSet->SetConstants(particleSimInfo, constantId++);
			computeConstantSet->SetConstants(streamBufferGPU, constantId++);
			computeConstantSet->SetConstants(m_SimStreamOffsets_Positions.m_Buffer, constantId++);
			computeConstantSet->SetConstants(m_SimStreamOffsets_Scales.m_Buffer, constantId++);
			computeConstantSet->SetConstants(m_SimStreamOffsets_Orientations.m_Buffer, constantId++);
			computeConstantSet->SetConstants(m_MatricesOffsets.m_Buffer, constantId++);
			computeConstantSet->SetConstants(m_Matrices.m_Buffer, constantId++);
			computeConstantSet->UpdateConstantValues();
			computeDispatch.m_ConstantSet = computeConstantSet;

			// Push constant
			if (!PK_VERIFY(computeDispatch.m_PushConstants.PushBack().Valid()))
				return false;
			u32	*drPushConstant = reinterpret_cast<u32*>(&computeDispatch.m_PushConstants.Last());
			drPushConstant[0] = dri; // used to index mesh offsets

			// State
			const PKSample::EComputeShaderType	type = ComputeType_ComputeMeshMatrices;
			computeDispatch.m_State = rCacheInstance->m_Cache->GetComputeState(type);

			// Dispatch args
			computeDispatch.m_ThreadGroups = CInt3((Mem::Align(drParticleCountEst, PK_RH_GPU_THREADGROUP_SIZE) / PK_RH_GPU_THREADGROUP_SIZE), 1, 1);

			computeMeshMatrices.PushBack(computeDispatch);
		}
	} // end of iteration over draw requests

	for (PKSample::SRHIComputeDispatchs &dispatch : computeParticleCountPerMesh)
		renderContext.m_DrawOutputs.m_ComputeDispatchs.PushBack(dispatch);

	// Compute : init indirection offsets buffer (one dispatch that handles every DR and DC)
	{
		PKSample::SRHIComputeDispatchs	computeDispatch = PKSample::SRHIComputeDispatchs();
		const bool						hasLODNoAtlas = m_HasMeshLODs && !m_HasMeshIDs; // If true, indirection offsets buffer is indexed differently.

		// Constant set
		RHI::SConstantSetLayout	layout;
		PKSample::CreateInitIndirectionOffsetsBufferConstantSetLayout(layout, hasLODNoAtlas);
		RHI::PConstantSet	computeConstantSet = m_ApiManager->CreateConstantSet(RHI::SRHIResourceInfos("Compute Indirection Offsets Constant Set"), layout);
		u32					constantId = 0;
		computeConstantSet->SetConstants(m_IndirectDraw.m_Buffer, constantId++);
		computeConstantSet->SetConstants(m_IndirectionOffsets.m_Buffer, constantId++);
		if (hasLODNoAtlas)
			computeConstantSet->SetConstants(m_LODsConstantBuffer.m_Buffer, constantId++);
		computeConstantSet->UpdateConstantValues();
		computeDispatch.m_ConstantSet = computeConstantSet;

		// Push constant
		if (!PK_VERIFY(computeDispatch.m_PushConstants.PushBack().Valid()))
			return false;
		u32	*drPushConstant = reinterpret_cast<u32*>(&computeDispatch.m_PushConstants.Last());
		drPushConstant[0] = drCount * (m_HasMeshIDs ? m_MeshCount : (m_HasMeshLODs ? lodCount : 1)); // DrawCall (!= drawRequest) count

		// State
		PKSample::EComputeShaderType	type =	hasLODNoAtlas ?
													PKSample::EComputeShaderType::ComputeType_InitIndirectionOffsetsBuffer_LODNoAtlas :
													PKSample::EComputeShaderType::ComputeType_InitIndirectionOffsetsBuffer;
		computeDispatch.m_State = rCacheInstance->m_Cache->GetComputeState(type);

		// Dispatch args
		const u32	indirectionOffsetsElementCount = drCount * (m_HasMeshIDs ? m_MeshCount : (m_HasMeshLODs ? lodCount : 1));
		computeDispatch.m_ThreadGroups = CInt3(	(Mem::Align(indirectionOffsetsElementCount, PK_RH_GPU_THREADGROUP_SIZE) / PK_RH_GPU_THREADGROUP_SIZE),
												(Mem::Align(indirectionOffsetsElementCount, PK_RH_GPU_THREADGROUP_SIZE) / PK_RH_GPU_THREADGROUP_SIZE),
												1);

		renderContext.m_DrawOutputs.m_ComputeDispatchs.PushBack(computeDispatch);
	}

	for (PKSample::SRHIComputeDispatchs &dispatch : computeIndirection)
		renderContext.m_DrawOutputs.m_ComputeDispatchs.PushBack(dispatch);

	for (PKSample::SRHIComputeDispatchs &dispatch : computeMeshMatrices)
		renderContext.m_DrawOutputs.m_ComputeDispatchs.PushBack(dispatch);

	return true;
#else
	(void)ctx;
	(void)toEmit;
	return false;
#endif // (PK_PARTICLES_UPDATER_USE_GPU != 0)
}

//----------------------------------------------------------------------------

u32	CRHIRendererBatch_Mesh_GPU::_SubmeshIDToLOD(CRendererCacheInstance_UpdateThread &refCacheInstance, u32 lodCount, u32 iSubMesh)
{
	// From the flat submesh id, determine the lod level
	u32		lod = 0;
	if (m_HasMeshLODs)
	{
		u32		totalMeshCount = 0;
		for (u32 i = 0; i < lodCount; ++i, ++lod)
		{
			totalMeshCount += refCacheInstance.m_PerLODMeshCount[0];
			if (iSubMesh < totalMeshCount)
				break;
		}
	}
	return lod;
}

//----------------------------------------------------------------------------
//
// CRHIRendererBatch_Decal_GPU
//
//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Decal_GPU::AllocBuffers(PopcornFX::SRenderContext &ctx)
{
	(void)ctx;
	return false;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Decal_GPU::EmitDrawCall(SRenderContext &ctx, const SDrawCallDesc &toEmit)
{
	(void)ctx; (void)toEmit;
	//SRHIRenderContext	&renderContext = static_cast<SRHIRenderContext&>(ctx);
	return false;
}

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
