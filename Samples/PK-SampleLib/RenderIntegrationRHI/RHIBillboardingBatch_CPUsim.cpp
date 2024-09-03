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
#include "RHIBillboardingBatch_CPUsim.h"

#include "pk_render_helpers/include/render_features/rh_features_basic.h"

#include "pk_render_helpers/include/batch_jobs/rh_batch_jobs_helpers.h" // For BB-Flags
#include "pk_render_helpers/include/batches/rh_billboard_ribbon_batch_helper.h" // For BB-Flags

#include <pk_rhi/include/interfaces/IApiManager.h>
#include <pk_rhi/include/interfaces/IGpuBuffer.h>

#include "PK-SampleLib/ShaderDefinitions/SampleLibShaderDefinitions.h"
#include "PK-SampleLib/RenderIntegrationRHI/RendererCache.h"
#include "RHIRenderIntegrationConfig.h"

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

// Maximum number of draw requests batched in a single billboarding batch policy (1 draw request = 1 particle renderer being drawn)
static u32	kMaxGeomDrawRequestCount = 0x100;

// PK-SampleLib implementation of renderer batches, for the CPU simulated particles:
// - using RHI (abstraction API for graphics APIs)
// - full integration of all features (supported by the editor)

// Each renderer-bach gathers renderers (that are compatible each others), and its purpose is to allocate, map, unmap and clear vertex buffers and to issue draw calls.

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

static u32	_GetVertexBillboardShaderOptions(const Drawers::SBillboard_BillboardingRequest &bbRequest)
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

CGuid	_GetDrawDebugColorIndex(const SRHIAdditionalFieldBatch &bufferBatch, const SGeneratedInputs &toGenerate)
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

CGuid	_GetDrawDebugRangeIndex(const SRHIAdditionalFieldBatch &bufferBatch, const SGeneratedInputs &toGenerate)
{
	for (u32 j = 0; j < bufferBatch.m_Fields.Count(); ++j)
	{
		const u32	i = bufferBatch.m_Fields[j].m_AdditionalInputIndex;
		if (toGenerate.m_AdditionalGeneratedInputs[i].m_Type == PopcornFX::EBaseTypeID::BaseType_Float &&
			toGenerate.m_AdditionalGeneratedInputs[i].m_Name == BasicRendererProperties::SID_LightAttenuation_Range())
			return j;
	}
	return CGuid::INVALID;
}

//----------------------------------------------------------------------------
//
// SRHICommonCPUBillboardBuffers
//
//----------------------------------------------------------------------------

bool	SRHICommonCPUBillboardBuffers::AllocBuffers(u32 indexCount, u32 vertexCount, const SGeneratedInputs &toGenerate, RHI::PApiManager manager)
{
	const u32	viewIndependentInputs = toGenerate.m_GeneratedInputs;
	const u32	indexCountAligned = Mem::Align<0x100>(indexCount);
	const u32	vertexCountAligned = Mem::Align<0x100>(vertexCount);

	m_IndexSize = (vertexCount > 0xFFFF) ? sizeof(u32) : sizeof(u16);

	// View independent indices (ie. Billboard with PlaneAligned Billboarding mode, or Ribbons that are view independent)
	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Indices Buffer"), (viewIndependentInputs & Drawers::GenInput_Indices) != 0, manager, m_Indices, RHI::IndexBuffer, indexCountAligned * m_IndexSize, indexCount * m_IndexSize))
		return false;

	// View independent vertex-data
	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Positions Vertex Buffer"), (viewIndependentInputs & Drawers::GenInput_Position) != 0, manager, m_Positions, RHI::VertexBuffer, vertexCountAligned * sizeof(CFloat4), vertexCount * sizeof(CFloat4)))
		return false;
	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("UV0s Vertex Buffer"), (viewIndependentInputs & Drawers::GenInput_UV0) != 0, manager, m_TexCoords0, RHI::VertexBuffer, vertexCountAligned * sizeof(CFloat2), vertexCount * sizeof(CFloat2)))
		return false;
	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Normals Vertex Buffer"), (viewIndependentInputs & Drawers::GenInput_Normal) != 0, manager, m_Normals, RHI::VertexBuffer, vertexCountAligned * sizeof(CFloat4), vertexCount * sizeof(CFloat4)))
		return false;
	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Tangents Vertex Buffer"), (viewIndependentInputs & Drawers::GenInput_Tangent) != 0, manager, m_Tangents, RHI::VertexBuffer, vertexCountAligned * sizeof(CFloat4), vertexCount * sizeof(CFloat4)))
		return false;
	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("UV1s Vertex Buffer"), (viewIndependentInputs & Drawers::GenInput_UV1) != 0, manager, m_TexCoords1, RHI::VertexBuffer, vertexCountAligned * sizeof(CFloat2), vertexCount * sizeof(CFloat2)))
		return false;
	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Raw UV0 Vertex Buffer"), (viewIndependentInputs & Drawers::GenInput_RawUV0) != 0, manager, m_RawTexCoords0, RHI::VertexBuffer, vertexCountAligned * sizeof(CFloat2), vertexCount * sizeof(CFloat2)))
		return false;

	// Specific to ribbons (necessary for the CorrectDeformation feature)
	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("UVRemaps Vertex Buffer"), (viewIndependentInputs & Drawers::GenInput_UVRemap) != 0, manager, m_UVRemap, RHI::VertexBuffer, vertexCountAligned * sizeof(CFloat4), vertexCount * sizeof(CFloat4)))
		return false;
	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("UVFactors Vertex Buffer"), (viewIndependentInputs & Drawers::GenInput_UVFactors) != 0, manager, m_UVFactors, RHI::VertexBuffer, vertexCountAligned * sizeof(CFloat4), vertexCount * sizeof(CFloat4)))
		return false;

	const u32	generatedViewCount = toGenerate.m_PerViewGeneratedInputs.Count();
	if (!m_PerViewBuffers.Resize(generatedViewCount))
		return false;
	for (u32 i = 0; i < generatedViewCount; ++i)
	{
		SPerView	&viewBuffers = m_PerViewBuffers[i];

		const u32	viewGeneratedInputs = toGenerate.m_PerViewGeneratedInputs[i].m_GeneratedInputs;
		if (viewGeneratedInputs == 0)
			continue; // Nothing to generate for this view

		// View dependent indices
		if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("View Sorted Indices Buffer"), (viewGeneratedInputs & Drawers::GenInput_Indices) != 0, manager, viewBuffers.m_Indices, RHI::IndexBuffer, indexCountAligned * m_IndexSize, indexCount * m_IndexSize))
			return false;

		// View dependent vertex-data
		if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("View Positions Vertex Buffer"), (viewGeneratedInputs & Drawers::GenInput_Position) != 0, manager, viewBuffers.m_Positions, RHI::VertexBuffer, vertexCountAligned * sizeof(CFloat4), vertexCount * sizeof(CFloat4)))
			return false;
		if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("View Normals Vertex Buffer"), (viewGeneratedInputs & Drawers::GenInput_Normal) != 0, manager, viewBuffers.m_Normals, RHI::VertexBuffer, vertexCountAligned * sizeof(CFloat4), vertexCount * sizeof(CFloat4)))
			return false;
		if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("View Tangents Vertex Buffer"), (viewGeneratedInputs & Drawers::GenInput_Tangent) != 0, manager, viewBuffers.m_Tangents, RHI::VertexBuffer, vertexCountAligned * sizeof(CFloat4), vertexCount * sizeof(CFloat4)))
			return false;

		// Specific to ribbons (necessary for the CorrectDeformation feature)
		if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("View UVFactors Vertex Buffer"), (viewGeneratedInputs & Drawers::GenInput_UVFactors) != 0, manager, viewBuffers.m_UVFactors, RHI::VertexBuffer, vertexCountAligned * sizeof(CFloat4), vertexCount * sizeof(CFloat4)))
			return false;
	}

	return true;
}

//----------------------------------------------------------------------------

void	SRHICommonCPUBillboardBuffers::UnmapBuffers(RHI::PApiManager manager)
{
	m_Indices.UnmapIFN(manager);
	m_Positions.UnmapIFN(manager);
	m_Normals.UnmapIFN(manager);
	m_Tangents.UnmapIFN(manager);
	m_TexCoords0.UnmapIFN(manager);
	m_TexCoords1.UnmapIFN(manager);
	m_RawTexCoords0.UnmapIFN(manager);
	m_UVRemap.UnmapIFN(manager);
	m_UVFactors.UnmapIFN(manager);

	for (u32 i = 0; i < m_PerViewBuffers.Count(); ++i)
	{
		m_PerViewBuffers[i].m_Indices.UnmapIFN(manager);
		m_PerViewBuffers[i].m_Positions.UnmapIFN(manager);
		m_PerViewBuffers[i].m_Normals.UnmapIFN(manager);
		m_PerViewBuffers[i].m_Tangents.UnmapIFN(manager);
		m_PerViewBuffers[i].m_UVFactors.UnmapIFN(manager);
	}
}

//----------------------------------------------------------------------------
//
// SRHIAdditionalFieldBatch
//
//----------------------------------------------------------------------------

bool	SRHIAdditionalFieldBatch::AllocBuffers(u32 vCount, RHI::PApiManager manager,  RHI::EBufferType type /*= RHI::VertexBuffer*/)
{
	if (!m_MappedFields.Resize(m_Fields.Count()))
		return false;
	const u32	vCountAligned = Mem::Align<0x100>(vCount);
	for (u32 i = 0; i < m_Fields.Count(); ++i)
	{
		// Create a vertex buffer for each additional field that your engine supports:
		// Those vertex buffers will be filled with particle data from the matching streams.
		// Data is not interleaved.
		SAdditionalInputs	&decl = m_Fields[i];
		if (!PK_VERIFY(_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("AdditionalInputs Vertex Buffer"), true, manager, decl.m_Buffer, type, vCountAligned * decl.m_ByteSize, vCount * decl.m_ByteSize)))
			return false;
	}
	return true;
}

//----------------------------------------------------------------------------

bool	SRHIAdditionalFieldBatch::MapBuffers(u32 vCount, RHI::PApiManager manager)
{
	PK_ASSERT(m_Fields.Count() == m_MappedFields.Count());
	for (u32 i = 0; i < m_Fields.Count(); ++i)
	{
		if (m_Fields[i].m_Buffer.Used())
		{
			void	*mappedValue = manager->MapCpuView(m_Fields[i].m_Buffer.m_Buffer, 0, vCount * m_Fields[i].m_ByteSize);
			if (!PK_VERIFY(mappedValue != null))
				return false;
			m_MappedFields[i].m_AdditionalInputIndex = m_Fields[i].m_AdditionalInputIndex;
			m_MappedFields[i].m_Storage.m_Count = vCount;
			m_MappedFields[i].m_Storage.m_RawDataPtr = static_cast<u8*>(mappedValue);
			m_MappedFields[i].m_Storage.m_Stride = m_Fields[i].m_ByteSize;
		}
	}
	return true;
}

//----------------------------------------------------------------------------

void	SRHIAdditionalFieldBatch::UnmapBuffers(RHI::PApiManager manager)
{
	for (u32 i = 0; i < m_Fields.Count(); ++i)
	{
		m_Fields[i].m_Buffer.UnmapIFN(manager);
		m_MappedFields[i] = Drawers::SCopyFieldDesc();
	}
}

