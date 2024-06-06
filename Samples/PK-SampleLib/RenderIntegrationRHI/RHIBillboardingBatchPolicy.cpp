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
#include "RHIBillboardingBatchPolicy.h"

#include "RHIParticleRenderDataFactory.h"
#include "pk_render_helpers/include/render_features/rh_features_basic.h"

#include <pk_rhi/include/interfaces/IApiManager.h>
#include <pk_rhi/include/interfaces/IGpuBuffer.h>

#if	(PK_PARTICLES_UPDATER_USE_D3D11 != 0)
#	include <pk_rhi/include/D3D11/D3D11RHI.h>
#	include <pk_particles/include/Storage/D3D11/storage_d3d11.h>
#	include <pk_rhi/include/D3D11/D3D11ApiManager.h>
#endif
#if	(PK_PARTICLES_UPDATER_USE_D3D12 != 0)
#	include <pk_rhi/include/D3D12/D3D12RHI.h>
#	include <pk_particles/include/Storage/D3D12/storage_d3d12.h>
#	include <pk_rhi/include/D3D12/D3D12ApiManager.h>
#endif
#if	(PK_PARTICLES_UPDATER_USE_UNKNOWN2 != 0)
#	include <pk_particles/include/Storage/UNKNOWN2/storage_UNKNOWN2.h>
#	include <pk_rhi/include/UNKNOWN2/UNKNOWN2ApiManager.h>
#endif

#include "PK-SampleLib/ShaderDefinitions/SampleLibShaderDefinitions.h"
#include "RHIRenderIntegrationConfig.h"

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

// Maximum number of draw requests batched in a single billboarding batch policy (1 draw request = 1 particle renderer being drawn)
static u32	kMaxGeomDrawRequestCount = 0x100;
static u32	kMaxGPUDrawRequestCount = 0x100;

// PK-SampleLib implementation of batches policies:
// We created a master implementation that handles all renderer types so it's easier to read and understand
// You can create a single policy per batch as batches are created by your TParticleRenderDataFactory

// Batch policies purpose are to allocate, map, unmap and clear vertex buffers and to issue draw calls
// When creating your own, you should take this implementation as reference

// This implementation is both used by samples and v2 editor

//----------------------------------------------------------------------------
//
//	Billboarding batch policy:
//
//----------------------------------------------------------------------------

CRHIBillboardingBatchPolicy::CRHIBillboardingBatchPolicy()
:	m_GPUStorage(false)
,	m_RendererType(Renderer_Invalid)
,	m_UnusedCounter(0)
{
	_ClearFrame();
}

//----------------------------------------------------------------------------

CRHIBillboardingBatchPolicy::~CRHIBillboardingBatchPolicy()
{
}

//----------------------------------------------------------------------------
//
//	Cull draw requests with some custom metric, here we cull some draw request depending on the rendering pass
//
//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy::CanRender(const Drawers::SBillboard_DrawRequest *request, const PRendererCacheBase &rendererCache, SRenderContext &ctx)
{
	(void)request; (void)rendererCache;
	PK_ASSERT(request != null);
	PK_ASSERT(rendererCache != null);
	return ctx.IsRenderThreadPass();
}

//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy::CanRender(const Drawers::SRibbon_DrawRequest *request, const PRendererCacheBase &rendererCache, SRenderContext &ctx)
{
	(void)request; (void)rendererCache;
	PK_ASSERT(request != null);
	PK_ASSERT(rendererCache != null);
	return ctx.IsRenderThreadPass();
}

//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy::CanRender(const Drawers::SMesh_DrawRequest *request, const PRendererCacheBase &rendererCache, SRenderContext &ctx)
{
	(void)request; (void)rendererCache;
	PK_ASSERT(request != null);
	PK_ASSERT(rendererCache != null);
	return ctx.IsRenderThreadPass();
}

//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy::CanRender(const Drawers::SDecal_DrawRequest *request, const PRendererCacheBase &rendererCache, SRenderContext &ctx)
{
	(void)request; (void)rendererCache;
	PK_ASSERT(request != null);
	PK_ASSERT(rendererCache != null);
	return ctx.IsRenderThreadPass();
}

//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy::CanRender(const Drawers::STriangle_DrawRequest *request, const PRendererCacheBase &rendererCache, SRenderContext &ctx)
{
	(void)request; (void)rendererCache;
	PK_ASSERT(request != null);
	PK_ASSERT(rendererCache != null);
	return ctx.IsRenderThreadPass();
}

//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy::CanRender(const Drawers::SLight_DrawRequest *request, const PRendererCacheBase &rendererCache, SRenderContext &ctx)
{
	(void)request; (void)rendererCache;
	PK_ASSERT(request != null);
	PK_ASSERT(rendererCache != null);
	return ctx.IsRenderThreadPass();
}

//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy::CanRender(const Drawers::SSound_DrawRequest *request, const PRendererCacheBase &rendererCache, SRenderContext &ctx)
{
	(void)request; (void)rendererCache;
	PK_ASSERT(request != null);
	PK_ASSERT(rendererCache != null);
	return ctx.IsPostUpdateFencePass();
}

//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy::Tick(SRenderContext &ctx, const TMemoryView<SSceneView> &views)
{
	(void)views; (void)ctx;
	// Tick function called on draw calls thread, here we remove ourselves if we haven't been used for rendering after 10 (collected) frames:
	// 10 PKFX Update()/UpdateFence() without being drawn
	if (m_UnusedCounter++ > 10)
		return false;
	return true;
}

//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy::AreBillboardingBatchable(const PCRendererCacheBase &firstCache, const PCRendererCacheBase &secondCache) const
{
	// Return true if firstCache and secondCache can be batched together (same draw call)
	// Simplest approach here is to break batching when those two materials are incompatible (varying uniforms, mismatching textures, ..)
	return	firstCache == secondCache ||
			*checked_cast<const CRendererCacheInstance_UpdateThread *>(firstCache.Get()) ==
			*checked_cast<const CRendererCacheInstance_UpdateThread *>(secondCache.Get());
}

//----------------------------------------------------------------------------
//
//	Buffers allocations
//
//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy::AllocBuffers(SRenderContext &ctx, const SBuffersToAlloc &allocBuffers, const TMemoryView<SSceneView> &views, ERendererClass rendererType)
{
	PK_NAMEDSCOPEDPROFILE("CRHIBillboardingBatchPolicy::AllocBuffers");

	// Reset the unused counter
	m_UnusedCounter = 0;

	// Clear previous frame data
	_ClearFrame(allocBuffers.m_ToGenerate.m_PerViewGeneratedInputs.Count());

	PK_ASSERT(!allocBuffers.m_DrawRequests.Empty());
	PK_ASSERT(allocBuffers.m_DrawRequests.Count() == allocBuffers.m_RendererCaches.Count());
	PK_ASSERT((allocBuffers.m_TotalVertexCount > 0 && allocBuffers.m_TotalIndexCount > 0) ||
		allocBuffers.m_TotalParticleCount > 0);
	PK_ASSERT(allocBuffers.m_DrawRequests.First() != null);

	m_GPUStorage = allocBuffers.m_DrawRequests.First()->StreamToRender_MainMemory() == null;
	if (m_RendererType == Renderer_Invalid)
	{
		m_RendererType = rendererType;
	}
	else if (!PK_VERIFY(m_RendererType == rendererType))
	{
		return false;
	}
	m_TotalBBox = allocBuffers.m_TotalBBox;

	if (m_RendererType == Renderer_Sound)
	{
		m_TotalParticleCount = allocBuffers.m_TotalParticleCount;
		// For sounds, no vertex buffers allocate/map, we'll just need the main view matrix later (so we can implement doppler) when issuing the "draw calls"
		PK_ASSERT(!views.Empty());
		m_MainViewMatrix = views[0].m_InvViewMatrix.Inverse();
		return true;
	}

	RHI::PApiManager	manager = ctx.ApiManager();

	// Setup counts
	m_TotalParticleCount = allocBuffers.m_TotalParticleCount;
	m_TotalVertexCount = allocBuffers.m_TotalVertexCount;
	m_TotalIndexCount = allocBuffers.m_TotalIndexCount;
	m_IndexSize = (m_TotalVertexCount > 0xFFFF) ? sizeof(u32) : sizeof(u16);

	// Can help avoid GPU buffers resizing
	// You'll need a proper GPU buffers pooling system in your engine
	m_TotalParticleCount_OverEstimated = Mem::Align(m_TotalParticleCount, 0x100);
	m_TotalVertexCount_OverEstimated = Mem::Align(m_TotalVertexCount, 0x1000);
	m_TotalIndexCount_OverEstimated = Mem::Align(m_TotalIndexCount, 0x1000);

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	// Editor only: for debugging purposes, we'll remove that from samples code later
	const bool	canDrawSelectedParticles = !m_GPUStorage && (m_RendererType != Renderer_Sound) && (ctx.Selection().HasParticlesSelected() || ctx.Selection().HasRendersSelected());
	const u32	elemCount = m_TotalVertexCount != 0 ? m_TotalVertexCount : m_TotalParticleCount;
	const u32	overEstimatedElemCount = m_TotalVertexCount_OverEstimated != 0 ? m_TotalVertexCount_OverEstimated : m_TotalParticleCount_OverEstimated;
	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Selection Vertex Buffer"), canDrawSelectedParticles, manager, m_IsParticleSelected, RHI::VertexBuffer, overEstimatedElemCount * sizeof(float), elemCount * sizeof(float)))
		return false;
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)

	//----------------------------------------------------------------------------
	// View independent inputs:
	// SBuffersToAlloc contains what is needed to construct your vertex declaration

	if (m_RendererType != Renderer_Light)
	{
		if (m_GPUStorage)
		{
			// GPU simulated particles: their data is already on GPU
			if (!_AllocateBuffers_GPU(manager, allocBuffers))
				return false;
		}
		else
		{
			// CPU simulated particles: their data from RAM need to be copied to GPU buffers
			if (!_AllocateBuffers_Main(manager, allocBuffers))
				return false;
			if (!_AllocateBuffers_GeomShaderBillboarding(manager, allocBuffers.m_ToGenerate.m_GeneratedInputs))
				return false;
		}
	}
	else
	{
		if (!_AllocateBuffers_Lights(manager, allocBuffers.m_ToGenerate))
			return false;
	}

	if (!allocBuffers.m_IsNewFrame)
		return true;
	// Only allocate additional input buffers if we are billboarding a new collected frame (see documentation for more detail)
	if (!_AllocateBuffers_AdditionalInputs(manager, allocBuffers.m_ToGenerate))
		return false;

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy::_AllocateBuffers_GPU(const RHI::PApiManager &manager, const SBuffersToAlloc &allocBuffers)
{
	// GPU Storage: the draw request data is contained on GPU memory because the simulation happens on the GPU
	// In that case, the draw calls and billboarding process is not the same

	// PK-SampleLib code only supports billboards and meshes for GPU sim right now
	PK_ASSERT(m_RendererType == Renderer_Billboard || m_RendererType == Renderer_Mesh);

	// GPU sim: we don't allocate any vertex buffers, the sim buffers will be used directly as input to the vertex/geom shaders:
	// we issue an indirect draw, using the storage's particle count, stored in GPU memory
	// So we'll just need to map GPU sim buffers for our draw call
	m_DrawRequestCount = allocBuffers.m_DrawRequests.Count();

	if (m_RendererType == Renderer_Billboard)
	{
		const u32	size = sizeof(RHI::SDrawIndirectArgs) * m_DrawRequestCount;
		if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Billboard DrawRequests Buffer"), true, manager, m_IndirectDraw, RHI::IndirectDrawBuffer, size, size))
			return false;
	}
	else if (m_RendererType == Renderer_Mesh)
	{
		m_MeshCount = 0;
		const u32	lodCount = allocBuffers.m_RendererCaches.First()->LODCount();
		for (u32 i = 0; i < lodCount; ++i)
			m_MeshCount += allocBuffers.m_RendererCaches.First()->m_PerLODMeshCount[i];
		m_HasMeshIDs = allocBuffers.m_HasMeshIDs;
		m_HasMeshLODs = allocBuffers.m_HasMeshLODs;
		if (!PK_VERIFY(m_PerMeshIndexCount.Resize(m_MeshCount)))
			return false;

		CRendererCacheInstance_UpdateThread		*refCacheInstance = static_cast<CRendererCacheInstance_UpdateThread*>(allocBuffers.m_RendererCaches.First().Get());
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
				// TODO: Maybe handle indexCounts in allocbuffer ?
				const Utils::GpuBufferViews	&bufferView = rCacheInstance->m_AdditionalGeometry->m_PerGeometryViews[i];

				// Get index count
				u32	indexCount;
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

		// Indirect draw buffer
		const u32	indirectBufferSizeInBytes = sizeof(RHI::SDrawIndexedIndirectArgs) * m_MeshCount * m_DrawRequestCount;
		if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Indirect Draw Args Buffer"), true, manager, m_IndirectDraw, RHI::RawIndirectDrawBuffer, indirectBufferSizeInBytes, indirectBufferSizeInBytes))
			return false;

		// Indirection buffer
		if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Indirection Buffer"), true, manager, m_Indirection, RHI::RawBuffer, m_TotalParticleCount_OverEstimated * sizeof(u32), m_TotalParticleCount_OverEstimated * sizeof(u32)))
			return false;
		// Indirection offsets buffer:
		// * Mesh atlas enabled: one u32 per sub mesh (sub mesh array is "flat" when LODs are enabled, total sub meshes is the total number of sub meshes per LODs)
		// * LOD enabled: one u32 per lod level
		// * No mesh atlas/LOD: one u32 per draw request
		// times 2, the first part of the buffer is used as a scratch buffer for computing the per-particle indirections
		const u32	indirectionOffsetsSizeInBytes = 2 * m_DrawRequestCount * sizeof(u32) * (m_HasMeshIDs ? m_MeshCount : (m_HasMeshLODs ? allocBuffers.m_RendererCaches.First()->LODCount() : 1));
		if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Indirection Offsets Buffer"), true, manager, m_IndirectionOffsets, RHI::RawBuffer, indirectionOffsetsSizeInBytes, indirectionOffsetsSizeInBytes))
			return false;

		// Allocate once, max number of draw requests, indexed by DC from push constant
		const u32	offsetsSizeInBytes = kMaxGPUDrawRequestCount * sizeof(u32); // u32 offsets
		if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Offsets_Positions Buffer"), true, manager, m_SimStreamOffsets_Positions, RHI::RawBuffer, offsetsSizeInBytes, offsetsSizeInBytes) ||
			!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Offsets_Scales Buffer"), true, manager, m_SimStreamOffsets_Scales, RHI::RawBuffer, offsetsSizeInBytes, offsetsSizeInBytes) ||
			!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Offsets_Orientations Buffer"), true, manager, m_SimStreamOffsets_Orientations, RHI::RawBuffer, offsetsSizeInBytes, offsetsSizeInBytes) ||
			!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Offsets_Enableds Buffer"), true, manager, m_SimStreamOffsets_Enableds, RHI::RawBuffer, offsetsSizeInBytes, offsetsSizeInBytes) ||
			!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Offsets_MeshIDs Buffer"), m_HasMeshIDs, manager, m_SimStreamOffsets_MeshIDs, RHI::RawBuffer, offsetsSizeInBytes, offsetsSizeInBytes) ||
			!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Offsets_MeshLODs Buffer"), m_HasMeshLODs, manager, m_SimStreamOffsets_MeshLODs, RHI::RawBuffer, offsetsSizeInBytes, offsetsSizeInBytes) ||
			!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Offsets_Matrices Buffer"), true, manager, m_MatricesOffsets, RHI::RawBuffer, offsetsSizeInBytes, offsetsSizeInBytes))
			return false;

		const u32	matricesSizeInBytes = m_TotalParticleCount_OverEstimated * sizeof(CFloat4x4);
		if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Matrices Buffer"), true, manager, m_Matrices, RHI::RawBuffer, matricesSizeInBytes, matricesSizeInBytes))
			return false;

		// Constant buffer containing an array of 16 uints for per lod level submesh count (and a second array of the same size + 1, if we have per-particle LODs)
		const u32	cbSizeInBytes = (sizeof(u32) * 0x10 + (m_HasMeshLODs ? sizeof(u32) * (0x10 + 1) : 0)) * 4; // We limit the max number of mesh LODs. * 4 as all uint are 16bytes aligned.
		if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("LODOffsets Constant Buffer"), true, manager, m_LODsConstantBuffer, RHI::ConstantBuffer, cbSizeInBytes, cbSizeInBytes))
			return false;
		{
			volatile u32	*mappedLODOffsets = static_cast<u32*>(manager->MapCpuView(m_LODsConstantBuffer.m_Buffer, 0, cbSizeInBytes));
			if (!PK_VERIFY(mappedLODOffsets != null))
				return false;
			// Clear everything, just in case.
			Mem::Clear_Uncached(mappedLODOffsets, cbSizeInBytes);

			// Write PerMeshLODCount
			for (u32 i = 0; i < lodCount; ++i)
				*Mem::AdvanceRawPointer(mappedLODOffsets, i * 0x10) = allocBuffers.m_RendererCaches.First()->m_PerLODMeshCount[i];
			mappedLODOffsets = Mem::AdvanceRawPointer(mappedLODOffsets, 0x100);

			// Accumulate PerMeshLODCount into the second constant buffer member
			if (m_HasMeshLODs)
			{
				u32	submeshCount = 0;
				for (u32 i = 0; i < lodCount; ++i)
				{
					*Mem::AdvanceRawPointer(mappedLODOffsets, i * 0x10) = submeshCount;
					submeshCount += allocBuffers.m_RendererCaches.First()->m_PerLODMeshCount[i];
				}
				// Write at last LOD level + 1 the total number of submeshes, to simplify the shader a bit.
				*Mem::AdvanceRawPointer(mappedLODOffsets, lodCount * 0x10) = submeshCount;
			}
			manager->UnmapCpuView(m_LODsConstantBuffer.m_Buffer);
		}
	}

	// The offset in the indirect buffer of the current emitted draw call
	m_DrawCallCurrentOffset = 0;
	return true;
}

//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy::_AllocateBuffers_Main(const RHI::PApiManager &manager, const SBuffersToAlloc &allocBuffers)
{
	PK_ASSERT(m_RendererType != Renderer_Sound);
	PK_ASSERT(!m_GPUStorage);

	const SGeneratedInputs	&toGenerate = allocBuffers.m_ToGenerate;
	const u32				viewIndependentInputs = toGenerate.m_GeneratedInputs;
	if (m_RendererType == Renderer_Mesh || m_RendererType == Renderer_Decal)
	{
		// Meshes
		m_PerMeshParticleCount = allocBuffers.m_PerMeshParticleCount; // Array of particle counts per sub mesh
		m_PerMeshBufferOffset = allocBuffers.m_PerMeshBufferOffset; // Array of particle counts per sub mesh
		m_HasMeshIDs = allocBuffers.m_HasMeshIDs;
		m_HasMeshLODs = allocBuffers.m_HasMeshLODs;

		// Matrices (view independent), generated from particle's Orientation stream
		if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Matrices Vertex Buffer"), (viewIndependentInputs & Drawers::GenInput_Matrices) != 0, manager, m_Matrices, RHI::VertexBuffer, m_TotalVertexCount_OverEstimated * sizeof(CFloat4x4), m_TotalVertexCount * sizeof(CFloat4x4)))
			return false;
		if (m_RendererType == Renderer_Decal)
		{
			if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("InvMatrices Vertex Buffer"), (viewIndependentInputs & Drawers::GenInput_Matrices) != 0, manager, m_InvMatrices, RHI::VertexBuffer, m_TotalVertexCount_OverEstimated * sizeof(CFloat4x4), m_TotalVertexCount * sizeof(CFloat4x4)))
				return false;
		}
	}
	else
	{
		// Billboards, Ribbons (Billboarding done on CPU)

		// View independent indices (ie. Billboard with PlaneAligned Billboarding mode, or Ribbons that are view independent)
		if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Indices Buffer"), (viewIndependentInputs & Drawers::GenInput_Indices) != 0, manager, m_Indices, RHI::IndexBuffer, m_TotalIndexCount_OverEstimated * m_IndexSize, m_TotalIndexCount * m_IndexSize))
			return false;
		// View independent positions (ie. Billboard with PlaneAligned Billboarding mode, or Ribbons that are view independent)
		if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Positions Vertex Buffer"), (viewIndependentInputs & Drawers::GenInput_Position) != 0, manager, m_Positions, RHI::VertexBuffer, m_TotalVertexCount_OverEstimated * sizeof(CFloat4), m_TotalVertexCount * sizeof(CFloat4)))
			return false;
		// Texcoords are only generated when the "m_HasUV" flag is set on your side (see ps_renderer_base.h SBillboardingFlags)
		if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("UV0s Vertex Buffer"), (viewIndependentInputs & Drawers::GenInput_UV0) != 0, manager, m_TexCoords0, RHI::VertexBuffer, m_TotalVertexCount_OverEstimated * sizeof(CFloat2), m_TotalVertexCount * sizeof(CFloat2)))
			return false;

		// Normal/Tangent are only generated when the "m_HasNormal/m_HasTangent" flag is set on your side (see ps_renderer_base.h SBillboardingFlags)
		{
			if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Normals Vertex Buffer"), (viewIndependentInputs & Drawers::GenInput_Normal) != 0, manager, m_Normals, RHI::VertexBuffer, m_TotalVertexCount_OverEstimated * sizeof(CFloat4), m_TotalVertexCount * sizeof(CFloat4)))
				return false;
			if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Tangents Vertex Buffer"), (viewIndependentInputs & Drawers::GenInput_Tangent) != 0, manager, m_Tangents, RHI::VertexBuffer, m_TotalVertexCount_OverEstimated * sizeof(CFloat4), m_TotalVertexCount * sizeof(CFloat4)))
				return false;
		}

		if (m_RendererType == Renderer_Billboard || m_RendererType == Renderer_Ribbon)
		{
			// Specific to billboard/ribbon renderers (necessary for the Linear Atlas Frame Blending mode) - Only enabled if m_SoftAnimationBlending is set on your side (see ps_renderer_base.h SBillboardingFlags)
			if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("UV1s Vertex Buffer"), (viewIndependentInputs & Drawers::GenInput_UV1) != 0, manager, m_TexCoords1, RHI::VertexBuffer, m_TotalVertexCount_OverEstimated * sizeof(CFloat2), m_TotalVertexCount * sizeof(CFloat2)))
				return false;
			if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("AtlasIDs Vertex Buffer"), (viewIndependentInputs & Drawers::GenInput_AtlasId) != 0, manager, m_AtlasIDs, RHI::VertexBuffer, m_TotalVertexCount_OverEstimated * sizeof(float), m_TotalVertexCount * sizeof(float)))
				return false;
		}
		if (m_RendererType == Renderer_Ribbon)
		{
			// Specific to ribbons (necessary for the CorrectDeformation feature) - Only enabled if m_HasRibbonCorrectDeformation is set on your side (see ps_renderer_base.h SBillboardingFlags)
			if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("UVRemaps Vertex Buffer"), (viewIndependentInputs & Drawers::GenInput_UVRemap) != 0, manager, m_UVRemap, RHI::VertexBuffer, m_TotalVertexCount_OverEstimated * sizeof(CFloat4), m_TotalVertexCount * sizeof(CFloat4)))
				return false;
			if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("UVFactors Vertex Buffer"), (viewIndependentInputs & Drawers::GenInput_UVFactors) != 0, manager, m_UVFactors, RHI::VertexBuffer, m_TotalVertexCount_OverEstimated * sizeof(CFloat4), m_TotalVertexCount * sizeof(CFloat4)))
				return false;
		}
		if (!_AllocateBuffers_ViewDependent(manager, toGenerate))
			return false;
	}
	return true;
}