//----------------------------------------------------------------------------
//
// CRHIRendererBatch_Billboard_CPUBB
//
//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Billboard_CPUBB::Setup(const CRendererDataBase *renderer, const CParticleRenderMedium *owner, const CFrameCollector *fc, const CStringId &storageClass)
{
	if (!CRendererBatchJobs_Billboard_CPUBB::Setup(renderer, owner, fc, storageClass))
		return false;

	// Setup additional fields:
	// The additional fields are supposed to be the same for all renderers in a batch.
	// If not, then you can recompute then on the "Bind()" method.

	const auto		&toGenerate = m_DrawPass->m_ToGenerate;
	const u32		additionalFieldsCount = toGenerate.m_AdditionalGeneratedInputs.Count();

	if (!PK_VERIFY(m_AdditionalFieldsBatch.m_Fields.Reserve(additionalFieldsCount)))
		return false;

	for (u32 i = 0; i < additionalFieldsCount; ++i)
	{
		// no ignored field

		const u32	typeSize = CBaseTypeTraits::Traits(toGenerate.m_AdditionalGeneratedInputs[i].m_Type).Size;
		m_AdditionalFieldsBatch.m_Fields.PushBackUnsafe(SAdditionalInputs(typeSize, i));
	}

	m_ColorStreamIdx = _GetDrawDebugColorIndex(m_AdditionalFieldsBatch, toGenerate);

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Billboard_CPUBB::AreRenderersCompatible(const CRendererDataBase *rendererA, const CRendererDataBase *rendererB) const
{
	return	CRendererBatchJobs_Billboard_CPUBB::AreRenderersCompatible(rendererA, rendererB) &&
			AreBillboardingBatchable(rendererA->m_RendererCache, rendererB->m_RendererCache);
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Billboard_CPUBB::AllocBuffers(PopcornFX::SRenderContext &ctx)
{
	(void)ctx;

	const u32		totalVertexCount = m_BB_Billboard.TotalVertexCount();
	const u32		totalIndexCount = m_BB_Billboard.TotalIndexCount();

	if (!m_CommonBuffers.AllocBuffers(totalIndexCount, totalVertexCount, m_DrawPass->m_ToGenerate, m_ApiManager))
		return false;

	if (!m_AdditionalFieldsBatch.AllocBuffers(totalVertexCount, m_ApiManager))
		return false;

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	// Editor only: for debugging purposes, we'll remove that from samples code later
	PKSample::SRHIRenderContext	&ctxEditor = *static_cast<PKSample::SRHIRenderContext*>(&ctx);
	if (ctxEditor.Selection().HasParticlesSelected() || ctxEditor.Selection().HasRendersSelected())
	{
		if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Selection Vertex Buffer"), true, m_ApiManager, m_IsParticleSelected, RHI::VertexBuffer, Mem::Align<0x100>(totalVertexCount) * sizeof(float), totalVertexCount * sizeof(float)))
			return false;
	}
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Billboard_CPUBB::MapBuffers(SRenderContext &ctx)
{
	(void)ctx;

	const u32				totalVertexCount = m_BB_Billboard.TotalVertexCount();
	const u32				totalIndexCount = m_BB_Billboard.TotalIndexCount();
	const SGeneratedInputs	&toMap = m_DrawPass->m_ToGenerate;

	// View independent inputs:
	if (toMap.m_GeneratedInputs & Drawers::GenInput_Indices)
	{
		PK_ASSERT(m_CommonBuffers.m_Indices.Used());
		void	*mappedValue = m_ApiManager->MapCpuView(m_CommonBuffers.m_Indices.m_Buffer, 0 , totalIndexCount * m_CommonBuffers.m_IndexSize);
		if (!PK_VERIFY(mappedValue != null))
			return false;
		m_BBJobs_Billboard.m_Exec_Indices.m_IndexStream.Setup(mappedValue, totalIndexCount, m_CommonBuffers.m_IndexSize == sizeof(u32));
	}
	if (toMap.m_GeneratedInputs & Drawers::GenInput_Position)
	{
		PK_ASSERT(m_CommonBuffers.m_Positions.Used());
		void	*mappedValue = m_ApiManager->MapCpuView(m_CommonBuffers.m_Positions.m_Buffer, 0, totalVertexCount * sizeof(CFloat4));
		if (!PK_VERIFY(mappedValue != null))
			return false;
		m_BBJobs_Billboard.m_Exec_PNT.m_Positions = TStridedMemoryView<CFloat3, 0x10>(static_cast<CFloat3*>(mappedValue), totalVertexCount, 0x10);
	}
	if (toMap.m_GeneratedInputs & Drawers::GenInput_Normal)
	{
		PK_ASSERT(m_CommonBuffers.m_Normals.Used());
		void	*mappedValue = m_ApiManager->MapCpuView(m_CommonBuffers.m_Normals.m_Buffer, 0, totalVertexCount * sizeof(CFloat4));
		if (!PK_VERIFY(mappedValue != null))
			return false;
		m_BBJobs_Billboard.m_Exec_PNT.m_Normals = TStridedMemoryView<CFloat3, 0x10>(static_cast<CFloat3*>(mappedValue), totalVertexCount, 0x10);
	}
	if (toMap.m_GeneratedInputs & Drawers::GenInput_Tangent)
	{
		PK_ASSERT(m_CommonBuffers.m_Tangents.Used());
		void	*mappedValue = m_ApiManager->MapCpuView(m_CommonBuffers.m_Tangents.m_Buffer, 0, totalVertexCount * sizeof(CFloat4));
		if (!PK_VERIFY(mappedValue != null))
			return false;
		m_BBJobs_Billboard.m_Exec_PNT.m_Tangents = TStridedMemoryView<CFloat4, 0x10>(static_cast<CFloat4*>(mappedValue), totalVertexCount, 0x10);
	}
	if (toMap.m_GeneratedInputs & Drawers::GenInput_UV0)
	{
		PK_ASSERT(m_CommonBuffers.m_TexCoords0.Used());
		void	*mappedValue = m_ApiManager->MapCpuView(m_CommonBuffers.m_TexCoords0.m_Buffer, 0, totalVertexCount * sizeof(CFloat2));
		if (!PK_VERIFY(mappedValue != null))
			return false;
		m_BBJobs_Billboard.m_Exec_Texcoords.m_Texcoords = TStridedMemoryView<CFloat2>(static_cast<CFloat2*>(mappedValue), totalVertexCount);
	}
	if (toMap.m_GeneratedInputs & Drawers::GenInput_UV1)
	{
		PK_ASSERT(m_CommonBuffers.m_TexCoords1.Used());
		void	*mappedValue = m_ApiManager->MapCpuView(m_CommonBuffers.m_TexCoords1.m_Buffer, 0, totalVertexCount * sizeof(CFloat2));
		if (!PK_VERIFY(mappedValue != null))
			return false;
		m_BBJobs_Billboard.m_Exec_Texcoords.m_Texcoords2 = TStridedMemoryView<CFloat2>(static_cast<CFloat2*>(mappedValue), totalVertexCount);
	}
	if (toMap.m_GeneratedInputs & Drawers::GenInput_RawUV0)
	{
		PK_ASSERT(m_CommonBuffers.m_RawTexCoords0.Used());
		void	*mappedValue = m_ApiManager->MapCpuView(m_CommonBuffers.m_RawTexCoords0.m_Buffer, 0, totalVertexCount * sizeof(CFloat2));
		if (!PK_VERIFY(mappedValue != null))
			return false;
		m_BBJobs_Billboard.m_Exec_Texcoords.m_RawTexcoords = TStridedMemoryView<CFloat2>(static_cast<CFloat2*>(mappedValue), totalVertexCount);
	}

	PK_ASSERT((toMap.m_GeneratedInputs & Drawers::GenInput_UVRemap) == 0);
	PK_ASSERT((toMap.m_GeneratedInputs & Drawers::GenInput_UVFactors) == 0);

	// View dependent inputs:
	PK_ASSERT(m_CommonBuffers.m_PerViewBuffers.Count() == m_BBJobs_Billboard.m_PerView.Count());
	for (u32 i = 0; i < m_CommonBuffers.m_PerViewBuffers.Count(); ++i)
	{
		const u32	viewGeneratedInputs = toMap.m_PerViewGeneratedInputs[i].m_GeneratedInputs;
		if (viewGeneratedInputs == 0)
			continue;

		if (viewGeneratedInputs & Drawers::GenInput_Indices)
		{
			PK_ASSERT(m_CommonBuffers.m_PerViewBuffers[i].m_Indices.Used());
			void	*mappedValue = m_ApiManager->MapCpuView(m_CommonBuffers.m_PerViewBuffers[i].m_Indices.m_Buffer, 0, totalIndexCount * m_CommonBuffers.m_IndexSize);
			if (!PK_VERIFY(mappedValue != null))
				return false;
			m_BBJobs_Billboard.m_PerView[i].m_Exec_Indices.m_IndexStream.Setup(mappedValue, totalIndexCount, m_CommonBuffers.m_IndexSize == sizeof(u32));
		}
		if (viewGeneratedInputs & Drawers::GenInput_Position)
		{
			PK_ASSERT(m_CommonBuffers.m_PerViewBuffers[i].m_Positions.Used());
			void	*mappedValue = m_ApiManager->MapCpuView(m_CommonBuffers.m_PerViewBuffers[i].m_Positions.m_Buffer, 0, totalVertexCount * sizeof(CFloat4));
			if (!PK_VERIFY(mappedValue != null))
				return false;
			m_BBJobs_Billboard.m_PerView[i].m_Exec_PNT.m_Positions = TStridedMemoryView<CFloat3, 0x10>(static_cast<CFloat3*>(mappedValue), totalVertexCount, 0x10);
		}
		if (viewGeneratedInputs & Drawers::GenInput_Tangent)
		{
			PK_ASSERT(m_CommonBuffers.m_PerViewBuffers[i].m_Tangents.Used());
			void	*mappedValue = m_ApiManager->MapCpuView(m_CommonBuffers.m_PerViewBuffers[i].m_Tangents.m_Buffer, 0, totalVertexCount * sizeof(CFloat4));
			if (!PK_VERIFY(mappedValue != null))
				return false;
			m_BBJobs_Billboard.m_PerView[i].m_Exec_PNT.m_Tangents = TStridedMemoryView<CFloat4, 0x10>(static_cast<CFloat4*>(mappedValue), totalVertexCount, 0x10);
		}
		if (viewGeneratedInputs & Drawers::GenInput_Normal)
		{
			PK_ASSERT(m_CommonBuffers.m_PerViewBuffers[i].m_Normals.Used());
			void	*mappedValue = m_ApiManager->MapCpuView(m_CommonBuffers.m_PerViewBuffers[i].m_Normals.m_Buffer, 0, totalVertexCount * sizeof(CFloat4));
			if (!PK_VERIFY(mappedValue != null))
				return false;
			m_BBJobs_Billboard.m_PerView[i].m_Exec_PNT.m_Normals = TStridedMemoryView<CFloat3, 0x10>(static_cast<CFloat3*>(mappedValue), totalVertexCount, 0x10);
		}
		PK_ASSERT((viewGeneratedInputs & Drawers::GenInput_UVFactors) == 0);
	}

	// Additional inputs:
	if (!m_AdditionalFieldsBatch.MapBuffers(totalVertexCount, m_ApiManager))
		return false;
	m_BBJobs_Billboard.m_Exec_CopyField.m_FieldsToCopy = m_AdditionalFieldsBatch.m_MappedFields;

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	// Editor only: for debugging purposes, we'll remove that from samples code later
	PKSample::SRHIRenderContext	&ctxEditor = *static_cast<PKSample::SRHIRenderContext*>(&ctx);
	if (ctxEditor.Selection().HasParticlesSelected() || ctxEditor.Selection().HasRendersSelected())
	{
		m_BillboardCustomParticleSelectTask.Clear();
		PK_ASSERT(m_IsParticleSelected.Used());
		void	*mappedValue = m_ApiManager->MapCpuView(m_IsParticleSelected.m_Buffer, 0, sizeof(float) * totalVertexCount);
		if (!PK_VERIFY(mappedValue != null))
			return false;
		m_BillboardCustomParticleSelectTask.m_DstSelectedParticles = TStridedMemoryView<float>(static_cast<float*>(mappedValue), totalVertexCount, sizeof(float));
		m_BillboardCustomParticleSelectTask.m_SrcParticleSelected = ctxEditor.Selection();
	}
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Billboard_CPUBB::LaunchCustomTasks(SRenderContext &ctx)
{
	(void)ctx;
#if	(PK_HAS_PARTICLES_SELECTION != 0)
	// Editor only: for debugging purposes, we'll remove that from samples code later
	PKSample::SRHIRenderContext	&ctxEditor = *static_cast<PKSample::SRHIRenderContext*>(&ctx);
	if (ctxEditor.Selection().HasParticlesSelected() || ctxEditor.Selection().HasRendersSelected())
	{
		m_BB_Billboard.AddExecPage(&m_BillboardCustomParticleSelectTask);
	}
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)
	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Billboard_CPUBB::UnmapBuffers(SRenderContext &ctx)
{
	(void)ctx;

	m_CommonBuffers.UnmapBuffers(m_ApiManager);

	m_AdditionalFieldsBatch.UnmapBuffers(m_ApiManager);

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	m_IsParticleSelected.UnmapIFN(m_ApiManager);
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Billboard_CPUBB::EmitDrawCall(SRenderContext &ctx, const SDrawCallDesc &toEmit)
{
	SRHIRenderContext	&renderContext = static_cast<SRHIRenderContext&>(ctx);

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

	const bool	hasAtlas = rCacheInstance->m_HasAtlas;
	const bool	hasRawUV0 = rCacheInstance->m_HasRawUV0;

	if (!PK_VERIFY(renderContext.m_DrawOutputs.m_DrawCalls.PushBack().Valid()))
	{
		CLog::Log(PK_ERROR, "Failed to create a draw-call");
		return false;
	}
	SRHIDrawCall	&outDrawCall = renderContext.m_DrawOutputs.m_DrawCalls.Last();

	outDrawCall.m_Batch = this;
	outDrawCall.m_RendererCacheInstance = renderCacheInstance;
	outDrawCall.m_Type = SRHIDrawCall::DrawCall_Regular;
	outDrawCall.m_ShaderOptions = PKSample::Option_VertexPassThrough;
	outDrawCall.m_RendererType = Renderer_Billboard;

	outDrawCall.m_IndexOffset = toEmit.m_IndexOffset;
	outDrawCall.m_VertexCount = toEmit.m_TotalVertexCount;
	outDrawCall.m_IndexCount = toEmit.m_TotalIndexCount;
	outDrawCall.m_IndexSize = (m_CommonBuffers.m_IndexSize == sizeof(u32)) ? RHI::IndexBuffer32Bit : RHI::IndexBuffer16Bit;

	// Some meta-data (the Editor uses them)
	{
		outDrawCall.m_BBox = toEmit.m_BBox;
		outDrawCall.m_TotalBBox = m_DrawPass->m_TotalBBox;
		outDrawCall.m_SlicedDC = toEmit.m_TotalParticleCount != m_DrawPass->m_TotalParticleCount;
	}

	if (!m_CommonBuffers.m_PerViewBuffers.Empty() && m_CommonBuffers.m_PerViewBuffers[0].m_Indices.Used())
		outDrawCall.m_IndexBuffer = m_CommonBuffers.m_PerViewBuffers[0].m_Indices.m_Buffer;
	else
		outDrawCall.m_IndexBuffer = m_CommonBuffers.m_Indices.m_Buffer;
	PK_ASSERT(outDrawCall.m_IndexBuffer != null);

	if (!m_CommonBuffers.m_PerViewBuffers.Empty() && m_CommonBuffers.m_PerViewBuffers[0].m_Positions.Used())
	{
		// View dependent buffers (Right now this code only takes in account first view)
		// If the first view contains an allocated Positions buffer, it means we have view dependent geometry.
		if (!PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(m_CommonBuffers.m_PerViewBuffers[0].m_Positions.m_Buffer).Valid()))
			return false;
		if (m_CommonBuffers.m_PerViewBuffers[0].m_Normals.Used() && !PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(m_CommonBuffers.m_PerViewBuffers[0].m_Normals.m_Buffer).Valid()))
			return false;
		if (m_CommonBuffers.m_PerViewBuffers[0].m_Tangents.Used() && !PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(m_CommonBuffers.m_PerViewBuffers[0].m_Tangents.m_Buffer).Valid()))
			return false;
	}
	else
	{
		if (m_CommonBuffers.m_Positions.Used() && !PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(m_CommonBuffers.m_Positions.m_Buffer).Valid()))
			return false;
		if (m_CommonBuffers.m_Normals.Used() && !PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(m_CommonBuffers.m_Normals.m_Buffer).Valid()))
			return false;
		if (m_CommonBuffers.m_Tangents.Used() && !PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(m_CommonBuffers.m_Tangents.m_Buffer).Valid()))
			return false;
	}

	if (m_CommonBuffers.m_TexCoords0.Used())
	{
		if (!PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(m_CommonBuffers.m_TexCoords0.m_Buffer).Valid()))
			return false;

		// Atlas renderer feature enabled: None/Linear atlas blending share the same vertex declaration, so we just push empty buffers when blending is disabled
		if (hasAtlas || m_CommonBuffers.m_TexCoords1.Used())
		{
			// If we have invalid m_TexCoords1/m_AtlasIDs, bind a dummy vertex buffer, here m_TexCoords0
			if (!PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(m_CommonBuffers.m_TexCoords1.Used() ? m_CommonBuffers.m_TexCoords1.m_Buffer : m_CommonBuffers.m_TexCoords0.m_Buffer).Valid()))
				return false;
		}
		if (hasRawUV0)
		{
			if (!PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(m_CommonBuffers.m_RawTexCoords0.Used() ? m_CommonBuffers.m_RawTexCoords0.m_Buffer : m_CommonBuffers.m_TexCoords0.m_Buffer).Valid()))
				return false;
		}
	}

	if (m_CommonBuffers.m_UVRemap.Used() && !PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(m_CommonBuffers.m_UVRemap.m_Buffer).Valid()))
		return false;

	if (!m_CommonBuffers.m_PerViewBuffers.Empty() && m_CommonBuffers.m_PerViewBuffers[0].m_Positions.Used())
	{
		// Same check as above, but UVFactors are pushed in last to match vertex declaration expected by RHI Render states
		if (m_CommonBuffers.m_PerViewBuffers[0].m_UVFactors.Used() && !PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(m_CommonBuffers.m_PerViewBuffers[0].m_UVFactors.m_Buffer).Valid()))
			return false;
	}
	else
	{
		if (m_CommonBuffers.m_UVFactors.Used() && !PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(m_CommonBuffers.m_UVFactors.m_Buffer).Valid()))
			return false;
	}

	outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Position] = outDrawCall.m_VertexBuffers.First();
	if (m_ColorStreamIdx.Valid())
		outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Color] =  m_AdditionalFieldsBatch.m_Fields[m_ColorStreamIdx].m_Buffer.m_Buffer;

	// Push additional inputs vertex buffers:
	for (const auto &additionalField : m_AdditionalFieldsBatch.m_Fields)
	{
		PK_ASSERT(additionalField.m_Buffer.Used());
		if (!PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(additionalField.m_Buffer.m_Buffer).Valid()))
			return false;
	}

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	// Editor only: for debugging purposes, we'll remove that from samples code later
	PKSample::SRHIRenderContext	&ctxEditor = *static_cast<PKSample::SRHIRenderContext*>(&ctx);
	if ((ctxEditor.Selection().HasParticlesSelected() || ctxEditor.Selection().HasRendersSelected()) && m_IsParticleSelected.Used())
	{
		outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_IsParticleSelected] = m_IsParticleSelected.m_Buffer;
	}
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)

	return true;
}

//----------------------------------------------------------------------------
//
// CRHIRendererBatch_Billboard_GeomBB
//
//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Billboard_GeomBB::Setup(const CRendererDataBase *renderer, const CParticleRenderMedium *owner, const CFrameCollector *fc, const CStringId &storageClass)
{
	if (!CRendererBatchJobs_Billboard_GPUBB::Setup(renderer, owner, fc, storageClass))
		return false;

	// Setup additional fields:
	// The additional fields are supposed to be the same for all renderers in a batch.
	// If not, then you can recompute then on the "Bind()" method.

	const auto		&toGenerate = m_DrawPass->m_ToGenerate;
	const u32		additionalFieldsCount = toGenerate.m_AdditionalGeneratedInputs.Count();

	if (!PK_VERIFY(m_AdditionalFieldsBatch.m_Fields.Reserve(additionalFieldsCount)))
		return false;

	for (u32 i = 0; i < additionalFieldsCount; ++i)
	{
		// no ignored field

		const u32	typeSize = CBaseTypeTraits::Traits(toGenerate.m_AdditionalGeneratedInputs[i].m_Type).Size;
		m_AdditionalFieldsBatch.m_Fields.PushBackUnsafe(SAdditionalInputs(typeSize, i));
	}

	m_ColorStreamIdx = _GetDrawDebugColorIndex(m_AdditionalFieldsBatch, toGenerate);

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Billboard_GeomBB::AreRenderersCompatible(const CRendererDataBase *rendererA, const CRendererDataBase *rendererB) const
{
	return	CRendererBatchJobs_Billboard_GPUBB::AreRenderersCompatible(rendererA, rendererB) &&
			AreBillboardingBatchable(rendererA->m_RendererCache, rendererB->m_RendererCache);
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Billboard_GeomBB::AllocBuffers(SRenderContext &ctx)
{
	(void)ctx;

	const u32		totalParticleCount = m_DrawPass->m_TotalParticleCount;
	const u32		totalParticleCountAligned = Mem::Align<0x100>(totalParticleCount);
	const auto		&toGenerate = m_DrawPass->m_ToGenerate;

	// Particle positions stream
	PK_ASSERT((toGenerate.m_GeneratedInputs & Drawers::GenInput_ParticlePosition) != 0);
	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Positions Vertex Buffer"), true, m_ApiManager, m_GeomPositions, RHI::VertexBuffer, totalParticleCountAligned * sizeof(CFloat4), totalParticleCount * sizeof(CFloat4)))
		return false;

	// Constant buffer filled by CPU task, will contain simple description of draw request
	// We do this so we can batch various draw requests (renderers from various mediums) in a single "draw call"
	// This constant buffer will contain flags for each draw request
	// Each particle position will contain its associated draw request ID in position's W component (See sample geometry billboard shader for more detail)
	PK_STATIC_ASSERT(sizeof(Drawers::SBillboardDrawRequest) == sizeof(CFloat4));
	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Billboard DrawRequests Buffer"), true, m_ApiManager, m_GeomConstants, RHI::ConstantBuffer, kMaxGeomDrawRequestCount * sizeof(Drawers::SBillboardDrawRequest), kMaxGeomDrawRequestCount * sizeof(Drawers::SBillboardDrawRequest)))
		return false;

	// Can be either one
	PK_ASSERT((toGenerate.m_GeneratedInputs & Drawers::GenInput_ParticleSize) == 0 || (toGenerate.m_GeneratedInputs & Drawers::GenInput_ParticleSize2) == 0);
	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Sizes Vertex Buffer"), (toGenerate.m_GeneratedInputs & Drawers::GenInput_ParticleSize) != 0, m_ApiManager, m_GeomSizes, RHI::VertexBuffer, totalParticleCountAligned * sizeof(float), totalParticleCount * sizeof(float)))
		return false;
	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Size2s Vertex Buffer"), (toGenerate.m_GeneratedInputs & Drawers::GenInput_ParticleSize2) != 0, m_ApiManager, m_GeomSizes2, RHI::VertexBuffer, totalParticleCountAligned * sizeof(CFloat2), totalParticleCount * sizeof(CFloat2)))
		return false;

	// Rotation particle stream
	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Rotations Vertex Buffer"), (toGenerate.m_GeneratedInputs & Drawers::GenInput_ParticleRotation) != 0, m_ApiManager, m_GeomRotations, RHI::VertexBuffer, totalParticleCountAligned * sizeof(float), totalParticleCount * sizeof(float)))
		return false;
	// First billboarding axis (necessary for AxisAligned billboarding modes)
	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Axis0s Vertex Buffer"), (toGenerate.m_GeneratedInputs & Drawers::GenInput_ParticleAxis0) != 0, m_ApiManager, m_GeomAxis0, RHI::VertexBuffer, totalParticleCountAligned * sizeof(CFloat3), totalParticleCount * sizeof(CFloat3)))
		return false;
	// Second billboarding axis (necessary for PlaneAligned)
	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Axis1s Vertex Buffer"), (toGenerate.m_GeneratedInputs & Drawers::GenInput_ParticleAxis1) != 0, m_ApiManager, m_GeomAxis1, RHI::VertexBuffer, totalParticleCountAligned * sizeof(CFloat3), totalParticleCount * sizeof(CFloat3)))
		return false;

	// Indices:
	m_IndexSize = (totalParticleCount > 0xFFFF) ? sizeof(u32) : sizeof(u16);
	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Indices Buffer"), (toGenerate.m_GeneratedInputs & Drawers::GenInput_Indices) != 0, m_ApiManager, m_Indices, RHI::IndexBuffer, totalParticleCountAligned * m_IndexSize, totalParticleCount * m_IndexSize))
		return false;
	const u32	generatedViewCount = toGenerate.m_PerViewGeneratedInputs.Count();
	if (!m_PerViewIndicesBuffers.Resize(generatedViewCount))
		return false;
	for (u32 i = 0; i < generatedViewCount; ++i)
	{
		if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("View Sorted Indices Buffer"), (toGenerate.m_PerViewGeneratedInputs[i].m_GeneratedInputs & Drawers::GenInput_Indices) != 0, m_ApiManager, m_PerViewIndicesBuffers[i], RHI::IndexBuffer, totalParticleCountAligned * m_IndexSize, totalParticleCount * m_IndexSize))
			return false;
	}

	// Additional fields:
	if (!m_AdditionalFieldsBatch.AllocBuffers(totalParticleCount, m_ApiManager))
		return false;

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	// Editor only: for debugging purposes, we'll remove that from samples code later
	PKSample::SRHIRenderContext	&ctxEditor = *static_cast<PKSample::SRHIRenderContext*>(&ctx);
	if (ctxEditor.Selection().HasParticlesSelected() || ctxEditor.Selection().HasRendersSelected())
	{
		if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Selection Vertex Buffer"), true, m_ApiManager, m_IsParticleSelected, RHI::VertexBuffer, totalParticleCountAligned * sizeof(float), totalParticleCount * sizeof(float)))
			return false;
	}
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Billboard_GeomBB::MapBuffers(SRenderContext &ctx)
{
	(void)ctx;

	const u32				totalParticleCount = m_DrawPass->m_TotalParticleCount;
	const SGeneratedInputs	&toMap = m_DrawPass->m_ToGenerate;

	{
		PK_ASSERT(m_GeomPositions.Used());
		void	*mappedValue = m_ApiManager->MapCpuView(m_GeomPositions.m_Buffer, 0, totalParticleCount * sizeof(CFloat4));
		if (!PK_VERIFY(mappedValue != null))
			return false;
		m_BBJobs_Billboard.m_Exec_CopyBillboardingStreams.m_PositionsDrIds = TMemoryView<Drawers::SVertex_PositionDrId>(static_cast<Drawers::SVertex_PositionDrId*>(mappedValue), totalParticleCount);

		PK_ASSERT(m_GeomConstants.Used());
		void	*mappedConstants = m_ApiManager->MapCpuView(m_GeomConstants.m_Buffer, 0, kMaxGeomDrawRequestCount * sizeof(Drawers::SBillboardDrawRequest));
		if (!PK_VERIFY(mappedConstants != null))
			return false;
		m_BBJobs_Billboard.m_Exec_GeomBillboardDrawRequests.m_GeomDrawRequests = TMemoryView<Drawers::SBillboardDrawRequest>(static_cast<Drawers::SBillboardDrawRequest*>(mappedConstants), kMaxGeomDrawRequestCount);
	}
	if (toMap.m_GeneratedInputs & Drawers::GenInput_ParticleSize)
	{
		PK_ASSERT(m_GeomSizes.Used());
		void	*mappedValue = m_ApiManager->MapCpuView(m_GeomSizes.m_Buffer, 0, totalParticleCount * sizeof(float));
		if (!PK_VERIFY(mappedValue != null))
			return false;
		m_BBJobs_Billboard.m_Exec_CopyBillboardingStreams.m_Sizes = TMemoryView<float>(static_cast<float*>(mappedValue), totalParticleCount);
	}
	else if (toMap.m_GeneratedInputs & Drawers::GenInput_ParticleSize2)
	{
		PK_ASSERT(m_GeomSizes2.Used());
		void	*mappedValue = m_ApiManager->MapCpuView(m_GeomSizes2.m_Buffer, 0, totalParticleCount * sizeof(CFloat2));
		if (!PK_VERIFY(mappedValue != null))
			return false;
		m_BBJobs_Billboard.m_Exec_CopyBillboardingStreams.m_Sizes2 = TMemoryView<CFloat2>(static_cast<CFloat2*>(mappedValue), totalParticleCount);
	}
	if (toMap.m_GeneratedInputs & Drawers::GenInput_ParticleRotation)
	{
		PK_ASSERT(m_GeomRotations.Used());
		void	*mappedValue = m_ApiManager->MapCpuView(m_GeomRotations.m_Buffer, 0, totalParticleCount * sizeof(float));
		if (!PK_VERIFY(mappedValue != null))
			return false;
		m_BBJobs_Billboard.m_Exec_CopyBillboardingStreams.m_Rotations = TMemoryView<float>(static_cast<float*>(mappedValue), totalParticleCount);
	}
	if (toMap.m_GeneratedInputs & Drawers::GenInput_ParticleAxis0)
	{
		PK_ASSERT(m_GeomAxis0.Used());
		void	*mappedValue = m_ApiManager->MapCpuView(m_GeomAxis0.m_Buffer, 0, totalParticleCount * sizeof(CFloat3));
		if (!PK_VERIFY(mappedValue != null))
			return false;
		m_BBJobs_Billboard.m_Exec_CopyBillboardingStreams.m_Axis0 = TMemoryView<CFloat3>(static_cast<CFloat3*>(mappedValue), totalParticleCount);
	}
	if (toMap.m_GeneratedInputs & Drawers::GenInput_ParticleAxis1)
	{
		PK_ASSERT(m_GeomAxis1.Used());
		void	*mappedValue = m_ApiManager->MapCpuView(m_GeomAxis1.m_Buffer, 0, totalParticleCount * sizeof(CFloat3));
		if (!PK_VERIFY(mappedValue != null))
			return false;
		m_BBJobs_Billboard.m_Exec_CopyBillboardingStreams.m_Axis1 = TMemoryView<CFloat3>(static_cast<CFloat3*>(mappedValue), totalParticleCount);
	}

	// indices:
	if (toMap.m_GeneratedInputs & Drawers::GenInput_Indices)
	{
		PK_ASSERT(m_Indices.Used());
		void	*mappedValue = m_ApiManager->MapCpuView(m_Indices.m_Buffer, 0, m_IndexSize * totalParticleCount);
		if (!PK_VERIFY(mappedValue != null))
			return false;
		m_BBJobs_Billboard.m_Exec_Indices.m_IndexStream.Setup(mappedValue, totalParticleCount, m_IndexSize == sizeof(u32));
	}
	PK_ASSERT(m_PerViewIndicesBuffers.Count() == m_BBJobs_Billboard.m_PerView.Count());
	for (u32 i = 0; i < m_PerViewIndicesBuffers.Count(); ++i)
	{
		const u32	viewGeneratedInputs = toMap.m_PerViewGeneratedInputs[i].m_GeneratedInputs;
		if (viewGeneratedInputs & Drawers::GenInput_Indices)
		{
			PK_ASSERT(m_PerViewIndicesBuffers[i].Used());
			void	*mappedValue = m_ApiManager->MapCpuView(m_PerViewIndicesBuffers[i].m_Buffer, 0, totalParticleCount * m_IndexSize);
			if (!PK_VERIFY(mappedValue != null))
				return false;
			m_BBJobs_Billboard.m_PerView[i].m_Exec_Indices.m_IndexStream.Setup(mappedValue, totalParticleCount, m_IndexSize == sizeof(u32));
		}
	}

	// Additional inputs:
	if (!m_AdditionalFieldsBatch.MapBuffers(totalParticleCount, m_ApiManager))
		return false;
	m_BBJobs_Billboard.m_Exec_CopyAdditionalFields.m_FieldsToCopy = m_AdditionalFieldsBatch.m_MappedFields;

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	// Editor only: for debugging purposes, we'll remove that from samples code later
	PKSample::SRHIRenderContext	&ctxEditor = *static_cast<PKSample::SRHIRenderContext*>(&ctx);
	if (ctxEditor.Selection().HasParticlesSelected() || ctxEditor.Selection().HasRendersSelected())
	{
		m_CopyCustomParticleSelectTask.Clear();
		PK_ASSERT(m_IsParticleSelected.Used());
		void	*mappedValue = m_ApiManager->MapCpuView(m_IsParticleSelected.m_Buffer, 0, sizeof(float) * totalParticleCount);
		if (!PK_VERIFY(mappedValue != null))
			return false;
		m_CopyCustomParticleSelectTask.m_DstSelectedParticles = TStridedMemoryView<float>(static_cast<float*>(mappedValue), totalParticleCount, sizeof(float));
		m_CopyCustomParticleSelectTask.m_SrcParticleSelected = ctxEditor.Selection();
	}
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Billboard_GeomBB::LaunchCustomTasks(SRenderContext &ctx)
{
	(void)ctx;
#if	(PK_HAS_PARTICLES_SELECTION != 0)
	// Editor only: for debugging purposes, we'll remove that from samples code later
	PKSample::SRHIRenderContext	&ctxEditor = *static_cast<PKSample::SRHIRenderContext*>(&ctx);
	if (ctxEditor.Selection().HasParticlesSelected() || ctxEditor.Selection().HasRendersSelected())
	{
		m_BB_Billoard.AddExecAsyncPage(&m_CopyCustomParticleSelectTask);
	}
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)
	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Billboard_GeomBB::UnmapBuffers(SRenderContext &ctx)
{
	(void)ctx;

	m_GeomPositions.UnmapIFN(m_ApiManager);
	m_GeomConstants.UnmapIFN(m_ApiManager);
	m_GeomSizes.UnmapIFN(m_ApiManager);
	m_GeomSizes2.UnmapIFN(m_ApiManager);
	m_GeomRotations.UnmapIFN(m_ApiManager);
	m_GeomAxis0.UnmapIFN(m_ApiManager);
	m_GeomAxis1.UnmapIFN(m_ApiManager);

	m_Indices.UnmapIFN(m_ApiManager);
	for (auto &buf : m_PerViewIndicesBuffers)
		buf.UnmapIFN(m_ApiManager);

	m_AdditionalFieldsBatch.UnmapBuffers(m_ApiManager);

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	m_IsParticleSelected.UnmapIFN(m_ApiManager);
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Billboard_GeomBB::EmitDrawCall(SRenderContext &ctx, const SDrawCallDesc &toEmit)
{
	SRHIRenderContext	&renderContext = static_cast<SRHIRenderContext&>(ctx);

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

	const Drawers::SBillboard_DrawRequest	*dr = static_cast<const Drawers::SBillboard_DrawRequest*>(toEmit.m_DrawRequests.First());

	if (!PK_VERIFY(renderContext.m_DrawOutputs.m_DrawCalls.PushBack().Valid()))
	{
		CLog::Log(PK_ERROR, "Failed to create a draw-call");
		return false;
	}
	SRHIDrawCall	&outDrawCall = renderContext.m_DrawOutputs.m_DrawCalls.Last();

	outDrawCall.m_Batch = this;
	outDrawCall.m_RendererCacheInstance = renderCacheInstance;
	outDrawCall.m_Type = SRHIDrawCall::DrawCall_Regular;
	outDrawCall.m_ShaderOptions = PKSample::Option_VertexPassThrough | _GetGeomBillboardShaderOptions(dr->m_BB);
	outDrawCall.m_RendererType = Renderer_Billboard;

	outDrawCall.m_IndexOffset = toEmit.m_IndexOffset;
	outDrawCall.m_VertexCount = toEmit.m_TotalVertexCount;
	outDrawCall.m_IndexCount = toEmit.m_TotalIndexCount;
	outDrawCall.m_IndexSize = (m_IndexSize == sizeof(u32)) ? RHI::IndexBuffer32Bit : RHI::IndexBuffer16Bit;

	// Some meta-data (the Editor uses them)
	{
		outDrawCall.m_BBox = toEmit.m_BBox;
		outDrawCall.m_TotalBBox = m_DrawPass->m_TotalBBox;
		outDrawCall.m_SlicedDC = toEmit.m_TotalParticleCount != m_DrawPass->m_TotalParticleCount;
	}

	// Indices are mandatory, here we check if we have view independent indices or not:
	if (!m_PerViewIndicesBuffers.Empty() && m_PerViewIndicesBuffers[0].Used())
		outDrawCall.m_IndexBuffer = m_PerViewIndicesBuffers[0].m_Buffer;
	else
		outDrawCall.m_IndexBuffer = m_Indices.m_Buffer;
	PK_ASSERT(outDrawCall.m_IndexBuffer != null);

	// Vertex buffers below have been filled with particle sim data (via CCopyStream_CPU tasks)
	// So the billboarding can be done in a geometry shader
	if (m_GeomPositions.Used())
	{
		if (!PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(m_GeomPositions.m_Buffer).Valid()))
			return false;
		outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Position] = m_GeomPositions.m_Buffer;
		PK_ASSERT(m_GeomConstants.Used());
	}
	if (m_GeomSizes.Used())
	{
		if (!PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(m_GeomSizes.m_Buffer).Valid()))
			return false;
		outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Size] = m_GeomSizes.m_Buffer;
	}
	else if (m_GeomSizes2.Used())
	{
		if (!PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(m_GeomSizes2.m_Buffer).Valid()))
			return false;
		outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Size] = m_GeomSizes2.m_Buffer;
	}
	if (m_GeomRotations.Used())
	{
		if (!PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(m_GeomRotations.m_Buffer).Valid()))
			return false;
		outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Rotation] = m_GeomRotations.m_Buffer;
	}
	if (m_GeomAxis0.Used())
	{
		if (!PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(m_GeomAxis0.m_Buffer).Valid()))
			return false;
		outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Axis0] = m_GeomAxis0.m_Buffer;
	}
	if (m_GeomAxis1.Used())
	{
		if (!PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(m_GeomAxis1.m_Buffer).Valid()))
			return false;
		outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Axis1] = m_GeomAxis1.m_Buffer;
	}

	if (m_GeomConstants.Used())
		outDrawCall.m_UBSemanticsPtr[SRHIDrawCall::UBSemantic_GPUBillboard] = m_GeomConstants.m_Buffer;

	if (m_ColorStreamIdx.Valid())
		outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Color] =  m_AdditionalFieldsBatch.m_Fields[m_ColorStreamIdx].m_Buffer.m_Buffer;


	// Push additional inputs vertex buffers:
	for (const auto &additionalField : m_AdditionalFieldsBatch.m_Fields)
	{
		PK_ASSERT(additionalField.m_Buffer.Used());
		if (!PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(additionalField.m_Buffer.m_Buffer).Valid()))
			return false;
	}

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	// Editor only: for debugging purposes, we'll remove that from samples code later
	PKSample::SRHIRenderContext	&ctxEditor = *static_cast<PKSample::SRHIRenderContext*>(&ctx);
	if ((ctxEditor.Selection().HasParticlesSelected() || ctxEditor.Selection().HasRendersSelected()) && m_IsParticleSelected.Used())
	{
		outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_IsParticleSelected] = m_IsParticleSelected.m_Buffer;
	}
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)

	return true;
}