//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy::_AllocateBuffers_ViewDependent(const RHI::PApiManager &manager, const SGeneratedInputs &toGenerate)
{
	PK_ASSERT(m_RendererType == Renderer_Billboard || m_RendererType == Renderer_Ribbon || m_RendererType == Renderer_Triangle);
	PK_ASSERT(!m_GPUStorage);

	// View dependent inputs (typically Indices/Positions/Normals/Tangents):
	//----------------------------------------------------------------------------

	// Specific to billboards and ribbons, as meshes don't have view specific data to billboard (matrices are view independent)
	u32			viewBuffersIndex = 0;
	const u32	generatedViewCount = toGenerate.m_PerViewGeneratedInputs.Count();
	for (u32 i = 0; i < generatedViewCount; ++i)
	{
		const u32	viewGeneratedInputs = toGenerate.m_PerViewGeneratedInputs[i].m_GeneratedInputs;
		if (viewGeneratedInputs == 0)
			continue; // Nothing to generate for this view

		SPerView	&viewBuffers = m_PerViewBuffers[viewBuffersIndex++];

		viewBuffers.m_ViewIdx = i;
		// View dependent indices
		if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("View Sorted Indices Buffer"), (viewGeneratedInputs & Drawers::GenInput_Indices) != 0, manager, viewBuffers.m_Indices, RHI::IndexBuffer, m_TotalIndexCount_OverEstimated * m_IndexSize, m_TotalIndexCount * m_IndexSize))
			return false;
		if (m_RendererType == Renderer_Ribbon || m_RendererType == Renderer_Billboard)
		{
			// View dependent positions
			if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("View Positions Vertex Buffer"), (viewGeneratedInputs & Drawers::GenInput_Position) != 0, manager, viewBuffers.m_Positions, RHI::VertexBuffer, m_TotalVertexCount_OverEstimated * sizeof(CFloat4), m_TotalVertexCount * sizeof(CFloat4)))
				return false;

			// Normal/Tangent are only generated when the "m_HasNormal/m_HasTangent" flag is set on your side (see ps_renderer_base.h SBillboardingFlags)
			{
				if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("View Normals Vertex Buffer"), (viewGeneratedInputs & Drawers::GenInput_Normal) != 0, manager, viewBuffers.m_Normals, RHI::VertexBuffer, m_TotalVertexCount_OverEstimated * sizeof(CFloat4), m_TotalVertexCount * sizeof(CFloat4)))
					return false;
				if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("View Tangents Vertex Buffer"), (viewGeneratedInputs & Drawers::GenInput_Tangent) != 0, manager, viewBuffers.m_Tangents, RHI::VertexBuffer, m_TotalVertexCount_OverEstimated * sizeof(CFloat4), m_TotalVertexCount * sizeof(CFloat4)))
					return false;
			}
		}
		if (m_RendererType == Renderer_Ribbon)
		{
			// Specific to ribbons (necessary for the CorrectDeformation feature) - Only enabled if m_HasRibbonCorrectDeformation is set on your side (see ps_renderer_base.h SBillboardingFlags)
			if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("View UVFactors Vertex Buffer"), (viewGeneratedInputs & Drawers::GenInput_UVFactors) != 0, manager, viewBuffers.m_UVFactors, RHI::VertexBuffer, m_TotalVertexCount_OverEstimated * sizeof(CFloat4), m_TotalVertexCount * sizeof(CFloat4)))
				return false;
		}
	}
	return true;
}

//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy::_IsAdditionalInputIgnored(const SRendererFeatureFieldDefinition &input)
{
	// 'TriangleCustomUVs' & 'TriangleCustomNormals' are additional inputs, but we do not want them passed to the vertex / fragment shaders.
	// Those particle streams are used by CPU billboarding tasks and data is generated in the buffers mapped for 'GenInput_UV' & 'GenInput_Normal'
	if (input.m_Type == PopcornFX::EBaseTypeID::BaseType_Float3)
	{
		if (input.m_Name == BasicRendererProperties::SID_TriangleCustomNormals_Normal1() ||
			input.m_Name == BasicRendererProperties::SID_TriangleCustomNormals_Normal2() ||
			input.m_Name == BasicRendererProperties::SID_TriangleCustomNormals_Normal3())
			return true;
	}

	if (input.m_Type == PopcornFX::EBaseTypeID::BaseType_Float2)
	{
		if (input.m_Name == BasicRendererProperties::SID_TriangleCustomUVs_UV1() ||
			input.m_Name == BasicRendererProperties::SID_TriangleCustomUVs_UV2() ||
			input.m_Name == BasicRendererProperties::SID_TriangleCustomUVs_UV3())
			return true;
	}

	// MeshLOD.LOD: Additional input, but we don't want it passed to the VS/PS shaders (Explicitely ignored as considered to be a builtin pin, see MaterialToRHI.cpp::_IsMeshAtlasFeature)
	if (input.m_Type == PopcornFX::EBaseTypeID::BaseType_I32)
	{
		if (input.m_Name == BasicRendererProperties::SID_MeshLOD_LOD())
			return true;
	}

	return false;
}

//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy::_AllocateBuffers_AdditionalInputs(const RHI::PApiManager &manager, const SGeneratedInputs &toGenerate)
{
	//----------------------------------------------------------------------------
	// Additional inputs:
	// Input streams not required by billboarding (Color, TextureID, AlphaRemapCursor, Additional shader inputs, ..)

	const u32	additionalInputCount = toGenerate.m_AdditionalGeneratedInputs.Count();

	if (m_GPUStorage)
	{
		if (m_RendererType == Renderer_Mesh)
		{
			if (!PK_VERIFY(m_SimStreamOffsets_AdditionalInputs.Reserve(additionalInputCount)) ||			// GPU buffer holding offsets into the GPU sim storage (1 element per draw request)
				!PK_VERIFY(m_MappedSimStreamOffsets_AdditionalInputs.Reserve(additionalInputCount)) ||		// CPU writable, we know the offsets on the CPU
				!PK_VERIFY(m_AdditionalFields.Reserve(additionalInputCount)))								// Holds the additional input id for each additional input index (as some are ignored)
				return false;
			const u32	offsetsSizeInBytes = kMaxGPUDrawRequestCount * sizeof(u32); // u32 offsets
			for (u32 i = 0, j = 0; i < additionalInputCount; ++i)
			{
				if (_IsAdditionalInputIgnored(toGenerate.m_AdditionalGeneratedInputs[i]))
					continue;
				if (j >= m_AdditionalFields.Count())
				{
					if (!PK_VERIFY(m_SimStreamOffsets_AdditionalInputs.PushBack().Valid()) ||
						!PK_VERIFY(m_MappedSimStreamOffsets_AdditionalInputs.PushBack().Valid()) ||
						!PK_VERIFY(m_AdditionalFields.PushBack().Valid()))
						return false;
				}
				if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Additional Input Offsets Buffer"), true, manager, m_SimStreamOffsets_AdditionalInputs[i], RHI::RawBuffer, offsetsSizeInBytes, offsetsSizeInBytes))
					return false;
				if (toGenerate.m_AdditionalGeneratedInputs[i].m_Name == BasicRendererProperties::SID_Diffuse_Color() ||
					toGenerate.m_AdditionalGeneratedInputs[i].m_Name == BasicRendererProperties::SID_Distortion_Color())
				{
					m_HasColorStream = true;
				}
				m_AdditionalFields[j].m_AdditionalInputIndex = i; // Important, we need that index internally in copy stream tasks
				++j;
			}
		}
	}
	else
	{
		if (!PK_VERIFY(m_AdditionalFields.Reserve(additionalInputCount)))
			return false;

		// 2 possible debug color in order:
		CGuid		diffuseColor;
		CGuid		distortionColor;

		for (u32 i = 0, j = 0; i < additionalInputCount; ++i)
		{
			// Create a vertex buffer for each additional field that your engine supports:
			// Those vertex buffers will be filled with particle data from the matching streams, per vertex, ie:
			// Particle stream with 2 particles, ViewposAligned so 4 vertices per particle :[Color0][Color1][TextureID0][TextureID1]
			// Dst Vertex buffers														   :[Color0][Color0][Color0][Color0][Color1][Color1][Color1][Color1][TextureID0][TextureID0][TextureID0][TextureID0][TextureID1][TextureID1][TextureID1][TextureID1]
			// Or it does a single copy per instance (for mesh particles, or when using geometry shader billboarding)
			// This policy creates a gpu buffer per additional input, but you could choose to only copy out specific particle fields

			if (_IsAdditionalInputIgnored(toGenerate.m_AdditionalGeneratedInputs[i]))
				continue;

			if (j >= m_AdditionalFields.Count())
			{
				if (!PK_VERIFY(m_AdditionalFields.PushBack().Valid()))
					return false;
			}

			SAdditionalInputs	&decl = (j >= m_AdditionalFields.Count()) ? m_AdditionalFields.Last() : m_AdditionalFields[j];

			const u32	typeSize = CBaseTypeTraits::Traits(toGenerate.m_AdditionalGeneratedInputs[i].m_Type).Size;
			decl.m_ByteSize = typeSize;
			decl.m_AdditionalInputIndex = i; // Important, we need that index internally in copy stream tasks

			 // Here is where you can discard specific additional inputs if your engine does not support them
			 // Ie. If the target engine doesn't support additional inputs, don't allocate vertex buffers for it
			if (toGenerate.m_AdditionalGeneratedInputs[i].m_Type == PopcornFX::EBaseTypeID::BaseType_Float4 &&
				toGenerate.m_AdditionalGeneratedInputs[i].m_Name == BasicRendererProperties::SID_Diffuse_Color())
				diffuseColor = i;
			else if (toGenerate.m_AdditionalGeneratedInputs[i].m_Type == PopcornFX::EBaseTypeID::BaseType_Float4 &&
				toGenerate.m_AdditionalGeneratedInputs[i].m_Name == BasicRendererProperties::SID_Distortion_Color())
				distortionColor = i;
			else if (toGenerate.m_AdditionalGeneratedInputs[i].m_Type == PopcornFX::EBaseTypeID::BaseType_Float &&
				toGenerate.m_AdditionalGeneratedInputs[i].m_Name == BasicRendererProperties::SID_LightAttenuation_Range())
				decl.m_Semantic = SRHIDrawCall::DebugDrawGPUBuffer_InstanceScales; // Flag that additional field for debug draw (editor only)

			if (!PK_VERIFY(_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("AdditionalInputs Vertex Buffer"), true, manager, decl.m_Buffer, RHI::VertexBuffer, m_TotalVertexCount_OverEstimated * typeSize, m_TotalVertexCount * typeSize)))
				return false;

			++j;
		}

		if (diffuseColor.Valid())
			m_AdditionalFields[diffuseColor].m_Semantic = SRHIDrawCall::DebugDrawGPUBuffer_Color; // Flag that additional field for debug draw (editor only)
		else if (distortionColor.Valid())
			m_AdditionalFields[distortionColor].m_Semantic = SRHIDrawCall::DebugDrawGPUBuffer_Color; // Flag that additional field for debug draw (editor only)
	}
	return true;
}