//----------------------------------------------------------------------------
//
// CRHIRendererBatch_Billboard_VertexBB
//
//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Billboard_VertexBB::Setup(const CRendererDataBase *renderer, const CParticleRenderMedium *owner, const CFrameCollector *fc, const CStringId &storageClass)
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

	if (!PK_VERIFY(m_AdditionalFieldsBatch.m_Fields.Reserve(additionalFieldsCount)))
		return false;

	for (u32 i = 0; i < additionalFieldsCount; ++i)
	{
		const u32	typeSize = CBaseTypeTraits::Traits(toGenerate.m_AdditionalGeneratedInputs[i].m_Type).Size;
		m_AdditionalFieldsBatch.m_Fields.PushBackUnsafe(SAdditionalInputs(typeSize, i));
	}

	m_ColorStreamIdx = _GetDrawDebugColorIndex(m_AdditionalFieldsBatch, toGenerate);

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Billboard_VertexBB::AreRenderersCompatible(const CRendererDataBase *rendererA, const CRendererDataBase *rendererB) const
{
	return CRendererBatchJobs_Billboard_GPUBB::AreRenderersCompatible(rendererA, rendererB) && AreBillboardingBatchable(rendererA->m_RendererCache, rendererB->m_RendererCache);
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Billboard_VertexBB::AllocBuffers(SRenderContext &ctx)
{
	(void)ctx;

	const u32		totalParticleCount = m_BB_Billoard.TotalParticleCount();
	const u32		totalParticleCountAligned = Mem::Align<0x100>(totalParticleCount);
	const auto		&toGenerate = m_DrawPass->m_ToGenerate;

	if (!m_Initialized)
		m_Initialized = _InitStaticBuffers();
	if (!m_Initialized)
		return false;

	// Indices (for sorting)
	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Indices Buffer"), (toGenerate.m_GeneratedInputs & Drawers::GenInput_Indices) != 0, m_ApiManager, m_Indices, RHI::RawBuffer, totalParticleCountAligned * sizeof(u32), totalParticleCount * sizeof(u32)))
		return false;
	const u32	generatedViewCount = toGenerate.m_PerViewGeneratedInputs.Count();
	if (!m_PerViewIndicesBuffers.Resize(generatedViewCount))
		return false;
	for (u32 i = 0; i < generatedViewCount; ++i)
	{
		if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("View Sorted Indices Buffer"), (toGenerate.m_PerViewGeneratedInputs[i].m_GeneratedInputs & Drawers::GenInput_Indices) != 0, m_ApiManager, m_PerViewIndicesBuffers[i], RHI::RawBuffer, totalParticleCountAligned * sizeof(u32), totalParticleCount * sizeof(u32)))
			return false;
	}

	// Position particle stream
	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Positions Buffer"), true, m_ApiManager, m_Positions, RHI::RawBuffer, totalParticleCountAligned * sizeof(CFloat4), totalParticleCount * sizeof(CFloat4)))
		return false;

	// Can be either one
	{
		if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Sizes Buffer"), (toGenerate.m_GeneratedInputs & Drawers::GenInput_ParticleSize) != 0, m_ApiManager, m_Sizes, RHI::RawBuffer, totalParticleCountAligned * sizeof(float), totalParticleCount * sizeof(float)))
			return false;
		if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Size2s Buffer"), (toGenerate.m_GeneratedInputs & Drawers::GenInput_ParticleSize2) != 0, m_ApiManager, m_Sizes2, RHI::RawBuffer, totalParticleCountAligned * sizeof(CFloat2), totalParticleCount * sizeof(CFloat2)))
			return false;
	}

	// Rotation particle stream
	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Rotations Buffer"), (toGenerate.m_GeneratedInputs & Drawers::GenInput_ParticleRotation) != 0, m_ApiManager, m_Rotations, RHI::RawBuffer, totalParticleCountAligned * sizeof(float), totalParticleCount * sizeof(float)))
		return false;
	// First billboarding axis (necessary for AxisAligned, AxisAlignedSpheroid, AxisAlignedCapsule)
	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Axis0s Buffer"), (toGenerate.m_GeneratedInputs & Drawers::GenInput_ParticleAxis0) != 0, m_ApiManager, m_Axis0s, RHI::RawBuffer, totalParticleCountAligned * sizeof(CFloat3), totalParticleCount * sizeof(CFloat3)))
		return false;
	// Second billboarding axis (necessary for PlaneAligned)
	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Axis1s Buffer"), (toGenerate.m_GeneratedInputs & Drawers::GenInput_ParticleAxis1) != 0, m_ApiManager, m_Axis1s, RHI::RawBuffer, totalParticleCountAligned * sizeof(CFloat3), totalParticleCount * sizeof(CFloat3)))
		return false;

	// Constant buffer filled by CPU task, will contain simple contants per draw request (normals bending factor, ...)
	// Each vertex position 0 will contain its associated draw request ID in position's W component (See sample vertex shader for more detail)
	PK_STATIC_ASSERT(sizeof(Drawers::SBillboardDrawRequest) == sizeof(CFloat4));
	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Billboard DrawRequests Buffer"), true, m_ApiManager, m_DrawRequests, RHI::ConstantBuffer, kMaxGeomDrawRequestCount * sizeof(Drawers::SBillboardDrawRequest), kMaxGeomDrawRequestCount * sizeof(Drawers::SBillboardDrawRequest)))
		return false;

	if (!m_AdditionalFieldsBatch.AllocBuffers(totalParticleCount, m_ApiManager, RHI::RawBuffer))
		return false;

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	// Editor only: for debugging purposes, we'll remove that from samples code later
	PKSample::SRHIRenderContext	&ctxEditor = *static_cast<PKSample::SRHIRenderContext*>(&ctx);
	if (ctxEditor.Selection().HasParticlesSelected() || ctxEditor.Selection().HasRendersSelected())
	{
		if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Selection Vertex Buffer"), true, m_ApiManager, m_IsParticleSelected, RHI::RawBuffer, totalParticleCountAligned * sizeof(float), totalParticleCount * sizeof(float)))
			return false;
	}
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Billboard_VertexBB::MapBuffers(SRenderContext &ctx)
{
	(void)ctx;
	const u32		totalParticleCount = m_BB_Billoard.TotalParticleCount();
	const auto		&toMap = m_DrawPass->m_ToGenerate;

	if (toMap.m_GeneratedInputs & Drawers::GenInput_ParticlePosition)
	{
		PK_ASSERT(m_Positions.Used());
		void	*mappedValue = m_ApiManager->MapCpuView(m_Positions.m_Buffer, 0, totalParticleCount * sizeof(CFloat4));
		if (!PK_VERIFY(mappedValue != null))
			return false;
		m_BBJobs_Billboard.m_Exec_CopyBillboardingStreams.m_PositionsDrIds = TMemoryView<Drawers::SVertex_PositionDrId>(static_cast<Drawers::SVertex_PositionDrId*>(mappedValue), totalParticleCount);
	}
	if (toMap.m_GeneratedInputs & Drawers::GenInput_ParticleSize)
	{
		PK_ASSERT(m_Sizes.Used());
		void	*mappedValue = m_ApiManager->MapCpuView(m_Sizes.m_Buffer, 0, totalParticleCount * sizeof(float));
		if (!PK_VERIFY(mappedValue != null))
			return false;
		m_BBJobs_Billboard.m_Exec_CopyBillboardingStreams.m_Sizes = TMemoryView<float>(static_cast<float*>(mappedValue), totalParticleCount);
	}
	else if (toMap.m_GeneratedInputs & Drawers::GenInput_ParticleSize2)
	{
		PK_ASSERT(m_Sizes2.Used());
		void	*mappedValue = m_ApiManager->MapCpuView(m_Sizes2.m_Buffer, 0, totalParticleCount * sizeof(CFloat2));
		if (!PK_VERIFY(mappedValue != null))
			return false;
		m_BBJobs_Billboard.m_Exec_CopyBillboardingStreams.m_Sizes2 = TMemoryView<CFloat2>(static_cast<CFloat2*>(mappedValue), totalParticleCount);
	}
	if (toMap.m_GeneratedInputs & Drawers::GenInput_ParticleRotation)
	{
		PK_ASSERT(m_Rotations.Used());
		void	*mappedValue = m_ApiManager->MapCpuView(m_Rotations.m_Buffer, 0, totalParticleCount * sizeof(float));
		if (!PK_VERIFY(mappedValue != null))
			return false;
		m_BBJobs_Billboard.m_Exec_CopyBillboardingStreams.m_Rotations = TMemoryView<float>(static_cast<float*>(mappedValue), totalParticleCount);
	}
	if (toMap.m_GeneratedInputs & Drawers::GenInput_ParticleAxis0)
	{
		PK_ASSERT(m_Axis0s.Used());
		void	*mappedValue = m_ApiManager->MapCpuView(m_Axis0s.m_Buffer, 0, totalParticleCount * sizeof(CFloat3));
		if (!PK_VERIFY(mappedValue != null))
			return false;
		m_BBJobs_Billboard.m_Exec_CopyBillboardingStreams.m_Axis0 = TMemoryView<CFloat3>(static_cast<CFloat3*>(mappedValue), totalParticleCount);
	}
	if (toMap.m_GeneratedInputs & Drawers::GenInput_ParticleAxis1)
	{
		PK_ASSERT(m_Axis1s.Used());
		void	*mappedValue = m_ApiManager->MapCpuView(m_Axis1s.m_Buffer, 0, totalParticleCount * sizeof(CFloat3));
		if (!PK_VERIFY(mappedValue != null))
			return false;
		m_BBJobs_Billboard.m_Exec_CopyBillboardingStreams.m_Axis1 = TMemoryView<CFloat3>(static_cast<CFloat3*>(mappedValue), totalParticleCount);
	}

	{
		PK_ASSERT(m_DrawRequests.Used());
		void	*mappedDrawRequests = m_ApiManager->MapCpuView(m_DrawRequests.m_Buffer, 0, kMaxGeomDrawRequestCount * sizeof(Drawers::SBillboardDrawRequest));
		if (!PK_VERIFY(mappedDrawRequests != null))
			return false;
		m_BBJobs_Billboard.m_Exec_GeomBillboardDrawRequests.m_GeomDrawRequests = TMemoryView<Drawers::SBillboardDrawRequest>(static_cast<Drawers::SBillboardDrawRequest*>(mappedDrawRequests), kMaxGeomDrawRequestCount);
	}

	if (toMap.m_GeneratedInputs & Drawers::GenInput_Indices)
	{
		PK_ASSERT(m_Indices.Used());
		void	*mappedValue = m_ApiManager->MapCpuView(m_Indices.m_Buffer, 0, totalParticleCount * sizeof(u32));
		if (!PK_VERIFY(mappedValue != null))
			return false;
		m_BBJobs_Billboard.m_Exec_Indices.m_IndexStream.Setup(mappedValue, totalParticleCount, true);
	}
	PK_ASSERT(m_PerViewIndicesBuffers.Count() == m_BBJobs_Billboard.m_PerView.Count());
	for (u32 i = 0; i < m_PerViewIndicesBuffers.Count(); ++i)
	{
		const u32	viewGeneratedInputs = toMap.m_PerViewGeneratedInputs[i].m_GeneratedInputs;
		if (viewGeneratedInputs & Drawers::GenInput_Indices)
		{
			void	*mappedValue = m_ApiManager->MapCpuView(m_PerViewIndicesBuffers[i].m_Buffer, 0, totalParticleCount * sizeof(u32));
			if (!PK_VERIFY(mappedValue != null))
				return false;
			m_BBJobs_Billboard.m_PerView[i].m_Exec_Indices.m_IndexStream.Setup(mappedValue, totalParticleCount, true);
		}
	}

	if (!m_AdditionalFieldsBatch.MapBuffers(totalParticleCount, m_ApiManager))
		return false;
	m_BBJobs_Billboard.m_Exec_CopyAdditionalFields.m_FieldsToCopy = m_AdditionalFieldsBatch.m_MappedFields;

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	// Editor only: for debugging purposes, we'll remove that from samples code later
	PKSample::SRHIRenderContext	&ctxEditor = *static_cast<PKSample::SRHIRenderContext*>(&ctx);
	if (ctxEditor.Selection().HasParticlesSelected() || ctxEditor.Selection().HasRendersSelected())
	{
		m_GeomBillboardCustomParticleSelectTask.Clear();
		PK_ASSERT(m_IsParticleSelected.Used());
		void	*mappedValue = m_ApiManager->MapCpuView(m_IsParticleSelected.m_Buffer, 0, sizeof(float) * totalParticleCount);
		if (!PK_VERIFY(mappedValue != null))
			return false;
		m_GeomBillboardCustomParticleSelectTask.m_DstSelectedParticles = TStridedMemoryView<float>(static_cast<float*>(mappedValue), totalParticleCount, sizeof(float));
		m_GeomBillboardCustomParticleSelectTask.m_SrcParticleSelected = ctxEditor.Selection();
	}
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Billboard_VertexBB::LaunchCustomTasks(SRenderContext &ctx)
{
	(void)ctx;
#if	(PK_HAS_PARTICLES_SELECTION != 0)
	// Editor only: for debugging purposes, we'll remove that from samples code later
	PKSample::SRHIRenderContext	&ctxEditor = *static_cast<PKSample::SRHIRenderContext*>(&ctx);
	if (ctxEditor.Selection().HasParticlesSelected() || ctxEditor.Selection().HasRendersSelected())
	{
		m_BB_Billoard.AddExecAsyncPage(&m_GeomBillboardCustomParticleSelectTask);
	}
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)
	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Billboard_VertexBB::UnmapBuffers(SRenderContext &ctx)
{
	(void)ctx;

	m_Positions.UnmapIFN(m_ApiManager);
	m_Sizes.UnmapIFN(m_ApiManager);
	m_Sizes2.UnmapIFN(m_ApiManager);
	m_Rotations.UnmapIFN(m_ApiManager);
	m_Axis0s.UnmapIFN(m_ApiManager);
	m_Axis1s.UnmapIFN(m_ApiManager);

	m_DrawRequests.UnmapIFN(m_ApiManager);

	m_Indices.UnmapIFN(m_ApiManager);
	for (auto &buffer : m_PerViewIndicesBuffers)
		buffer.UnmapIFN(m_ApiManager);

	m_AdditionalFieldsBatch.UnmapBuffers(m_ApiManager);

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	m_IsParticleSelected.UnmapIFN(m_ApiManager);
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Billboard_VertexBB::EmitDrawCall(SRenderContext &ctx, const SDrawCallDesc &toEmit)
{
	SRHIRenderContext	&renderContext = static_cast<SRHIRenderContext&>(ctx);

	// No need to iterate on all draw requests, just take the first as reference as they wouldn't have been batched if not compatible
	CRendererCacheInstance_UpdateThread	*refCacheInstance = static_cast<CRendererCacheInstance_UpdateThread*>(toEmit.m_RendererCaches.First().Get());
	if (!PK_VERIFY(refCacheInstance != null))
		return false;

	PKSample::PCRendererCacheInstance	rCacheInstance = refCacheInstance->RenderThread_GetCacheInstance();
	if (!PK_VERIFY(rCacheInstance != null))
		return false;

	const Drawers::SBillboard_DrawRequest	*dr = static_cast<const Drawers::SBillboard_DrawRequest*>(toEmit.m_DrawRequests.First());
	const u32	shaderOptions = PKSample::Option_VertexPassThrough | _GetVertexBillboardShaderOptions(dr->m_BB);

	if (!PK_VERIFY(renderContext.m_DrawOutputs.m_DrawCalls.PushBack().Valid()))
	{
		CLog::Log(PK_ERROR, "Failed to create a draw-call");
		return false;
	}
	SRHIDrawCall	&outDrawCall = renderContext.m_DrawOutputs.m_DrawCalls.Last();

	outDrawCall.m_Batch = this;
	outDrawCall.m_RendererCacheInstance = refCacheInstance;
	outDrawCall.m_Type = SRHIDrawCall::DrawCall_IndexedInstanced;
	outDrawCall.m_ShaderOptions = shaderOptions;
	outDrawCall.m_RendererType = Renderer_Billboard;

	outDrawCall.m_IndexOffset = 0;
	outDrawCall.m_VertexCount = m_CapsulesDC ? 6 : 4;
	outDrawCall.m_IndexCount = m_CapsulesDC ? 12 : 6;
	outDrawCall.m_InstanceCount = toEmit.m_TotalParticleCount;
	outDrawCall.m_IndexSize = RHI::IndexBuffer16Bit;
	outDrawCall.m_IndexBuffer = m_DrawIndices.m_Buffer;

	// Some meta-data (the Editor uses them)
	{
		outDrawCall.m_BBox = toEmit.m_BBox;
		outDrawCall.m_TotalBBox = m_DrawPass->m_TotalBBox;
		outDrawCall.m_SlicedDC = toEmit.m_TotalParticleCount != m_DrawPass->m_TotalParticleCount;
	}

	// A single vertex buffer is used for the instanced draw: the texcoords buffer, contains the direction in which vertices should be expanded
	PK_ASSERT(m_TexCoords.Used());
	if (!PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(m_TexCoords.m_Buffer).Valid()))
		return false;
	outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Texcoords] = m_TexCoords.m_Buffer;

	// GPUBillboardPushConstants
	if (!PK_VERIFY(outDrawCall.m_PushConstants.PushBack().Valid()))
		return false;
	u32		&indexOffset = *reinterpret_cast<u32*>(&outDrawCall.m_PushConstants.Last());
	indexOffset = toEmit.m_IndexOffset; // == instanceOffset

	// Fill the constant-set with all SRVs
	// TODO: update the constant-set only when buffers have been resized/changed.
	{
		u32 i = 0;

		// TODO: We only support a single view right now
		SGpuBuffer	&indices = (!m_PerViewIndicesBuffers.Empty() && m_PerViewIndicesBuffers[0].Used()) ? m_PerViewIndicesBuffers[0] : m_Indices;
		PK_ASSERT(indices.Used());
		if (!PK_VERIFY(m_VertexBBSimDataConstantSet->SetConstants(indices.m_Buffer, i++)))
			return false;
		outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Indices] = indices.m_Buffer;

		if (m_Positions.Used())
		{
			if (!PK_VERIFY(m_VertexBBSimDataConstantSet->SetConstants(m_Positions.m_Buffer, i++)))
				return false;
			outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Position] = m_Positions.m_Buffer;
		}

		SGpuBuffer	&sizes = m_Sizes.Used() ? m_Sizes : m_Sizes2;
		if (sizes.Used())
		{
			if (!PK_VERIFY(m_VertexBBSimDataConstantSet->SetConstants(sizes.m_Buffer, i++)))
				return false;
			outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Size] = sizes.m_Buffer;
		}

		if (m_Rotations.Used())
		{
			if (!PK_VERIFY(m_VertexBBSimDataConstantSet->SetConstants(m_Rotations.m_Buffer, i++)))
				return false;
			outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Rotation] = m_Rotations.m_Buffer;
		}

		if (m_Axis0s.Used())
		{
			if (!PK_VERIFY(m_VertexBBSimDataConstantSet->SetConstants(m_Axis0s.m_Buffer, i++)))
				return false;
			outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Axis0] = m_Axis0s.m_Buffer;
		}

		if (m_Axis1s.Used())
		{
			if (!PK_VERIFY(m_VertexBBSimDataConstantSet->SetConstants(m_Axis1s.m_Buffer, i++)))
				return false;
			outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Axis1] = m_Axis1s.m_Buffer;
		}

		outDrawCall.m_UBSemanticsPtr[SRHIDrawCall::UBSemantic_GPUBillboard] = m_DrawRequests.m_Buffer;

		if (m_ColorStreamIdx.Valid())
			outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Color] = m_AdditionalFieldsBatch.m_Fields[m_ColorStreamIdx].m_Buffer.m_Buffer;

		for (SAdditionalInputs &addField : m_AdditionalFieldsBatch.m_Fields)
		{
			if (!addField.m_Buffer.Used())
				continue;
			if (!PK_VERIFY(m_VertexBBSimDataConstantSet->SetConstants(addField.m_Buffer.m_Buffer, i++)))
				return false;
		}

		m_VertexBBSimDataConstantSet->UpdateConstantValues();
	}
	outDrawCall.m_GPUStorageSimDataConstantSet = m_VertexBBSimDataConstantSet;

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	PKSample::SRHIRenderContext	&ctxEditor = *static_cast<PKSample::SRHIRenderContext*>(&ctx);
	if (ctxEditor.Selection().HasParticlesSelected() || ctxEditor.Selection().HasRendersSelected())
	{
		if (m_SelectionConstantSet != null)
		{
			m_SelectionConstantSet->SetConstants(m_IsParticleSelected.m_Buffer, 0);
			m_SelectionConstantSet->UpdateConstantValues();
		}
		outDrawCall.m_SelectionConstantSet = m_SelectionConstantSet;
		outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_IsParticleSelected] = m_IsParticleSelected.m_Buffer;
	}
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Billboard_VertexBB::_InitStaticBuffers()
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

		const Drawers::SBillboard_DrawRequest	*dr = static_cast<const Drawers::SBillboard_DrawRequest*>(m_DrawPass->m_DrawRequests.First());
		const u32	shaderOptions = PKSample::Option_VertexPassThrough | _GetVertexBillboardShaderOptions(dr->m_BB);

		const RHI::SConstantSetLayout	*simDataConstantSetLayout = null;
		const RHI::SConstantSetLayout	*offsetsConstantSetLayout = null;
		if (!PK_VERIFY(cacheInstance->m_Cache->GetGPUStorageConstantSets(static_cast<PKSample::EShaderOptions>(shaderOptions), simDataConstantSetLayout, offsetsConstantSetLayout)) ||
			!PK_VERIFY(simDataConstantSetLayout != null) ||
			simDataConstantSetLayout->m_Constants.Empty())
			return false;

		m_VertexBBSimDataConstantSet = m_ApiManager->CreateConstantSet(RHI::SRHIResourceInfos("Sim Data Constant Set"), *simDataConstantSetLayout);
		if (!PK_VERIFY(m_VertexBBSimDataConstantSet != null))
			return false;
	}

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	{
		RHI::SConstantSetLayout	selectionSetLayout(RHI::VertexShaderMask);
		selectionSetLayout.AddConstantsLayout(RHI::SRawBufferDesc("Selections"));
		m_SelectionConstantSet = m_ApiManager->CreateConstantSet(RHI::SRHIResourceInfos("Selection Constant Set"), selectionSetLayout);
	}