//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy::_AllocateBuffers_GeomShaderBillboarding(const RHI::PApiManager &manager, u32 viewIndependentInputs)
{
	PK_ASSERT(!m_GPUStorage);

	// Specific to billboarding done in a geometry shader, ignore if you don't want to billboard that way

	// For CPU simulation (as GPU sim is just mapping existing GPU buffers to the draw call)
	// We issue tasks to fill vbuffers with particle sim data necessary for billboarding
	if (viewIndependentInputs & Drawers::GenInput_ParticlePosition)
	{
		// Particle positions stream
		PK_ASSERT(m_TotalVertexCount == m_TotalParticleCount);
		if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Positions Vertex Buffer"), true, manager, m_GeomPositions, RHI::VertexBuffer, m_TotalVertexCount_OverEstimated * sizeof(CFloat4), m_TotalParticleCount * sizeof(CFloat4)))
			return false;

		// Constant buffer filled by CPU task, will contain simple description of draw request
		// We do this so we can batch various draw requests (renderers from various mediums) in a single "draw call"
		// This constant buffer will contain flags for each draw request
		// Each particle position will contain its associated draw request ID in position's W component (See sample geometry billboard shader for more detail)
		PK_STATIC_ASSERT(sizeof(Drawers::SBillboardDrawRequest) == sizeof(CFloat4));
		if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Billboard DrawRequests Buffer"), true, manager, m_GeomConstants, RHI::ConstantBuffer, kMaxGeomDrawRequestCount * sizeof(Drawers::SBillboardDrawRequest), kMaxGeomDrawRequestCount * sizeof(Drawers::SBillboardDrawRequest)))
			return false;
	}

	// Can be either one
	{
		if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Sizes Vertex Buffer"), (viewIndependentInputs & Drawers::GenInput_ParticleSize) != 0, manager, m_GeomSizes, RHI::VertexBuffer, m_TotalVertexCount_OverEstimated * sizeof(float), m_TotalParticleCount * sizeof(float)))
			return false;
		if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Size2s Vertex Buffer"), (viewIndependentInputs & Drawers::GenInput_ParticleSize2) != 0, manager, m_GeomSizes2, RHI::VertexBuffer, m_TotalVertexCount_OverEstimated * sizeof(CFloat2), m_TotalParticleCount * sizeof(CFloat2)))
			return false;
	}

	// Rotation particle stream
	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Rotations Vertex Buffer"), (viewIndependentInputs & Drawers::GenInput_ParticleRotation) != 0, manager, m_GeomRotations, RHI::VertexBuffer, m_TotalVertexCount_OverEstimated * sizeof(float), m_TotalParticleCount * sizeof(float)))
		return false;
	// First billboarding axis (necessary for AxisAligned billboarding modes)
	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Axis0s Vertex Buffer"), (viewIndependentInputs & Drawers::GenInput_ParticleAxis0) != 0, manager, m_GeomAxis0, RHI::VertexBuffer, m_TotalVertexCount_OverEstimated * sizeof(CFloat3), m_TotalParticleCount * sizeof(CFloat3)))
		return false;
	// Second billboarding axis (necessary for PlaneAligned)
	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Axis1s Vertex Buffer"), (viewIndependentInputs & Drawers::GenInput_ParticleAxis1) != 0, manager, m_GeomAxis1, RHI::VertexBuffer, m_TotalVertexCount_OverEstimated * sizeof(CFloat3), m_TotalParticleCount * sizeof(CFloat3)))
		return false;

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy::_AllocateBuffers_Lights(const RHI::PApiManager &manager, const SGeneratedInputs &toGenerate)
{
	PK_ASSERT(!m_GPUStorage);

	// Here we first allocate the light positions buffer (there will always be some light positions):
	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Light Positions Vertex Buffer"), true, manager, m_LightsPositions, RHI::VertexBuffer, m_TotalParticleCount_OverEstimated * sizeof(CFloat3), m_TotalParticleCount * sizeof(CFloat3)))
		return false;

	const u32	additionalFieldsCount = toGenerate.m_AdditionalGeneratedInputs.Count();

	if (!m_AdditionalFields.Resize(additionalFieldsCount))
		return false;

	// Alloc the additional inputs for the lights:
	for (u32 i = 0; i < additionalFieldsCount; ++i)
	{
		const u32	typeSize = CBaseTypeTraits::Traits(toGenerate.m_AdditionalGeneratedInputs[i].m_Type).Size;
		m_AdditionalFields[i].m_ByteSize = typeSize;
		m_AdditionalFields[i].m_AdditionalInputIndex = i; // Important, we need that index internally in copy stream tasks

		// Here is where you can discard specific additional inputs if your engine does not support them
		// Ie. If the target engine doesn't support additional inputs, don't allocate vertex buffers for it
		if (toGenerate.m_AdditionalGeneratedInputs[i].m_Type == PopcornFX::EBaseTypeID::BaseType_Float4 &&
			(toGenerate.m_AdditionalGeneratedInputs[i].m_Name == BasicRendererProperties::SID_Diffuse_Color() ||
			 toGenerate.m_AdditionalGeneratedInputs[i].m_Name == BasicRendererProperties::SID_Distortion_Color()))
			m_AdditionalFields[i].m_Semantic = SRHIDrawCall::DebugDrawGPUBuffer_Color;
		else
			m_AdditionalFields[i].m_Semantic = SRHIDrawCall::_DebugDrawGPUBuffer_Count;

		if (!PK_VERIFY(_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Light AdditionalInputs Vertex Buffer"), true, manager, m_AdditionalFields[i].m_Buffer, RHI::VertexBuffer, m_TotalParticleCount_OverEstimated * typeSize, m_TotalParticleCount * typeSize)))
			return false;
	}
	return true;
}

//----------------------------------------------------------------------------
//
//	Billboards (CPU billboarding)
//
//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy::MapBuffers(SRenderContext &ctx, const TMemoryView<SSceneView> &views, SBillboardBatchJobs *billboardBatch, const SGeneratedInputs &toMap)
{
	PK_ASSERT(!m_GPUStorage);

	(void)views;
	PK_NAMEDSCOPEDPROFILE("CRHIBillboardingBatchPolicy::MapBuffers (billboards)");

	if (billboardBatch == null)
	{
		// TODO: FIX This, MapBuffers shouldn't be called with an empty SBillboardBatchJobs for sound draw requests
		if (ctx.IsPostUpdateFencePass()) // This is normal, we are processing sounds
			return true;
		PK_ASSERT_NOT_REACHED();
		return false;
	}

	RHI::PApiManager	manager = ctx.ApiManager();

	// Assign mapped vertex/index buffers on CPU tasks that will run async on worker threads
	// All buffers used below have been allocated in AllocBuffers either during this BeginCollectingDrawCalls
	// They could also have been allocated in a previous BeginCollectingDrawCalls call, see documentation for more detail
	// CBillboard_Exec_Indices will generate indices and sort particles based on the renderer sort metric if necessary
	// CBillboard_Exec_PositionsNormals will generate particles geometry (quads/capsules)
	// CBillboard_Exec_Texcoords will generate particle texcoords (using an atlas if specified)
	// CBillboard_Exec_CopyField will do a straight copy of particle data into vertex buffers (once per vertex or once per particle if CBillboard_Exec_CopyField::m_PerVertex is set to false)

	// View independent inputs:
	if (toMap.m_GeneratedInputs & Drawers::GenInput_Indices)
	{
		PK_ASSERT(m_Indices.Used());
		void	*mappedValue = manager->MapCpuView(m_Indices.m_Buffer, 0, m_IndexSize * m_TotalIndexCount);
		if (!PK_VERIFY(mappedValue != null))
			return false;
		billboardBatch->m_Exec_Indices.m_IndexStream.Setup(mappedValue, m_TotalIndexCount, m_IndexSize == sizeof(u32));
	}
	if (toMap.m_GeneratedInputs & Drawers::GenInput_Position)
	{
		PK_ASSERT(m_Positions.Used());
		void	*mappedValue = manager->MapCpuView(m_Positions.m_Buffer, 0, m_TotalVertexCount * sizeof(CFloat4));
		if (!PK_VERIFY(mappedValue != null))
			return false;
		billboardBatch->m_Exec_PNT.m_Positions = TStridedMemoryView<CFloat3, 0x10>(static_cast<CFloat3*>(mappedValue), m_TotalVertexCount, 0x10);
	}
	if (toMap.m_GeneratedInputs & Drawers::GenInput_Normal)
	{
		PK_ASSERT(m_Normals.Used());
		void	*mappedValue = manager->MapCpuView(m_Normals.m_Buffer, 0, m_TotalVertexCount * sizeof(CFloat4));
		if (!PK_VERIFY(mappedValue != null))
			return false;
		billboardBatch->m_Exec_PNT.m_Normals = TStridedMemoryView<CFloat3, 0x10>(static_cast<CFloat3*>(mappedValue), m_TotalVertexCount, 0x10);
	}
	if (toMap.m_GeneratedInputs & Drawers::GenInput_Tangent)
	{
		PK_ASSERT(m_Tangents.Used());
		void	*mappedValue = manager->MapCpuView(m_Tangents.m_Buffer, 0, m_TotalVertexCount * sizeof(CFloat4));
		if (!PK_VERIFY(mappedValue != null))
			return false;
		billboardBatch->m_Exec_PNT.m_Tangents = TStridedMemoryView<CFloat4, 0x10>(static_cast<CFloat4*>(mappedValue), m_TotalVertexCount, 0x10);
	}
	if (toMap.m_GeneratedInputs & Drawers::GenInput_UV0)
	{
		PK_ASSERT(m_TexCoords0.Used());
		void	*mappedValue = manager->MapCpuView(m_TexCoords0.m_Buffer, 0, m_TotalVertexCount * sizeof(CFloat2));
		if (!PK_VERIFY(mappedValue != null))
			return false;
		billboardBatch->m_Exec_Texcoords.m_Texcoords = TStridedMemoryView<CFloat2>(static_cast<CFloat2*>(mappedValue), m_TotalVertexCount);
	}
	if (toMap.m_GeneratedInputs & Drawers::GenInput_UV1)
	{
		PK_ASSERT(m_TexCoords1.Used());
		void	*mappedValue = manager->MapCpuView(m_TexCoords1.m_Buffer, 0, m_TotalVertexCount * sizeof(CFloat2));
		if (!PK_VERIFY(mappedValue != null))
			return false;
		billboardBatch->m_Exec_Texcoords.m_Texcoords2 = TStridedMemoryView<CFloat2>(static_cast<CFloat2*>(mappedValue), m_TotalVertexCount);
	}
	if (toMap.m_GeneratedInputs & Drawers::GenInput_AtlasId)
	{
		PK_ASSERT(m_AtlasIDs.Used());
		void	*mappedValue = manager->MapCpuView(m_AtlasIDs.m_Buffer, 0, m_TotalVertexCount * sizeof(float));
		if (!PK_VERIFY(mappedValue != null))
			return false;
		billboardBatch->m_Exec_Texcoords.m_AtlasIds = TMemoryView<float>(static_cast<float*>(mappedValue), m_TotalVertexCount);
	}

	// View dependent inputs:
	PK_ASSERT(m_PerViewBuffers.Count() == billboardBatch->m_PerView.Count());
	for (u32 i = 0; i < m_PerViewBuffers.Count(); ++i)
	{
		const u32	viewGeneratedInputs = toMap.m_PerViewGeneratedInputs[i].m_GeneratedInputs;

		PK_ASSERT(m_PerViewBuffers[i].m_ViewIdx == billboardBatch->m_PerView[i].m_ViewIndex);
		if (viewGeneratedInputs & Drawers::GenInput_Indices)
		{
			PK_ASSERT(m_PerViewBuffers[i].m_Indices.Used());
			void	*mappedValue = manager->MapCpuView(m_PerViewBuffers[i].m_Indices.m_Buffer, 0, m_TotalIndexCount * m_IndexSize);
			if (!PK_VERIFY(mappedValue != null))
				return false;
			billboardBatch->m_PerView[i].m_Exec_Indices.m_IndexStream.Setup(mappedValue, m_TotalIndexCount, m_IndexSize == sizeof(u32));
		}
		if (viewGeneratedInputs & Drawers::GenInput_Position)
		{
			PK_ASSERT(m_PerViewBuffers[i].m_Positions.Used());
			void	*mappedValue = manager->MapCpuView(m_PerViewBuffers[i].m_Positions.m_Buffer, 0, m_TotalVertexCount * sizeof(CFloat4));
			if (!PK_VERIFY(mappedValue != null))
				return false;
			billboardBatch->m_PerView[i].m_Exec_PNT.m_Positions = TStridedMemoryView<CFloat3, 0x10>(static_cast<CFloat3*>(mappedValue), m_TotalVertexCount, 0x10);
		}
		if (viewGeneratedInputs & Drawers::GenInput_Normal)
		{
			PK_ASSERT(m_PerViewBuffers[i].m_Normals.Used());
			void	*mappedValue = manager->MapCpuView(m_PerViewBuffers[i].m_Normals.m_Buffer, 0, m_TotalVertexCount * sizeof(CFloat4));
			if (!PK_VERIFY(mappedValue != null))
				return false;
			billboardBatch->m_PerView[i].m_Exec_PNT.m_Normals = TStridedMemoryView<CFloat3, 0x10>(static_cast<CFloat3*>(mappedValue), m_TotalVertexCount, 0x10);
		}
		if (viewGeneratedInputs & Drawers::GenInput_Tangent)
		{
			PK_ASSERT(m_PerViewBuffers[i].m_Tangents.Used());
			void	*mappedValue = manager->MapCpuView(m_PerViewBuffers[i].m_Tangents.m_Buffer, 0, m_TotalVertexCount * sizeof(CFloat4));
			if (!PK_VERIFY(mappedValue != null))
				return false;
			billboardBatch->m_PerView[i].m_Exec_PNT.m_Tangents = TStridedMemoryView<CFloat4, 0x10>(static_cast<CFloat4*>(mappedValue), m_TotalVertexCount, 0x10);
		}
	}

	// Additional inputs:
	PK_ASSERT(toMap.m_AdditionalGeneratedInputs.Count() >= m_AdditionalFields.Count());
	if (!m_AdditionalFields.Empty())
	{
		m_MappedAdditionalFields.Clear();
		if (!PK_VERIFY(m_MappedAdditionalFields.Reserve(m_AdditionalFields.Count())))
			return false;
		for (u32 i = 0; i < m_AdditionalFields.Count(); ++i)
		{
			if (m_AdditionalFields[i].m_Buffer.Used())
			{
				void	*mappedValue = manager->MapCpuView(m_AdditionalFields[i].m_Buffer.m_Buffer, 0, m_TotalVertexCount * m_AdditionalFields[i].m_ByteSize);
				if (!PK_VERIFY(mappedValue != null))
					return false;

				Drawers::SCopyFieldDesc	mappedAdditionnalField;
				mappedAdditionnalField.m_AdditionalInputIndex = m_AdditionalFields[i].m_AdditionalInputIndex;
				mappedAdditionnalField.m_Storage.m_Count = m_TotalVertexCount;
				mappedAdditionnalField.m_Storage.m_RawDataPtr = static_cast<u8*>(mappedValue);
				mappedAdditionnalField.m_Storage.m_Stride = m_AdditionalFields[i].m_ByteSize;
				m_MappedAdditionalFields.PushBackUnsafe(mappedAdditionnalField);
			}
		}
		billboardBatch->m_Exec_CopyField.m_FieldsToCopy = m_MappedAdditionalFields.View();
	}

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy::LaunchCustomTasks(SRenderContext &ctx, const TMemoryView<const Drawers::SBillboard_DrawRequest * const> &drawRequests, Drawers::CBillboard_CPU *billboardBatch)
{
	(void)ctx; (void)drawRequests; (void)billboardBatch;
#if	(PK_HAS_PARTICLES_SELECTION != 0)
	// Editor only: for debugging purposes, we'll remove that from samples code later
	if (ctx.Selection().HasParticlesSelected() || ctx.Selection().HasRendersSelected())
	{
		// If we have a CPU storage, we can render the particles that are selected as wire-frame
		m_BillboardCustomParticleSelectTask.Clear();
		PK_ASSERT(m_IsParticleSelected.Used());
		void	*mappedValue = ctx.ApiManager()->MapCpuView(m_IsParticleSelected.m_Buffer, 0, sizeof(float) * m_TotalVertexCount);
		if (!PK_VERIFY(mappedValue != null))
			return false;
		m_BillboardCustomParticleSelectTask.m_DstSelectedParticles = TStridedMemoryView<float>(static_cast<float*>(mappedValue), m_TotalVertexCount, sizeof(float));
		m_BillboardCustomParticleSelectTask.m_SrcParticleSelected = ctx.Selection();
		billboardBatch->AddExecPage(&m_BillboardCustomParticleSelectTask);
	}
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)
	return true;
}

//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy::_IssueDrawCall_Billboard_CPU(SRenderContext &ctx, const SDrawCallDesc &toEmit, SRHIDrawOutputs &output)
{
	PK_ASSERT(!m_GPUStorage);

	PK_NAMEDSCOPEDPROFILE("CRHIBillboardingBatchPolicy::EmitDrawCall Billboard (CPU)");

	// Common draw call code for CPU billboarding and geom shader billboarding
	// Draw calls are not issued inline here, but later in PK-SampleLib's render loop
	// _CreateDrawCalls outputs a new pushed draw call that will be processed later (in SRHIDrawOutputs)

	SRHIDrawCall	*_outDrawCall = _CreateDrawCall(toEmit, output, SRHIDrawCall::DrawCall_Regular, PKSample::Option_VertexPassThrough);
	if (!PK_VERIFY(_outDrawCall != null))
	{
		CLog::Log(PK_ERROR, "Failed to create a draw-call");
		return false;
	}

	SRHIDrawCall	&outDrawCall = *_outDrawCall;

	// We had a geometry shader batch, ignore if you don't want to billboard that way:
	if (m_GeomPositions.Used())
	{
		const Drawers::SBillboard_DrawRequest	*dr = static_cast<const Drawers::SBillboard_DrawRequest*>(toEmit.m_DrawRequests.First());

		outDrawCall.m_ShaderOptions |= _GetGeomBillboardShaderOptions(dr->m_BB);
		PK_ASSERT((outDrawCall.m_ShaderOptions & Option_GeomBillboarding) != 0);

		if (!_SetupGeomBillboardVertexBuffers(outDrawCall))
			return false;
	}
	else
	{
		bool	hasAtlas = false;
		CRendererCacheInstance_UpdateThread		*refCacheInstance = static_cast<CRendererCacheInstance_UpdateThread*>(toEmit.m_RendererCaches.First().Get());
		if (PK_VERIFY(refCacheInstance != null))
		{
			PKSample::PCRendererCacheInstance		rCacheInstance = refCacheInstance->RenderThread_GetCacheInstance();
			if (PK_VERIFY(rCacheInstance != null))
				hasAtlas = rCacheInstance->m_HasAtlas;
		}
		if (!_SetupCommonBillboardVertexBuffers(outDrawCall, hasAtlas))
			return false;
	}
	if (!_SetupBillboardDrawCall(ctx, toEmit, outDrawCall))
		return false;
	return true;
}

//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy::_SetupCommonBillboardVertexBuffers(SRHIDrawCall &outDrawCall, bool hasAtlas)
{
	if (!m_PerViewBuffers.Empty() && m_PerViewBuffers[0].m_Positions.Used())
	{
		// View dependent buffers (Right now this code only takes in account first view)
		// If the first view contains an allocated Positions buffer, it means we have view dependent geometry:
		// For billboard, everything that isn't PlanarAxisAligned -> Positions/Normals/Tangents need to be billboarded per view to be correct
		// For ribbons, everything that is ViewposAligned -> same
		if (!PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(m_PerViewBuffers[0].m_Positions.m_Buffer).Valid()))
			return false;
		if (m_PerViewBuffers[0].m_Normals.Used() && !PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(m_PerViewBuffers[0].m_Normals.m_Buffer).Valid()))
			return false;
		if (m_PerViewBuffers[0].m_Tangents.Used() && !PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(m_PerViewBuffers[0].m_Tangents.m_Buffer).Valid()))
			return false;
	}
	else
	{
		if (m_Positions.Used() && !PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(m_Positions.m_Buffer).Valid()))
			return false;
		if (m_Normals.Used() && !PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(m_Normals.m_Buffer).Valid()))
			return false;
		if (m_Tangents.Used() && !PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(m_Tangents.m_Buffer).Valid()))
			return false;
	}

	if (m_TexCoords0.Used())
	{
		if (!PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(m_TexCoords0.m_Buffer).Valid()))
			return false;

		// Atlas renderer feature enabled: None/Linear atlas blending share the same vertex declaration, so we just push empty buffers when blending is disabled
		PK_ASSERT(m_TexCoords1.Used() == m_AtlasIDs.Used());
		if (hasAtlas || m_TexCoords1.Used())
		{
			// If we have invalid m_TexCoords1/m_AtlasIDs, bind a dummy vertex buffer, here m_TexCoords0
			if (!PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(m_TexCoords1.Used() ? m_TexCoords1.m_Buffer : m_TexCoords0.m_Buffer).Valid()) ||
				!PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(m_AtlasIDs.Used() ? m_AtlasIDs.m_Buffer : m_TexCoords0.m_Buffer).Valid()))
				return false;
		}
	}

	if (m_UVRemap.Used() && !PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(m_UVRemap.m_Buffer).Valid()))
		return false;

	if (!m_PerViewBuffers.Empty() && m_PerViewBuffers[0].m_Positions.Used())
	{
		// Same check as above, but UVFactors are pushed in last to match vertex declaration expected by RHI Render states
		if (m_PerViewBuffers[0].m_UVFactors.Used() && !PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(m_PerViewBuffers[0].m_UVFactors.m_Buffer).Valid()))
			return false;
	}
	else
	{
		if (m_UVFactors.Used() && !PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(m_UVFactors.m_Buffer).Valid()))
			return false;
	}

	// Editor only: for debugging purposes, we'll remove that from samples code later
	outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Position] = outDrawCall.m_VertexBuffers.First();
	return true;
}

//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy::_SetupBillboardDrawCall(SRenderContext &ctx, const SDrawCallDesc &toEmit, SRHIDrawCall &outDrawCall)
{
	(void)ctx;
	outDrawCall.m_IndexOffset = toEmit.m_IndexOffset;
	outDrawCall.m_VertexCount = toEmit.m_TotalVertexCount;
	outDrawCall.m_IndexCount = toEmit.m_TotalIndexCount;
	outDrawCall.m_IndexSize = (m_IndexSize == sizeof(u32)) ? RHI::IndexBuffer32Bit : RHI::IndexBuffer16Bit;

	// Indices are mandatory, here we check if we have view independent indices or not:
	if (!m_PerViewBuffers.Empty() && m_PerViewBuffers[0].m_Indices.Used())
		outDrawCall.m_IndexBuffer = m_PerViewBuffers[0].m_Indices.m_Buffer;
	else
		outDrawCall.m_IndexBuffer = m_Indices.m_Buffer;
	PK_ASSERT(outDrawCall.m_IndexBuffer != null);

	// Push additional inputs vertex buffers:
	for (u32 i = 0; i < m_AdditionalFields.Count(); ++i)
	{
		if (!m_AdditionalFields[i].m_Buffer.Used())
			continue;
		if (!PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(m_AdditionalFields[i].m_Buffer.m_Buffer).Valid()))
			return false;

		// Editor only: for debugging purposes, we'll remove that from samples code later
		if (m_AdditionalFields[i].m_Semantic == SRHIDrawCall::DebugDrawGPUBuffer_Color)
		{
			PK_ASSERT(outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Color] == null);
			outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Color] = m_AdditionalFields[i].m_Buffer.m_Buffer;
		}
	}

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	// Editor only: for debugging purposes, we'll remove that from samples code later
	if ((ctx.Selection().HasParticlesSelected() || ctx.Selection().HasRendersSelected()) && m_IsParticleSelected.Used())
	{
		outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_IsParticleSelected] = m_IsParticleSelected.m_Buffer;
	}
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)
	return true;
}