#endif

	return true;
}

//----------------------------------------------------------------------------
//
// CRHIRendererBatch_Ribbon_CPU
//
//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Ribbon_CPU::Setup(const CRendererDataBase *renderer, const CParticleRenderMedium *owner, const CFrameCollector *fc, const CStringId &storageClass)
{
	if (!CRendererBatchJobs_Ribbon_CPUBB::Setup(renderer, owner, fc, storageClass))
		return false;

	// Setup additional fields:
	// The additional fields are supposed to be the same for all renderers in a batch.
	// If not, then you can recompute then on the "Bind()" method.

	const auto		&toGenerate = m_DrawPass->m_ToGenerate;
	const u32		additionalFieldsCount = toGenerate.m_AdditionalGeneratedInputs.Count();

	if (!PK_VERIFY(m_AdditionalFieldsBatch.m_Fields.Reserve(additionalFieldsCount)))
		return false;

	for (u32 i = 0; i < additionalFieldsCount; ++i)
	{
		// no ignored field

		const u32	typeSize = CBaseTypeTraits::Traits(toGenerate.m_AdditionalGeneratedInputs[i].m_Type).Size;
		m_AdditionalFieldsBatch.m_Fields.PushBackUnsafe(SAdditionalInputs(typeSize, i));
	}

	m_ColorStreamIdx = _GetDrawDebugColorIndex(m_AdditionalFieldsBatch, toGenerate);

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Ribbon_CPU::AreRenderersCompatible(const CRendererDataBase *rendererA, const CRendererDataBase *rendererB) const
{
	return	CRendererBatchJobs_Ribbon_CPUBB::AreRenderersCompatible(rendererA, rendererB) &&
			AreBillboardingBatchable(rendererA->m_RendererCache, rendererB->m_RendererCache);
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Ribbon_CPU::AllocBuffers(PopcornFX::SRenderContext &ctx)
{
	(void)ctx;

	const u32		totalVertexCount = m_BB_Ribbon.TotalVertexCount();
	const u32		totalIndexCount = m_BB_Ribbon.TotalIndexCount();

	if (!m_CommonBuffers.AllocBuffers(totalIndexCount, totalVertexCount, m_DrawPass->m_ToGenerate, m_ApiManager))
		return false;

	if (!m_AdditionalFieldsBatch.AllocBuffers(totalVertexCount, m_ApiManager))
		return false;

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	// Editor only: for debugging purposes, we'll remove that from samples code later
	PKSample::SRHIRenderContext	&ctxEditor = *static_cast<PKSample::SRHIRenderContext*>(&ctx);
	if (ctxEditor.Selection().HasParticlesSelected() || ctxEditor.Selection().HasRendersSelected())
	{
		if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Selection Vertex Buffer"), true, m_ApiManager, m_IsParticleSelected, RHI::VertexBuffer, Mem::Align<0x100>(totalVertexCount) * sizeof(float), totalVertexCount * sizeof(float)))
			return false;
	}
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Ribbon_CPU::MapBuffers(SRenderContext &ctx)
{
	(void)ctx;

	const u32				totalVertexCount = m_BB_Ribbon.TotalVertexCount();
	const u32				totalIndexCount = m_BB_Ribbon.TotalIndexCount();
	const SGeneratedInputs	&toMap = m_DrawPass->m_ToGenerate;

	// View independent inputs:
	if (toMap.m_GeneratedInputs & Drawers::GenInput_Indices)
	{
		PK_ASSERT(m_CommonBuffers.m_Indices.Used());
		void	*mappedValue = m_ApiManager->MapCpuView(m_CommonBuffers.m_Indices.m_Buffer, 0 , totalIndexCount * m_CommonBuffers.m_IndexSize);
		if (!PK_VERIFY(mappedValue != null))
			return false;
		m_BBJobs_Ribbon.m_Exec_Indices.m_IndexStream.Setup(mappedValue, totalIndexCount, m_CommonBuffers.m_IndexSize == sizeof(u32));
	}
	if (toMap.m_GeneratedInputs & Drawers::GenInput_Position)
	{
		PK_ASSERT(m_CommonBuffers.m_Positions.Used());
		void	*mappedValue = m_ApiManager->MapCpuView(m_CommonBuffers.m_Positions.m_Buffer, 0, totalVertexCount * sizeof(CFloat4));
		if (!PK_VERIFY(mappedValue != null))
			return false;
		m_BBJobs_Ribbon.m_Exec_PNT.m_Positions = TStridedMemoryView<CFloat3, 0x10>(static_cast<CFloat3*>(mappedValue), totalVertexCount, 0x10);
	}
	if (toMap.m_GeneratedInputs & Drawers::GenInput_Normal)
	{
		PK_ASSERT(m_CommonBuffers.m_Normals.Used());
		void	*mappedValue = m_ApiManager->MapCpuView(m_CommonBuffers.m_Normals.m_Buffer, 0, totalVertexCount * sizeof(CFloat4));
		if (!PK_VERIFY(mappedValue != null))
			return false;
		m_BBJobs_Ribbon.m_Exec_PNT.m_Normals = TStridedMemoryView<CFloat3, 0x10>(static_cast<CFloat3*>(mappedValue), totalVertexCount, 0x10);
	}
	if (toMap.m_GeneratedInputs & Drawers::GenInput_Tangent)
	{
		PK_ASSERT(m_CommonBuffers.m_Tangents.Used());
		void	*mappedValue = m_ApiManager->MapCpuView(m_CommonBuffers.m_Tangents.m_Buffer, 0, totalVertexCount * sizeof(CFloat4));
		if (!PK_VERIFY(mappedValue != null))
			return false;
		m_BBJobs_Ribbon.m_Exec_PNT.m_Tangents = TStridedMemoryView<CFloat4, 0x10>(static_cast<CFloat4*>(mappedValue), totalVertexCount, 0x10);
	}
	if (toMap.m_GeneratedInputs & Drawers::GenInput_UV0)
	{
		PK_ASSERT(m_CommonBuffers.m_TexCoords0.Used());
		void	*mappedValue = m_ApiManager->MapCpuView(m_CommonBuffers.m_TexCoords0.m_Buffer, 0, totalVertexCount * sizeof(CFloat2));
		if (!PK_VERIFY(mappedValue != null))
			return false;
		m_BBJobs_Ribbon.m_Exec_Texcoords.m_Texcoords = TStridedMemoryView<CFloat2>(static_cast<CFloat2*>(mappedValue), totalVertexCount);
	}
	if (toMap.m_GeneratedInputs & Drawers::GenInput_UV1)
	{
		PK_ASSERT(m_CommonBuffers.m_TexCoords1.Used());
		void	*mappedValue = m_ApiManager->MapCpuView(m_CommonBuffers.m_TexCoords1.m_Buffer, 0, totalVertexCount * sizeof(CFloat2));
		if (!PK_VERIFY(mappedValue != null))
			return false;
		m_BBJobs_Ribbon.m_Exec_Texcoords.m_Texcoords2 = TStridedMemoryView<CFloat2>(static_cast<CFloat2*>(mappedValue), totalVertexCount);
	}
	if (toMap.m_GeneratedInputs & Drawers::GenInput_RawUV0)
	{
		PK_ASSERT(m_CommonBuffers.m_RawTexCoords0.Used());
		void	*mappedValue = m_ApiManager->MapCpuView(m_CommonBuffers.m_RawTexCoords0.m_Buffer, 0, totalVertexCount * sizeof(CFloat2));
		if (!PK_VERIFY(mappedValue != null))
			return false;
		m_BBJobs_Ribbon.m_Exec_Texcoords.m_RawTexcoords = TStridedMemoryView<CFloat2>(static_cast<CFloat2*>(mappedValue), totalVertexCount);
	}
	if (toMap.m_GeneratedInputs & Drawers::GenInput_UVRemap)
	{
		PK_ASSERT(m_CommonBuffers.m_UVRemap.Used());
		void	*mappedValue = m_ApiManager->MapCpuView(m_CommonBuffers.m_UVRemap.m_Buffer, 0, totalVertexCount * sizeof(CFloat4));
		if (!PK_VERIFY(mappedValue != null))
			return false;
		m_BBJobs_Ribbon.m_Exec_UVRemap.m_UVRemap = TStridedMemoryView<CFloat4>(static_cast<CFloat4*>(mappedValue), totalVertexCount);
		m_BBJobs_Ribbon.m_Exec_Texcoords.m_ForUVFactor = true;
	}
	if (toMap.m_GeneratedInputs & Drawers::GenInput_UVFactors)
	{
		PK_ASSERT(m_CommonBuffers.m_UVFactors.Used());
		void	*mappedValue = m_ApiManager->MapCpuView(m_CommonBuffers.m_UVFactors.m_Buffer, 0, totalVertexCount * sizeof(CFloat4));
		if (!PK_VERIFY(mappedValue != null))
			return false;
		m_BBJobs_Ribbon.m_Exec_PNT.m_UVFactors4 = TStridedMemoryView<CFloat4>(static_cast<CFloat4*>(mappedValue), totalVertexCount);
	}

	// View dependent inputs:
	PK_ASSERT(m_CommonBuffers.m_PerViewBuffers.Count() == m_BBJobs_Ribbon.m_PerView.Count());
	for (u32 i = 0; i < m_CommonBuffers.m_PerViewBuffers.Count(); ++i)
	{
		const u32	viewGeneratedInputs = toMap.m_PerViewGeneratedInputs[i].m_GeneratedInputs;
		if (viewGeneratedInputs == 0)
			continue;

		if (viewGeneratedInputs & Drawers::GenInput_Indices)
		{
			PK_ASSERT(m_CommonBuffers.m_PerViewBuffers[i].m_Indices.Used());
			void	*mappedValue = m_ApiManager->MapCpuView(m_CommonBuffers.m_PerViewBuffers[i].m_Indices.m_Buffer, 0, totalIndexCount * m_CommonBuffers.m_IndexSize);
			if (!PK_VERIFY(mappedValue != null))
				return false;
			m_BBJobs_Ribbon.m_PerView[i].m_Exec_Indices.m_IndexStream.Setup(mappedValue, totalIndexCount, m_CommonBuffers.m_IndexSize == sizeof(u32));
		}
		if (viewGeneratedInputs & Drawers::GenInput_Position)
		{
			PK_ASSERT(m_CommonBuffers.m_PerViewBuffers[i].m_Positions.Used());
			void	*mappedValue = m_ApiManager->MapCpuView(m_CommonBuffers.m_PerViewBuffers[i].m_Positions.m_Buffer, 0, totalVertexCount * sizeof(CFloat4));
			if (!PK_VERIFY(mappedValue != null))
				return false;
			m_BBJobs_Ribbon.m_PerView[i].m_Exec_PNT.m_Positions = TStridedMemoryView<CFloat3, 0x10>(static_cast<CFloat3*>(mappedValue), totalVertexCount, 0x10);
		}
		if (viewGeneratedInputs & Drawers::GenInput_Tangent)
		{
			PK_ASSERT(m_CommonBuffers.m_PerViewBuffers[i].m_Tangents.Used());
			void	*mappedValue = m_ApiManager->MapCpuView(m_CommonBuffers.m_PerViewBuffers[i].m_Tangents.m_Buffer, 0, totalVertexCount * sizeof(CFloat4));
			if (!PK_VERIFY(mappedValue != null))
				return false;
			m_BBJobs_Ribbon.m_PerView[i].m_Exec_PNT.m_Tangents = TStridedMemoryView<CFloat4, 0x10>(static_cast<CFloat4*>(mappedValue), totalVertexCount, 0x10);
		}
		if (viewGeneratedInputs & Drawers::GenInput_Normal)
		{
			PK_ASSERT(m_CommonBuffers.m_PerViewBuffers[i].m_Normals.Used());
			void	*mappedValue = m_ApiManager->MapCpuView(m_CommonBuffers.m_PerViewBuffers[i].m_Normals.m_Buffer, 0, totalVertexCount * sizeof(CFloat4));
			if (!PK_VERIFY(mappedValue != null))
				return false;
			m_BBJobs_Ribbon.m_PerView[i].m_Exec_PNT.m_Normals = TStridedMemoryView<CFloat3, 0x10>(static_cast<CFloat3*>(mappedValue), totalVertexCount, 0x10);
		}
		if (viewGeneratedInputs & Drawers::GenInput_UVFactors)
		{
			PK_ASSERT(m_CommonBuffers.m_PerViewBuffers[i].m_UVFactors.Used());
			void	*mappedValue = m_ApiManager->MapCpuView(m_CommonBuffers.m_PerViewBuffers[i].m_UVFactors.m_Buffer, 0, totalVertexCount * sizeof(CFloat4));
			if (!PK_VERIFY(mappedValue != null))
				return false;
			m_BBJobs_Ribbon.m_PerView[i].m_Exec_PNT.m_UVFactors4 = TStridedMemoryView<CFloat4>(static_cast<CFloat4*>(mappedValue), totalVertexCount);
		}
	}

	// Additional inputs:
	if (!m_AdditionalFieldsBatch.MapBuffers(totalVertexCount, m_ApiManager))
		return false;
	m_BBJobs_Ribbon.m_Exec_CopyField.m_FieldsToCopy = m_AdditionalFieldsBatch.m_MappedFields;

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	// Editor only: for debugging purposes, we'll remove that from samples code later
	PKSample::SRHIRenderContext	&ctxEditor = *static_cast<PKSample::SRHIRenderContext*>(&ctx);
	if (ctxEditor.Selection().HasParticlesSelected() || ctxEditor.Selection().HasRendersSelected())
	{
		m_RibbonCustomParticleSelectTask.Clear();
		PK_ASSERT(m_IsParticleSelected.Used());
		void	*mappedValue = m_ApiManager->MapCpuView(m_IsParticleSelected.m_Buffer, 0, sizeof(float) * totalVertexCount);
		if (!PK_VERIFY(mappedValue != null))
			return false;
		m_RibbonCustomParticleSelectTask.m_DstSelectedParticles = TStridedMemoryView<float>(static_cast<float*>(mappedValue), totalVertexCount, sizeof(float));
		m_RibbonCustomParticleSelectTask.m_SrcParticleSelected = ctxEditor.Selection();
	}
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Ribbon_CPU::LaunchCustomTasks(SRenderContext &ctx)
{
	(void)ctx;
#if	(PK_HAS_PARTICLES_SELECTION != 0)
	// Editor only: for debugging purposes, we'll remove that from samples code later
	PKSample::SRHIRenderContext	&ctxEditor = *static_cast<PKSample::SRHIRenderContext*>(&ctx);
	if (ctxEditor.Selection().HasParticlesSelected() || ctxEditor.Selection().HasRendersSelected())
	{
		m_BB_Ribbon.AddExecBatch(&m_RibbonCustomParticleSelectTask);
	}
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)
	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Ribbon_CPU::UnmapBuffers(SRenderContext &ctx)
{
	(void)ctx;

	m_CommonBuffers.UnmapBuffers(m_ApiManager);

	m_AdditionalFieldsBatch.UnmapBuffers(m_ApiManager);

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	m_IsParticleSelected.UnmapIFN(m_ApiManager);
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Ribbon_CPU::EmitDrawCall(SRenderContext &ctx, const SDrawCallDesc &toEmit)
{
	SRHIRenderContext	&renderContext = static_cast<SRHIRenderContext&>(ctx);

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

	const bool	hasAtlas = rCacheInstance->m_HasAtlas;
	const bool	hasRawUV0 = rCacheInstance->m_HasRawUV0;

	if (!PK_VERIFY(renderContext.m_DrawOutputs.m_DrawCalls.PushBack().Valid()))
	{
		CLog::Log(PK_ERROR, "Failed to create a draw-call");
		return false;
	}
	SRHIDrawCall	&outDrawCall = renderContext.m_DrawOutputs.m_DrawCalls.Last();

	outDrawCall.m_Batch = this;
	outDrawCall.m_RendererCacheInstance = renderCacheInstance;
	outDrawCall.m_Type = SRHIDrawCall::DrawCall_Regular;
	outDrawCall.m_ShaderOptions = PKSample::Option_VertexPassThrough;
	outDrawCall.m_RendererType = Renderer_Ribbon;

	outDrawCall.m_IndexOffset = toEmit.m_IndexOffset;
	outDrawCall.m_VertexCount = toEmit.m_TotalVertexCount;
	outDrawCall.m_IndexCount = toEmit.m_TotalIndexCount;
	outDrawCall.m_IndexSize = (m_CommonBuffers.m_IndexSize == sizeof(u32)) ? RHI::IndexBuffer32Bit : RHI::IndexBuffer16Bit;

	// Some meta-data (the Editor uses them)
	{
		outDrawCall.m_BBox = toEmit.m_BBox;
		outDrawCall.m_TotalBBox = m_DrawPass->m_TotalBBox;
		outDrawCall.m_SlicedDC = toEmit.m_TotalParticleCount != m_DrawPass->m_TotalParticleCount;
	}

	if (!m_CommonBuffers.m_PerViewBuffers.Empty() && m_CommonBuffers.m_PerViewBuffers[0].m_Indices.Used())
		outDrawCall.m_IndexBuffer = m_CommonBuffers.m_PerViewBuffers[0].m_Indices.m_Buffer;
	else
		outDrawCall.m_IndexBuffer = m_CommonBuffers.m_Indices.m_Buffer;
	PK_ASSERT(outDrawCall.m_IndexBuffer != null);

	if (!m_CommonBuffers.m_PerViewBuffers.Empty() && m_CommonBuffers.m_PerViewBuffers[0].m_Positions.Used())
	{
		// View dependent buffers (Right now this code only takes in account first view)
		// If the first view contains an allocated Positions buffer, it means we have view dependent geometry.
		if (!PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(m_CommonBuffers.m_PerViewBuffers[0].m_Positions.m_Buffer).Valid()))
			return false;
		if (m_CommonBuffers.m_PerViewBuffers[0].m_Normals.Used() && !PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(m_CommonBuffers.m_PerViewBuffers[0].m_Normals.m_Buffer).Valid()))
			return false;
		if (m_CommonBuffers.m_PerViewBuffers[0].m_Tangents.Used() && !PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(m_CommonBuffers.m_PerViewBuffers[0].m_Tangents.m_Buffer).Valid()))
			return false;
	}
	else
	{
		if (m_CommonBuffers.m_Positions.Used() && !PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(m_CommonBuffers.m_Positions.m_Buffer).Valid()))
			return false;
		if (m_CommonBuffers.m_Normals.Used() && !PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(m_CommonBuffers.m_Normals.m_Buffer).Valid()))
			return false;
		if (m_CommonBuffers.m_Tangents.Used() && !PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(m_CommonBuffers.m_Tangents.m_Buffer).Valid()))
			return false;
	}

	if (m_CommonBuffers.m_TexCoords0.Used())
	{
		if (!PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(m_CommonBuffers.m_TexCoords0.m_Buffer).Valid()))
			return false;

		// Atlas renderer feature enabled: None/Linear atlas blending share the same vertex declaration, so we just push empty buffers when blending is disabled
		if (hasAtlas || m_CommonBuffers.m_TexCoords1.Used())
		{
			// If we have invalid m_TexCoords1, bind a dummy vertex buffer, here m_TexCoords0
			if (!PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(m_CommonBuffers.m_TexCoords1.Used() ? m_CommonBuffers.m_TexCoords1.m_Buffer : m_CommonBuffers.m_TexCoords0.m_Buffer).Valid()))
				return false;
		}
		if (hasRawUV0)
		{
			if (!PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(m_CommonBuffers.m_RawTexCoords0.Used() ? m_CommonBuffers.m_RawTexCoords0.m_Buffer : m_CommonBuffers.m_TexCoords0.m_Buffer).Valid()))
				return false;
		}
	}

	if (m_CommonBuffers.m_UVRemap.Used() && !PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(m_CommonBuffers.m_UVRemap.m_Buffer).Valid()))
		return false;

	if (!m_CommonBuffers.m_PerViewBuffers.Empty() && m_CommonBuffers.m_PerViewBuffers[0].m_Positions.Used())
	{
		// Same check as above, but UVFactors are pushed in last to match vertex declaration expected by RHI Render states
		if (m_CommonBuffers.m_PerViewBuffers[0].m_UVFactors.Used() && !PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(m_CommonBuffers.m_PerViewBuffers[0].m_UVFactors.m_Buffer).Valid()))
			return false;
	}
	else
	{
		if (m_CommonBuffers.m_UVFactors.Used() && !PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(m_CommonBuffers.m_UVFactors.m_Buffer).Valid()))
			return false;
	}

	outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Position] = outDrawCall.m_VertexBuffers.First();

	if (m_ColorStreamIdx.Valid())
		outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Color] =  m_AdditionalFieldsBatch.m_Fields[m_ColorStreamIdx].m_Buffer.m_Buffer;

	// Push additional inputs vertex buffers:
	for (const auto &additionalField : m_AdditionalFieldsBatch.m_Fields)
	{
		PK_ASSERT(additionalField.m_Buffer.Used());
		if (!PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(additionalField.m_Buffer.m_Buffer).Valid()))
			return false;
	}

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	// Editor only: for debugging purposes, we'll remove that from samples code later
	PKSample::SRHIRenderContext	&ctxEditor = *static_cast<PKSample::SRHIRenderContext*>(&ctx);
	if ((ctxEditor.Selection().HasParticlesSelected() || ctxEditor.Selection().HasRendersSelected()) && m_IsParticleSelected.Used())
	{
		outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_IsParticleSelected] = m_IsParticleSelected.m_Buffer;
	}
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)

	return true;
}

//----------------------------------------------------------------------------
//
// CRHIRendererBatch_Mesh_CPU
//
//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Mesh_CPU::Setup(const CRendererDataBase *renderer, const CParticleRenderMedium *owner, const CFrameCollector *fc, const CStringId &storageClass)
{
	if (!CRendererBatchJobs_Mesh_CPUBB::Setup(renderer, owner, fc, storageClass))
		return false;

	// Setup additional fields:
	// The additional fields are supposed to be the same for all renderers in a batch.
	// If not, then you can recompute then on the "Bind()" method.

	const auto		&toGenerate = m_DrawPass->m_ToGenerate;
	const u32		additionalFieldsCount = toGenerate.m_AdditionalGeneratedInputs.Count();

	if (!PK_VERIFY(m_AdditionalFieldsBatch.m_Fields.Reserve(additionalFieldsCount)))
		return false;

	for (u32 i = 0; i < additionalFieldsCount; ++i)
	{
		if (toGenerate.m_AdditionalGeneratedInputs[i].m_Name == BasicRendererProperties::SID_MeshLOD_LOD())
			continue; // ignored field

		const u32	typeSize = CBaseTypeTraits::Traits(toGenerate.m_AdditionalGeneratedInputs[i].m_Type).Size;

		m_AdditionalFieldsBatch.m_Fields.PushBackUnsafe(SAdditionalInputs(typeSize, i));
	}

	m_ColorStreamIdx = _GetDrawDebugColorIndex(m_AdditionalFieldsBatch, toGenerate);

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Mesh_CPU::AreRenderersCompatible(const CRendererDataBase *rendererA, const CRendererDataBase *rendererB) const
{
	return	CRendererBatchJobs_Mesh_CPUBB::AreRenderersCompatible(rendererA, rendererB) &&
			AreBillboardingBatchable(rendererA->m_RendererCache, rendererB->m_RendererCache);
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Mesh_CPU::AllocBuffers(PopcornFX::SRenderContext &ctx)
{
	(void)ctx;

	const u32		totalParticleCount = m_DrawPass->m_TotalParticleCount;
	const u32		totalParticleCountAligned = Mem::Align<0x100>(totalParticleCount);
	const auto		&toGenerate = m_DrawPass->m_ToGenerate;

	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Matrices Vertex Buffer"), (toGenerate.m_GeneratedInputs & Drawers::GenInput_Matrices) != 0, m_ApiManager, m_Matrices, RHI::VertexBuffer, totalParticleCountAligned * sizeof(CFloat4x4), totalParticleCount * sizeof(CFloat4x4)))
		return false;

	if (!m_AdditionalFieldsBatch.AllocBuffers(totalParticleCount, m_ApiManager))
		return false;

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	// Editor only: for debugging purposes, we'll remove that from samples code later
	PKSample::SRHIRenderContext	&ctxEditor = *static_cast<PKSample::SRHIRenderContext*>(&ctx);
	if (ctxEditor.Selection().HasParticlesSelected() || ctxEditor.Selection().HasRendersSelected())
	{
		if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Selection Vertex Buffer"), true, m_ApiManager, m_IsParticleSelected, RHI::VertexBuffer, totalParticleCountAligned * sizeof(float), totalParticleCount * sizeof(float)))
			return false;
	}
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Mesh_CPU::MapBuffers(SRenderContext &ctx)
{
	(void)ctx;

	const u32		totalParticleCount = m_DrawPass->m_TotalParticleCount;
	const auto		&toGenerate = m_DrawPass->m_ToGenerate;

	if (toGenerate.m_GeneratedInputs & Drawers::GenInput_Matrices)
	{
		PK_ASSERT(m_Matrices.Used());
		CFloat4x4	*mappedMatrices = static_cast<CFloat4x4*>(m_ApiManager->MapCpuView(m_Matrices.m_Buffer, 0, totalParticleCount * sizeof(CFloat4x4)));
		if (!PK_VERIFY(mappedMatrices != null))
			return false;
		m_BBJobs_Mesh.m_Exec_Matrices.m_Matrices = TMemoryView<CFloat4x4>(mappedMatrices, totalParticleCount);
	}

	if (!m_AdditionalFieldsBatch.MapBuffers(totalParticleCount, m_ApiManager))
		return false;
	m_BBJobs_Mesh.m_Exec_CopyField.m_FieldsToCopy = m_AdditionalFieldsBatch.m_MappedFields;

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	// Editor only: for debugging purposes, we'll remove that from samples code later
	PKSample::SRHIRenderContext	&ctxEditor = *static_cast<PKSample::SRHIRenderContext*>(&ctx);
	if (ctxEditor.Selection().HasParticlesSelected() || ctxEditor.Selection().HasRendersSelected())
	{
		m_MeshCustomParticleSelectTask.Clear();
		PK_ASSERT(m_IsParticleSelected.Used());
		void	*mappedValue = m_ApiManager->MapCpuView(m_IsParticleSelected.m_Buffer, 0, sizeof(float) * m_DrawPass->m_TotalParticleCount);
		if (!PK_VERIFY(mappedValue != null))
			return false;
		m_MeshCustomParticleSelectTask.m_DstSelectedParticles = TStridedMemoryView<float>(static_cast<float*>(mappedValue), m_DrawPass->m_TotalParticleCount, sizeof(float));
		m_MeshCustomParticleSelectTask.m_SrcParticleSelected = ctxEditor.Selection();
	}
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Mesh_CPU::LaunchCustomTasks(SRenderContext &ctx)
{
	(void)ctx;
#if	(PK_HAS_PARTICLES_SELECTION != 0)
	// Editor only: for debugging purposes, we'll remove that from samples code later
	PKSample::SRHIRenderContext	&ctxEditor = *static_cast<PKSample::SRHIRenderContext*>(&ctx);
	if (ctxEditor.Selection().HasParticlesSelected() || ctxEditor.Selection().HasRendersSelected())
	{
		m_BB_Mesh.AddExecPage(&m_MeshCustomParticleSelectTask);
	}
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)
	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Mesh_CPU::UnmapBuffers(SRenderContext &ctx)
{
	(void)ctx;

	m_Matrices.UnmapIFN(m_ApiManager);

	m_AdditionalFieldsBatch.UnmapBuffers(m_ApiManager);

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	m_IsParticleSelected.UnmapIFN(m_ApiManager);
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Mesh_CPU::EmitDrawCall(SRenderContext &ctx, const SDrawCallDesc &toEmit)
{
	SRHIRenderContext	&renderContext = static_cast<SRHIRenderContext&>(ctx);

	// No need to iterate on all draw requests, just take the first as reference as they wouldn't have been batched if not compatible
	CRendererCacheInstance_UpdateThread			*refCacheInstance = static_cast<CRendererCacheInstance_UpdateThread*>(toEmit.m_RendererCaches.First().Get());
	if (!PK_VERIFY(refCacheInstance != null))
		return false;
	PKSample::PCRendererCacheInstance		rCacheInstance = refCacheInstance->RenderThread_GetCacheInstance();

	if (rCacheInstance == null)
		return false;

	// Mesh not resolved, shouldn't assert.
	if (rCacheInstance->m_AdditionalGeometry == null)
		return true;

	// Will search the attribute description in render state, because renderer features has been checked at renderer cache build in MaterialToRHI.cpp:256 (_SetGeneratedInputs)
	const u32			shaderOptions = PKSample::Option_VertexPassThrough;

	TMemoryView<const RHI::SVertexAttributeDesc>	vertexBuffDescView;
	if (rCacheInstance->m_Cache != null)
	{
		RHI::PCRenderState	renderState = rCacheInstance->m_Cache->GetRenderState(static_cast<PKSample::EShaderOptions>(shaderOptions));
		if (renderState != null)
		{
			vertexBuffDescView = renderState->m_RenderState.m_ShaderBindings.m_InputAttributes.View();

			PK_ONLY_IF_ASSERTS({
				const u32	vertexBufferMaxCount = Utils::__MaxMeshSemantics + 1 + m_AdditionalFieldsBatch.m_Fields.Count();
				PK_ASSERT(vertexBuffDescView.Count() <= vertexBufferMaxCount);
			});
		}
	}

	const u32	subMeshCount = rCacheInstance->m_AdditionalGeometry->m_PerGeometryViews.Count();
	if (subMeshCount == 0)
		return false;

	const TMemoryView<const u32>	perMeshParticleCount = m_BB_Mesh.PerMeshParticleCount();
	const TMemoryView<const u32>	perMeshBufferOffset = m_BB_Mesh.PerMeshBufferOffset();

	PK_ASSERT(perMeshParticleCount.Count() == subMeshCount);

	for (u32 iSubMesh = 0; iSubMesh < subMeshCount; ++iSubMesh)
	{
		if (perMeshParticleCount[iSubMesh] == 0)
			continue;

		const u32	particleOffset = perMeshBufferOffset[iSubMesh];

		if (!PK_VERIFY(renderContext.m_DrawOutputs.m_DrawCalls.PushBack().Valid()))
		{
			CLog::Log(PK_ERROR, "Failed to create a draw-call");
			return false;
		}
		SRHIDrawCall	&outDrawCall = renderContext.m_DrawOutputs.m_DrawCalls.Last();

		outDrawCall.m_Batch = this;
		outDrawCall.m_RendererCacheInstance = refCacheInstance;
		outDrawCall.m_Type = SRHIDrawCall::DrawCall_IndexedInstanced;
		outDrawCall.m_ShaderOptions = shaderOptions;
		outDrawCall.m_RendererType = Renderer_Mesh;

		// Some meta-data (the Editor uses them)
		{
			outDrawCall.m_BBox = toEmit.m_BBox;
			outDrawCall.m_TotalBBox = m_DrawPass->m_TotalBBox;
			outDrawCall.m_SlicedDC = toEmit.m_TotalParticleCount != m_DrawPass->m_TotalParticleCount;
		}
		outDrawCall.m_Valid =	refCacheInstance != null &&
								rCacheInstance != null &&
								rCacheInstance->m_Cache != null &&
								rCacheInstance->m_Cache->GetRenderState(static_cast<EShaderOptions>(shaderOptions)) != null;

		// Try gathering mesh from renderer cache instance
		bool	success = true;

		// Push mesh buffers:
		const Utils::GpuBufferViews						&bufferView = rCacheInstance->m_AdditionalGeometry->m_PerGeometryViews[iSubMesh];

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
			if (!success)
			{
				CLog::Log(PK_ERROR, "Missing mesh data required by the material");
				return false;
			}
		}

		outDrawCall.m_InstanceCount = perMeshParticleCount[iSubMesh];

		// Matrices:
		success &= outDrawCall.m_VertexOffsets.PushBack(particleOffset * u32(sizeof(CFloat4x4))).Valid();
		success &= outDrawCall.m_VertexBuffers.PushBack(m_Matrices.m_Buffer).Valid();

		// Additional inputs:
		for (const auto &additionalField : m_AdditionalFieldsBatch.m_Fields)
		{
			PK_ASSERT(additionalField.m_Buffer.Used());

			const u32	bufferOffset = particleOffset * additionalField.m_ByteSize;

			success &= outDrawCall.m_VertexOffsets.PushBack(bufferOffset).Valid();
			success &= outDrawCall.m_VertexBuffers.PushBack(additionalField.m_Buffer.m_Buffer).Valid();
		}

		PK_ASSERT(vertexBuffDescView.Count() == outDrawCall.m_VertexBuffers.Count() ||
				  (vertexBuffDescView.Empty() && !outDrawCall.m_Valid));

		// Fill the semantics for the debug draws:
		outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Position] = bufferView.m_VertexBuffers[Utils::MeshPositions];
		outDrawCall.m_DebugDrawGPUBufferOffsets[SRHIDrawCall::DebugDrawGPUBuffer_Position] = 0;

		outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_InstanceTransforms] = m_Matrices.m_Buffer;
		outDrawCall.m_DebugDrawGPUBufferOffsets[SRHIDrawCall::DebugDrawGPUBuffer_InstanceTransforms] = particleOffset * u32(sizeof(CFloat4x4));

		if (m_ColorStreamIdx.Valid())
		{
			outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Color] =  m_AdditionalFieldsBatch.m_Fields[m_ColorStreamIdx].m_Buffer.m_Buffer;
			outDrawCall.m_DebugDrawGPUBufferOffsets[SRHIDrawCall::DebugDrawGPUBuffer_Color] = particleOffset * m_AdditionalFieldsBatch.m_Fields[m_ColorStreamIdx].m_ByteSize;
		}

#if	(PK_HAS_PARTICLES_SELECTION != 0)
		// Editor only: for debugging purposes, we'll remove that from samples code later
		PKSample::SRHIRenderContext	&ctxEditor = *static_cast<PKSample::SRHIRenderContext*>(&ctx);
		if ((ctxEditor.Selection().HasParticlesSelected() || ctxEditor.Selection().HasRendersSelected()) && m_IsParticleSelected.Used())
		{
			outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_IsParticleSelected] = m_IsParticleSelected.m_Buffer;
			outDrawCall.m_DebugDrawGPUBufferOffsets[SRHIDrawCall::DebugDrawGPUBuffer_IsParticleSelected] = particleOffset * u32(sizeof(float));
		}
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)

		if (!success)
		{
			CLog::Log(PK_ERROR, "Could not add all the mesh vertex data");
			return false;
		}

		// Set index buffer
		outDrawCall.m_IndexBuffer = bufferView.m_IndexBuffer;
		if (bufferView.m_IndexBufferSize == RHI::IndexBuffer16Bit)
			outDrawCall.m_IndexCount = bufferView.m_IndexBuffer->GetByteSize() / sizeof(u16);
		else if (bufferView.m_IndexBufferSize == RHI::IndexBuffer32Bit)
			outDrawCall.m_IndexCount = bufferView.m_IndexBuffer->GetByteSize() / sizeof(u32);
		else
		{
			PK_ASSERT_NOT_REACHED();
			return false;
		}
		outDrawCall.m_IndexSize = bufferView.m_IndexBufferSize;
		outDrawCall.m_IndexOffset = 0;
	}
	return true;
}