//----------------------------------------------------------------------------
//
//	Billboards (Geometry shader billboarding)
//
//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy::MapBuffers(SRenderContext &ctx, const TMemoryView<SSceneView> &views, SGPUBillboardBatchJobs *billboardGeomBatch, const SGeneratedInputs &toMap)
{
	(void)views;
	PK_NAMEDSCOPEDPROFILE("CRHIBillboardingBatchPolicy::MapBuffers (geom billboard)");

	RHI::PApiManager	manager = ctx.ApiManager();

	if (billboardGeomBatch == null)
	{
		// If the billboard batch is null, we have GPU storage:
		m_MappedIndirectBuffer = static_cast<RHI::SDrawIndirectArgs*>(manager->MapCpuView(m_IndirectDraw.m_Buffer));
		return PK_VERIFY(m_MappedIndirectBuffer != null);
	}
	PK_ASSERT(!m_GPUStorage);

	// Assign mapped vertex/index buffers on CPU tasks that will run async on worker threads
	// All buffers used below have been allocated in AllocBuffers either during this BeginCollectingDrawCalls
	// They could also have been allocated in a previous BeginCollectingDrawCalls call, see documentation for more detail
	// CCopyStream_Exec_Indices will generate indices and sort particles based on the renderer sort metric if necessary
	// CCopyStream_Exec_Billboard_Std will copy particle sim data necessary for billboarding (Positions/Sizes/Rotations/Axis0s/Axis1s)
	// This tasks will also write in Position's W component the associated draw request ID
	// CCopyStream_Exec_GPUBillboardDrawRequests will write draw request datas packed
	// CCopyStream_Exec_AdditionalField will do a straight copy of specified particle data into the vertex buffers

	if (toMap.m_GeneratedInputs & Drawers::GenInput_Indices)
	{
		PK_ASSERT(m_Indices.Used());
		void	*mappedValue = manager->MapCpuView(m_Indices.m_Buffer, 0, m_IndexSize * m_TotalParticleCount);
		if (!PK_VERIFY(mappedValue != null))
			return false;
		billboardGeomBatch->m_Exec_Indices.m_IndexStream.Setup(mappedValue, m_TotalParticleCount, m_IndexSize == sizeof(u32));
	}
	if (toMap.m_GeneratedInputs & Drawers::GenInput_ParticlePosition)
	{
		PK_ASSERT(m_GeomPositions.Used());
		void	*mappedValue = manager->MapCpuView(m_GeomPositions.m_Buffer, 0, m_TotalParticleCount * sizeof(CFloat4));
		if (!PK_VERIFY(mappedValue != null))
			return false;
		billboardGeomBatch->m_Exec_CopyBillboardingStreams.m_PositionsDrIds = TMemoryView<Drawers::SVertex_PositionDrId>(static_cast<Drawers::SVertex_PositionDrId*>(mappedValue), m_TotalParticleCount);

		PK_ASSERT(m_GeomConstants.Used());
		void	*mappedConstants = manager->MapCpuView(m_GeomConstants.m_Buffer, 0, kMaxGeomDrawRequestCount * sizeof(Drawers::SBillboardDrawRequest));
		if (!PK_VERIFY(mappedConstants != null))
			return false;

		billboardGeomBatch->m_Exec_GeomBillboardDrawRequests.m_GeomDrawRequests = TMemoryView<Drawers::SBillboardDrawRequest>(static_cast<Drawers::SBillboardDrawRequest*>(mappedConstants), kMaxGeomDrawRequestCount);
	}
	if (toMap.m_GeneratedInputs & Drawers::GenInput_ParticleSize)
	{
		PK_ASSERT(m_GeomSizes.Used());
		void	*mappedValue = manager->MapCpuView(m_GeomSizes.m_Buffer, 0, m_TotalParticleCount * sizeof(float));
		if (!PK_VERIFY(mappedValue != null))
			return false;
		billboardGeomBatch->m_Exec_CopyBillboardingStreams.m_Sizes = TMemoryView<float>(static_cast<float*>(mappedValue), m_TotalParticleCount);
	}
	else if (toMap.m_GeneratedInputs & Drawers::GenInput_ParticleSize2)
	{
		PK_ASSERT(m_GeomSizes2.Used());
		void	*mappedValue = manager->MapCpuView(m_GeomSizes2.m_Buffer, 0, m_TotalParticleCount * sizeof(CFloat2));
		if (!PK_VERIFY(mappedValue != null))
			return false;
		billboardGeomBatch->m_Exec_CopyBillboardingStreams.m_Sizes2 = TMemoryView<CFloat2>(static_cast<CFloat2*>(mappedValue), m_TotalParticleCount);
	}
	if (toMap.m_GeneratedInputs & Drawers::GenInput_ParticleRotation)
	{
		PK_ASSERT(m_GeomRotations.Used());
		void	*mappedValue = manager->MapCpuView(m_GeomRotations.m_Buffer, 0, m_TotalParticleCount * sizeof(float));
		if (!PK_VERIFY(mappedValue != null))
			return false;
		billboardGeomBatch->m_Exec_CopyBillboardingStreams.m_Rotations = TMemoryView<float>(static_cast<float*>(mappedValue), m_TotalParticleCount);
	}
	if (toMap.m_GeneratedInputs & Drawers::GenInput_ParticleAxis0)
	{
		PK_ASSERT(m_GeomAxis0.Used());
		void	*mappedValue = manager->MapCpuView(m_GeomAxis0.m_Buffer, 0, m_TotalParticleCount * sizeof(CFloat3));
		if (!PK_VERIFY(mappedValue != null))
			return false;
		billboardGeomBatch->m_Exec_CopyBillboardingStreams.m_Axis0 = TMemoryView<CFloat3>(static_cast<CFloat3*>(mappedValue), m_TotalParticleCount);
	}
	if (toMap.m_GeneratedInputs & Drawers::GenInput_ParticleAxis1)
	{
		PK_ASSERT(m_GeomAxis1.Used());
		void	*mappedValue = manager->MapCpuView(m_GeomAxis1.m_Buffer, 0, m_TotalParticleCount * sizeof(CFloat3));
		if (!PK_VERIFY(mappedValue != null))
			return false;
		billboardGeomBatch->m_Exec_CopyBillboardingStreams.m_Axis1 = TMemoryView<CFloat3>(static_cast<CFloat3*>(mappedValue), m_TotalParticleCount);
	}

	// View dependent inputs:
	PK_ASSERT(m_PerViewBuffers.Count() == billboardGeomBatch->m_PerView.Count());
	for (u32 i = 0; i < m_PerViewBuffers.Count(); ++i)
	{
		const u32	viewGeneratedInputs = toMap.m_PerViewGeneratedInputs[i].m_GeneratedInputs;

		PK_ASSERT(m_PerViewBuffers[i].m_ViewIdx == billboardGeomBatch->m_PerView[i].m_ViewIndex);
		if (viewGeneratedInputs & Drawers::GenInput_Indices)
		{
			PK_ASSERT(m_PerViewBuffers[i].m_Indices.Used());
			void	*mappedValue = manager->MapCpuView(m_PerViewBuffers[i].m_Indices.m_Buffer, 0, m_TotalParticleCount * m_IndexSize);
			if (!PK_VERIFY(mappedValue != null))
				return false;
			billboardGeomBatch->m_PerView[i].m_Exec_Indices.m_IndexStream.Setup(mappedValue, m_TotalParticleCount, m_IndexSize == sizeof(u32));
		}
	}

	// Additional inputs:
	PK_ASSERT(toMap.m_AdditionalGeneratedInputs.Count() >= m_AdditionalFields.Count());
	if (!m_AdditionalFields.Empty())
	{
		m_MappedAdditionalFields.Clear();
		if (!PK_VERIFY(m_MappedAdditionalFields.Reserve(m_AdditionalFields.Count())))
			return false;
		for (u32 i = 0; i < m_AdditionalFields.Count(); ++i)
		{
			if (m_AdditionalFields[i].m_Buffer.Used())
			{
				void	*mappedValue = manager->MapCpuView(m_AdditionalFields[i].m_Buffer.m_Buffer, 0, m_TotalParticleCount * m_AdditionalFields[i].m_ByteSize);
				if (!PK_VERIFY(mappedValue != null))
					return false;

				Drawers::SCopyFieldDesc	mappedAdditionnalField;
				mappedAdditionnalField.m_AdditionalInputIndex = m_AdditionalFields[i].m_AdditionalInputIndex;
				mappedAdditionnalField.m_Storage.m_Count = m_TotalParticleCount;
				mappedAdditionnalField.m_Storage.m_RawDataPtr = static_cast<u8*>(mappedValue);
				mappedAdditionnalField.m_Storage.m_Stride = m_AdditionalFields[i].m_ByteSize;
				m_MappedAdditionalFields.PushBackUnsafe(mappedAdditionnalField);
			}
		}
		billboardGeomBatch->m_Exec_CopyAdditionalFields.m_FieldsToCopy = m_MappedAdditionalFields.View();
	}

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy::LaunchCustomTasks(SRenderContext &ctx, const TMemoryView<const Drawers::SBillboard_DrawRequest * const> &drawRequests, Drawers::CCopyStream_CPU *billboardBatch)
{
	(void)ctx;
	// If the billboard batch is null, we have GPU storage:
	if (billboardBatch == null)
	{
		PK_ASSERT(m_MappedIndirectBuffer != null);
		PK_ASSERT(m_GPUStorage);

		for (u32 i = 0; i < drawRequests.Count(); ++i)
		{
			m_MappedIndirectBuffer[i].m_InstanceCount = 1;
			m_MappedIndirectBuffer[i].m_InstanceOffset = 0;
			m_MappedIndirectBuffer[i].m_VertexOffset = 0;
			m_MappedIndirectBuffer[i].m_VertexCount = drawRequests[i]->RenderedParticleCount();
		}

		return true;
	}
	PK_ASSERT(!m_GPUStorage);

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	// Editor only: for debugging purposes, we'll remove that from samples code later
	if (ctx.Selection().HasParticlesSelected() || ctx.Selection().HasRendersSelected())
	{
		// If we have a CPU storage, we can render the particles that are selected as wire-frame
		m_CopyStreamCustomParticleSelectTask.Clear();
		PK_ASSERT(m_IsParticleSelected.Used());
		void	*mappedValue = ctx.ApiManager()->MapCpuView(m_IsParticleSelected.m_Buffer, 0, sizeof(float) * m_TotalVertexCount);
		if (!PK_VERIFY(mappedValue != null))
			return false;
		m_CopyStreamCustomParticleSelectTask.m_DstSelectedParticles = TStridedMemoryView<float>(static_cast<float*>(mappedValue), m_TotalVertexCount, sizeof(float));
		m_CopyStreamCustomParticleSelectTask.m_SrcParticleSelected = ctx.Selection();
		billboardBatch->AddExecAsyncPage(&m_CopyStreamCustomParticleSelectTask);
	}
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)
	return true;
}

//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy::_IssueDrawCall_Billboard_GPU(SRenderContext &ctx, const SDrawCallDesc &toEmit, SRHIDrawOutputs &output)
{
	PK_ASSERT(m_GPUStorage);

	PK_NAMEDSCOPEDPROFILE("CRHIBillboardingBatchPolicy::EmitDrawCall Billboard (GPU)");

	RHI::PApiManager	manager = ctx.ApiManager();

	// GPU particles: we billboard them in a geometry shader and using indirect draws
	const u32	drCount = toEmit.m_DrawRequests.Count();
	for (u32 dri = 0; dri < drCount; ++dri)
	{
		PK_ASSERT(toEmit.m_DrawRequests[dri] != null);
		const Drawers::SBillboard_DrawRequest			*dr = static_cast<const Drawers::SBillboard_DrawRequest*>(toEmit.m_DrawRequests[dri]);
		const Drawers::SBillboard_BillboardingRequest	*bbRequest = static_cast<const Drawers::SBillboard_BillboardingRequest*>(&dr->BaseBillboardingRequest());
		const CParticleStreamToRender					*streamToRender = &dr->StreamToRender();

		SRHIDrawCall	*_outDrawCall = _CreateDrawCall(toEmit, output, SRHIDrawCall::DrawCall_InstancedIndirect, PKSample::Option_VertexPassThrough);
		if (!PK_VERIFY(_outDrawCall != null))
		{
			CLog::Log(PK_ERROR, "Failed to create a draw-call");
			return false;
		}

		SRHIDrawCall		&outDrawCall = *_outDrawCall;

		// Retrieve the particle streams in GPU storage:
		static const u32	kBillboardingInputs = 5; // Positions/Sizes/Rotations/Axis0s/Axis1s
		const u32			maxBufferCount = bbRequest->m_AdditionalInputs.Count() + kBillboardingInputs;
		if (!PK_VERIFY(outDrawCall.m_VertexBuffers.Reserve(maxBufferCount)))
			return false;

		// Mandatory streams
		u32					enabledOffset = 0;
		u32					posOffset = 0;
		u32					sizeOffset = 0;
		RHI::PGpuBuffer		enableds = _RetrieveStorageBuffer(manager, streamToRender, bbRequest->m_EnabledStreamId, enabledOffset);
		RHI::PGpuBuffer		positions = _RetrieveStorageBuffer(manager, streamToRender, bbRequest->m_PositionStreamId, posOffset);
		RHI::PGpuBuffer		sizes = _RetrieveStorageBuffer(manager, streamToRender, bbRequest->m_SizeStreamId, sizeOffset);

		if (!PK_VERIFY(positions != null) ||
			!PK_VERIFY(sizes != null) ||
			!PK_VERIFY(enableds != null))
			return false;

		// We can't rely on GPU storages being valid or not: now all possible renderer inputs are guaranteed to have valid stream id
		const u32			geomInputs = bbRequest->GetGeomGeneratedVertexInputsFlags();
		u32					rotOffset = 0;
		u32					axis0Offset = 0;
		u32					axis1Offset = 0;
		RHI::PGpuBuffer		rotations = null;
		RHI::PGpuBuffer		axis0s = null;
		RHI::PGpuBuffer		axis1s = null;

		if (geomInputs & Drawers::GenInput_ParticleRotation)
		{
			rotations = _RetrieveStorageBuffer(manager, streamToRender, bbRequest->m_RotationStreamId, rotOffset);
			PK_ASSERT(rotations != null);
		}
		if (geomInputs & Drawers::GenInput_ParticleAxis0)
		{
			axis0s = _RetrieveStorageBuffer(manager, streamToRender, bbRequest->m_Axis0StreamId, axis0Offset);
			PK_ASSERT(axis0s != null);
		}
		if (geomInputs & Drawers::GenInput_ParticleAxis1)
		{
			axis1s = _RetrieveStorageBuffer(manager, streamToRender, bbRequest->m_Axis1StreamId, axis1Offset);
			PK_ASSERT(axis1s != null);
		}

		RHI::PGpuBuffer		bufferIsSelected = null;
#if	(PK_HAS_PARTICLES_SELECTION != 0)
		// Editor only: for debugging purposes, we'll remove that from samples code later
		bufferIsSelected = ctx.Selection().HasGPUParticlesSelected() ? GetIsSelectedBuffer(ctx.Selection(), *dr) : null;
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
			RHI::PGpuBuffer		buffer = _RetrieveStorageBuffer(manager, streamToRender, bbRequest->m_AdditionalInputs[iInput].m_StreamId, offset);
			if (!PK_VERIFY(buffer != null) ||
				!PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(buffer).Valid()) ||
				!PK_VERIFY(outDrawCall.m_VertexOffsets.PushBack(offset).Valid()))
				return false;

			// Editor only: for debugging purposes, we'll remove that from samples code later
			if (outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Color] != null)
				continue;
			if (bbRequest->m_AdditionalInputs[iInput].m_Type == PopcornFX::EBaseTypeID::BaseType_Float4 &&
				(bbRequest->m_AdditionalInputs[iInput].m_Name == BasicRendererProperties::SID_Diffuse_Color() ||
				 bbRequest->m_AdditionalInputs[iInput].m_Name == BasicRendererProperties::SID_Distortion_Color()))
			{
				outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Color] = buffer;
				outDrawCall.m_DebugDrawGPUBufferOffsets[SRHIDrawCall::DebugDrawGPUBuffer_Color] = offset;
			}
		}

		PK_TODO("Fill the VB semantics here so that the debug draw modes work with GPU particles...");

		outDrawCall.m_IndirectBuffer = m_IndirectDraw.m_Buffer;
		outDrawCall.m_IndirectBufferOffset = m_DrawCallCurrentOffset;
		outDrawCall.m_EstimatedParticleCount = dr->RenderedParticleCount();

		// Now we need to create the indirect buffer to store the draw informations
		// We do not want to read-back the exact particle count from the GPU, so we are creating an
		// indirect buffer and send a copy command to copy the exact particle count to this buffer:
		RHI::PGpuBuffer		particleSimInfo = _RetrieveParticleInfoBuffer(manager, streamToRender);

		if (!PK_VERIFY(particleSimInfo != null))
			return false;

		if (!PK_VERIFY(output.m_CopyCommands.PushBack().Valid()))
			return false;

		SRHICopyCommand		&copyCommand = output.m_CopyCommands.Last();

		// We retrieve the particles info buffer:
		copyCommand.m_SrcBuffer = particleSimInfo;
		PK_TODO("Retrieve the live count offset from the D3D11_ParticleStream");
		copyCommand.m_SrcOffset = 0;
		copyCommand.m_DstBuffer = m_IndirectDraw.m_Buffer;
		copyCommand.m_DstOffset = m_DrawCallCurrentOffset + PK_MEMBER_OFFSET(RHI::SDrawIndirectArgs, m_VertexCount);
		copyCommand.m_SizeToCopy = sizeof(u32);

		m_DrawCallCurrentOffset += sizeof(RHI::SDrawIndirectArgs);

		outDrawCall.m_ShaderOptions |= Option_GPUStorage;
		outDrawCall.m_ShaderOptions |= _GetGeomBillboardShaderOptions(*bbRequest);

		if (!PK_VERIFY(outDrawCall.m_PushConstants.PushBack().Valid()))
			return false;
		PK_STATIC_ASSERT(sizeof(Drawers::SBillboardDrawRequest) == sizeof(CFloat4));
		Drawers::SBillboardDrawRequest	&desc = *reinterpret_cast<Drawers::SBillboardDrawRequest*>(&outDrawCall.m_PushConstants.Last());
		desc.Setup(*bbRequest);
	}
	return true;
}

//----------------------------------------------------------------------------