//----------------------------------------------------------------------------
//
// CRHIRendererBatch_Decal_CPU
//
//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Decal_CPU::Setup(const CRendererDataBase *renderer, const CParticleRenderMedium *owner, const CFrameCollector *fc, const CStringId &storageClass)
{
	if (!CRendererBatchJobs_Decal_CPUBB::Setup(renderer, owner, fc, storageClass))
		return false;

	// Setup additional fields:
	// The additional fields are supposed to be the same for all renderers in a batch.
	// If not, then you can recompute then on the "Bind()" method.

	const auto		&toGenerate = m_DrawPass->m_ToGenerate;
	const u32		additionalFieldsCount = toGenerate.m_AdditionalGeneratedInputs.Count();

	if (!PK_VERIFY(m_AdditionalFieldsBatch.m_Fields.Reserve(additionalFieldsCount)))
		return false;

	for (u32 i = 0; i < additionalFieldsCount; ++i)
	{
		if (toGenerate.m_AdditionalGeneratedInputs[i].m_Name == BasicRendererProperties::SID_MeshLOD_LOD())
			continue; // ignored field

		const u32	typeSize = CBaseTypeTraits::Traits(toGenerate.m_AdditionalGeneratedInputs[i].m_Type).Size;
		m_AdditionalFieldsBatch.m_Fields.PushBackUnsafe(SAdditionalInputs(typeSize, i));
	}

	m_ColorStreamIdx = _GetDrawDebugColorIndex(m_AdditionalFieldsBatch, toGenerate);

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Decal_CPU::AreRenderersCompatible(const CRendererDataBase *rendererA, const CRendererDataBase *rendererB) const
{
	return	CRendererBatchJobs_Decal_CPUBB::AreRenderersCompatible(rendererA, rendererB) &&
			AreBillboardingBatchable(rendererA->m_RendererCache, rendererB->m_RendererCache);
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Decal_CPU::AllocBuffers(PopcornFX::SRenderContext &ctx)
{
	(void)ctx;

	const u32		totalParticleCount = m_DrawPass->m_TotalParticleCount;
	const u32		totalParticleCountAligned = Mem::Align<0x100>(totalParticleCount);
	const auto		&toGenerate = m_DrawPass->m_ToGenerate;

	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Matrices Vertex Buffer"), (toGenerate.m_GeneratedInputs & Drawers::GenInput_Matrices) != 0, m_ApiManager, m_Matrices, RHI::VertexBuffer, totalParticleCountAligned * sizeof(CFloat4x4), totalParticleCount * sizeof(CFloat4x4)))
		return false;
	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("InvMatrices Vertex Buffer"), (toGenerate.m_GeneratedInputs & Drawers::GenInput_Matrices) != 0, m_ApiManager, m_InvMatrices, RHI::VertexBuffer, totalParticleCountAligned * sizeof(CFloat4x4), totalParticleCount * sizeof(CFloat4x4)))
		return false;

	if (!m_AdditionalFieldsBatch.AllocBuffers(totalParticleCount, m_ApiManager))
		return false;

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	// Editor only: for debugging purposes, we'll remove that from samples code later
	PKSample::SRHIRenderContext	&ctxEditor = *static_cast<PKSample::SRHIRenderContext*>(&ctx);
	if (ctxEditor.Selection().HasParticlesSelected() || ctxEditor.Selection().HasRendersSelected())
	{
		if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Selection Vertex Buffer"), true, m_ApiManager, m_IsParticleSelected, RHI::VertexBuffer, totalParticleCountAligned * sizeof(float), totalParticleCount * sizeof(float)))
			return false;
	}
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Decal_CPU::MapBuffers(SRenderContext &ctx)
{
	(void)ctx;

	const u32		totalParticleCount = m_DrawPass->m_TotalParticleCount;
	const auto		&toGenerate = m_DrawPass->m_ToGenerate;

	if (toGenerate.m_GeneratedInputs & Drawers::GenInput_Matrices)
	{
		PK_ASSERT(m_Matrices.Used());
		CFloat4x4	*mappedMatrices = static_cast<CFloat4x4*>(m_ApiManager->MapCpuView(m_Matrices.m_Buffer, 0, totalParticleCount * sizeof(CFloat4x4)));
		if (!PK_VERIFY(mappedMatrices != null))
			return false;
		CFloat4x4	*mappedInvMatrices = static_cast<CFloat4x4*>(m_ApiManager->MapCpuView(m_InvMatrices.m_Buffer, 0, totalParticleCount * sizeof(CFloat4x4)));
		if (!PK_VERIFY(mappedInvMatrices != null))
			return false;

		m_BBJobs_Decal.m_Exec_Matrices.m_Matrices = TMemoryView<CFloat4x4>(mappedMatrices, totalParticleCount);
		m_BBJobs_Decal.m_Exec_Matrices.m_InvMatrices = TMemoryView<CFloat4x4>(mappedInvMatrices, totalParticleCount);
	}

	if (!m_AdditionalFieldsBatch.MapBuffers(totalParticleCount, m_ApiManager))
		return false;
	m_CopyAdditionalFieldsTask.m_FieldsToCopy = m_AdditionalFieldsBatch.m_MappedFields;

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	// Editor only: for debugging purposes, we'll remove that from samples code later
	PKSample::SRHIRenderContext	&ctxEditor = *static_cast<PKSample::SRHIRenderContext*>(&ctx);
	if (ctxEditor.Selection().HasParticlesSelected() || ctxEditor.Selection().HasRendersSelected())
	{
		m_CopyStreamCustomParticleSelectTask.Clear();
		PK_ASSERT(m_IsParticleSelected.Used());
		void	*mappedValue = m_ApiManager->MapCpuView(m_IsParticleSelected.m_Buffer, 0, sizeof(float) * m_DrawPass->m_TotalParticleCount);
		if (!PK_VERIFY(mappedValue != null))
			return false;
		m_CopyStreamCustomParticleSelectTask.m_DstSelectedParticles = TStridedMemoryView<float>(static_cast<float*>(mappedValue), m_DrawPass->m_TotalParticleCount, sizeof(float));
		m_CopyStreamCustomParticleSelectTask.m_SrcParticleSelected = ctxEditor.Selection();
	}
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Decal_CPU::LaunchCustomTasks(SRenderContext &ctx)
{
	(void)ctx;
	if (!m_CopyTasks.Prepare(m_DrawPass->m_DrawRequests.View()))
		return false;
	m_CopyTasks.AddExecAsyncPage(&m_CopyAdditionalFieldsTask);
#if	(PK_HAS_PARTICLES_SELECTION != 0)
	// Editor only: for debugging purposes, we'll remove that from samples code later
	PKSample::SRHIRenderContext	&ctxEditor = *static_cast<PKSample::SRHIRenderContext*>(&ctx);
	if (ctxEditor.Selection().HasParticlesSelected() || ctxEditor.Selection().HasRendersSelected())
	{
		m_CopyTasks.AddExecAsyncPage(&m_CopyStreamCustomParticleSelectTask);
	}
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)
	m_CopyTasks.LaunchTasks(null);
	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Decal_CPU::WaitForCustomTasks(SRenderContext &ctx)
{
	(void)ctx;
	m_CopyTasks.WaitTasks();
	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Decal_CPU::UnmapBuffers(SRenderContext &ctx)
{
	(void)ctx;

	m_Matrices.UnmapIFN(m_ApiManager);
	m_InvMatrices.UnmapIFN(m_ApiManager);

	m_AdditionalFieldsBatch.UnmapBuffers(m_ApiManager);

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	m_IsParticleSelected.UnmapIFN(m_ApiManager);
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Decal_CPU::EmitDrawCall(SRenderContext &ctx, const SDrawCallDesc &toEmit)
{
	SRHIRenderContext	&renderContext = static_cast<SRHIRenderContext&>(ctx);

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
	// Cube mesh used to render the decals not created:
	if (!PK_VERIFY(rCacheInstance->m_AdditionalGeometry != null))
		return false;
	if (!PK_VERIFY(rCacheInstance->m_AdditionalGeometry->m_PerGeometryViews.Count() == 1))
		return false;

	if (!PK_VERIFY(renderContext.m_DrawOutputs.m_DrawCalls.PushBack().Valid()))
	{
		CLog::Log(PK_ERROR, "Failed to create a draw-call");
		return false;
	}
	SRHIDrawCall	&outDrawCall = renderContext.m_DrawOutputs.m_DrawCalls.Last();

	outDrawCall.m_Batch = this;
	outDrawCall.m_RendererCacheInstance = renderCacheInstance;
	outDrawCall.m_Type = SRHIDrawCall::DrawCall_IndexedInstanced;
	outDrawCall.m_ShaderOptions = PKSample::Option_VertexPassThrough;
	outDrawCall.m_RendererType = Renderer_Decal;

	const Utils::GpuBufferViews		&bufferView = rCacheInstance->m_AdditionalGeometry->m_PerGeometryViews.First();

	RHI::PCRenderState					renderState = rCacheInstance->m_Cache->GetRenderState(PKSample::Option_VertexPassThrough);
	if (!PK_VERIFY(renderState != null))
		return false;
	TMemoryView<const RHI::SVertexAttributeDesc>	vertexBuffDescView = renderState->m_RenderState.m_ShaderBindings.m_InputAttributes.View();
	if (!PK_VERIFY(outDrawCall.m_VertexBuffers.Reserve(vertexBuffDescView.Count())))
		return false;

	bool	success = true;

	// Cube positions:
	success &= outDrawCall.m_VertexBuffers.PushBack(bufferView.m_VertexBuffers.First()).Valid();
	// Matrices:
	success &= outDrawCall.m_VertexBuffers.PushBack(m_Matrices.m_Buffer).Valid();
	// Inv matrices:
	success &= outDrawCall.m_VertexBuffers.PushBack(m_InvMatrices.m_Buffer).Valid();

	// Additional inputs:
	for (const auto &additionalField : m_AdditionalFieldsBatch.m_Fields)
	{
		PK_ASSERT(additionalField.m_Buffer.Used());
		success &= outDrawCall.m_VertexBuffers.PushBack(additionalField.m_Buffer.m_Buffer).Valid();
	}

	// Fill the semantics for the debug draws:
	outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Position] = outDrawCall.m_VertexBuffers.First();
	outDrawCall.m_DebugDrawGPUBufferOffsets[SRHIDrawCall::DebugDrawGPUBuffer_Position] = 0;

	outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_InstanceTransforms] = m_Matrices.m_Buffer;
	outDrawCall.m_DebugDrawGPUBufferOffsets[SRHIDrawCall::DebugDrawGPUBuffer_InstanceTransforms] = 0;

	if (m_ColorStreamIdx.Valid())
	{
		outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Color] =  m_AdditionalFieldsBatch.m_Fields[m_ColorStreamIdx].m_Buffer.m_Buffer;
		outDrawCall.m_DebugDrawGPUBufferOffsets[SRHIDrawCall::DebugDrawGPUBuffer_Color] = 0;
	}

	// Set index buffer
	outDrawCall.m_IndexBuffer = bufferView.m_IndexBuffer;
	if (bufferView.m_IndexBufferSize == RHI::IndexBuffer16Bit)
		outDrawCall.m_IndexCount = bufferView.m_IndexBuffer->GetByteSize() / sizeof(u16);
	else if (bufferView.m_IndexBufferSize == RHI::IndexBuffer32Bit)
		outDrawCall.m_IndexCount = bufferView.m_IndexBuffer->GetByteSize() / sizeof(u32);
	else
	{
		PK_ASSERT_NOT_REACHED();
		return false;
	}
	outDrawCall.m_IndexSize = bufferView.m_IndexBufferSize;
	outDrawCall.m_IndexOffset = 0;

	outDrawCall.m_InstanceCount = m_DrawPass->m_TotalParticleCount;

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	// Editor only: for debugging purposes, we'll remove that from samples code later
	PKSample::SRHIRenderContext	&ctxEditor = *static_cast<PKSample::SRHIRenderContext*>(&ctx);
	if ((ctxEditor.Selection().HasParticlesSelected() || ctxEditor.Selection().HasRendersSelected()) && m_IsParticleSelected.Used())
	{
		outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_IsParticleSelected] = m_IsParticleSelected.m_Buffer;
		outDrawCall.m_DebugDrawGPUBufferOffsets[SRHIDrawCall::DebugDrawGPUBuffer_IsParticleSelected] = 0;
	}
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)

	if (!success)
	{
		CLog::Log(PK_ERROR, "Could not emit the decal draw call");
		return false;
	}
	return true;
}

//----------------------------------------------------------------------------
//
// CRHIRendererBatch_Triangle_CPUBB
//
//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Triangle_CPUBB::Setup(const CRendererDataBase *renderer, const CParticleRenderMedium *owner, const CFrameCollector *fc, const CStringId &storageClass)
{
	if (!CRendererBatchJobs_Triangle_CPUBB::Setup(renderer, owner, fc, storageClass))
		return false;

	// Setup additional fields:
	// The additional fields are supposed to be the same for all renderers in a batch.
	// If not, then you can recompute then on the "Bind()" method.

	const auto		&toGenerate = m_DrawPass->m_ToGenerate;
	const u32		additionalFieldsCount = toGenerate.m_AdditionalGeneratedInputs.Count();

	if (!PK_VERIFY(m_AdditionalFieldsBatch.m_Fields.Reserve(additionalFieldsCount)))
		return false;

	for (u32 i = 0; i < additionalFieldsCount; ++i)
	{
		if (toGenerate.m_AdditionalGeneratedInputs[i].m_Name == BasicRendererProperties::SID_TriangleCustomNormals_Normal1() ||
			toGenerate.m_AdditionalGeneratedInputs[i].m_Name == BasicRendererProperties::SID_TriangleCustomNormals_Normal2() ||
			toGenerate.m_AdditionalGeneratedInputs[i].m_Name == BasicRendererProperties::SID_TriangleCustomNormals_Normal3() ||
			toGenerate.m_AdditionalGeneratedInputs[i].m_Name == BasicRendererProperties::SID_TriangleCustomUVs_UV1() ||
			toGenerate.m_AdditionalGeneratedInputs[i].m_Name == BasicRendererProperties::SID_TriangleCustomUVs_UV2() ||
			toGenerate.m_AdditionalGeneratedInputs[i].m_Name == BasicRendererProperties::SID_TriangleCustomUVs_UV3())
			continue; // ignore the field (declared as additional-input but not given as vertex-buffer)

		const u32	typeSize = CBaseTypeTraits::Traits(toGenerate.m_AdditionalGeneratedInputs[i].m_Type).Size;
		m_AdditionalFieldsBatch.m_Fields.PushBackUnsafe(SAdditionalInputs(typeSize, i));
	}

	m_ColorStreamIdx = _GetDrawDebugColorIndex(m_AdditionalFieldsBatch, toGenerate);

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Triangle_CPUBB::AreRenderersCompatible(const CRendererDataBase *rendererA, const CRendererDataBase *rendererB) const
{
	return CRendererBatchJobs_Triangle_CPUBB::AreRenderersCompatible(rendererA, rendererB) && AreBillboardingBatchable(rendererA->m_RendererCache, rendererB->m_RendererCache);
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Triangle_CPUBB::AllocBuffers(PopcornFX::SRenderContext &ctx)
{
	(void)ctx;

	const u32		totalVertexCount = m_BB_Triangle.TotalVertexCount();
	const u32		totalVertexCountAligned = Mem::Align<0x100>(totalVertexCount);
	const u32		totalIndexCount = m_BB_Triangle.TotalIndexCount();
	const u32		totalIndexCountAligned = Mem::Align<0x100>(totalIndexCount);
	const auto		&toGenerate = m_DrawPass->m_ToGenerate;

	m_IndexSize = (totalVertexCount > 0xFFFF) ? sizeof(u32) : sizeof(u16);

	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Indices Buffer"), (toGenerate.m_GeneratedInputs & Drawers::GenInput_Indices) != 0, m_ApiManager, m_Indices, RHI::IndexBuffer, totalIndexCountAligned * m_IndexSize, totalIndexCount * m_IndexSize))
		return false;
	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Positions Vertex Buffer"), (toGenerate.m_GeneratedInputs & Drawers::GenInput_Position) != 0, m_ApiManager, m_Positions, RHI::VertexBuffer, totalVertexCountAligned * sizeof(CFloat4), totalVertexCount * sizeof(CFloat4)))
		return false;
	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("UV0s Vertex Buffer"), (toGenerate.m_GeneratedInputs & Drawers::GenInput_UV0) != 0, m_ApiManager, m_TexCoords0, RHI::VertexBuffer, totalVertexCountAligned * sizeof(CFloat2), totalVertexCount * sizeof(CFloat2)))
		return false;
	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Normals Vertex Buffer"), (toGenerate.m_GeneratedInputs & Drawers::GenInput_Normal) != 0, m_ApiManager, m_Normals, RHI::VertexBuffer, totalVertexCountAligned * sizeof(CFloat4), totalVertexCount * sizeof(CFloat4)))
		return false;
	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Tangents Vertex Buffer"), (toGenerate.m_GeneratedInputs & Drawers::GenInput_Tangent) != 0, m_ApiManager, m_Tangents, RHI::VertexBuffer, totalVertexCountAligned * sizeof(CFloat4), totalVertexCount * sizeof(CFloat4)))
		return false;

	if (!m_AdditionalFieldsBatch.AllocBuffers(totalVertexCount, m_ApiManager))
		return false;

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	// Editor only: for debugging purposes, we'll remove that from samples code later
	PKSample::SRHIRenderContext	&ctxEditor = *static_cast<PKSample::SRHIRenderContext*>(&ctx);
	if (ctxEditor.Selection().HasParticlesSelected() || ctxEditor.Selection().HasRendersSelected())
	{
		if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Selection Vertex Buffer"), true, m_ApiManager, m_IsParticleSelected, RHI::VertexBuffer, totalVertexCountAligned * sizeof(float), totalVertexCount * sizeof(float)))
			return false;
	}
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Triangle_CPUBB::MapBuffers(SRenderContext &ctx)
{
	(void)ctx;

	const u32		totalVertexCount = m_BB_Triangle.TotalVertexCount();
	const u32		totalIndexCount = m_BB_Triangle.TotalIndexCount();
	const auto		&toGenerate = m_DrawPass->m_ToGenerate;

	if (toGenerate.m_GeneratedInputs & Drawers::GenInput_Indices)
	{
		PK_ASSERT(m_Indices.Used());
		void	*mappedValue = m_ApiManager->MapCpuView(m_Indices.m_Buffer, 0, totalIndexCount * m_IndexSize);
		if (!PK_VERIFY(mappedValue != null))
			return false;
		m_BBJobs_Triangle.m_Exec_Indices.m_IndexStream.Setup(mappedValue, totalIndexCount, m_IndexSize == sizeof(u32));
	}
	if (toGenerate.m_GeneratedInputs & Drawers::GenInput_Position)
	{
		PK_ASSERT(m_Positions.Used());
		void	*mappedValue = m_ApiManager->MapCpuView(m_Positions.m_Buffer, 0, totalVertexCount * sizeof(CFloat4));
		if (!PK_VERIFY(mappedValue != null))
			return false;
		m_BBJobs_Triangle.m_Exec_PNT.m_Positions = TStridedMemoryView<CFloat3, 0x10>(static_cast<CFloat3*>(mappedValue), totalVertexCount, 0x10);
	}
	if (toGenerate.m_GeneratedInputs & Drawers::GenInput_Normal)
	{
		PK_ASSERT(m_Normals.Used());
		void	*mappedValue = m_ApiManager->MapCpuView(m_Normals.m_Buffer, 0, totalVertexCount * sizeof(CFloat4));
		if (!PK_VERIFY(mappedValue != null))
			return false;
		m_BBJobs_Triangle.m_Exec_PNT.m_Normals = TStridedMemoryView<CFloat3, 0x10>(static_cast<CFloat3*>(mappedValue), totalVertexCount, 0x10);
	}
	if (toGenerate.m_GeneratedInputs & Drawers::GenInput_Tangent)
	{
		PK_ASSERT(m_Tangents.Used());
		void	*mappedValue = m_ApiManager->MapCpuView(m_Tangents.m_Buffer, 0, totalVertexCount * sizeof(CFloat4));
		if (!PK_VERIFY(mappedValue != null))
			return false;
		m_BBJobs_Triangle.m_Exec_PNT.m_Tangents = TStridedMemoryView<CFloat4, 0x10>(static_cast<CFloat4*>(mappedValue), totalVertexCount, 0x10);
	}
	if (toGenerate.m_GeneratedInputs & Drawers::GenInput_UV0)
	{
		PK_ASSERT(m_TexCoords0.Used());
		void	*mappedValue = m_ApiManager->MapCpuView(m_TexCoords0.m_Buffer, 0, totalVertexCount * sizeof(CFloat2));
		if (!PK_VERIFY(mappedValue != null))
			return false;
		m_BBJobs_Triangle.m_Exec_PNT.m_Texcoords = TStridedMemoryView<CFloat2>(static_cast<CFloat2*>(mappedValue), totalVertexCount);
	}

	if (!m_AdditionalFieldsBatch.MapBuffers(totalVertexCount, m_ApiManager))
		return false;
	m_BBJobs_Triangle.m_Exec_CopyField.m_FieldsToCopy = m_AdditionalFieldsBatch.m_MappedFields;

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	// Editor only: for debugging purposes, we'll remove that from samples code later
	PKSample::SRHIRenderContext	&ctxEditor = *static_cast<PKSample::SRHIRenderContext*>(&ctx);
	if (ctxEditor.Selection().HasParticlesSelected() || ctxEditor.Selection().HasRendersSelected())
	{
		m_TriangleCustomParticleSelectTask.Clear();
		PK_ASSERT(m_IsParticleSelected.Used());
		void	*mappedValue = m_ApiManager->MapCpuView(m_IsParticleSelected.m_Buffer, 0, sizeof(float) * totalVertexCount);
		if (!PK_VERIFY(mappedValue != null))
			return false;
		m_TriangleCustomParticleSelectTask.m_DstSelectedParticles = TStridedMemoryView<float>(static_cast<float*>(mappedValue), totalVertexCount, sizeof(float));
		m_TriangleCustomParticleSelectTask.m_SrcParticleSelected = ctxEditor.Selection();
	}
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Triangle_CPUBB::LaunchCustomTasks(SRenderContext &ctx)
{
	(void)ctx;
#if	(PK_HAS_PARTICLES_SELECTION != 0)
	// Editor only: for debugging purposes, we'll remove that from samples code later
	PKSample::SRHIRenderContext	&ctxEditor = *static_cast<PKSample::SRHIRenderContext*>(&ctx);
	if (ctxEditor.Selection().HasParticlesSelected() || ctxEditor.Selection().HasRendersSelected())
	{
		m_BB_Triangle.AddExecPage(&m_TriangleCustomParticleSelectTask);
	}
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)
	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Triangle_CPUBB::UnmapBuffers(SRenderContext &ctx)
{
	(void)ctx;
	m_Indices.UnmapIFN(m_ApiManager);
	m_Positions.UnmapIFN(m_ApiManager);
	m_Normals.UnmapIFN(m_ApiManager);
	m_Tangents.UnmapIFN(m_ApiManager);
	m_TexCoords0.UnmapIFN(m_ApiManager);

	m_AdditionalFieldsBatch.UnmapBuffers(m_ApiManager);

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	m_IsParticleSelected.UnmapIFN(m_ApiManager);
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Triangle_CPUBB::EmitDrawCall(SRenderContext &ctx, const SDrawCallDesc &toEmit)
{
	SRHIRenderContext	&renderContext = static_cast<SRHIRenderContext&>(ctx);

	// No need to iterate on all draw requests, just take the first as reference as they wouldn't have been batched if not compatible
	CRendererCacheInstance_UpdateThread	*refCacheInstance = static_cast<CRendererCacheInstance_UpdateThread*>(toEmit.m_RendererCaches.First().Get());
	if (!PK_VERIFY(refCacheInstance != null))
		return false;

	PKSample::PCRendererCacheInstance	rCacheInstance = refCacheInstance->RenderThread_GetCacheInstance();
	if (!PK_VERIFY(rCacheInstance != null))
		return false;

	if (!PK_VERIFY(renderContext.m_DrawOutputs.m_DrawCalls.PushBack().Valid()))
	{
		CLog::Log(PK_ERROR, "Failed to create a draw-call");
		return false;
	}
	SRHIDrawCall	&outDrawCall = renderContext.m_DrawOutputs.m_DrawCalls.Last();

	outDrawCall.m_Batch = this;
	outDrawCall.m_RendererCacheInstance = refCacheInstance;
	outDrawCall.m_Type = SRHIDrawCall::DrawCall_Regular;
	outDrawCall.m_ShaderOptions = PKSample::Option_VertexPassThrough;
	outDrawCall.m_RendererType = Renderer_Triangle;

	outDrawCall.m_IndexOffset = toEmit.m_IndexOffset;
	outDrawCall.m_VertexCount = toEmit.m_TotalVertexCount;
	outDrawCall.m_IndexCount = toEmit.m_TotalIndexCount;
	outDrawCall.m_IndexSize = (m_IndexSize == sizeof(u32)) ? RHI::IndexBuffer32Bit : RHI::IndexBuffer16Bit;

	// Some meta-data (the Editor uses them)
	{
		outDrawCall.m_BBox = toEmit.m_BBox;
		outDrawCall.m_TotalBBox = m_DrawPass->m_TotalBBox;
		outDrawCall.m_SlicedDC = toEmit.m_TotalParticleCount != m_DrawPass->m_TotalParticleCount;
	}

	outDrawCall.m_IndexBuffer = m_Indices.m_Buffer;
	PK_ASSERT(outDrawCall.m_IndexBuffer != null);

	if (m_Positions.Used() && !PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(m_Positions.m_Buffer).Valid()))
		return false;
	if (m_Normals.Used() && !PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(m_Normals.m_Buffer).Valid()))
		return false;
	if (m_Tangents.Used() && !PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(m_Tangents.m_Buffer).Valid()))
		return false;
	if (m_TexCoords0.Used() && !PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(m_TexCoords0.m_Buffer).Valid()))
		return false;

	outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Position] = outDrawCall.m_VertexBuffers.First();

	if (m_ColorStreamIdx.Valid())
		outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Color] =  m_AdditionalFieldsBatch.m_Fields[m_ColorStreamIdx].m_Buffer.m_Buffer;

	// Push additional inputs vertex buffers:
	for (const auto &additionalField : m_AdditionalFieldsBatch.m_Fields)
	{
		PK_ASSERT(additionalField.m_Buffer.Used());
		if (!PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(additionalField.m_Buffer.m_Buffer).Valid()))
			return false;
	}

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	// Editor only: for debugging purposes, we'll remove that from samples code later
	PKSample::SRHIRenderContext	&ctxEditor = *static_cast<PKSample::SRHIRenderContext*>(&ctx);
	if ((ctxEditor.Selection().HasParticlesSelected() || ctxEditor.Selection().HasRendersSelected()) && m_IsParticleSelected.Used())
	{
		outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_IsParticleSelected] = m_IsParticleSelected.m_Buffer;
	}
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)

	return true;
}

//----------------------------------------------------------------------------
//
// CRHIRendererBatch_Triangle_VertexBB
//
//----------------------------------------------------------------------------

bool CRHIRendererBatch_Triangle_VertexBB::Setup(const CRendererDataBase *renderer, const CParticleRenderMedium *owner, const CFrameCollector *fc, const CStringId &storageClass)
{
	if (!CRendererBatchJobs_Triangle_GPUBB::Setup(renderer, owner, fc, storageClass))
		return false;

	// Setup additional fields:
	// The additional fields are supposed to be the same for all renderers in a batch.
	// If not, then you can recompute then on the "Bind()" method.

	const auto		&toGenerate = m_DrawPass->m_ToGenerate;
	const u32		additionalFieldsCount = toGenerate.m_AdditionalGeneratedInputs.Count();

	if (!PK_VERIFY(m_AdditionalFieldsBatch.m_Fields.Reserve(additionalFieldsCount)))
		return false;

	for (u32 i = 0; i < additionalFieldsCount; ++i)
	{
		const u32	typeSize = CBaseTypeTraits::Traits(toGenerate.m_AdditionalGeneratedInputs[i].m_Type).Size;
		m_AdditionalFieldsBatch.m_Fields.PushBackUnsafe(SAdditionalInputs(typeSize, i));
	}

	m_ColorStreamIdx = _GetDrawDebugColorIndex(m_AdditionalFieldsBatch, toGenerate);

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Triangle_VertexBB::AreRenderersCompatible(const CRendererDataBase *rendererA, const CRendererDataBase *rendererB) const
{
	return CRendererBatchJobs_Triangle_GPUBB::AreRenderersCompatible(rendererA, rendererB) && AreBillboardingBatchable(rendererA->m_RendererCache, rendererB->m_RendererCache);
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Triangle_VertexBB::AllocBuffers(SRenderContext &ctx)
{
	(void)ctx;

	const u32		totalParticleCount = m_BB_Triangle.TotalParticleCount();
	const u32		totalParticleCountAligned = Mem::Align<0x100>(totalParticleCount);
	const auto		&toGenerate = m_DrawPass->m_ToGenerate;

	if (!m_Initialized)
		m_Initialized = _InitStaticBuffers();
	if (!m_Initialized)
		return false;

	// Indices (for sorting)
	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Indices Buffer"), (toGenerate.m_GeneratedInputs & Drawers::GenInput_Indices) != 0, m_ApiManager, m_Indices, RHI::RawBuffer, totalParticleCountAligned * sizeof(u32), totalParticleCount * sizeof(u32)))
		return false;
	const u32	generatedViewCount = toGenerate.m_PerViewGeneratedInputs.Count();
	if (!m_PerViewIndicesBuffers.Resize(generatedViewCount))
		return false;
	for (u32 i = 0; i < generatedViewCount; ++i)
	{
		if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("View Sorted Indices Buffer"), (toGenerate.m_PerViewGeneratedInputs[i].m_GeneratedInputs & Drawers::GenInput_Indices) != 0, m_ApiManager, m_PerViewIndicesBuffers[i], RHI::RawBuffer, totalParticleCountAligned * sizeof(u32), totalParticleCount * sizeof(u32)))
			return false;
	}

	// Vertex positions streams
	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Position0s Buffer"), (toGenerate.m_GeneratedInputs & Drawers::GenInput_ParticlePosition0) != 0, m_ApiManager, m_VertexPositions0, RHI::RawBuffer, totalParticleCountAligned * sizeof(CFloat4), totalParticleCount * sizeof(CFloat4)) ||
		!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Position1s Buffer"), (toGenerate.m_GeneratedInputs & Drawers::GenInput_ParticlePosition1) != 0, m_ApiManager, m_VertexPositions1, RHI::RawBuffer, totalParticleCountAligned * sizeof(CFloat3), totalParticleCount * sizeof(CFloat3)) ||
		!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Position2s Buffer"), (toGenerate.m_GeneratedInputs & Drawers::GenInput_ParticlePosition2) != 0, m_ApiManager, m_VertexPositions2, RHI::RawBuffer, totalParticleCountAligned * sizeof(CFloat3), totalParticleCount * sizeof(CFloat3)))
		return false;

	// Constant buffer filled by CPU task, will contain simple contants per draw request (normals bending factor, ...)
	// Each vertex position 0 will contain its associated draw request ID in position's W component (See sample vertex shader for more detail)
	PK_STATIC_ASSERT(sizeof(Drawers::STriangleDrawRequest) == sizeof(CFloat2));
	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Triangle DrawRequests Buffer"), true, m_ApiManager, m_DrawRequests, RHI::ConstantBuffer, kMaxGeomDrawRequestCount * sizeof(Drawers::STriangleDrawRequest), kMaxGeomDrawRequestCount * sizeof(Drawers::STriangleDrawRequest)))
		return false;

	if (!m_AdditionalFieldsBatch.AllocBuffers(totalParticleCount, m_ApiManager, RHI::RawBuffer))
		return false;

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	// Editor only: for debugging purposes, we'll remove that from samples code later
	PKSample::SRHIRenderContext	&ctxEditor = *static_cast<PKSample::SRHIRenderContext*>(&ctx);
	if (ctxEditor.Selection().HasParticlesSelected() || ctxEditor.Selection().HasRendersSelected())
	{
		if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Selection Vertex Buffer"), true, m_ApiManager, m_IsParticleSelected, RHI::RawBuffer, totalParticleCountAligned * sizeof(float), totalParticleCount * sizeof(float)))
			return false;
	}
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Triangle_VertexBB::MapBuffers(SRenderContext &ctx)
{
	(void)ctx;
	const u32		totalParticleCount = m_BB_Triangle.TotalParticleCount();
	const auto		&toMap = m_DrawPass->m_ToGenerate;

	if (toMap.m_GeneratedInputs & Drawers::GenInput_ParticlePosition0)
	{
		PK_ASSERT(toMap.m_GeneratedInputs & Drawers::GenInput_ParticlePosition1);
		PK_ASSERT(toMap.m_GeneratedInputs & Drawers::GenInput_ParticlePosition2);
		PK_ASSERT(m_VertexPositions0.Used());
		PK_ASSERT(m_VertexPositions1.Used());
		PK_ASSERT(m_VertexPositions2.Used());
		void	*mappedValue0 = m_ApiManager->MapCpuView(m_VertexPositions0.m_Buffer, 0, totalParticleCount * sizeof(CFloat4));
		void	*mappedValue1 = m_ApiManager->MapCpuView(m_VertexPositions1.m_Buffer, 0, totalParticleCount * sizeof(CFloat3));
		void	*mappedValue2 = m_ApiManager->MapCpuView(m_VertexPositions2.m_Buffer, 0, totalParticleCount * sizeof(CFloat3));
		if (!PK_VERIFY(mappedValue0 != null && mappedValue1 != null && mappedValue2 != null))
			return false;
		// PositionsDrIds contains the positions of the triangles first vertex in its XYZ components and the draw request ID for each triangle in the W component
		// Positions1 and Positions2 only contains the positions of the triangles second and third vertices
		m_BBJobs_Triangle.m_Exec_CopyBillboardingStreams.m_PositionsDrIds = TMemoryView<Drawers::SVertex_PositionDrId>(static_cast<Drawers::SVertex_PositionDrId*>(mappedValue0), totalParticleCount);
		m_BBJobs_Triangle.m_Exec_CopyBillboardingStreams.m_Positions1 = TMemoryView<CFloat3>(static_cast<CFloat3*>(mappedValue1), totalParticleCount);
		m_BBJobs_Triangle.m_Exec_CopyBillboardingStreams.m_Positions2 = TMemoryView<CFloat3>(static_cast<CFloat3*>(mappedValue2), totalParticleCount);
	}

	{
		PK_ASSERT(m_DrawRequests.Used());
		void	*mappedDrawRequests = m_ApiManager->MapCpuView(m_DrawRequests.m_Buffer, 0, kMaxGeomDrawRequestCount * sizeof(Drawers::STriangleDrawRequest));
		if (!PK_VERIFY(mappedDrawRequests != null))
			return false;
		m_BBJobs_Triangle.m_Exec_GPUTriangleDrawRequests.m_GPUDrawRequests = TMemoryView<Drawers::STriangleDrawRequest>(static_cast<Drawers::STriangleDrawRequest*>(mappedDrawRequests), kMaxGeomDrawRequestCount);
	}

	if (toMap.m_GeneratedInputs & Drawers::GenInput_Indices)
	{
		PK_ASSERT(m_Indices.Used());
		void	*mappedValue = m_ApiManager->MapCpuView(m_Indices.m_Buffer, 0, totalParticleCount * sizeof(u32));
		if (!PK_VERIFY(mappedValue != null))
			return false;
		m_BBJobs_Triangle.m_Exec_Indices.m_IndexStream.Setup(mappedValue, totalParticleCount, true);
	}
	PK_ASSERT(m_PerViewIndicesBuffers.Count() == m_BBJobs_Triangle.m_PerView.Count());
	for (u32 i = 0; i < m_PerViewIndicesBuffers.Count(); ++i)
	{
		const u32	viewGeneratedInputs = toMap.m_PerViewGeneratedInputs[i].m_GeneratedInputs;
		if (viewGeneratedInputs & Drawers::GenInput_Indices)
		{
			void	*mappedValue = m_ApiManager->MapCpuView(m_PerViewIndicesBuffers[i].m_Buffer, 0, totalParticleCount * sizeof(u32));
			if (!PK_VERIFY(mappedValue != null))
				return false;
			m_BBJobs_Triangle.m_PerView[i].m_Exec_Indices.m_IndexStream.Setup(mappedValue, totalParticleCount, true);
		}
	}

	if (!m_AdditionalFieldsBatch.MapBuffers(totalParticleCount, m_ApiManager))
		return false;
	m_BBJobs_Triangle.m_Exec_CopyAdditionalFields.m_FieldsToCopy = m_AdditionalFieldsBatch.m_MappedFields;

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	// Editor only: for debugging purposes, we'll remove that from samples code later
	PKSample::SRHIRenderContext	&ctxEditor = *static_cast<PKSample::SRHIRenderContext*>(&ctx);
	if (ctxEditor.Selection().HasParticlesSelected() || ctxEditor.Selection().HasRendersSelected())
	{
		m_GeomBillboardCustomParticleSelectTask.Clear();
		PK_ASSERT(m_IsParticleSelected.Used());
		void	*mappedValue = m_ApiManager->MapCpuView(m_IsParticleSelected.m_Buffer, 0, sizeof(float) * totalParticleCount);
		if (!PK_VERIFY(mappedValue != null))
			return false;
		m_GeomBillboardCustomParticleSelectTask.m_DstSelectedParticles = TStridedMemoryView<float>(static_cast<float*>(mappedValue), totalParticleCount, sizeof(float));
		m_GeomBillboardCustomParticleSelectTask.m_SrcParticleSelected = ctxEditor.Selection();
	}
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Triangle_VertexBB::LaunchCustomTasks(SRenderContext &ctx)
{
	(void)ctx;
#if	(PK_HAS_PARTICLES_SELECTION != 0)
	// Editor only: for debugging purposes, we'll remove that from samples code later
	PKSample::SRHIRenderContext	&ctxEditor = *static_cast<PKSample::SRHIRenderContext*>(&ctx);
	if (ctxEditor.Selection().HasParticlesSelected() || ctxEditor.Selection().HasRendersSelected())
	{
		m_BB_Triangle.AddExecAsyncPage(&m_GeomBillboardCustomParticleSelectTask);
	}
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)
	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Triangle_VertexBB::UnmapBuffers(SRenderContext &ctx)
{
	(void)ctx;

	m_VertexPositions0.UnmapIFN(m_ApiManager);
	m_VertexPositions1.UnmapIFN(m_ApiManager);
	m_VertexPositions2.UnmapIFN(m_ApiManager);

	m_DrawRequests.UnmapIFN(m_ApiManager);

	m_Indices.UnmapIFN(m_ApiManager);
	for (auto &buffer : m_PerViewIndicesBuffers)
		buffer.UnmapIFN(m_ApiManager);

	m_AdditionalFieldsBatch.UnmapBuffers(m_ApiManager);

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	m_IsParticleSelected.UnmapIFN(m_ApiManager);
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Triangle_VertexBB::EmitDrawCall(SRenderContext &ctx, const SDrawCallDesc &toEmit)
{
	SRHIRenderContext	&renderContext = static_cast<SRHIRenderContext&>(ctx);

	// No need to iterate on all draw requests, just take the first as reference as they wouldn't have been batched if not compatible
	CRendererCacheInstance_UpdateThread	*refCacheInstance = static_cast<CRendererCacheInstance_UpdateThread*>(toEmit.m_RendererCaches.First().Get());
	if (!PK_VERIFY(refCacheInstance != null))
		return false;

	PKSample::PCRendererCacheInstance	rCacheInstance = refCacheInstance->RenderThread_GetCacheInstance();
	if (!PK_VERIFY(rCacheInstance != null))
		return false;

	static const u32	shaderOptions = PKSample::Option_VertexPassThrough | PKSample::Option_TriangleVertexBillboarding;

	if (!PK_VERIFY(renderContext.m_DrawOutputs.m_DrawCalls.PushBack().Valid()))
	{
		CLog::Log(PK_ERROR, "Failed to create a draw-call");
		return false;
	}
	SRHIDrawCall	&outDrawCall = renderContext.m_DrawOutputs.m_DrawCalls.Last();

	outDrawCall.m_Batch = this;
	outDrawCall.m_RendererCacheInstance = refCacheInstance;
	outDrawCall.m_Type = SRHIDrawCall::DrawCall_IndexedInstanced;
	outDrawCall.m_ShaderOptions = shaderOptions;
	outDrawCall.m_RendererType = Renderer_Triangle;

	outDrawCall.m_IndexOffset = 0;
	outDrawCall.m_VertexCount = 3;
	outDrawCall.m_IndexCount = 3;
	outDrawCall.m_InstanceCount = toEmit.m_TotalParticleCount;
	outDrawCall.m_IndexSize = RHI::IndexBuffer16Bit;
	outDrawCall.m_IndexBuffer = m_DrawIndices.m_Buffer;

	// Some meta-data (the Editor uses them)
	{
		outDrawCall.m_BBox = toEmit.m_BBox;
		outDrawCall.m_TotalBBox = m_DrawPass->m_TotalBBox;
		outDrawCall.m_SlicedDC = toEmit.m_TotalParticleCount != m_DrawPass->m_TotalParticleCount;
	}

	// GPUBillboardPushConstants
	if (!PK_VERIFY(outDrawCall.m_PushConstants.PushBack().Valid()))
		return false;
	u32		&indexOffset = *reinterpret_cast<u32*>(&outDrawCall.m_PushConstants.Last());
	indexOffset = toEmit.m_IndexOffset; // == instanceOffset

	// Fill the constant-set with all SRVs
	// TODO: update the constant-set only when buffers have been resized/changed.
	{
		u32 i = 0;

		// TODO: We only support a single view right now
		SGpuBuffer	&indices = (!m_PerViewIndicesBuffers.Empty() && m_PerViewIndicesBuffers[0].Used()) ? m_PerViewIndicesBuffers[0] : m_Indices;
		PK_ASSERT(indices.Used());
		if (!PK_VERIFY(m_VertexBBSimDataConstantSet->SetConstants(indices.m_Buffer, i++)))
			return false;
		outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Indices] = indices.m_Buffer;

		if (!PK_VERIFY(m_VertexBBSimDataConstantSet->SetConstants(m_VertexPositions0.m_Buffer, i++)))
			return false;
		outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_VertexPosition0] = m_VertexPositions0.m_Buffer;

		if (!PK_VERIFY(m_VertexBBSimDataConstantSet->SetConstants(m_VertexPositions1.m_Buffer, i++)))
			return false;
		outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_VertexPosition1] = m_VertexPositions1.m_Buffer;

		if (!PK_VERIFY(m_VertexBBSimDataConstantSet->SetConstants(m_VertexPositions2.m_Buffer, i++)))
			return false;
		outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_VertexPosition2] = m_VertexPositions2.m_Buffer;

		outDrawCall.m_UBSemanticsPtr[SRHIDrawCall::UBSemantic_GPUBillboard] = m_DrawRequests.m_Buffer;

		if (m_ColorStreamIdx.Valid())
			outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Color] =  m_AdditionalFieldsBatch.m_Fields[m_ColorStreamIdx].m_Buffer.m_Buffer;

		for (SAdditionalInputs &addField : m_AdditionalFieldsBatch.m_Fields)
		{
			if (!addField.m_Buffer.Used())
				continue;
			if (!PK_VERIFY(m_VertexBBSimDataConstantSet->SetConstants(addField.m_Buffer.m_Buffer, i++)))
				return false;
		}

		m_VertexBBSimDataConstantSet->UpdateConstantValues();
	}
	outDrawCall.m_GPUStorageSimDataConstantSet = m_VertexBBSimDataConstantSet;

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	PKSample::SRHIRenderContext	&ctxEditor = *static_cast<PKSample::SRHIRenderContext*>(&ctx);
	if (ctxEditor.Selection().HasParticlesSelected() || ctxEditor.Selection().HasRendersSelected())
	{
		if (m_SelectionConstantSet != null)
		{
			m_SelectionConstantSet->SetConstants(m_IsParticleSelected.m_Buffer, 0);
			m_SelectionConstantSet->UpdateConstantValues();
		}
		outDrawCall.m_SelectionConstantSet = m_SelectionConstantSet;
		outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_IsParticleSelected] = m_IsParticleSelected.m_Buffer;
	}
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Triangle_VertexBB::_InitStaticBuffers()
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

		static const u32	shaderOptions = PKSample::Option_VertexPassThrough | PKSample::Option_TriangleVertexBillboarding;

		const RHI::SConstantSetLayout	*simDataConstantSetLayout = null;
		const RHI::SConstantSetLayout	*offsetsConstantSetLayout = null;
		if (!PK_VERIFY(cacheInstance->m_Cache->GetGPUStorageConstantSets(static_cast<PKSample::EShaderOptions>(shaderOptions), simDataConstantSetLayout, offsetsConstantSetLayout)) ||
			!PK_VERIFY(simDataConstantSetLayout != null) ||
			simDataConstantSetLayout->m_Constants.Empty())
			return false;

		m_VertexBBSimDataConstantSet = m_ApiManager->CreateConstantSet(RHI::SRHIResourceInfos("Sim Data Constant Set"), *simDataConstantSetLayout);
		if (!PK_VERIFY(m_VertexBBSimDataConstantSet != null))
			return false;
	}

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	{
		RHI::SConstantSetLayout	selectionSetLayout(RHI::VertexShaderMask);
		selectionSetLayout.AddConstantsLayout(RHI::SRawBufferDesc("Selections"));
		m_SelectionConstantSet = m_ApiManager->CreateConstantSet(RHI::SRHIResourceInfos("Selection Constant Set"), selectionSetLayout);
	}