RHI::PGpuBuffer		CRHIBillboardingBatchPolicy::_RetrieveStorageBuffer(const RHI::PApiManager &manager, const CParticleStreamToRender *streams, CGuid streamIdx, u32 &storageOffset)
{
	PK_ASSERT(m_GPUStorage);
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

RHI::PGpuBuffer		CRHIBillboardingBatchPolicy::_RetrieveParticleInfoBuffer(const RHI::PApiManager &manager, const CParticleStreamToRender *streams)
{
	PK_ASSERT(m_GPUStorage);

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

bool	CRHIBillboardingBatchPolicy::_SetupGeomBillboardVertexBuffers(SRHIDrawCall &outDrawCall)
{
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
	return true;
}

//----------------------------------------------------------------------------

u32	CRHIBillboardingBatchPolicy::_GetGeomBillboardShaderOptions(const Drawers::SBillboard_BillboardingRequest &bbRequest)
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
//
//	Ribbons (CPU billboarding)
//
//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy::MapBuffers(SRenderContext &ctx, const TMemoryView<SSceneView> &views, SRibbonBatchJobs *billboardBatch, const SGeneratedInputs &toMap)
{
	(void)views;
	PK_NAMEDSCOPEDPROFILE("CRHIBillboardingBatchPolicy::MapBuffers (ribbons)");

	if (!PK_VERIFY(billboardBatch != null))
		return false;

	RHI::PApiManager	manager = ctx.ApiManager();

	// Assign mapped vertex/index buffers on CPU tasks that will run async on worker threads
	// All buffers used below have been allocated in AllocBuffers either during this BeginCollectingDrawCalls
	// They could also have been allocated in a previous BeginCollectingDrawCalls call, see documentation for more detail
	// CRibbon_Exec_FillSortIndices will generate indices and sort particles based on the renderer sort metric if necessary
	// CRibbon_Exec_Positions will generate particle geometry by connecting particles together
	// CRibbon_Exec_Texcoords will generate uvs for ribbons
	// CRibbon_Exec_UVRemap will generate necessary data for CorrectDeformation renderer feature to correct UVs in the vertex shader
	// CRibbon_Exec_CopyField will do a straight copy of particle sim data fields into the vertex buffers (once per vertex or once per particle if CRibbon_Exec_CopyField::m_PerVertex is set to false)

	// View independent inputs:
	if (toMap.m_GeneratedInputs & Drawers::GenInput_Indices)
	{
		PK_ASSERT(m_Indices.Used());
		void	*mappedValue = manager->MapCpuView(m_Indices.m_Buffer, 0 , m_TotalIndexCount * m_IndexSize);
		if (!PK_VERIFY(mappedValue != null))
			return false;
		billboardBatch->m_Exec_Indices.m_IndexStream.Setup(mappedValue, m_TotalIndexCount, m_IndexSize == sizeof(u32));
	}
	if (toMap.m_GeneratedInputs & Drawers::GenInput_Position)
	{
		PK_ASSERT(m_Positions.Used());
		void	*mappedValue = manager->MapCpuView(m_Positions.m_Buffer, 0, m_TotalVertexCount * sizeof(CFloat4));
		if (!PK_VERIFY(mappedValue != null))
			return false;
		billboardBatch->m_Exec_PNT.m_Positions = TStridedMemoryView<CFloat3, 0x10>(static_cast<CFloat3*>(mappedValue), m_TotalVertexCount, 0x10);
	}
	if (toMap.m_GeneratedInputs & Drawers::GenInput_Normal)
	{
		PK_ASSERT(m_Normals.Used());
		void	*mappedValue = manager->MapCpuView(m_Normals.m_Buffer, 0, m_TotalVertexCount * sizeof(CFloat4));
		if (!PK_VERIFY(mappedValue != null))
			return false;
		billboardBatch->m_Exec_PNT.m_Normals = TStridedMemoryView<CFloat3, 0x10>(static_cast<CFloat3*>(mappedValue), m_TotalVertexCount, 0x10);
	}
	if (toMap.m_GeneratedInputs & Drawers::GenInput_Tangent)
	{
		PK_ASSERT(m_Tangents.Used());
		void	*mappedValue = manager->MapCpuView(m_Tangents.m_Buffer, 0, m_TotalVertexCount * sizeof(CFloat4));
		if (!PK_VERIFY(mappedValue != null))
			return false;
		billboardBatch->m_Exec_PNT.m_Tangents = TStridedMemoryView<CFloat4, 0x10>(static_cast<CFloat4*>(mappedValue), m_TotalVertexCount, 0x10);
	}
	if (toMap.m_GeneratedInputs & Drawers::GenInput_UV0)
	{
		PK_ASSERT(m_TexCoords0.Used());
		void	*mappedValue = manager->MapCpuView(m_TexCoords0.m_Buffer, 0, m_TotalVertexCount * sizeof(CFloat2));
		if (!PK_VERIFY(mappedValue != null))
			return false;
		billboardBatch->m_Exec_Texcoords.m_Texcoords = TStridedMemoryView<CFloat2>(static_cast<CFloat2*>(mappedValue), m_TotalVertexCount);
	}
	if (toMap.m_GeneratedInputs & Drawers::GenInput_UV1)
	{
		PK_ASSERT(m_TexCoords1.Used());
		void	*mappedValue = manager->MapCpuView(m_TexCoords1.m_Buffer, 0, m_TotalVertexCount * sizeof(CFloat2));
		if (!PK_VERIFY(mappedValue != null))
			return false;
		billboardBatch->m_Exec_Texcoords.m_Texcoords2 = TStridedMemoryView<CFloat2>(static_cast<CFloat2*>(mappedValue), m_TotalVertexCount);
	}
	if (toMap.m_GeneratedInputs & Drawers::GenInput_AtlasId)
	{
		PK_ASSERT(m_AtlasIDs.Used());
		void	*mappedValue = manager->MapCpuView(m_AtlasIDs.m_Buffer, 0, m_TotalVertexCount * sizeof(float));
		if (!PK_VERIFY(mappedValue != null))
			return false;
		billboardBatch->m_Exec_Texcoords.m_AtlasIds = TMemoryView<float>(static_cast<float*>(mappedValue), m_TotalVertexCount);
	}
	if (toMap.m_GeneratedInputs & Drawers::GenInput_UVRemap)
	{
		PK_ASSERT(m_UVRemap.Used());
		void	*mappedValue = manager->MapCpuView(m_UVRemap.m_Buffer, 0, m_TotalVertexCount * sizeof(CFloat4));
		if (!PK_VERIFY(mappedValue != null))
			return false;
		billboardBatch->m_Exec_UVRemap.m_UVRemap = TStridedMemoryView<CFloat4>(static_cast<CFloat4*>(mappedValue), m_TotalVertexCount);

		billboardBatch->m_Exec_Texcoords.m_ForUVFactor = true;
	}
	if (toMap.m_GeneratedInputs & Drawers::GenInput_UVFactors)
	{
		PK_ASSERT(m_UVFactors.Used());
		void	*mappedValue = manager->MapCpuView(m_UVFactors.m_Buffer, 0, m_TotalVertexCount * sizeof(CFloat4));
		if (!PK_VERIFY(mappedValue != null))
			return false;
		billboardBatch->m_Exec_PNT.m_UVFactors4 = TStridedMemoryView<CFloat4>(static_cast<CFloat4*>(mappedValue), m_TotalVertexCount);
	}

	// View dependent inputs:
	PK_ASSERT(m_PerViewBuffers.Count() == billboardBatch->m_PerView.Count());
	for (u32 i = 0; i < m_PerViewBuffers.Count(); ++i)
	{
		const u32	viewGeneratedInputs = toMap.m_PerViewGeneratedInputs[i].m_GeneratedInputs;

		PK_ASSERT(m_PerViewBuffers[i].m_ViewIdx == billboardBatch->m_PerView[i].m_ViewIndex);
		if (viewGeneratedInputs & Drawers::GenInput_Indices)
		{
			PK_ASSERT(m_PerViewBuffers[i].m_Indices.Used());
			void	*mappedValue = manager->MapCpuView(m_PerViewBuffers[i].m_Indices.m_Buffer, 0, m_TotalIndexCount * m_IndexSize);
			if (!PK_VERIFY(mappedValue != null))
				return false;
			billboardBatch->m_PerView[i].m_Exec_Indices.m_IndexStream.Setup(mappedValue, m_TotalIndexCount, m_IndexSize == sizeof(u32));
		}
		if (viewGeneratedInputs & Drawers::GenInput_Position)
		{
			PK_ASSERT(m_PerViewBuffers[i].m_Positions.Used());
			void	*mappedValue = manager->MapCpuView(m_PerViewBuffers[i].m_Positions.m_Buffer, 0, m_TotalVertexCount * sizeof(CFloat4));
			if (!PK_VERIFY(mappedValue != null))
				return false;
			billboardBatch->m_PerView[i].m_Exec_PNT.m_Positions = TStridedMemoryView<CFloat3, 0x10>(static_cast<CFloat3*>(mappedValue), m_TotalVertexCount, 0x10);
		}
		if (viewGeneratedInputs & Drawers::GenInput_Tangent)
		{
			PK_ASSERT(m_PerViewBuffers[i].m_Tangents.Used());
			void	*mappedValue = manager->MapCpuView(m_PerViewBuffers[i].m_Tangents.m_Buffer, 0, m_TotalVertexCount * sizeof(CFloat4));
			if (!PK_VERIFY(mappedValue != null))
				return false;
			billboardBatch->m_PerView[i].m_Exec_PNT.m_Tangents = TStridedMemoryView<CFloat4, 0x10>(static_cast<CFloat4*>(mappedValue), m_TotalVertexCount, 0x10);
		}
		if (viewGeneratedInputs & Drawers::GenInput_Normal)
		{
			PK_ASSERT(m_PerViewBuffers[i].m_Normals.Used());
			void	*mappedValue = manager->MapCpuView(m_PerViewBuffers[i].m_Normals.m_Buffer, 0, m_TotalVertexCount * sizeof(CFloat4));
			if (!PK_VERIFY(mappedValue != null))
				return false;
			billboardBatch->m_PerView[i].m_Exec_PNT.m_Normals = TStridedMemoryView<CFloat3, 0x10>(static_cast<CFloat3*>(mappedValue), m_TotalVertexCount, 0x10);
		}
		if (viewGeneratedInputs & Drawers::GenInput_UVFactors)
		{
			PK_ASSERT(m_PerViewBuffers[i].m_UVFactors.Used());
			void	*mappedValue = manager->MapCpuView(m_PerViewBuffers[i].m_UVFactors.m_Buffer, 0, m_TotalVertexCount * sizeof(CFloat4));
			if (!PK_VERIFY(mappedValue != null))
				return false;
			billboardBatch->m_PerView[i].m_Exec_PNT.m_UVFactors4 = TStridedMemoryView<CFloat4>(static_cast<CFloat4*>(mappedValue), m_TotalVertexCount);
		}
	}

	// Additional inputs:
	PK_ASSERT(toMap.m_AdditionalGeneratedInputs.Count() >= m_AdditionalFields.Count());
	if (!m_AdditionalFields.Empty())
	{
		m_MappedAdditionalFields.Clear();
		if (!PK_VERIFY(m_MappedAdditionalFields.Reserve(m_AdditionalFields.Count())))
			return false;
		for (u32 i = 0; i < m_AdditionalFields.Count(); ++i)
		{
			if (m_AdditionalFields[i].m_Buffer.Used())
			{
				void	*mappedValue = manager->MapCpuView(m_AdditionalFields[i].m_Buffer.m_Buffer, 0, m_TotalVertexCount * m_AdditionalFields[i].m_ByteSize);
				if (!PK_VERIFY(mappedValue != null))
					return false;

				Drawers::SCopyFieldDesc	mappedAdditionnalField;
				mappedAdditionnalField.m_AdditionalInputIndex = m_AdditionalFields[i].m_AdditionalInputIndex;
				mappedAdditionnalField.m_Storage.m_Count = m_TotalVertexCount;
				mappedAdditionnalField.m_Storage.m_RawDataPtr = static_cast<u8*>(mappedValue);
				mappedAdditionnalField.m_Storage.m_Stride = m_AdditionalFields[i].m_ByteSize;
				m_MappedAdditionalFields.PushBackUnsafe(mappedAdditionnalField);
			}
		}
		billboardBatch->m_Exec_CopyField.m_FieldsToCopy = m_MappedAdditionalFields.View();
	}

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy::LaunchCustomTasks(SRenderContext &ctx, const TMemoryView<const Drawers::SRibbon_DrawRequest * const> &drawRequests, Drawers::CRibbon_CPU *billboardBatch)
{
	(void)ctx; (void)drawRequests; (void)billboardBatch;
#if	(PK_HAS_PARTICLES_SELECTION != 0)
	// Editor only: for debugging purposes, we'll remove that from samples code later
	if (ctx.Selection().HasParticlesSelected() || ctx.Selection().HasRendersSelected())
	{
		// If we have a CPU storage, we can render the particles that are selected as wire-frame
		m_RibbonCustomParticleSelectTask.Clear();
		PK_ASSERT(m_IsParticleSelected.Used());
		void	*mappedValue = ctx.ApiManager()->MapCpuView(m_IsParticleSelected.m_Buffer, 0, sizeof(float) * m_TotalVertexCount);
		if (!PK_VERIFY(mappedValue != null))
			return false;
		m_RibbonCustomParticleSelectTask.m_DstSelectedParticles = TStridedMemoryView<float>(static_cast<float*>(mappedValue), m_TotalVertexCount, sizeof(float));
		m_RibbonCustomParticleSelectTask.m_SrcParticleSelected = ctx.Selection();
		billboardBatch->AddExecBatch(&m_RibbonCustomParticleSelectTask);
	}
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)
	return true;
}

//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy::LaunchCustomTasks(SRenderContext &ctx, const TMemoryView<const Drawers::SRibbon_DrawRequest * const> &drawRequests, Drawers::CCopyStream_CPU *batch)
{
	(void)ctx; (void)drawRequests; (void)batch;

	PK_ASSERT_NOT_REACHED();

	return false;
}

//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy::_IssueDrawCall_Ribbon(SRenderContext &ctx, const SDrawCallDesc &toEmit, SRHIDrawOutputs &output)
{
	PK_NAMEDSCOPEDPROFILE("CRHIBillboardingBatchPolicy::EmitDrawCall Ribbon (CPU)");

	PK_ASSERT(!m_GeomPositions.Used()); // Cannot happen, we do not support geom shaders for ribbons yet

	SRHIDrawCall	*_outDrawCall = _CreateDrawCall(toEmit, output, SRHIDrawCall::DrawCall_Regular, PKSample::Option_VertexPassThrough);
	if (!PK_VERIFY(_outDrawCall != null))
	{
		CLog::Log(PK_ERROR, "Failed to create a draw-call");
		return false;
	}

	bool	hasAtlas = false;
	CRendererCacheInstance_UpdateThread		*refCacheInstance = static_cast<CRendererCacheInstance_UpdateThread*>(toEmit.m_RendererCaches.First().Get());
	if (PK_VERIFY(refCacheInstance != null))
	{
		PKSample::PCRendererCacheInstance		rCacheInstance = refCacheInstance->RenderThread_GetCacheInstance();
		if (PK_VERIFY(rCacheInstance != null))
			hasAtlas = rCacheInstance->m_HasAtlas;
	}

	SRHIDrawCall	&outDrawCall = *_outDrawCall;
	if (!_SetupCommonBillboardVertexBuffers(outDrawCall, hasAtlas))
		return false;
	if (!_SetupBillboardDrawCall(ctx, toEmit, outDrawCall))
		return false;
	return true;
}

//----------------------------------------------------------------------------
//
//	Meshes
//
//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy::MapBuffers(SRenderContext &ctx, const TMemoryView<SSceneView> &views, SMeshBatchJobs *meshBatch, const SGeneratedInputs &toMap)
{
	(void)views;
	PK_NAMEDSCOPEDPROFILE("CRHIBillboardingBatchPolicy::MapBuffers (meshes)");

	RHI::PApiManager	manager = ctx.ApiManager();

	// Assign mapped vertex buffers on CPU tasks that will run async on worker threads
	// All buffers used below have been allocated in AllocBuffers either during this BeginCollectingDrawCalls
	// They could also have been allocated in a previous BeginCollectingDrawCalls call, see documentation for more detail
	// CMesh_Exec_Matrices will generate per particle (instance) transforms
	// CMesh_Exec_CopyField will do a straight copy of particle sim data fields into the vertex buffers
	// All other buffers (mesh positions/normals/tangents/etc) should be retrieved from the actual mesh rendered

	if (meshBatch == null) // GPU Storage
	{
		// Indirect draw buffer
		m_MappedIndexedIndirectBuffer = static_cast<RHI::SDrawIndexedIndirectArgs*>(manager->MapCpuView(m_IndirectDraw.m_Buffer));
		if (!PK_VERIFY(m_MappedIndexedIndirectBuffer != null))
			return false;

		// Sim stream offsets
		m_MappedSimStreamOffsets_Positions = static_cast<u32*>(manager->MapCpuView(m_SimStreamOffsets_Positions.m_Buffer));
		if (!PK_VERIFY(m_MappedSimStreamOffsets_Positions != null))
			return false;
		m_MappedSimStreamOffsets_Orientations = static_cast<u32*>(manager->MapCpuView(m_SimStreamOffsets_Orientations.m_Buffer));
		if (!PK_VERIFY(m_MappedSimStreamOffsets_Orientations != null))
			return false;
		m_MappedSimStreamOffsets_Scales = static_cast<u32*>(manager->MapCpuView(m_SimStreamOffsets_Scales.m_Buffer));
		if (!PK_VERIFY(m_MappedSimStreamOffsets_Scales != null))
			return false;
		m_MappedSimStreamOffsets_Enableds = static_cast<u32*>(manager->MapCpuView(m_SimStreamOffsets_Enableds.m_Buffer));
		if (!PK_VERIFY(m_MappedSimStreamOffsets_Enableds != null))
			return false;
		if (m_HasMeshIDs)
		{
			m_MappedSimStreamOffsets_MeshIDs = static_cast<u32*>(manager->MapCpuView(m_SimStreamOffsets_MeshIDs.m_Buffer));
			if (!PK_VERIFY(m_MappedSimStreamOffsets_MeshIDs != null))
				return false;
		}
		if (m_HasMeshLODs)
		{
			m_MappedSimStreamOffsets_LODs = static_cast<u32*>(manager->MapCpuView(m_SimStreamOffsets_MeshLODs.m_Buffer));
			if (!PK_VERIFY(m_MappedSimStreamOffsets_LODs != null))
				return false;
		}
		for (u32 i = 0; i < m_SimStreamOffsets_AdditionalInputs.Count(); ++i)
		{
			m_MappedSimStreamOffsets_AdditionalInputs[i] = static_cast<u32*>(manager->MapCpuView(m_SimStreamOffsets_AdditionalInputs[i].m_Buffer));
			if (!PK_VERIFY(m_MappedSimStreamOffsets_AdditionalInputs[i] != null))
				return false;
		}
		// Matrices offsets
		m_MappedMatricesOffsets = static_cast<u32*>(manager->MapCpuView(m_MatricesOffsets.m_Buffer));
		if (!PK_VERIFY(m_MappedMatricesOffsets != null))
			return false;

		// Indirection offsets
		m_MappedIndirectionOffsets = static_cast<u32*>(manager->MapCpuView(m_IndirectionOffsets.m_Buffer));
		if (!PK_VERIFY(m_MappedIndirectionOffsets != null))
			return false;

		return true;
	}

	PK_ASSERT(!m_GPUStorage);

	if (toMap.m_GeneratedInputs & Drawers::GenInput_Matrices)
	{
		PK_ASSERT(m_Matrices.Used());
		CFloat4x4	*mappedMatrices = static_cast<CFloat4x4*>(manager->MapCpuView(m_Matrices.m_Buffer, 0, m_TotalParticleCount * sizeof(CFloat4x4)));
		if (!PK_VERIFY(mappedMatrices != null))
			return false;

		meshBatch->m_Exec_Matrices.m_Matrices = TMemoryView<CFloat4x4>(mappedMatrices, m_TotalParticleCount);
	}

	// Additional inputs:
	PK_ASSERT(toMap.m_AdditionalGeneratedInputs.Count() >= m_AdditionalFields.Count());
	if (!m_AdditionalFields.Empty())
	{
		m_MappedAdditionalFields.Clear();
		if (!PK_VERIFY(m_MappedAdditionalFields.Reserve(m_AdditionalFields.Count())))
			return false;
		for (u32 i = 0; i < m_AdditionalFields.Count(); ++i)
		{
			if (m_AdditionalFields[i].m_Buffer.Used())
			{
				void	*mappedValue = manager->MapCpuView(m_AdditionalFields[i].m_Buffer.m_Buffer, 0, m_TotalParticleCount * m_AdditionalFields[i].m_ByteSize);
				if (!PK_VERIFY(mappedValue != null))
					return false;

				Drawers::SCopyFieldDesc	mappedAdditionnalField;
				mappedAdditionnalField.m_AdditionalInputIndex = m_AdditionalFields[i].m_AdditionalInputIndex;
				mappedAdditionnalField.m_Storage.m_Count = m_TotalParticleCount;
				mappedAdditionnalField.m_Storage.m_RawDataPtr = static_cast<u8*>(mappedValue);
				mappedAdditionnalField.m_Storage.m_Stride = m_AdditionalFields[i].m_ByteSize;
				m_MappedAdditionalFields.PushBackUnsafe(mappedAdditionnalField);
			}
		}
		meshBatch->m_Exec_CopyField.m_FieldsToCopy = m_MappedAdditionalFields.View();
	}

	return true;
}


//----------------------------------------------------------------------------
//
//	Decals
//
//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy::MapBuffers(SRenderContext &ctx, const TMemoryView<SSceneView> &views, SDecalBatchJobs *decalBatch, const SGeneratedInputs &toMap)
{
	(void)views;
	PK_NAMEDSCOPEDPROFILE("CRHIBillboardingBatchPolicy::MapBuffers (decals)");

	RHI::PApiManager	manager = ctx.ApiManager();

	if (!PK_VERIFY(decalBatch != null))
		return false;

	if (toMap.m_GeneratedInputs & Drawers::GenInput_Matrices)
	{
		PK_ASSERT(m_Matrices.Used());
		CFloat4x4	*mappedMatrices = static_cast<CFloat4x4*>(manager->MapCpuView(m_Matrices.m_Buffer, 0, m_TotalParticleCount * sizeof(CFloat4x4)));
		if (!PK_VERIFY(mappedMatrices != null))
			return false;
		CFloat4x4	*mappedInvMatrices = static_cast<CFloat4x4*>(manager->MapCpuView(m_InvMatrices.m_Buffer, 0, m_TotalParticleCount * sizeof(CFloat4x4)));
		if (!PK_VERIFY(mappedInvMatrices != null))
			return false;

		decalBatch->m_Exec_Matrices.m_Matrices = TMemoryView<CFloat4x4>(mappedMatrices, m_TotalParticleCount);
		decalBatch->m_Exec_Matrices.m_InvMatrices = TMemoryView<CFloat4x4>(mappedInvMatrices, m_TotalParticleCount);
	}

	// Additional inputs:
	PK_ASSERT(toMap.m_AdditionalGeneratedInputs.Count() >= m_AdditionalFields.Count());
	if (!m_AdditionalFields.Empty())
	{
		m_MappedAdditionalFields.Clear();
		if (!PK_VERIFY(m_MappedAdditionalFields.Reserve(m_AdditionalFields.Count())))
			return false;
		for (u32 i = 0; i < m_AdditionalFields.Count(); ++i)
		{
			if (m_AdditionalFields[i].m_Buffer.Used())
			{
				void	*mappedValue = manager->MapCpuView(m_AdditionalFields[i].m_Buffer.m_Buffer, 0, m_TotalParticleCount * m_AdditionalFields[i].m_ByteSize);
				if (!PK_VERIFY(mappedValue != null))
					return false;

				Drawers::SCopyFieldDesc	mappedAdditionnalField;
				mappedAdditionnalField.m_AdditionalInputIndex = m_AdditionalFields[i].m_AdditionalInputIndex;
				mappedAdditionnalField.m_Storage.m_Count = m_TotalParticleCount;
				mappedAdditionnalField.m_Storage.m_RawDataPtr = static_cast<u8*>(mappedValue);
				mappedAdditionnalField.m_Storage.m_Stride = m_AdditionalFields[i].m_ByteSize;
				m_MappedAdditionalFields.PushBackUnsafe(mappedAdditionnalField);
			}
		}
		m_CopyAdditionalFieldsTask.m_FieldsToCopy = m_MappedAdditionalFields.View();
	}

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy::MapBuffers(SRenderContext &ctx, const TMemoryView<SSceneView> &views, STriangleBatchJobs *triangleBatch, const SGeneratedInputs &toMap)
{
	(void)views;
	PK_NAMEDSCOPEDPROFILE("CRHIBillboardingBatchPolicy::MapBuffers (triangles)");

	RHI::PApiManager	manager = ctx.ApiManager();

	if (!PK_VERIFY(triangleBatch != null))
		return false;

	if (toMap.m_GeneratedInputs & Drawers::GenInput_Indices)
	{
		PK_ASSERT(m_Indices.Used());
		void	*mappedValue = manager->MapCpuView(m_Indices.m_Buffer, 0, m_TotalIndexCount * m_IndexSize);
		if (!PK_VERIFY(mappedValue != null))
			return false;
		triangleBatch->m_Exec_Indices.m_IndexStream.Setup(mappedValue, m_TotalIndexCount, m_IndexSize == sizeof(u32));
	}
	if (toMap.m_GeneratedInputs & Drawers::GenInput_Position)
	{
		PK_ASSERT(m_Positions.Used());
		void	*mappedValue = manager->MapCpuView(m_Positions.m_Buffer, 0, m_TotalVertexCount * sizeof(CFloat4));
		if (!PK_VERIFY(mappedValue != null))
			return false;
		triangleBatch->m_Exec_PNT.m_Positions = TStridedMemoryView<CFloat3, 0x10>(static_cast<CFloat3*>(mappedValue), m_TotalVertexCount, 0x10);
	}
	if (toMap.m_GeneratedInputs & Drawers::GenInput_Normal)
	{
		PK_ASSERT(m_Normals.Used());
		void	*mappedValue = manager->MapCpuView(m_Normals.m_Buffer, 0, m_TotalVertexCount * sizeof(CFloat4));
		if (!PK_VERIFY(mappedValue != null))
			return false;
		triangleBatch->m_Exec_PNT.m_Normals = TStridedMemoryView<CFloat3, 0x10>(static_cast<CFloat3*>(mappedValue), m_TotalVertexCount, 0x10);
	}
	if (toMap.m_GeneratedInputs & Drawers::GenInput_Tangent)
	{
		PK_ASSERT(m_Tangents.Used());
		void	*mappedValue = manager->MapCpuView(m_Tangents.m_Buffer, 0, m_TotalVertexCount * sizeof(CFloat4));
		if (!PK_VERIFY(mappedValue != null))
			return false;
		triangleBatch->m_Exec_PNT.m_Tangents = TStridedMemoryView<CFloat4, 0x10>(static_cast<CFloat4*>(mappedValue), m_TotalVertexCount, 0x10);
	}
	if (toMap.m_GeneratedInputs & Drawers::GenInput_UV0)
	{
		PK_ASSERT(m_TexCoords0.Used());
		void	*mappedValue = manager->MapCpuView(m_TexCoords0.m_Buffer, 0, m_TotalVertexCount * sizeof(CFloat2));
		if (!PK_VERIFY(mappedValue != null))
			return false;
		triangleBatch->m_Exec_PNT.m_Texcoords = TStridedMemoryView<CFloat2>(static_cast<CFloat2*>(mappedValue), m_TotalVertexCount);
	}

	// View dependent inputs:
	PK_ASSERT(m_PerViewBuffers.Count() == triangleBatch->m_PerView.Count());
	for (u32 i = 0; i < m_PerViewBuffers.Count(); ++i)
	{
		const u32	viewGeneratedInputs = toMap.m_PerViewGeneratedInputs[i].m_GeneratedInputs;

		PK_ASSERT(m_PerViewBuffers[i].m_ViewIdx == triangleBatch->m_PerView[i].m_ViewIndex);
		if (viewGeneratedInputs & Drawers::GenInput_Indices)
		{
			PK_ASSERT(m_PerViewBuffers[i].m_Indices.Used());
			void	*mappedValue = manager->MapCpuView(m_PerViewBuffers[i].m_Indices.m_Buffer, 0, m_TotalIndexCount * m_IndexSize);
			if (!PK_VERIFY(mappedValue != null))
				return false;
			triangleBatch->m_PerView[i].m_Exec_Indices.m_IndexStream.Setup(mappedValue, m_TotalIndexCount, m_IndexSize == sizeof(u32));
		}
	}

	// Additional inputs:
	PK_ASSERT(toMap.m_AdditionalGeneratedInputs.Count() >= m_AdditionalFields.Count());
	if (!m_AdditionalFields.Empty())
	{
		m_MappedAdditionalFields.Clear();
		if (!PK_VERIFY(m_MappedAdditionalFields.Reserve(m_AdditionalFields.Count())))
			return false;
		for (u32 i = 0; i < m_AdditionalFields.Count(); ++i)
		{
			if (m_AdditionalFields[i].m_Buffer.Used())
			{
				void	*mappedValue = manager->MapCpuView(m_AdditionalFields[i].m_Buffer.m_Buffer, 0, m_TotalVertexCount * m_AdditionalFields[i].m_ByteSize);
				if (!PK_VERIFY(mappedValue != null))
					return false;

				Drawers::SCopyFieldDesc	mappedAdditionnalField;
				mappedAdditionnalField.m_AdditionalInputIndex = m_AdditionalFields[i].m_AdditionalInputIndex;
				mappedAdditionnalField.m_Storage.m_Count = m_TotalVertexCount;
				mappedAdditionnalField.m_Storage.m_RawDataPtr = static_cast<u8*>(mappedValue);
				mappedAdditionnalField.m_Storage.m_Stride = m_AdditionalFields[i].m_ByteSize;
				m_MappedAdditionalFields.PushBackUnsafe(mappedAdditionnalField);
			}
		}
		triangleBatch->m_Exec_CopyField.m_FieldsToCopy = m_MappedAdditionalFields.View();
	}

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy::MapBuffers(SRenderContext &ctx, const TMemoryView<SSceneView> &views, SGPUTriangleBatchJobs *GPUTriangleBatch, const SGeneratedInputs &toMap)
{
	(void)ctx; (void)views; (void)GPUTriangleBatch; (void)toMap;
	PK_NAMEDSCOPEDPROFILE("CRHIBillboardingBatchPolicy::MapBuffers (GPU Triangle)");

	PK_ASSERT_NOT_REACHED();

	return false;
}

//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy::MapBuffers(SRenderContext &ctx, const TMemoryView<SSceneView> &views, SGPURibbonBatchJobs *GPURibbonBatch, const SGeneratedInputs &toMap)
{
	(void)ctx; (void)views; (void)GPURibbonBatch; (void)toMap;
	PK_NAMEDSCOPEDPROFILE("CRHIBillboardingBatchPolicy::MapBuffers (GPU ribbon)");

	PK_ASSERT_NOT_REACHED();

	return false;
}

//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy::LaunchCustomTasks(SRenderContext &ctx, const TMemoryView<const Drawers::SMesh_DrawRequest * const> &drawRequests, Drawers::CMesh_CPU *meshBatch)
{
	(void)ctx; (void)drawRequests; (void)meshBatch;

	if (meshBatch == null) // GPU strorage
	{
		PK_ASSERT(m_GPUStorage);
		#if (PK_PARTICLES_UPDATER_USE_GPU != 0)
		// Init. draw indirect buffer (instance counts we will be set by compute shader)
		if (m_MappedIndexedIndirectBuffer != null)
		{
			for (u32 dri = 0; dri < m_DrawRequestCount; dri++)
			{
				for (u32 meshi = 0; meshi < m_MeshCount; meshi++)
				{
					m_MappedIndexedIndirectBuffer[m_MeshCount * dri + meshi].m_IndexCount = m_PerMeshIndexCount[meshi];
					m_MappedIndexedIndirectBuffer[m_MeshCount * dri + meshi].m_InstanceCount = 0;
					m_MappedIndexedIndirectBuffer[m_MeshCount * dri + meshi].m_IndexOffset = 0;
					m_MappedIndexedIndirectBuffer[m_MeshCount * dri + meshi].m_VertexOffset = 0;
					m_MappedIndexedIndirectBuffer[m_MeshCount * dri + meshi].m_InstanceOffset = 0;
				}
			}
		}

		// Setup stream offsets
		//	Pos/Scale/Orientation/Enabled [ + MeshIds ] [ + MeshLODs ] [ + Additional inputs ]
		const u32	streamCount = 4 + (m_HasMeshIDs ? 1 : 0) + (m_HasMeshLODs ? 1 : 0) + m_SimStreamOffsets_AdditionalInputs.Count();
		PK_STACKALIGNEDMEMORYVIEW(u32, streamsOffsets, m_DrawRequestCount * streamCount, 0x10);
		for (u32 dri = 0; dri < m_DrawRequestCount; dri++)
		{
			// Fill in stream offsets for each draw request
			const Drawers::SMesh_DrawRequest			*dr = static_cast<const Drawers::SMesh_DrawRequest*>(drawRequests[dri]);
			const Drawers::SMesh_BillboardingRequest	*bbRequest = static_cast<const Drawers::SMesh_BillboardingRequest*>(&dr->BaseBillboardingRequest());
			const CParticleStreamToRender_GPU			*streamToRender = dr->StreamToRender_GPU();

			u32		offset = 0;
			streamsOffsets[offset++ * m_DrawRequestCount + dri] = streamToRender->StreamOffset(bbRequest->m_PositionStreamId);
			streamsOffsets[offset++ * m_DrawRequestCount + dri] = streamToRender->StreamOffset(bbRequest->m_ScaleStreamId);
			streamsOffsets[offset++ * m_DrawRequestCount + dri] = streamToRender->StreamOffset(bbRequest->m_OrientationStreamId);
			streamsOffsets[offset++ * m_DrawRequestCount + dri] = streamToRender->StreamOffset(bbRequest->m_EnabledStreamId);
			if (m_HasMeshIDs)
				streamsOffsets[offset++ * m_DrawRequestCount + dri] = streamToRender->StreamOffset(bbRequest->m_MeshIDStreamId);
			if (m_HasMeshLODs)
				streamsOffsets[offset++ * m_DrawRequestCount + dri] = streamToRender->StreamOffset(bbRequest->m_MeshLODStreamId);

			// Add all non-virtual stream additional inputs
			const u32	additionalInputCount = m_SimStreamOffsets_AdditionalInputs.Count();
			for (u32 iInput = 0; iInput < additionalInputCount; ++iInput)
			{
				const u32	additionalInputId = m_AdditionalFields[iInput].m_AdditionalInputIndex;
				streamsOffsets[offset++ * m_DrawRequestCount + dri] = streamToRender->StreamOffset(bbRequest->m_AdditionalInputs[additionalInputId].m_StreamId);
			}
		}
		// Non temporal writes to gpu mem, aligned and contiguous
		u32	streamOffset = 0;
		Mem::Copy_Uncached(m_MappedSimStreamOffsets_Positions, &streamsOffsets[streamOffset++ * m_DrawRequestCount], sizeof(u32) * m_DrawRequestCount);
		Mem::Copy_Uncached(m_MappedSimStreamOffsets_Scales, &streamsOffsets[streamOffset++ * m_DrawRequestCount], sizeof(u32) * m_DrawRequestCount);
		Mem::Copy_Uncached(m_MappedSimStreamOffsets_Orientations, &streamsOffsets[streamOffset++ * m_DrawRequestCount], sizeof(u32) * m_DrawRequestCount);
		Mem::Copy_Uncached(m_MappedSimStreamOffsets_Enableds, &streamsOffsets[streamOffset++ * m_DrawRequestCount], sizeof(u32) * m_DrawRequestCount);
		if (m_HasMeshIDs)
			Mem::Copy_Uncached(m_MappedSimStreamOffsets_MeshIDs, &streamsOffsets[streamOffset++ * m_DrawRequestCount], sizeof(u32) * m_DrawRequestCount);
		if (m_HasMeshLODs)
			Mem::Copy_Uncached(m_MappedSimStreamOffsets_LODs, &streamsOffsets[streamOffset++ * m_DrawRequestCount], sizeof(u32) * m_DrawRequestCount);
		for (u32 iInput = 0; iInput < m_MappedSimStreamOffsets_AdditionalInputs.Count(); ++iInput)
			Mem::Copy_Uncached(m_MappedSimStreamOffsets_AdditionalInputs[iInput], &streamsOffsets[streamOffset++ * m_DrawRequestCount], sizeof(u32) * m_DrawRequestCount);

		// Init indirection offsets
		const u32	indirectionOffsetsSizeInBytes = 2 * m_DrawRequestCount * sizeof(u32) * (m_HasMeshIDs ? m_MeshCount : (m_HasMeshLODs ? drawRequests[0]->m_BB.LODCount() : 1));
		Mem::Clear_Uncached(m_MappedIndirectionOffsets, indirectionOffsetsSizeInBytes);

		// Setup matrices offset
		RHI::PApiManager	manager = ctx.ApiManager();
		u32					offset = 0u;
		for (u32 i = 0; i < m_DrawRequestCount; i++)
		{
			m_MappedMatricesOffsets[i] = offset;

			const Drawers::SMesh_DrawRequest				*dr = static_cast<const Drawers::SMesh_DrawRequest*>(drawRequests[i]);
			offset += dr->InputParticleCount() * sizeof(CFloat4x4);
		}
		#endif // (PK_PARTICLES_UPDATER_USE_GPU != 0)

		return true;
	}

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	// Editor only: for debugging purposes, we'll remove that from samples code later
	if (ctx.Selection().HasParticlesSelected() || ctx.Selection().HasRendersSelected())
	{
		// If we have a CPU storage, we can render the particles that are selected as wire-frame
		m_MeshCustomParticleSelectTask.Clear();
		PK_ASSERT(m_IsParticleSelected.Used());
		void	*mappedValue = ctx.ApiManager()->MapCpuView(m_IsParticleSelected.m_Buffer, 0, sizeof(float) * m_TotalParticleCount);
		if (!PK_VERIFY(mappedValue != null))
			return false;
		m_MeshCustomParticleSelectTask.m_DstSelectedParticles = TStridedMemoryView<float>(static_cast<float*>(mappedValue), m_TotalVertexCount, sizeof(float));
		m_MeshCustomParticleSelectTask.m_SrcParticleSelected = ctx.Selection();
		meshBatch->AddExecPage(&m_MeshCustomParticleSelectTask);
	}
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)

	return true;
}


//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy::LaunchCustomTasks(SRenderContext &ctx, const TMemoryView<const Drawers::SDecal_DrawRequest * const> &drawRequests, Drawers::CDecal_CPU *decalBatch)
{
	(void)ctx; (void)decalBatch;

	m_CopyTasks.Prepare(TMemoryView<const Drawers::SBase_DrawRequest * const>::Reinterpret(drawRequests));
	m_CopyTasks.AddExecAsyncPage(&m_CopyAdditionalFieldsTask);
#if	(PK_HAS_PARTICLES_SELECTION != 0)
	// Editor only: for debugging purposes, we'll remove that from samples code later
	if (ctx.Selection().HasParticlesSelected() || ctx.Selection().HasRendersSelected())
	{
		// If we have a CPU storage, we can render the particles that are selected as wire-frame
		m_CopyStreamCustomParticleSelectTask.Clear();
		PK_ASSERT(m_IsParticleSelected.Used());
		void	*mappedValue = ctx.ApiManager()->MapCpuView(m_IsParticleSelected.m_Buffer, 0, sizeof(float) * m_TotalParticleCount);
		if (!PK_VERIFY(mappedValue != null))
			return false;
		m_CopyStreamCustomParticleSelectTask.m_DstSelectedParticles = TStridedMemoryView<float>(static_cast<float*>(mappedValue), m_TotalVertexCount, sizeof(float));
		m_CopyStreamCustomParticleSelectTask.m_SrcParticleSelected = ctx.Selection();
		m_CopyTasks.AddExecAsyncPage(&m_CopyStreamCustomParticleSelectTask);
	}
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)
	m_CopyTasks.LaunchTasks(null);
	return true;
}

//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy::_IssueDrawCall_Mesh_CPU(SRenderContext &ctx, const SDrawCallDesc &toEmit, SRHIDrawOutputs &output)
{
	(void)ctx;
	PK_NAMEDSCOPEDPROFILE("CRHIBillboardingBatchPolicy::EmitDrawCall Mesh");

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
				const u32	vertexBufferMaxCount = Utils::__MaxMeshSemantics + 1 + m_AdditionalFields.Count();
				PK_ASSERT(vertexBuffDescView.Count() <= vertexBufferMaxCount);
			});
		}
	}

	const u32	subMeshCount = rCacheInstance->m_AdditionalGeometry->m_PerGeometryViews.Count();
	if (subMeshCount == 0)
		return false;

	PK_ASSERT(!m_HasMeshIDs || m_PerMeshParticleCount.Count() == subMeshCount);

	for (u32 iSubMesh = 0; iSubMesh < subMeshCount; ++iSubMesh)
	{
		if (m_PerMeshParticleCount[iSubMesh] == 0)
			continue;

		const u32	particleOffset = m_PerMeshBufferOffset[iSubMesh];

		SRHIDrawCall	*_outDrawCall = _CreateDrawCall(toEmit, output, SRHIDrawCall::DrawCall_IndexedInstanced, shaderOptions);
		if (!PK_VERIFY(_outDrawCall != null))
		{
			CLog::Log(PK_ERROR, "Failed to create a draw-call");
			return false;
		}

		SRHIDrawCall		&outDrawCall = *_outDrawCall;

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

		if (m_PerMeshParticleCount.Empty())
			outDrawCall.m_InstanceCount = toEmit.m_TotalParticleCount;
		else
			outDrawCall.m_InstanceCount = m_PerMeshParticleCount[iSubMesh];

		// Matrices:
		success &= outDrawCall.m_VertexOffsets.PushBack(particleOffset * u32(sizeof(CFloat4x4))).Valid();
		success &= outDrawCall.m_VertexBuffers.PushBack(m_Matrices.m_Buffer).Valid();

		// Additional inputs:
		for (u32 i = 0; i < m_AdditionalFields.Count(); ++i)
		{
			if (m_AdditionalFields[i].m_Buffer.Used())
			{
				const u32	bufferOffset = particleOffset * m_AdditionalFields[i].m_ByteSize;

				success &= outDrawCall.m_VertexOffsets.PushBack(bufferOffset).Valid();
				success &= outDrawCall.m_VertexBuffers.PushBack(m_AdditionalFields[i].m_Buffer.m_Buffer).Valid();

				// Editor only: for debugging purposes, we'll remove that from samples code later
				if (m_AdditionalFields[i].m_Semantic == SRHIDrawCall::DebugDrawGPUBuffer_Color)
				{
					PK_ASSERT(m_AdditionalFields[i].m_ByteSize == sizeof(CFloat4));
					PK_ASSERT(outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Color] == null);
					outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Color] = m_AdditionalFields[i].m_Buffer.m_Buffer;
					outDrawCall.m_DebugDrawGPUBufferOffsets[SRHIDrawCall::DebugDrawGPUBuffer_Color] = bufferOffset;
				}
			}
		}

		PK_ASSERT(vertexBuffDescView.Count() == outDrawCall.m_VertexBuffers.Count() ||
				  (vertexBuffDescView.Empty() && !outDrawCall.m_Valid));

		// Fill the semantics for the debug draws:
		outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Position] = bufferView.m_VertexBuffers[Utils::MeshPositions];
		outDrawCall.m_DebugDrawGPUBufferOffsets[SRHIDrawCall::DebugDrawGPUBuffer_Position] = 0;

		outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_InstanceTransforms] = m_Matrices.m_Buffer;
		outDrawCall.m_DebugDrawGPUBufferOffsets[SRHIDrawCall::DebugDrawGPUBuffer_InstanceTransforms] = particleOffset * u32(sizeof(CFloat4x4));