#endif

	return true;
}

//----------------------------------------------------------------------------
//
// CRHIRendererBatch_Light_CPU
//
//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Light_CPU::Setup(const CRendererDataBase *renderer, const CParticleRenderMedium *owner, const CFrameCollector *fc, const CStringId &storageClass)
{
	if (!CRendererBatchJobs_Light_Std::Setup(renderer, owner, fc, storageClass))
		return false;

	// Setup additional fields:
	// The additional fields are supposed to be the same for all renderers in a batch.
	// If not, then you can recompute then on the "Bind()" method.

	const auto		&toGenerate = m_DrawPass->m_ToGenerate;
	const u32		additionalFieldsCount = toGenerate.m_AdditionalGeneratedInputs.Count();

	if (!PK_VERIFY(m_AdditionalFieldsBatch.m_Fields.Reserve(additionalFieldsCount)))
		return false;

	for (u32 i = 0; i < additionalFieldsCount; ++i)
	{
		// no field to ignore.

		const u32	typeSize = CBaseTypeTraits::Traits(toGenerate.m_AdditionalGeneratedInputs[i].m_Type).Size;
		m_AdditionalFieldsBatch.m_Fields.PushBackUnsafe(SAdditionalInputs(typeSize, i));
	}

	m_ColorStreamIdx = _GetDrawDebugColorIndex(m_AdditionalFieldsBatch, toGenerate);
	m_RangeStreamIdx = _GetDrawDebugRangeIndex(m_AdditionalFieldsBatch, toGenerate);

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Light_CPU::AreRenderersCompatible(const CRendererDataBase *rendererA, const CRendererDataBase *rendererB) const
{
	return CRendererBatchJobs_Light_Std::AreRenderersCompatible(rendererA, rendererB) && AreBillboardingBatchable(rendererA->m_RendererCache, rendererB->m_RendererCache);
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Light_CPU::AllocBuffers(SRenderContext &ctx)
{
	(void)ctx;

	const u32		totalParticleCount = m_DrawPass->m_TotalParticleCount;
	const u32		totalParticleCountAligned = Mem::Align<0x100>(totalParticleCount);

	// Here we first allocate the light positions buffer (there will always be some light positions):
	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Light Positions Vertex Buffer"), true, m_ApiManager, m_LightsPositions, RHI::VertexBuffer, totalParticleCountAligned * sizeof(CFloat3), totalParticleCount * sizeof(CFloat3)))
		return false;

	if (!m_AdditionalFieldsBatch.AllocBuffers(totalParticleCount, m_ApiManager))
		return false;

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	// Editor only: for debugging purposes, we'll remove that from samples code later
	PKSample::SRHIRenderContext	&ctxEditor = *static_cast<PKSample::SRHIRenderContext*>(&ctx);
	if (ctxEditor.Selection().HasParticlesSelected() || ctxEditor.Selection().HasRendersSelected())
	{
		if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Selection Vertex Buffer"), true, m_ApiManager, m_IsParticleSelected, RHI::VertexBuffer, totalParticleCountAligned * sizeof(float), totalParticleCount * sizeof(float)))
			return false;
	}
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Light_CPU::MapBuffers(SRenderContext &ctx)
{
	(void)ctx;
	m_BB_Lights.Clear();

	const u32	totalParticleCount = m_DrawPass->m_TotalParticleCount;

	// Map the light origins buffer:
	CFloat3				*mappedPos = (CFloat3*)m_ApiManager->MapCpuView(m_LightsPositions.m_Buffer, 0, totalParticleCount * sizeof(CFloat3));
	if (!PK_VERIFY(mappedPos != null))
		return false;

	// We feed the mapped buffer to our custom task (it just copies the positions in the mapped buffer):
	m_BB_Lights.m_Positions = TMemoryView<CFloat3>(mappedPos, totalParticleCount);

	if (!m_AdditionalFieldsBatch.MapBuffers(totalParticleCount, m_ApiManager))
		return false;
	m_CopyAdditionalFieldsTask.m_FieldsToCopy = m_AdditionalFieldsBatch.m_MappedFields;

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	// Editor only: for debugging purposes, we'll remove that from samples code later
	PKSample::SRHIRenderContext	&ctxEditor = *static_cast<PKSample::SRHIRenderContext*>(&ctx);
	if (ctxEditor.Selection().HasParticlesSelected() || ctxEditor.Selection().HasRendersSelected())
	{
		m_CopyStreamCustomParticleSelectTask.Clear();
		PK_ASSERT(m_IsParticleSelected.Used());
		void	*mappedValue = m_ApiManager->MapCpuView(m_IsParticleSelected.m_Buffer, 0, sizeof(float) * totalParticleCount);
		if (!PK_VERIFY(mappedValue != null))
			return false;
		m_CopyStreamCustomParticleSelectTask.m_DstSelectedParticles = TStridedMemoryView<float>(static_cast<float*>(mappedValue), totalParticleCount, sizeof(float));
		m_CopyStreamCustomParticleSelectTask.m_SrcParticleSelected = ctxEditor.Selection();
	}
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Light_CPU::LaunchCustomTasks(SRenderContext &ctx)
{
	(void)ctx;
	if (!m_CopyTasks.Prepare(m_DrawPass->m_DrawRequests.View()))
		return false;
	m_CopyTasks.AddExecAsyncPage(&m_BB_Lights);
	m_CopyTasks.AddExecAsyncPage(&m_CopyAdditionalFieldsTask);
#if	(PK_HAS_PARTICLES_SELECTION != 0)
	// Editor only: for debugging purposes, we'll remove that from samples code later
	PKSample::SRHIRenderContext	&ctxEditor = *static_cast<PKSample::SRHIRenderContext*>(&ctx);
	if (ctxEditor.Selection().HasParticlesSelected() || ctxEditor.Selection().HasRendersSelected())
	{
		m_CopyTasks.AddExecAsyncPage(&m_CopyStreamCustomParticleSelectTask);
	}
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)

	m_CopyTasks.LaunchTasks(null);
	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Light_CPU::WaitForCustomTasks(SRenderContext &ctx)
{
	(void)ctx;
	m_CopyTasks.WaitTasks();
	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Light_CPU::UnmapBuffers(SRenderContext &ctx)
{
	(void)ctx;

	m_LightsPositions.UnmapIFN(m_ApiManager);

	m_AdditionalFieldsBatch.UnmapBuffers(m_ApiManager);

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	m_IsParticleSelected.UnmapIFN(m_ApiManager);
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIRendererBatch_Light_CPU::EmitDrawCall(SRenderContext &ctx, const SDrawCallDesc &toEmit)
{
	SRHIRenderContext	&renderContext = static_cast<SRHIRenderContext&>(ctx);

	// No need to iterate on all draw requests, just take the first as reference as they wouldn't have been batched if not compatible
	CRendererCacheInstance_UpdateThread	*refCacheInstance = static_cast<CRendererCacheInstance_UpdateThread*>(toEmit.m_RendererCaches.First().Get());
	if (!PK_VERIFY(refCacheInstance != null))
		return false;

	PKSample::PCRendererCacheInstance	rCacheInstance = refCacheInstance->RenderThread_GetCacheInstance();
	if (!PK_VERIFY(rCacheInstance != null))
		return false;
	// Cube mesh used to render the decals not created:
	if (!PK_VERIFY(rCacheInstance->m_AdditionalGeometry != null))
		return false;
	if (!PK_VERIFY(rCacheInstance->m_AdditionalGeometry->m_PerGeometryViews.Count() == 1))
		return false;

	CRendererCacheInstance_UpdateThread		*renderCacheInstance = static_cast<CRendererCacheInstance_UpdateThread*>(toEmit.m_RendererCaches.First().Get());
	if (!PK_VERIFY(renderCacheInstance != null))
	{
		CLog::Log(PK_ERROR, "Invalid renderer cache instance");
		return false;
	}

	if (!PK_VERIFY(renderContext.m_DrawOutputs.m_DrawCalls.PushBack().Valid()))
	{
		CLog::Log(PK_ERROR, "Failed to create a draw-call");
		return false;
	}
	SRHIDrawCall	&outDrawCall = renderContext.m_DrawOutputs.m_DrawCalls.Last();

	outDrawCall.m_Batch = this;
	outDrawCall.m_RendererCacheInstance = renderCacheInstance;
	outDrawCall.m_Type = SRHIDrawCall::DrawCall_IndexedInstanced;
	outDrawCall.m_ShaderOptions = PKSample::Option_VertexPassThrough;
	outDrawCall.m_RendererType = Renderer_Light;

	// Editor only: for debugging purposes, we'll remove that from samples code later
	{
		outDrawCall.m_BBox = toEmit.m_BBox;
		outDrawCall.m_TotalBBox = m_DrawPass->m_TotalBBox;
		outDrawCall.m_SlicedDC = toEmit.m_TotalParticleCount != m_DrawPass->m_TotalParticleCount;
	}

	if (rCacheInstance->m_Cache == null)
	{
		outDrawCall.m_Valid = false;
		return false;
	}

	RHI::PCRenderState					renderState = rCacheInstance->m_Cache->GetRenderState(PKSample::Option_VertexPassThrough);
	if (!PK_VERIFY(renderState != null))
	{
		outDrawCall.m_Valid = false;
		return false;
	}

	TMemoryView<const RHI::SVertexAttributeDesc>	vertexBuffDescView = renderState->m_RenderState.m_ShaderBindings.m_InputAttributes.View();
	if (!PK_VERIFY(outDrawCall.m_VertexBuffers.Reserve(vertexBuffDescView.Count())))
		return false;

	const Utils::GpuBufferViews		&bufferView = rCacheInstance->m_AdditionalGeometry->m_PerGeometryViews.First();

	bool	success = true;

	// Sphere positions:
	success &= outDrawCall.m_VertexBuffers.PushBack(bufferView.m_VertexBuffers.First()).Valid();
	// Light positions:
	success &= outDrawCall.m_VertexBuffers.PushBack(m_LightsPositions.m_Buffer).Valid();

	// Additional inputs:
	for (const auto &additionalField : m_AdditionalFieldsBatch.m_Fields)
	{
		PK_ASSERT(additionalField.m_Buffer.Used());
		success &= outDrawCall.m_VertexBuffers.PushBack(additionalField.m_Buffer.m_Buffer).Valid();
	}

	// Fill the semantics for the debug draws:
	outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Position] = outDrawCall.m_VertexBuffers.First();
	outDrawCall.m_DebugDrawGPUBufferOffsets[SRHIDrawCall::DebugDrawGPUBuffer_Position] = 0;

	outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_InstancePositions] = m_LightsPositions.m_Buffer;
	outDrawCall.m_DebugDrawGPUBufferOffsets[SRHIDrawCall::DebugDrawGPUBuffer_InstancePositions] = 0;

	if (m_ColorStreamIdx.Valid())
		outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Color] =  m_AdditionalFieldsBatch.m_Fields[m_ColorStreamIdx].m_Buffer.m_Buffer;

	if (m_RangeStreamIdx.Valid())
		outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_InstanceScales] =  m_AdditionalFieldsBatch.m_Fields[m_RangeStreamIdx].m_Buffer.m_Buffer;


	// Set index buffer
	outDrawCall.m_IndexBuffer = bufferView.m_IndexBuffer;
	if (bufferView.m_IndexBufferSize == RHI::IndexBuffer16Bit)
		outDrawCall.m_IndexCount = bufferView.m_IndexBuffer->GetByteSize() / sizeof(u16);
	else if (bufferView.m_IndexBufferSize == RHI::IndexBuffer32Bit)
		outDrawCall.m_IndexCount = bufferView.m_IndexBuffer->GetByteSize() / sizeof(u32);
	else
	{
		PK_ASSERT_NOT_REACHED();
		return false;
	}
	outDrawCall.m_IndexSize = bufferView.m_IndexBufferSize;
	outDrawCall.m_IndexOffset = toEmit.m_IndexOffset;
	outDrawCall.m_InstanceCount = toEmit.m_TotalParticleCount;

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	// Editor only: for debugging purposes, we'll remove that from samples code later
	PKSample::SRHIRenderContext	&ctxEditor = *static_cast<PKSample::SRHIRenderContext*>(&ctx);
	if ((ctxEditor.Selection().HasParticlesSelected() || ctxEditor.Selection().HasRendersSelected()) && m_IsParticleSelected.Used())
	{
		outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_IsParticleSelected] = m_IsParticleSelected.m_Buffer;
		outDrawCall.m_DebugDrawGPUBufferOffsets[SRHIDrawCall::DebugDrawGPUBuffer_IsParticleSelected] = 0;
	}
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)

	if (!success)
	{
		outDrawCall.m_Valid = false;
		CLog::Log(PK_ERROR, "Could not emit the light draw call");
		return false;
	}

	return true;
}

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