#if	(PK_HAS_PARTICLES_SELECTION != 0)
		// Editor only: for debugging purposes, we'll remove that from samples code later
		if ((ctx.Selection().HasParticlesSelected() || ctx.Selection().HasRendersSelected()) && m_IsParticleSelected.Used())
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

#if (PK_PARTICLES_UPDATER_USE_GPU != 0)
u32	CRHIBillboardingBatchPolicy::_SubmeshIDToLOD(CRendererCacheInstance_UpdateThread &refCacheInstance, u32 lodCount, u32 iSubMesh)
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

bool	CRHIBillboardingBatchPolicy::_IssueDrawCall_Mesh_GPU(SRenderContext &ctx, const SDrawCallDesc &toEmit, SRHIDrawOutputs &output)
{
	PK_ASSERT(m_GPUStorage);

	PK_NAMEDSCOPEDPROFILE("CRHIBillboardingBatchPolicy::EmitDrawCall Mesh (GPU)");

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

	RHI::PApiManager				manager = ctx.ApiManager();

	// Get simData and offsets constant sets layouts
	const RHI::SConstantSetLayout	*simDataConstantSetLayout = null;
	const RHI::SConstantSetLayout	*offsetsConstantSetLayout = null;
	if (!PK_VERIFY(rCacheInstance->m_Cache->GetGPUStorageConstantSets(static_cast<PKSample::EShaderOptions>(shaderOptions), simDataConstantSetLayout, offsetsConstantSetLayout)) ||
		!PK_VERIFY(simDataConstantSetLayout != null) ||
		!PK_VERIFY(offsetsConstantSetLayout != null) ||
		simDataConstantSetLayout->m_Constants.Empty() ||
		offsetsConstantSetLayout->m_Constants.Empty())
		return false;

	const u32	lodCount = refCacheInstance->m_PerLODMeshCount.Count();
	// Iterate on draw requests
	for (u32 dri = 0; dri < m_DrawRequestCount; ++dri)
	{
		const Drawers::SMesh_DrawRequest				*dr = static_cast<const Drawers::SMesh_DrawRequest*>(toEmit.m_DrawRequests[dri]);
		const Drawers::SMesh_BillboardingRequest		*bbRequest = static_cast<const Drawers::SMesh_BillboardingRequest*>(&dr->BaseBillboardingRequest());
		const CParticleStreamToRender_GPU				*streamToRender = dr->StreamToRender_GPU();

		// Get particle sim info GPU buffer
		RHI::PGpuBuffer									particleSimInfo = _RetrieveParticleInfoBuffer(manager, streamToRender);
		if (!PK_VERIFY(particleSimInfo != null))
			return false;

		// Get particle stream GPU buffer
		u32					offset = 0;
		RHI::PGpuBuffer		streamBufferGPU = _RetrieveStorageBuffer(manager, streamToRender, bbRequest->m_PositionStreamId, offset);
		PK_ASSERT(streamBufferGPU != null);

		// Get particle count estimated for dispatch
		const u32										drParticleCountEst = dr->InputParticleCount();

		// Update simData constant set
		m_SimDataConstantSet = manager->CreateConstantSet(RHI::SRHIResourceInfos("Sim Data Constant Set"), *simDataConstantSetLayout);
		if (!PK_VERIFY(m_SimDataConstantSet != null))
			return false;
		PK_ASSERT(m_SimDataConstantSet->GetConstantValues().Count() == 3); // SimBuffer, transforms (from computes), indirection (from computes)
		if (!PK_VERIFY(m_SimDataConstantSet->SetConstants(streamBufferGPU, 0)) ||
			!PK_VERIFY(m_SimDataConstantSet->SetConstants(m_Matrices.m_Buffer, 1)) ||
			!PK_VERIFY(m_SimDataConstantSet->SetConstants(m_Indirection.m_Buffer, 2)))
			return false;
		m_SimDataConstantSet->UpdateConstantValues();

		// Update offsets constant set
		m_OffsetsConstantSet = manager->CreateConstantSet(RHI::SRHIResourceInfos("Offsets Constant Set"), *offsetsConstantSetLayout);
		if (!PK_VERIFY(m_OffsetsConstantSet != null))
			return false;
		PK_ASSERT(!m_SimStreamOffsets_AdditionalInputs.Empty());
		PK_ASSERT(m_OffsetsConstantSet->GetConstantValues().Count() == m_SimStreamOffsets_AdditionalInputs.Count() + 2); // transforms offsets, indirection offsets, additional inputs
		if (!PK_VERIFY(m_OffsetsConstantSet->SetConstants(m_MatricesOffsets.m_Buffer, 0)) ||
			!PK_VERIFY(m_OffsetsConstantSet->SetConstants(m_IndirectionOffsets.m_Buffer, 1)))
			return false;
		for (u32 i = 2; i < m_OffsetsConstantSet->GetConstantValues().Count(); ++i)
		{
			if (!PK_VERIFY(m_OffsetsConstantSet->SetConstants(m_SimStreamOffsets_AdditionalInputs[i - 2].m_Buffer, i)))
				return false;
		}
		m_OffsetsConstantSet->UpdateConstantValues();

#if	(PK_HAS_PARTICLES_SELECTION != 0)
		// Editor only: for debugging purposes, we'll remove that from samples code later
		RHI::PGpuBuffer		bufferIsSelected = ctx.Selection().HasGPUParticlesSelected() ? GetIsSelectedBuffer(ctx.Selection(), *dr) : null;
		if (bufferIsSelected != null)
		{
			RHI::SConstantSetLayout	GPUSelectionConstantSetLayout(RHI::VertexShaderMask);
			GPUSelectionConstantSetLayout.AddConstantsLayout(RHI::SRawBufferDesc("IsSelected"));
			m_GPUSelectionConstantSet = manager->CreateConstantSet(RHI::SRHIResourceInfos("IsSelected Constant Set"), GPUSelectionConstantSetLayout);
			m_GPUSelectionConstantSet->SetConstants(bufferIsSelected, 0);
			m_GPUSelectionConstantSet->UpdateConstantValues();
		}
		else
		{
			m_GPUSelectionConstantSet = null;
		}
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)

		// For each mesh, setup draw call description
		for (u32 iSubMesh = 0; iSubMesh < subMeshCount; ++iSubMesh)
		{
			SRHIDrawCall	*_outDrawCall = _CreateDrawCall(toEmit, output, SRHIDrawCall::DrawCall_IndexedInstancedIndirect, shaderOptions);
			if (!PK_VERIFY(_outDrawCall != null))
			{
				CLog::Log(PK_ERROR, "Failed to create a draw-call");
				return false;
			}

			SRHIDrawCall	&outDrawCall = *_outDrawCall;

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
			outDrawCall.m_SelectionConstantSet = m_GPUSelectionConstantSet;
#endif
			const u32	currentLOD = _SubmeshIDToLOD(*refCacheInstance, lodCount, iSubMesh);

			// Draw call push constants
			if (!PK_VERIFY(outDrawCall.m_PushConstants.PushBack().Valid()))
				return false;
			u32	*drPushConstant = reinterpret_cast<u32*>(&outDrawCall.m_PushConstants.Last());
			drPushConstant[0] = dri; // Draw request ID (used to get streams offsets from stream offsets buffers)
			drPushConstant[1] = m_HasMeshIDs ?
									m_MeshCount * m_DrawRequestCount + m_MeshCount * dri + iSubMesh :
									(m_HasMeshLODs ? lodCount * m_DrawRequestCount + lodCount * dri + currentLOD : m_DrawRequestCount + dri); // Used to get the indirection offset from indirection offsets buffer

			// Set index buffer
			outDrawCall.m_IndexBuffer = bufferView.m_IndexBuffer;
			outDrawCall.m_IndexSize = bufferView.m_IndexBufferSize;

			// Set indirect buffer (will be updated by computeParticleCountPerMesh compute shader)
			outDrawCall.m_IndirectBuffer = m_IndirectDraw.m_Buffer;
			outDrawCall.m_IndirectBufferOffset = m_DrawCallCurrentOffset;

			// Fill the semantics for the debug draws:
			outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Position] = bufferView.m_VertexBuffers[Utils::MeshPositions];
			outDrawCall.m_DebugDrawGPUBufferOffsets[SRHIDrawCall::DebugDrawGPUBuffer_Position] = 0;
			outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_TransformsOffsets] = m_MatricesOffsets.m_Buffer;
			outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_IndirectionOffsets] = m_IndirectionOffsets.m_Buffer;
			outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_ColorsOffsets] = m_HasColorStream ? m_SimStreamOffsets_AdditionalInputs[0].m_Buffer : null;

			m_DrawCallCurrentOffset += sizeof(RHI::SDrawIndexedIndirectArgs);
		} // end of draw call description setup

		// Compute : Count particles per mesh
		{
			PKSample::SRHIComputeDispatchs	computeDispatch = PKSample::SRHIComputeDispatchs();

			// Constant set
			RHI::SConstantSetLayout	layout;
			PKSample::CreateComputeParticleCountPerMeshConstantSetLayout(layout, m_HasMeshIDs, m_HasMeshLODs);
			RHI::PConstantSet	computeConstantSet = manager->CreateConstantSet(RHI::SRHIResourceInfos("Compute Mesh PCount Constant Set"), layout);
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
			RHI::PConstantSet	computeConstantSet = manager->CreateConstantSet(RHI::SRHIResourceInfos("Compute Indirection Constant Set"), layout);
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
			RHI::PConstantSet	computeConstantSet = manager->CreateConstantSet(RHI::SRHIResourceInfos("Build Matrices Constant Set"), layout);
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

	for (PKSample::SRHIComputeDispatchs dispatch : computeParticleCountPerMesh)
		output.m_ComputeDispatchs.PushBack(dispatch);

	// Compute : init indirection offsets buffer (one dispatch that handles every DR and DC)
	{
		PKSample::SRHIComputeDispatchs	computeDispatch = PKSample::SRHIComputeDispatchs();
		const bool						hasLODNoAtlas = m_HasMeshLODs && !m_HasMeshIDs; // If true, indirection offsets buffer is indexed differently.

		// Constant set
		RHI::SConstantSetLayout	layout;
		PKSample::CreateInitIndirectionOffsetsBufferConstantSetLayout(layout, hasLODNoAtlas);
		RHI::PConstantSet	computeConstantSet = manager->CreateConstantSet(RHI::SRHIResourceInfos("Compute Indirection Offsets Constant Set"), layout);
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
		drPushConstant[0] = m_DrawRequestCount * (m_HasMeshIDs ? m_MeshCount : (m_HasMeshLODs ? lodCount : 1)); // DrawCall (!= drawRequest) count

		// State
		PKSample::EComputeShaderType	type =	hasLODNoAtlas ?
													PKSample::EComputeShaderType::ComputeType_InitIndirectionOffsetsBuffer_LODNoAtlas :
													PKSample::EComputeShaderType::ComputeType_InitIndirectionOffsetsBuffer;
		computeDispatch.m_State = rCacheInstance->m_Cache->GetComputeState(type);

		// Dispatch args
		const u32	indirectionOffsetsElementCount = m_DrawRequestCount * (m_HasMeshIDs ? m_MeshCount : (m_HasMeshLODs ? lodCount : 1));
		computeDispatch.m_ThreadGroups = CInt3(	(Mem::Align(indirectionOffsetsElementCount, PK_RH_GPU_THREADGROUP_SIZE) / PK_RH_GPU_THREADGROUP_SIZE),
												(Mem::Align(indirectionOffsetsElementCount, PK_RH_GPU_THREADGROUP_SIZE) / PK_RH_GPU_THREADGROUP_SIZE),
												1);

		output.m_ComputeDispatchs.PushBack(computeDispatch);
	}

	for (PKSample::SRHIComputeDispatchs dispatch : computeIndirection)
		output.m_ComputeDispatchs.PushBack(dispatch);

	for (PKSample::SRHIComputeDispatchs dispatch : computeMeshMatrices)
		output.m_ComputeDispatchs.PushBack(dispatch);

	return true;
}
#endif // (PK_PARTICLES_UPDATER_USE_GPU != 0)

//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy::_IssueDrawCall_Decal(SRenderContext &ctx, const SDrawCallDesc &toEmit, SRHIDrawOutputs &output)
{
	(void)ctx;

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


	SRHIDrawCall	*_outDrawCall = _CreateDrawCall(toEmit, output, SRHIDrawCall::DrawCall_IndexedInstanced, PKSample::Option_VertexPassThrough);
	if (!PK_VERIFY(_outDrawCall != null))
	{
		CLog::Log(PK_ERROR, "Failed to create a draw-call");
		return false;
	}
	SRHIDrawCall					&outDrawCall = *_outDrawCall;
	const Utils::GpuBufferViews		&bufferView = rCacheInstance->m_AdditionalGeometry->m_PerGeometryViews.First();

	if (rCacheInstance->m_Cache == null)
		return false;

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
	for (u32 i = 0; i < m_AdditionalFields.Count(); ++i)
	{
		if (m_AdditionalFields[i].m_Buffer.Used())
		{
			success &= outDrawCall.m_VertexBuffers.PushBack(m_AdditionalFields[i].m_Buffer.m_Buffer).Valid();

			// Editor only: for debugging purposes, we'll remove that from samples code later
			if (m_AdditionalFields[i].m_Semantic == SRHIDrawCall::DebugDrawGPUBuffer_Color)
			{
				PK_ASSERT(m_AdditionalFields[i].m_ByteSize == sizeof(CFloat4));
				PK_ASSERT(outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Color] == null);
				outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Color] = m_AdditionalFields[i].m_Buffer.m_Buffer;
				outDrawCall.m_DebugDrawGPUBufferOffsets[SRHIDrawCall::DebugDrawGPUBuffer_Color] = 0;
			}
		}
	}

	// Fill the semantics for the debug draws:
	outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Position] = outDrawCall.m_VertexBuffers.First();
	outDrawCall.m_DebugDrawGPUBufferOffsets[SRHIDrawCall::DebugDrawGPUBuffer_Position] = 0;

	outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_InstanceTransforms] = m_Matrices.m_Buffer;
	outDrawCall.m_DebugDrawGPUBufferOffsets[SRHIDrawCall::DebugDrawGPUBuffer_InstanceTransforms] = 0;

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

	outDrawCall.m_InstanceCount = m_TotalParticleCount;

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	// Editor only: for debugging purposes, we'll remove that from samples code later
	if ((ctx.Selection().HasParticlesSelected() || ctx.Selection().HasRendersSelected()) && m_IsParticleSelected.Used())
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

bool	CRHIBillboardingBatchPolicy::_IssueDrawCall_Triangle(SRenderContext &ctx, const SDrawCallDesc &toEmit, SRHIDrawOutputs &output)
{
	(void)ctx;

	// No need to iterate on all draw requests, just take the first as reference as they wouldn't have been batched if not compatible
	CRendererCacheInstance_UpdateThread	*refCacheInstance = static_cast<CRendererCacheInstance_UpdateThread*>(toEmit.m_RendererCaches.First().Get());
	if (!PK_VERIFY(refCacheInstance != null))
		return false;

	PKSample::PCRendererCacheInstance	rCacheInstance = refCacheInstance->RenderThread_GetCacheInstance();
	if (!PK_VERIFY(rCacheInstance != null))
		return false;

	SRHIDrawCall	*_outDrawCall = _CreateDrawCall(toEmit, output, SRHIDrawCall::DrawCall_Regular, PKSample::Option_VertexPassThrough);
	if (!PK_VERIFY(_outDrawCall != null))
	{
		CLog::Log(PK_ERROR, "Failed to create a draw-call");
		return false;
	}
	SRHIDrawCall					&outDrawCall = *_outDrawCall;

	if (!_SetupCommonBillboardVertexBuffers(outDrawCall, false))
		return false;
	if (!_SetupBillboardDrawCall(ctx, toEmit, outDrawCall))
		return false;
	return true;
}

//----------------------------------------------------------------------------
//
//	Sounds (CPU)
//
//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy::LaunchCustomTasks(SRenderContext &ctx, const TMemoryView<const Drawers::SSound_DrawRequest * const> &drawRequests, Drawers::CBillboard_CPU *batch)
{
	(void)ctx; (void)drawRequests; (void)batch;
	PK_ASSERT(batch == null);
	// TODO: transform position/velocity in the camera-space with CustomTasks => rh_sound_cpu.{h,cpp}
	return true;
}

//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy::_IssueDrawCall_Sound(SRenderContext &ctx, const SDrawCallDesc &toEmit, SRHIDrawOutputs &output)
{
	(void)output;
	PK_NAMEDSCOPEDPROFILE("CRHIBillboardingBatchPolicy::EmitDrawCall Sound");

	// Sounds have no tasks to setup, and no vertex buffers.
	// Iterate on all draw requests
	const u32	drCount = toEmit.m_DrawRequests.Count();
	for (u32 dri = 0; dri < drCount; ++dri)
	{
		PK_ASSERT(toEmit.m_DrawRequests[dri] != null);
		const Drawers::SSound_DrawRequest			*dr = static_cast<const Drawers::SSound_DrawRequest*>(toEmit.m_DrawRequests[dri]);
		const Drawers::SSound_BillboardingRequest	&br = dr->m_BB;
		CRendererCacheInstance_UpdateThread			*rCache = static_cast<CRendererCacheInstance_UpdateThread*>(toEmit.m_RendererCaches[dri].Get());
		if (!PK_VERIFY(rCache != null))
		{
			CLog::Log(PK_ERROR, "Invalid renderer cache instance");
			return false;
		}

		PKSample::PCRendererCacheInstance	rCacheInstance = rCache->RenderThread_GetCacheInstance(); /* WARNING: bad method name - we are in the update-thread */
		if (rCacheInstance == null)
			continue; // side-effect of the RHIGraphicResource that loads the resource progressively

		CSoundResource	*curSoundRes = rCacheInstance->m_Sound.Get();
		if (!PK_VERIFY(curSoundRes != null))
		{
			CLog::Log(PK_ERROR, "Sound Resource is null");
			continue;
		}

		if (curSoundRes->m_SoundData == null)
		{
			// The sound failed to load, but don't assert and don't spam the log
			continue;
		}

		const CParticleStreamToRender_MainMemory	*streamToRender = dr->StreamToRender_MainMemory();
		PK_ASSERT(streamToRender != null);

		// Map the sound-pool with particles
		for (u32 pageIdx = 0; pageIdx < streamToRender->PageCount(); ++pageIdx)
		{
			const CParticlePageToRender_MainMemory	*page = streamToRender->Page(pageIdx);
			PK_ASSERT(page != null);
			if (page->Culled())
				continue;

			const u32	partCount = page->InputParticleCount();
			const u8	enabledTrue = u8(-1);

			PK_ASSERT(br.m_InvLifeStreamId.Valid());
			PK_ASSERT(br.m_LifeRatioStreamId.Valid());
			PK_ASSERT(br.m_PositionStreamId.Valid());
			TStridedMemoryView<const float>		bufLRt = page->StreamForReading<float>(br.m_LifeRatioStreamId);
			TStridedMemoryView<const float>		bufLif = page->StreamForReading<float>(br.m_InvLifeStreamId);
			TStridedMemoryView<const CFloat3>	bufPos = page->StreamForReading<CFloat3>(br.m_PositionStreamId);

			TStridedMemoryView<const CFloat3>	bufVel = (br.m_VelocityStreamId.Valid()) ? page->StreamForReading<CFloat3>(br.m_VelocityStreamId) : TStridedMemoryView<const CFloat3>(&CFloat4::ZERO.xyz(), partCount, 0);
			TStridedMemoryView<const float>		bufVol = (br.m_VolumeStreamId.Valid()) ? page->StreamForReading<float>(br.m_VolumeStreamId) : TStridedMemoryView<const float>(&CFloat4::ZERO.x(), partCount, 0);
			TStridedMemoryView<const float>		bufRad = (br.m_RangeStreamId.Valid()) ? page->StreamForReading<float>(br.m_RangeStreamId) : TStridedMemoryView<const float>(&CFloat4::ZERO.x(), partCount, 0);
			TStridedMemoryView<const u8>		bufEna = (br.m_EnabledStreamId.Valid()) ? page->StreamForReading<bool>(br.m_EnabledStreamId) : TStridedMemoryView<const u8>(&enabledTrue, partCount, 0);

			for (u32 idx = 0; idx < partCount; ++idx)
			{
				if (!bufEna[idx])
					continue;

				const float		ptime = bufLRt[idx] / bufLif[idx];
				if (ptime > curSoundRes->m_Length)
					continue;

				const CFloat3	posRel = m_MainViewMatrix.TransformVector(bufPos[idx]);

				const float		dist = posRel.Length();

				float	volumePerceived = bufVol[idx];
				if (br.m_AttenuationMode == 0)
				{
					volumePerceived *= (bufRad[idx] < 1.e-4) ? 0.f : 1.f - dist / bufRad[idx];
				}
				else if (br.m_AttenuationMode == 1)
				{
					volumePerceived *= (bufRad[idx] < 1.e-4) ? 0.f : 1.1f / (dist / bufRad[idx] + 0.1f) - 1.f;
				}
				if (volumePerceived < 0.001f)
					continue;

				const CFloat3	velRel = m_MainViewMatrix.RotateVector(bufVel[idx]);

				// We have a sound !
				// -> find the closest unused sound-element in the pool
				{
					const s32	sPoolMatchIdx = ctx.SoundPool().FindBestMatchingSoundSlot(curSoundRes, 0.200f * ctx.SimSpeed(), ptime);
					if (sPoolMatchIdx != -1)
					{
						SSoundElement	&matchSound = ctx.SoundPool()[sPoolMatchIdx];
						const float		deltaTime = matchSound.m_PlayTime - ptime;
						const float		deltaTimeAbs = PKAbs(deltaTime);
						matchSound.m_Used = true;
						matchSound.m_Position = posRel;
						matchSound.m_Velocity = velRel;
						matchSound.m_PerceivedVolume = volumePerceived;
						matchSound.m_DopplerLevel = br.m_DopplerFactor;
						// Soft-sync method
						if (deltaTimeAbs > 0.060f * ctx.SimSpeed())
						{
							//matchSound.m_PlaySpeed = (1.f - deltaTime / 5.f);
							//CLog::Log(PK_ERROR, "FMOD Soft-sync sound %d by %f s - playspeed = %f", sPoolMatchIdx, deltaTime, matchSound.m_PlaySpeed);
						}
						continue; // next particle
					}
					//if (sPoolMatchIdx == -1 && ptime > 0.050f)
					//	CLog::Log(PK_ERROR, "Sound-Renderer ; fail to get matching sound for particle %d (vol=%f, time=%f, minDt=%f)", idx, volumePerceived, ptime, sPoolMatchDtAbs);
				}

				// Not found, then get a free slot in the pool
				const CGuid	slotId = ctx.SoundPool().GetFreeSoundSlot(curSoundRes);
				if (slotId.Valid())
				{
					SSoundElement	&repSound = ctx.SoundPool()[slotId];
					repSound.m_Used = true;
					repSound.m_NeedResync = true;
					repSound.m_Position = posRel;
					repSound.m_Velocity = velRel;
					repSound.m_PlayTime = ptime;
					repSound.m_PerceivedVolume = volumePerceived;
					repSound.m_DopplerLevel = br.m_DopplerFactor;
					repSound.m_PlaySpeed = 1.f;
					repSound.m_Resource = curSoundRes;
				}
			} // end loop on particles
		} // end loop on pages
	} // end loop on draw-request
	return true;
}

//----------------------------------------------------------------------------
//
//	Lights
//
//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy::LaunchCustomTasks(SRenderContext &ctx, const TMemoryView<const Drawers::SLight_DrawRequest * const> &drawRequests, Drawers::CBillboard_CPU *batch)
{
	(void)batch;

	RHI::PApiManager	manager = ctx.ApiManager();

	// Map the light origins buffer:
	CFloat3				*mappedPos = (CFloat3*)manager->MapCpuView(m_LightsPositions.m_Buffer, 0, m_TotalParticleCount * sizeof(CFloat3));

	if (!PK_VERIFY(mappedPos != null))
		return false;

	m_LightDataCopyTask.Clear();
	// We feed the mapped buffer to our custom task (it just copies the positions in the mapped buffer):
	m_LightDataCopyTask.m_Positions = TMemoryView<CFloat3>(mappedPos, m_TotalParticleCount);

	// Additional inputs:
	if (!m_AdditionalFields.Empty())
	{
		m_MappedAdditionalFields.Clear();
		if (!PK_VERIFY(m_MappedAdditionalFields.Reserve(m_AdditionalFields.Count())))
			return false;
		for (u32 i = 0; i < m_AdditionalFields.Count(); ++i)
		{
			if (m_AdditionalFields[i].m_Buffer.Used())
			{
				void	*mappedValue = manager->MapCpuView(m_AdditionalFields[i].m_Buffer.m_Buffer, 0, m_TotalParticleCount * m_AdditionalFields[i].m_ByteSize);
				if (!PK_VERIFY(mappedValue != null))
					return false;

				Drawers::SCopyFieldDesc	mappedAdditionnalField;
				mappedAdditionnalField.m_AdditionalInputIndex = m_AdditionalFields[i].m_AdditionalInputIndex;
				mappedAdditionnalField.m_Storage.m_Count = m_TotalParticleCount;
				mappedAdditionnalField.m_Storage.m_RawDataPtr = static_cast<u8*>(mappedValue);
				mappedAdditionnalField.m_Storage.m_Stride = m_AdditionalFields[i].m_ByteSize;
				m_MappedAdditionalFields.PushBackUnsafe(mappedAdditionnalField);
			}
		}
		m_CopyAdditionalFieldsTask.m_FieldsToCopy = m_MappedAdditionalFields.View();
	}

	m_CopyTasks.Prepare(TMemoryView<const Drawers::SBase_DrawRequest * const>::Reinterpret(drawRequests));
	m_CopyTasks.AddExecAsyncPage(&m_LightDataCopyTask);
	m_CopyTasks.AddExecAsyncPage(&m_CopyAdditionalFieldsTask);
#if	(PK_HAS_PARTICLES_SELECTION != 0)
	// Editor only: for debugging purposes, we'll remove that from samples code later
	if (ctx.Selection().HasParticlesSelected() || ctx.Selection().HasRendersSelected())
	{
		// If we have a CPU storage, we can render the particles that are selected as wire-frame
		m_CopyStreamCustomParticleSelectTask.Clear();
		PK_ASSERT(m_IsParticleSelected.Used());
		void	*mappedValue = ctx.ApiManager()->MapCpuView(m_IsParticleSelected.m_Buffer, 0, sizeof(float) * m_TotalParticleCount);
		if (!PK_VERIFY(mappedValue != null))
			return false;
		m_CopyStreamCustomParticleSelectTask.m_DstSelectedParticles = TStridedMemoryView<float>(static_cast<float*>(mappedValue), m_TotalParticleCount, sizeof(float));
		m_CopyStreamCustomParticleSelectTask.m_SrcParticleSelected = ctx.Selection();
		m_CopyTasks.AddExecAsyncPage(&m_CopyStreamCustomParticleSelectTask);
	}
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)

	m_CopyTasks.LaunchTasks(null);
	return true;
}

//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy::_IssueDrawCall_Light(SRenderContext &ctx, const SDrawCallDesc &toEmit, SRHIDrawOutputs &output)
{
	(void)ctx;

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


	SRHIDrawCall	*_outDrawCall = _CreateDrawCall(toEmit, output, SRHIDrawCall::DrawCall_IndexedInstanced, PKSample::Option_VertexPassThrough);
	if (!PK_VERIFY(_outDrawCall != null))
	{
		CLog::Log(PK_ERROR, "Failed to create a draw-call");
		return false;
	}
	SRHIDrawCall					&outDrawCall = *_outDrawCall;
	const Utils::GpuBufferViews		&bufferView = rCacheInstance->m_AdditionalGeometry->m_PerGeometryViews.First();

	if (rCacheInstance->m_Cache == null)
		return false;

	RHI::PCRenderState					renderState = rCacheInstance->m_Cache->GetRenderState(PKSample::Option_VertexPassThrough);
	if (!PK_VERIFY(renderState != null))
		return false;
	TMemoryView<const RHI::SVertexAttributeDesc>	vertexBuffDescView = renderState->m_RenderState.m_ShaderBindings.m_InputAttributes.View();
	if (!PK_VERIFY(outDrawCall.m_VertexBuffers.Reserve(vertexBuffDescView.Count())))
		return false;

	bool	success = true;

	// Sphere positions:
	success &= outDrawCall.m_VertexBuffers.PushBack(bufferView.m_VertexBuffers.First()).Valid();
	// Light positions:
	success &= outDrawCall.m_VertexBuffers.PushBack(m_LightsPositions.m_Buffer).Valid();

	// Additional inputs:
	for (u32 i = 0; i < m_AdditionalFields.Count(); ++i)
	{
		if (m_AdditionalFields[i].m_Buffer.Used())
		{
			success &= outDrawCall.m_VertexBuffers.PushBack(m_AdditionalFields[i].m_Buffer.m_Buffer).Valid();

			// Editor only: for debugging purposes, we'll remove that from samples code later
			if (m_AdditionalFields[i].m_Semantic == SRHIDrawCall::DebugDrawGPUBuffer_Color)
			{
				PK_ASSERT(m_AdditionalFields[i].m_ByteSize == sizeof(CFloat4));
				PK_ASSERT(outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Color] == null);
				outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Color] = m_AdditionalFields[i].m_Buffer.m_Buffer;
				outDrawCall.m_DebugDrawGPUBufferOffsets[SRHIDrawCall::DebugDrawGPUBuffer_Color] = 0;
			}
			else if (m_AdditionalFields[i].m_Semantic == SRHIDrawCall::DebugDrawGPUBuffer_InstanceScales)
			{
				PK_ASSERT(m_AdditionalFields[i].m_ByteSize == sizeof(float));
				PK_ASSERT(outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_InstanceScales] == null);
				outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_InstanceScales] = m_AdditionalFields[i].m_Buffer.m_Buffer;
				outDrawCall.m_DebugDrawGPUBufferOffsets[SRHIDrawCall::DebugDrawGPUBuffer_InstanceScales] = 0;
			}
		}
	}

	// Fill the semantics for the debug draws:
	outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Position] = outDrawCall.m_VertexBuffers.First();
	outDrawCall.m_DebugDrawGPUBufferOffsets[SRHIDrawCall::DebugDrawGPUBuffer_Position] = 0;

	outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_InstancePositions] = m_LightsPositions.m_Buffer;
	outDrawCall.m_DebugDrawGPUBufferOffsets[SRHIDrawCall::DebugDrawGPUBuffer_InstancePositions] = 0;

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

	outDrawCall.m_InstanceCount = m_TotalParticleCount;

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	// Editor only: for debugging purposes, we'll remove that from samples code later
	if ((ctx.Selection().HasParticlesSelected() || ctx.Selection().HasRendersSelected()) && m_IsParticleSelected.Used())
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
// Triangles
//
//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy::LaunchCustomTasks(SRenderContext &ctx, const TMemoryView<const Drawers::STriangle_DrawRequest * const> &drawRequests, Drawers::CTriangle_CPU *batch)
{
	(void)ctx; (void)drawRequests; (void)batch;

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	// Editor only: for debugging purposes, we'll remove that from samples code later
	if (ctx.Selection().HasParticlesSelected() || ctx.Selection().HasRendersSelected())
	{
		// If we have a CPU storage, we can render the particles that are selected as wire-frame
		m_TriangleCustomParticleSelectTask.Clear();
		PK_ASSERT(m_IsParticleSelected.Used());
		void	*mappedValue = ctx.ApiManager()->MapCpuView(m_IsParticleSelected.m_Buffer, 0, sizeof(float) * m_TotalVertexCount);
		if (!PK_VERIFY(mappedValue != null))
			return false;
		m_TriangleCustomParticleSelectTask.m_DstSelectedParticles = TStridedMemoryView<float>(static_cast<float*>(mappedValue), m_TotalVertexCount, sizeof(float));
		m_TriangleCustomParticleSelectTask.m_SrcParticleSelected = ctx.Selection();
		batch->AddExecPage(&m_TriangleCustomParticleSelectTask);
	}
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy::LaunchCustomTasks(SRenderContext &ctx, const TMemoryView<const Drawers::STriangle_DrawRequest * const> &drawRequests, Drawers::CCopyStream_CPU *batch)
{
	(void)ctx; (void)drawRequests; (void)batch;

	PK_ASSERT_NOT_REACHED();

	return false;
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy::WaitForCustomTasks(SRenderContext &ctx)
{
	(void)ctx;
	if (m_RendererType == Renderer_Light || m_RendererType == Renderer_Decal)
	{
		m_CopyTasks.WaitTasks();
	}
	return true;
}

//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy::UnmapBuffers(SRenderContext &ctx)
{
	PK_NAMEDSCOPEDPROFILE("CRHIBillboardingBatchPolicy::UnmapBuffers");

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	// Editor only: for debugging purposes, we'll remove that from samples code later
	m_IsParticleSelected.UnmapIFN(ctx.ApiManager());
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)

	// UnmapBuffers is called once all CPU tasks (if any) has finished.
	// This gets called prior to issuing draw calls
	// Do not clear your vertex buffers here, just unmap

	if (ctx.IsPostUpdateFencePass()) // This is normal, we are processing sounds
		return true;

	RHI::PApiManager	manager = ctx.ApiManager();

	m_IndirectDraw.UnmapIFN(manager);

	m_Indices.UnmapIFN(manager);
	m_Positions.UnmapIFN(manager);
	m_Normals.UnmapIFN(manager);
	m_Tangents.UnmapIFN(manager);
	m_TexCoords0.UnmapIFN(manager);
	m_TexCoords1.UnmapIFN(manager);
	m_AtlasIDs.UnmapIFN(manager);
	m_Matrices.UnmapIFN(manager);
	m_InvMatrices.UnmapIFN(manager);
	m_UVRemap.UnmapIFN(manager);
	m_UVFactors.UnmapIFN(manager);
	m_LightsPositions.UnmapIFN(manager);

	// GPU stream offsets
	m_SimStreamOffsets_Positions.UnmapIFN(manager);
	m_SimStreamOffsets_Scales.UnmapIFN(manager);
	m_SimStreamOffsets_Orientations.UnmapIFN(manager);
	m_SimStreamOffsets_Enableds.UnmapIFN(manager);
	m_SimStreamOffsets_MeshIDs.UnmapIFN(manager);
	m_SimStreamOffsets_MeshLODs.UnmapIFN(manager);
		for (u32 i = 0; i < m_SimStreamOffsets_AdditionalInputs.Count(); ++i)
		m_SimStreamOffsets_AdditionalInputs[i].UnmapIFN(manager);
	m_IndirectionOffsets.UnmapIFN(manager);
	m_MatricesOffsets.UnmapIFN(manager);
	// m_Indirection is never mapped

	// Geom inputs:
	m_GeomPositions.UnmapIFN(manager);
	m_GeomConstants.UnmapIFN(manager);
	m_GeomSizes.UnmapIFN(manager);
	m_GeomSizes2.UnmapIFN(manager);
	m_GeomRotations.UnmapIFN(manager);
	m_GeomAxis0.UnmapIFN(manager);
	m_GeomAxis1.UnmapIFN(manager);

	// Additional inputs:
	for (u32 i = 0; i < m_AdditionalFields.Count(); ++i)
		m_AdditionalFields[i].m_Buffer.UnmapIFN(manager);

	// View dependent inputs:
	for (u32 i = 0; i < m_PerViewBuffers.Count(); ++i)
	{
		m_PerViewBuffers[i].m_Indices.UnmapIFN(manager);
		m_PerViewBuffers[i].m_Positions.UnmapIFN(manager);
		m_PerViewBuffers[i].m_Normals.UnmapIFN(manager);
		m_PerViewBuffers[i].m_Tangents.UnmapIFN(manager);
		m_PerViewBuffers[i].m_UVFactors.UnmapIFN(manager);
	}
	return true;
}

//----------------------------------------------------------------------------

void	CRHIBillboardingBatchPolicy::ClearBuffers(SRenderContext &ctx)
{
	(void)ctx;
	// This only gets called when a new frame has been collected (so before starting billboarding)
	// Clear here only resets the m_UsedThisFrame flags, it is not a proper clear as we want to avoid vbuffer resizing/allocations:
	// Batches (and their policy) can be reused for various renderers (no matter the layer),
	// so we ensure to bind the correct vertex buffers for draw calls.
	// It is up to you to find a proper vertex buffer pooling solution for particles and how/when to clear them

	m_IndirectDraw.Clear();

	m_Indices.Clear();
	m_Positions.Clear();
	m_Normals.Clear();
	m_Tangents.Clear();
	m_TexCoords0.Clear();
	m_TexCoords1.Clear();
	m_AtlasIDs.Clear();
	m_Matrices.Clear();
	m_UVRemap.Clear();
	m_UVFactors.Clear();

	// GPU Stream offsets
	m_SimStreamOffsets_Positions.Clear();
	m_SimStreamOffsets_Scales.Clear();
	m_SimStreamOffsets_Orientations.Clear();
	m_SimStreamOffsets_Enableds.Clear();
	m_SimStreamOffsets_MeshIDs.Clear();
	m_SimStreamOffsets_MeshLODs.Clear();
	for (SGpuBuffer &buffer : m_SimStreamOffsets_AdditionalInputs)
		buffer.Clear();
	m_Indirection.Clear();
	m_IndirectionOffsets.Clear();
	m_MatricesOffsets.Clear();

	// Geom inputs:
	m_GeomPositions.Clear();
	m_GeomConstants.Clear();
	m_GeomSizes.Clear();
	m_GeomSizes2.Clear();
	m_GeomRotations.Clear();
	m_GeomAxis0.Clear();
	m_GeomAxis1.Clear();

	// Additional inputs:
	for (u32 i = 0; i < m_AdditionalFields.Count(); ++i)
		m_AdditionalFields[i].m_Buffer.Clear();

	// View dependent inputs:
	for (u32 i = 0; i < m_PerViewBuffers.Count(); ++i)
	{
		m_PerViewBuffers[i].m_Indices.Clear();
		m_PerViewBuffers[i].m_Positions.Clear();
		m_PerViewBuffers[i].m_Normals.Clear();
		m_PerViewBuffers[i].m_Tangents.Clear();
		m_PerViewBuffers[i].m_UVFactors.Clear();
	}
}

//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy::EmitDrawCall(SRenderContext &ctx, const SDrawCallDesc &toEmit, SRHIDrawOutputs &output)
{
	PK_NAMEDSCOPEDPROFILE("CRHIBillboardingBatchPolicy::EmitDrawCall");

	PK_ASSERT(toEmit.m_TotalParticleCount <= m_TotalParticleCount); // <= if slicing is enabled
	PK_ASSERT(toEmit.m_TotalIndexCount <= m_TotalIndexCount);
	PK_ASSERT(!toEmit.m_DrawRequests.Empty());
	PK_ASSERT(toEmit.m_DrawRequests.First() != null);

	const bool	gpuStorage = toEmit.m_DrawRequests.First()->StreamToRender_MainMemory() == null;
	PK_ASSERT(gpuStorage == m_GPUStorage);

	const u32	dcCount = output.m_DrawCalls.Count();
	bool		success = false;

	switch (toEmit.m_Renderer)
	{
	case	Renderer_Billboard:
		PK_ASSERT(toEmit.m_TotalVertexCount > 0 && toEmit.m_TotalIndexCount > 0);
		if (gpuStorage)
			success = _IssueDrawCall_Billboard_GPU(ctx, toEmit, output);
		else
			success = _IssueDrawCall_Billboard_CPU(ctx, toEmit, output);
		break;
	case	Renderer_Ribbon:
		PK_ASSERT(toEmit.m_TotalVertexCount > 0 && toEmit.m_TotalIndexCount > 0);
		success = _IssueDrawCall_Ribbon(ctx, toEmit, output);
		break;
	case	Renderer_Light:
		success = _IssueDrawCall_Light(ctx, toEmit, output);
		break;
	case	Renderer_Mesh:
#if (PK_PARTICLES_UPDATER_USE_GPU != 0)
		if (gpuStorage)
			success = _IssueDrawCall_Mesh_GPU(ctx, toEmit, output);
		else
#endif // (PK_PARTICLES_UPDATER_USE_GPU != 0)
			success = _IssueDrawCall_Mesh_CPU(ctx, toEmit, output);
		break;
	case	Renderer_Triangle:
		success = _IssueDrawCall_Triangle(ctx, toEmit, output);
		break;
	case	Renderer_Decal:
		success = _IssueDrawCall_Decal(ctx, toEmit, output);
		break;
	case	Renderer_Sound:
		success = _IssueDrawCall_Sound(ctx, toEmit, output);
		break;
	default:
		PK_ASSERT_NOT_REACHED();
		break;
	}
	// Invalidate all draw-calls from this policy if any of them was not created properly:
	if (!success)
	{
		const u32	newDcCount = output.m_DrawCalls.Count();

		for (u32 i = dcCount; i < newDcCount; ++i)
		{
			output.m_DrawCalls[i].m_Valid = false;
			output.m_DrawCalls[i].m_RendererCacheInstance = null;
		}
	}
	return success;
}

//----------------------------------------------------------------------------

SRHIDrawCall	*CRHIBillboardingBatchPolicy::_CreateDrawCall(const SDrawCallDesc &toEmit, SRHIDrawOutputs &output, SRHIDrawCall::EDrawCallType drawCallType, u32 baseShaderOptions)
{
	CRendererCacheInstance_UpdateThread		*renderCacheInstance = static_cast<CRendererCacheInstance_UpdateThread*>(toEmit.m_RendererCaches.First().Get());
	if (!PK_VERIFY(renderCacheInstance != null))
	{
		CLog::Log(PK_ERROR, "Invalid renderer cache instance");
		return null;
	}

	if (!PK_VERIFY(output.m_DrawCalls.PushBack().Valid()))
		return null;
	SRHIDrawCall	*outDrawCall = &output.m_DrawCalls.Last();

	outDrawCall->m_Batch = this;
	outDrawCall->m_RendererCacheInstance = renderCacheInstance;
	outDrawCall->m_Type = drawCallType;
	outDrawCall->m_ShaderOptions = baseShaderOptions;
	outDrawCall->m_RendererType = m_RendererType;

	// Editor only: for debugging purposes, we'll remove that from samples code later
	{
		outDrawCall->m_BBox = toEmit.m_BBox;
		outDrawCall->m_TotalBBox = m_TotalBBox;
		outDrawCall->m_SlicedDC = toEmit.m_TotalIndexCount != m_TotalIndexCount;
	}

	outDrawCall->m_Valid =	renderCacheInstance != null &&
							renderCacheInstance->RenderThread_GetCacheInstance() != null &&
							renderCacheInstance->RenderThread_GetCacheInstance()->m_Cache != null &&
							renderCacheInstance->RenderThread_GetCacheInstance()->m_Cache->GetRenderState(static_cast<EShaderOptions>(baseShaderOptions)) != null;

	return outDrawCall;
}

//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy::_CreateOrResizeGpuBufferIf(const RHI::SRHIResourceInfos &infos, bool condition, const RHI::PApiManager &manager, SGpuBuffer &buffer, RHI::EBufferType type, u32 sizeToAlloc, u32 requiredSize)
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

void	CRHIBillboardingBatchPolicy::_ClearFrame(u32 activeViewCount)
{
	// Here we only clear the values that are changing from one frame to another.
	// Most of the vertex buffers will stay the same
	m_DrawRequestCount = 0;
	m_TotalParticleCount = 0;
	m_TotalParticleCount_OverEstimated = 0;
	m_TotalVertexCount = 0;
	m_TotalVertexCount_OverEstimated = 0;
	m_TotalIndexCount = 0;
	m_TotalIndexCount_OverEstimated = 0;
	m_IndexSize = 0;
	m_TotalBBox = CAABB::DEGENERATED;
	m_PerMeshParticleCount.Clear();
	m_HasMeshIDs = false;
	m_HasMeshLODs = false;
	m_PerViewBuffers.Resize(activeViewCount);
	m_MappedIndirectBuffer = null;
	m_MappedIndexedIndirectBuffer = null;
	m_DrawCallCurrentOffset = 0;
	m_PerMeshIndexCount.Clear();
	m_MappedSimStreamOffsets_Positions = null;
	m_MappedSimStreamOffsets_Scales = null;
	m_MappedSimStreamOffsets_Orientations = null;
	m_MappedSimStreamOffsets_Enableds = null;
	m_MappedSimStreamOffsets_MeshIDs = null;
	m_MappedSimStreamOffsets_LODs = null;
	m_MappedMatricesOffsets = null;
	m_HasColorStream = false;
	for (u32 i = 0; i < m_MappedSimStreamOffsets_AdditionalInputs.Count(); ++i)
		m_MappedSimStreamOffsets_AdditionalInputs[i] = null;
	PK_VERIFY(m_SimStreamOffsets_AdditionalInputs.Reserve(0x10)); // Reserved, memory not cleared
}

//----------------------------------------------------------------------------

void	CRHIBillboardingBatchPolicy::CCopyStream_Exec_LightsPositions::operator()(const Drawers::SCopyStream_ExecPage &execPage)
{
	const Drawers::SBase_DrawRequest			&baseDr = *execPage.m_DrawRequest;
	const Drawers::SLight_DrawRequest			&dr = static_cast<const Drawers::SLight_DrawRequest&>(baseDr);
	const Drawers::SLight_BillboardingRequest	&br = dr.m_BB;

	const CParticlePageToRender_MainMemory		&page = *execPage.m_Page;
	const u32									count = page.InputParticleCount();
	const u32									outCount = page.RenderedParticleCount();
	const u32									start = execPage.m_ParticleOffset;

	TMemoryView<CFloat3>				dstPos = m_Positions.Slice(start, outCount);
	TStridedMemoryView<const CFloat3>	srcPos = page.StreamForReading<CFloat3>(br.m_PositionStreamId);
	TStridedMemoryView<const u8>		srcEnabled = page.StreamForReading<bool>(br.m_EnabledStreamId);

	if (outCount == 0)
		return;

	if (count == outCount)
	{
		const CFloat3	*srcPosPtr = srcPos.Data();
		const u32		srcPosStride = srcPos.Stride();
		for (u32 partIdx = 0; partIdx < outCount; ++partIdx)
		{
			dstPos[partIdx] = *srcPosPtr;
			srcPosPtr = Mem::AdvanceRawPointer(srcPosPtr, srcPosStride);
		}
	}
	else
	{
		PK_ASSERT(!srcEnabled.Empty());
		CFloat3			*dstPosPtr = dstPos.Data();
		CFloat3			*dstPosStop = dstPos.DataEnd();
		const CFloat3	*srcPosPtr = srcPos.Data();
		const u32		srcPosStride = srcPos.Stride();
		const u8		*srcEnabledPtr = srcEnabled.Data();
		const u32		srcEnabledStride = srcEnabled.Stride();

		while (dstPosPtr < dstPosStop)
		{
			if (*srcEnabledPtr != 0)
				*dstPosPtr++ = *srcPosPtr;
			srcPosPtr = Mem::AdvanceRawPointer(srcPosPtr, srcPosStride);
			srcEnabledPtr = Mem::AdvanceRawPointer(srcEnabledPtr, srcEnabledStride);
		}
	}
}

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
