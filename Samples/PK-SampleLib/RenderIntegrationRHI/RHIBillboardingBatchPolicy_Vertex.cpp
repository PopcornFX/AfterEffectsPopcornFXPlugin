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
#include "RHIBillboardingBatchPolicy_Vertex.h"

#include "RHIParticleRenderDataFactory.h"
#include "pk_render_helpers/include/render_features/rh_features_basic.h"

#include <pk_rhi/include/interfaces/IApiManager.h>
#include <pk_rhi/include/interfaces/IGpuBuffer.h>

#if	(PK_PARTICLES_UPDATER_USE_D3D11 != 0)
//#	include <pk_rhi/include/D3D11/D3D11RHI.h>
#	include <pk_particles/include/Storage/D3D11/storage_d3d11.h>
#	include <pk_rhi/include/D3D11/D3D11ApiManager.h>
#endif
#if	(PK_PARTICLES_UPDATER_USE_D3D12 != 0)
//#	include <pk_rhi/include/D3D12/D3D12RHI.h>
#	include <pk_particles/include/Storage/D3D12/storage_d3d12.h>
#	include <pk_rhi/include/D3D12/D3D12ApiManager.h>
#endif
#if	(PK_PARTICLES_UPDATER_USE_UNKNOWN2 != 0)
//#	include <pk_rhi/include/UNKNOWN2/UNKNOWN2RHI.h>
#	include <pk_particles/include/Storage/UNKNOWN2/storage_UNKNOWN2.h>
#	include <pk_rhi/include/UNKNOWN2/UNKNOWN2ApiManager.h>
#endif

#include "PK-SampleLib/ShaderDefinitions/SampleLibShaderDefinitions.h"

#include "RHIRenderIntegrationConfig.h"

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

// Maximum number of draw requests batched in a single billboarding batch policy (1 draw request = 1 particle renderer being drawn)
static u32	kMaxDrawRequestCount = 0x100;

// Example implementation of billboard particles being rendered using instanced draw, their vertices are expanded in the vertex shader
// This implementation is both used by samples and v2 editor
// This is an example implementation using RHI as the graphics API abstraction, it's not the only way to provide particle data to render to shaders, feel free to contact us at support if you have specific layouts
//
// Billboarding method is the same for CPU and GPU particles: we pass in a texcoord vertex buffer and a simple index buffer, and instance that several times (number of particles)
// In the vertex shader, vertex positions are expanded based on texcoord values, and by extracting info from particle sim data (particle world's position, size, ..)
//
// CPU simulated particles:
// Particle data resides on main memory, we let PK-RenderHelpers tasks copy that data into GPU memory, by having a single gpu memory buffer per particle stream (position/size/..)
// Data is copied 1-1, except for positions which are setup to store the particle's draw request id in their w member, allowing compatible renderers batching, all other streams are straight copies
// You can create custom tasks to store data as you wish in your gpu buffers (for example in a single gpu buffer) if necessary
// An index buffer is generated if you map one to CPU tasks (sorted/sliced or not depending on the renderer settings and your usage)
//
// GPU simulated particles:
// Particle data resides on gpu memory directly, no copy task occur on CPU, we "retrieve" GPU buffers which basically create a PK-RHI "handle" on the graphics API native object (SRV/Buffer/..)
// A single buffer contains all particles for a given particle layer (might be shared by several draw requests), with particle streams offset in that buffer.
// As PK-RHI doesn't currently support structured buffers (only raw buffers), we create one gpu buffer per stream, containing offsets for that stream in all draw requests, but you can hook that as you wish
// Sorting is implemented for vertex billboarded GPU particles using a base 2 parallel radix sort on 16 bits keys.

//----------------------------------------------------------------------------
//
//	Billboarding batch policy:
//
//----------------------------------------------------------------------------

CRHIBillboardingBatchPolicy_Vertex::CRHIBillboardingBatchPolicy_Vertex()
:	m_RendererType(Renderer_Invalid)
,	m_Initialized(false)
,	m_CapsulesDC(false)
,	m_TubesDC(false)
,	m_MultiPlanesDC(false)
,	m_GpuBufferResizedOrCreated(false)
,	m_GPUStorage(false)
#if	(PK_HAS_PARTICLES_SELECTION != 0)
,	m_SelectionsResizedOrCreated(false)
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)
#if (PK_PARTICLES_UPDATER_USE_GPU != 0)
,	m_ColorStreamId(CGuid::INVALID)
,	m_NeedGPUSort(false)
#endif // (PK_PARTICLES_UPDATER_USE_GPU != 0)
,	m_UnusedCounter(0)
,	m_TotalParticleCount(0)
,	m_TotalParticleCount_OverEstimated(0)
,	m_DrawRequestCount(0)
,	m_IndexSize(0)
,	m_ShaderOptions(0)
{
	_ClearFrame();
}

//----------------------------------------------------------------------------

CRHIBillboardingBatchPolicy_Vertex::~CRHIBillboardingBatchPolicy_Vertex()
{
}

//----------------------------------------------------------------------------
//
//	Cull draw requests with some custom metric, here we cull some draw request depending on the rendering pass
//
//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy_Vertex::CanRender(const Drawers::SBillboard_DrawRequest *request, const PRendererCacheBase &rendererCache, SRenderContext &ctx)
{
	(void)request; (void)rendererCache;
	PK_ASSERT(request != null);
	PK_ASSERT(rendererCache != null);
	return ctx.IsRenderThreadPass();
}

//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy_Vertex::CanRender(const Drawers::STriangle_DrawRequest *request, const PRendererCacheBase &rendererCache, SRenderContext & ctx)
{
	(void)request; (void)rendererCache;
	PK_ASSERT(request != null);
	PK_ASSERT(rendererCache != null);
	return ctx.IsRenderThreadPass();
}

//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy_Vertex::CanRender(const Drawers::SRibbon_DrawRequest *request, const PRendererCacheBase &rendererCache, SRenderContext & ctx)
{
	(void)request; (void)rendererCache;
	PK_ASSERT(request != null);
	PK_ASSERT(rendererCache != null);
	return ctx.IsRenderThreadPass();
}

//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy_Vertex::Tick(SRenderContext &ctx, const TMemoryView<SSceneView> &views)
{
	(void)views; (void)ctx;
	// Tick function called on draw calls thread, here we remove ourselves if we haven't been used for rendering after 10 (collected) frames:
	// 10 PKFX Update()/UpdateFence() without being drawn
	if (m_UnusedCounter++ > 10)
		return false;
	return true;
}

//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy_Vertex::AreBillboardingBatchable(const PCRendererCacheBase &firstCache, const PCRendererCacheBase &secondCache) const
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

bool	CRHIBillboardingBatchPolicy_Vertex::AllocBuffers(SRenderContext &ctx, const SBuffersToAlloc &allocBuffers, const TMemoryView<SSceneView> &views, ERendererClass rendererType)
{
	(void)rendererType;
	(void)views;
	PK_NAMEDSCOPEDPROFILE("CRHIBillboardingBatchPolicy_Vertex::AllocBuffers");

	m_UnusedCounter = 0;

	PK_ASSERT(!allocBuffers.m_DrawRequests.Empty());
	PK_ASSERT(allocBuffers.m_DrawRequests.Count() == allocBuffers.m_RendererCaches.Count());
	PK_ASSERT((allocBuffers.m_TotalVertexCount > 0 && allocBuffers.m_TotalIndexCount > 0) ||
		allocBuffers.m_TotalParticleCount > 0);
	PK_ASSERT(allocBuffers.m_DrawRequests.First() != null);

	PK_ASSERT(rendererType != Renderer_Invalid);
	if (!m_Initialized)
		m_RendererType = rendererType;
	else if (!PK_VERIFY(m_RendererType == rendererType))
		return false;

	PK_ASSERT(rendererType == Renderer_Billboard || rendererType == Renderer_Triangle || rendererType == Renderer_Ribbon);

	RHI::PApiManager	manager = ctx.ApiManager();

	// Clear previous frame data
	_ClearFrame(allocBuffers.m_ToGenerate.m_PerViewGeneratedInputs.Count());

	// Setup counts
	PK_ASSERT(allocBuffers.m_TotalParticleCount == allocBuffers.m_TotalVertexCount);
	PK_ASSERT(allocBuffers.m_TotalParticleCount == allocBuffers.m_TotalIndexCount);
	m_TotalParticleCount = allocBuffers.m_TotalParticleCount;
	m_TotalIndexCount = allocBuffers.m_TotalIndexCount;
	m_IndexSize = (true /*m_TotalParticleCount > 0xFFFF*/) ? sizeof(u32) : sizeof(u16); // Right now, we only handle u32 indices
	m_GPUStorage = allocBuffers.m_DrawRequests.First()->StreamToRender_MainMemory() == null;
	m_TotalBBox = allocBuffers.m_TotalBBox;

#if (PK_PARTICLES_UPDATER_USE_GPU == 0)
	PK_ASSERT(!m_GPUStorage);
#endif // (PK_PARTICLES_UPDATER_USE_GPU != 0)

	if (rendererType == Renderer_Billboard)
	{
		// Determine whether or not we are a capsule aligned draw call
		const PopcornFX::Drawers::SBillboard_DrawRequest			*compatDr_Billboard = static_cast<const PopcornFX::Drawers::SBillboard_DrawRequest*>(allocBuffers.m_DrawRequests.First());
		const PopcornFX::Drawers::SBillboard_BillboardingRequest	&compatBr = compatDr_Billboard->m_BB;

		m_CapsulesDC = compatBr.m_Mode == PopcornFX::BillboardMode_AxisAlignedCapsule; // Right now, capsules are not batched with other billboarding modes
	}

	if (rendererType == Renderer_Ribbon)
	{
		// Determine whether or not we are a tube or multi-plane aligned draw call
		const PopcornFX::Drawers::SRibbon_DrawRequest			*compatDr_Ribbon = static_cast<const PopcornFX::Drawers::SRibbon_DrawRequest*>(allocBuffers.m_DrawRequests.First());
		const PopcornFX::Drawers::SRibbon_BillboardingRequest	&compatBr = compatDr_Ribbon->m_BB;

		// Right now, tubes & multi-plane ribbons are not batched with other billboarding modes
		m_MultiPlanesDC = compatBr.m_Mode == PopcornFX::RibbonMode_SideAxisAlignedMultiPlane;
		m_TubesDC = compatBr.m_Mode == PopcornFX::RibbonMode_SideAxisAlignedTube;
		m_ParticleQuadCount = compatBr.m_ParticleQuadCount;
	}

	if ((rendererType == Renderer_Billboard || rendererType == Renderer_Triangle || rendererType == Renderer_Ribbon) && !m_Initialized)
	{
		// TODO: The following index and vertex buffer could be shared between all vertex billboarding bb batch policies

		const u16	indexCount = rendererType == Renderer_Ribbon ? m_ParticleQuadCount*6 : 12;
		if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("VertexBB Draw Indices Buffer"), true, manager, m_DrawIndices, RHI::IndexBuffer, indexCount * sizeof(u16), indexCount * sizeof(u16), false))
			return false;

		volatile u16	*indices = static_cast<u16*>(manager->MapCpuView(m_DrawIndices.m_Buffer, 0, indexCount * sizeof(u16)));
		if (!PK_VERIFY(indices != null))
			return false;

		// RHI currently doesn't support DrawInstanced (without indices)
		if (rendererType == Renderer_Triangle)
		{
			indices[0] = 0;
			indices[1] = 1;
			indices[2] = 2;
		}
		else if (rendererType == Renderer_Billboard)
		{
			// TODO: This should match the CPU ribbon vertex order for clarity.
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
		}
		else if (rendererType == Renderer_Ribbon)
		{
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
		}

		m_DrawIndices.Unmap(manager);

		u32	uvCount = 4;
		if (m_CapsulesDC)
			uvCount = 6;
		else if (m_TubesDC) // Tube vertex count
			uvCount = (m_ParticleQuadCount + 1) * 2;
		else if (m_MultiPlanesDC) // Multi-Plane vertex count
			uvCount = m_ParticleQuadCount * 4;

		if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("VertexBB Texcoords Buffer"), true, manager, m_TexCoords, RHI::VertexBuffer, uvCount * sizeof(CFloat2), uvCount * sizeof(CFloat2), false))
			return false;

		volatile float	*texCoords = static_cast<float*>(manager->MapCpuView(m_TexCoords.m_Buffer, 0, uvCount * sizeof(CFloat2)));
		if (!PK_VERIFY(texCoords != null))
			return false;

		if (rendererType == Renderer_Billboard || rendererType == Renderer_Triangle)
		{
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
		}
		else
		{
			// We store the quad id in the U component of the texcoords. Since it is always 1.0f * quadId or -1.0f * quadId, it will be retrieved with abs(texcoord.x) in the vertex shader.
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
		}

		m_TexCoords.Unmap(manager);

		m_Initialized = true;
	}

	// Can help avoid GPU buffers resizing
	// You'll need a proper GPU buffers pooling system in your engine
	m_TotalParticleCount_OverEstimated = Mem::Align(m_TotalParticleCount, 0x100);

	//----------------------------------------------------------------------------
	// View independent inputs:
	// SBuffersToAlloc contains what is needed to construct your vertex declaration

	if (!m_GPUStorage)
	{
		// !Order matters!: expected constant set layout is layed out like so (see MaterialToRHI.cpp):
		// indices
		// bb data:
		// - positions
		// - sizes
		// - ..
		// additional data (material):
		// - colors
		// - ..

		if (!_AllocateBuffers_ViewDependent(manager, allocBuffers.m_ToGenerate))
			return false;
	}

	// Only allocate and map particle data buffers if we are billboarding a new collected frame (see documentation for more detail)
	if (allocBuffers.m_IsNewFrame)
	{
		m_DrawRequestCount = allocBuffers.m_DrawRequests.Count();

		if (m_GPUStorage)
		{
#if (PK_PARTICLES_UPDATER_USE_GPU != 0)
			if (!_AllocateBuffers_GPU(manager, allocBuffers))
				return false;

			// The offset in the indirect buffer of the current emitted draw call
			m_DrawCallCurrentOffset = 0;
#endif // (PK_PARTICLES_UPDATER_USE_GPU != 0)
		}
		else
		{
			if (!_AllocateBuffers_Main(manager, allocBuffers))
				return false;
		}
		if (!_AllocateBuffers_AdditionalInputs(manager, allocBuffers.m_ToGenerate))
			return false;
	}

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	// Editor only: for debugging purposes, we'll remove that from samples code later
	const bool	canDrawSelectedParticles = ctx.Selection().HasParticlesSelected() || ctx.Selection().HasRendersSelected();
	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Selection Buffer"), canDrawSelectedParticles, manager, m_Selections, RHI::RawBuffer, m_TotalParticleCount_OverEstimated * sizeof(float), m_TotalParticleCount * sizeof(float), false))
		return false;
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)
	return true;
}

//----------------------------------------------------------------------------

#if (PK_PARTICLES_UPDATER_USE_GPU != 0)
bool	CRHIBillboardingBatchPolicy_Vertex::_AllocateBuffers_GPU(const RHI::PApiManager &manager, const SBuffersToAlloc &allocBuffers)
{

	m_NeedGPUSort = allocBuffers.m_DrawRequests.First()->BaseBillboardingRequest().m_Flags.m_NeedSort;
	m_SortByCameraDistance = allocBuffers.m_DrawRequests.First()->BaseBillboardingRequest().m_SortByCameraDistance;

	// GPU particles are rendered using DrawIndexedInstancedIndirect
	// As detailed, we create on gpu buffer per stream, that will contain indices into draw request sim data buffers
	// For ribbon, this must be a raw indirect buffer to allow compute write (count is set from compute ribbon sort key pass instead of a copy command)
	const u32	indirectDrawBufferSizeInBytes = sizeof(RHI::SDrawIndexedIndirectArgs) * m_DrawRequestCount;
	const RHI::EBufferType	indirectBufferType = (m_RendererType == Renderer_Ribbon) ? RHI::RawIndirectDrawBuffer : RHI::IndirectDrawBuffer;
	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Indirect Draw Args Buffer"), true, manager, m_IndirectDraw, indirectBufferType, indirectDrawBufferSizeInBytes, indirectDrawBufferSizeInBytes, false))
		return false;

	const u32	viewIndependentInputs = allocBuffers.m_ToGenerate.m_GeneratedInputs;
	const u32	drCount = allocBuffers.m_DrawRequests.Count();

	// Camera sort
	if (m_NeedGPUSort)
	{
		if (m_CameraGPUSorters.Count() != drCount)
		{
			// For each draw request, we have indirection and sort key buffers
			// and a GPU sorter object, handling intermediates work buffers
			if (!PK_VERIFY(m_CameraSortIndirection.Resize(drCount)) ||
				!PK_VERIFY(m_CameraSortKeys.Resize(drCount)) ||
				!PK_VERIFY(m_CameraGPUSorters.Resize(drCount)))
				return false;
		}
		for (u32 i = 0; i < drCount; i++)
		{
			if (!PK_VERIFY(allocBuffers.m_DrawRequests[i]->RenderedParticleCount() > 0))
				continue;
			const u32	alignedParticleCount = Mem::Align<PK_GPU_SORT_NUM_KEY_PER_THREAD * PK_RH_GPU_THREADGROUP_SIZE>(allocBuffers.m_DrawRequests[i]->RenderedParticleCount());
			if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Camera Sort Indirection Buffer"), true, manager, m_CameraSortIndirection[i], RHI::RawBuffer, alignedParticleCount * sizeof(u32), alignedParticleCount * sizeof(u32), false) ||
				!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Camera Sort Keys Buffer"), true, manager, m_CameraSortKeys[i], RHI::RawBuffer, alignedParticleCount * sizeof(u32), alignedParticleCount * sizeof(u32), false))
				return false;
			// 16 bits key
			const u32	sortKeySizeInBits = 16;
			// Init sorter if needed and allocate buffers
			if (!PK_VERIFY(m_CameraGPUSorters[i].Init(sortKeySizeInBits, manager)) ||
				!PK_VERIFY(m_CameraGPUSorters[i].AllocateBuffers(allocBuffers.m_DrawRequests[i]->RenderedParticleCount(), manager)))
				return false;
		}
	}

	// Ribbon sort
	if (m_RendererType == Renderer_Ribbon)
	{
		if (m_RibbonGPUSorters.Count() != drCount)
		{
			// For each draw request, we have indirection and sort key buffers
			// and a GPU sorter object, handling intermediates work buffers
			if (!PK_VERIFY(m_RibbonSortIndirection.Resize(drCount)) ||
				!PK_VERIFY(m_RibbonSortKeys.Resize(drCount)) ||
				!PK_VERIFY(m_RibbonGPUSorters.Resize(drCount)))
				return false;
		}
		for (u32 i = 0; i < drCount; i++)
		{
			if (!PK_VERIFY(allocBuffers.m_DrawRequests[i]->RenderedParticleCount() > 0))
				continue;
			const u32	alignedParticleCount = Mem::Align<PK_GPU_SORT_NUM_KEY_PER_THREAD * PK_RH_GPU_THREADGROUP_SIZE>(allocBuffers.m_DrawRequests[i]->RenderedParticleCount());
			// Like for CPU ribbon, we have a 44 bits sort key from the self ID (26 bits) and the parentID (18 bits)
			const u32	sortKeySizeInBits = 44;
			const u32	sortKeyStrideInBytes = 2 * sizeof(u32);
			if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Ribbon Sort Indirection Buffer"), true, manager, m_RibbonSortIndirection[i], RHI::RawBuffer, alignedParticleCount * sizeof(u32), alignedParticleCount * sizeof(u32), false) ||
				!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Ribbon Sort Keys Buffer"), true, manager, m_RibbonSortKeys[i], RHI::RawBuffer, alignedParticleCount * sortKeyStrideInBytes, alignedParticleCount * sortKeyStrideInBytes, false))
				return false;
			// Init sorter if needed and allocate buffers
			if (!PK_VERIFY(m_RibbonGPUSorters[i].Init(sortKeySizeInBits, manager)) ||
				!PK_VERIFY(m_RibbonGPUSorters[i].AllocateBuffers(allocBuffers.m_DrawRequests[i]->RenderedParticleCount(), manager)))
				return false;
		}
	}

	// GPU buffer containing all draw request stream offsets.
	// The one flagged as simDataField will be used to fill constant set in _UpdateConstantSetsIFN_GPU().
	// Order must match materialToRHI simStreamOffsets constant sets.
	{
		// Allocate once, max number of draw requests, indexed by DC from push constant
		const u32	offsetsSizeInBytes = kMaxDrawRequestCount * sizeof(u32); // u32 offsets
		if (viewIndependentInputs & Drawers::GenInput_ParticlePosition)
		{
			// Particle positions stream
			if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Offsets_Enableds Buffer"), true, manager, m_SimStreamOffsets_Enableds, RHI::RawBuffer, offsetsSizeInBytes, offsetsSizeInBytes, true) ||
				!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Offsets_Positions Buffer"), true, manager, m_SimStreamOffsets_Positions, RHI::RawBuffer, offsetsSizeInBytes, offsetsSizeInBytes, true))
				return false;
		}

		// Particle parent IDs and self IDs for ribbon sort. Self ID is not flagged as simstream (not accessed in vertex shader)
		if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Offsets_ParentIDs Buffer"), m_RendererType == Renderer_Ribbon, manager, m_SimStreamOffsets_ParentIDs, RHI::RawBuffer, offsetsSizeInBytes, offsetsSizeInBytes, true) ||
			!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Offsets_SelfIDs Buffer"), m_RendererType == Renderer_Ribbon, manager, m_SimStreamOffsets_SelfIDs, RHI::RawBuffer, offsetsSizeInBytes, offsetsSizeInBytes, false))
			return false;

		// GPU sort related offset buffers
		{
			if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Offsets_CustomSortKeys Buffer"), m_NeedGPUSort && !m_SortByCameraDistance, manager, m_CustomSortKeysOffsets, RHI::RawBuffer, offsetsSizeInBytes, offsetsSizeInBytes, false))
				return false;
		}

		// Can be either one
		{
			if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Offsets_Sizes Buffer"), (viewIndependentInputs & Drawers::GenInput_ParticleSize) != 0, manager, m_SimStreamOffsets_Sizes, RHI::RawBuffer, offsetsSizeInBytes, offsetsSizeInBytes, true))
				return false;
			if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Offsets_Size2s Buffer"), (viewIndependentInputs & Drawers::GenInput_ParticleSize2) != 0, manager, m_SimStreamOffsets_Size2s, RHI::RawBuffer, offsetsSizeInBytes, offsetsSizeInBytes, true))
				return false;
		}

		// Rotation particle stream
		if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Offsets_Rotations Buffer"), (viewIndependentInputs & Drawers::GenInput_ParticleRotation) != 0, manager, m_SimStreamOffsets_Rotations, RHI::RawBuffer, offsetsSizeInBytes, offsetsSizeInBytes, true))
			return false;
		// First billboarding axis (necessary for billboard AxisAligned, billboard AxisAlignedSpheroid, billboard AxisAlignedCapsule, ribbon NormalAxisAligned and ribbon SideAxisAligned)
		if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Offsets_Axis0s Buffer"), (viewIndependentInputs & Drawers::GenInput_ParticleAxis0) != 0, manager, m_SimStreamOffsets_Axis0s, RHI::RawBuffer, offsetsSizeInBytes, offsetsSizeInBytes, true))
			return false;
		// Second billboarding axis (necessary for PlaneAligned)
		if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Offsets_Axis1s Buffer"), (viewIndependentInputs & Drawers::GenInput_ParticleAxis1) != 0, manager, m_SimStreamOffsets_Axis1s, RHI::RawBuffer, offsetsSizeInBytes, offsetsSizeInBytes, true))
			return false;
	}
	return true;
}
#endif // (PK_PARTICLES_UPDATER_USE_GPU != 0)

//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy_Vertex::_AllocateBuffers_Main(const RHI::PApiManager &manager, const SBuffersToAlloc &allocBuffers)
{
	PK_ASSERT(!m_GPUStorage);
	const u32	viewIndependentInputs = allocBuffers.m_ToGenerate.m_GeneratedInputs;

	// View independent indices (ie. Billboard with PlaneAligned Billboarding mode, or Ribbons that are view independent)
	if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Indices Buffer"), (viewIndependentInputs & Drawers::GenInput_Indices) != 0, manager, m_Indices, RHI::RawBuffer, m_TotalParticleCount_OverEstimated * m_IndexSize, m_TotalParticleCount * m_IndexSize, true))
		return false;

	// For CPU simulation (as GPU sim is just mapping existing GPU buffers to the draw call)
	// We issue tasks to fill vbuffers with particle sim data necessary for billboarding
	if (viewIndependentInputs & Drawers::GenInput_ParticlePosition)
	{
		PK_ASSERT(m_RendererType == Renderer_Billboard);
		// Particle positions stream
		if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Positions Buffer"), true, manager, m_Positions, RHI::RawBuffer, m_TotalParticleCount_OverEstimated * sizeof(CFloat4), m_TotalParticleCount * sizeof(CFloat4), true))
			return false;

		// Constant buffer filled by CPU task, will contain simple description of draw request
		// We do this so we can batch various draw requests (renderers from various mediums) in a single draw call
		// This constant buffer will contain flags for each draw request
		// Each particle position will contain its associated draw request ID in position's W component (See sample vertex shader for more detail)
		PK_STATIC_ASSERT(sizeof(Drawers::SBillboardDrawRequest) == sizeof(CFloat4));
		if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Billboard DrawRequests Buffer"), true, manager, m_DrawRequests, RHI::ConstantBuffer, kMaxDrawRequestCount * sizeof(Drawers::SBillboardDrawRequest), kMaxDrawRequestCount * sizeof(Drawers::SBillboardDrawRequest), false))
			return false;
	}

	if (viewIndependentInputs & Drawers::GenInput_ParticlePosition0)
	{
		PK_ASSERT(m_RendererType == Renderer_Triangle);
		PK_ASSERT(viewIndependentInputs & Drawers::GenInput_ParticlePosition1);
		PK_ASSERT(viewIndependentInputs & Drawers::GenInput_ParticlePosition2);

		// Vertex positions streams
		if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Position0s Buffer"), true, manager, m_VertexPositions0, RHI::RawBuffer, m_TotalParticleCount_OverEstimated * sizeof(CFloat4), m_TotalParticleCount * sizeof(CFloat4), true) ||
			!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Position1s Buffer"), true, manager, m_VertexPositions1, RHI::RawBuffer, m_TotalParticleCount_OverEstimated * sizeof(CFloat3), m_TotalParticleCount * sizeof(CFloat3), true) ||
			!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Position2s Buffer"), true, manager, m_VertexPositions2, RHI::RawBuffer, m_TotalParticleCount_OverEstimated * sizeof(CFloat3), m_TotalParticleCount * sizeof(CFloat3), true))
			return false;

		// Constant buffer filled by CPU task, will contain simple description of draw request
		// We do this so we can batch various draw requests (renderers from various mediums) in a single draw call
		// This constant buffer will contain normals bending factor for each draw request
		// Each vertex position 0 will contain its associated draw request ID in position's W component (See sample vertex shader for more detail)
		PK_STATIC_ASSERT(sizeof(Drawers::STriangleDrawRequest) == sizeof(float));
		if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Triangle DrawRequests Buffer"), true, manager, m_DrawRequests, RHI::ConstantBuffer, kMaxDrawRequestCount * sizeof(Drawers::STriangleDrawRequest), kMaxDrawRequestCount * sizeof(Drawers::STriangleDrawRequest), false))
			return false;
	}
	else
	{
		PK_ASSERT((viewIndependentInputs & Drawers::GenInput_ParticlePosition1) == 0);
		PK_ASSERT((viewIndependentInputs & Drawers::GenInput_ParticlePosition2) == 0);
	}

	if (m_RendererType == Renderer_Billboard)
	{
		// Can be either one
		{
			if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Sizes Buffer"), (viewIndependentInputs & Drawers::GenInput_ParticleSize) != 0, manager, m_Sizes, RHI::RawBuffer, m_TotalParticleCount_OverEstimated * sizeof(float), m_TotalParticleCount * sizeof(float), true))
				return false;
			if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Size2s Buffer"), (viewIndependentInputs & Drawers::GenInput_ParticleSize2) != 0, manager, m_Sizes2, RHI::RawBuffer, m_TotalParticleCount_OverEstimated * sizeof(CFloat2), m_TotalParticleCount * sizeof(CFloat2), true))
				return false;
		}

		// Rotation particle stream
		if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Rotations Buffer"), (viewIndependentInputs & Drawers::GenInput_ParticleRotation) != 0, manager, m_Rotations, RHI::RawBuffer, m_TotalParticleCount_OverEstimated * sizeof(float), m_TotalParticleCount * sizeof(float), true))
			return false;
		// First billboarding axis (necessary for AxisAligned, AxisAlignedSpheroid, AxisAlignedCapsule)
		if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Axis0s Buffer"), (viewIndependentInputs & Drawers::GenInput_ParticleAxis0) != 0, manager, m_Axis0s, RHI::RawBuffer, m_TotalParticleCount_OverEstimated * sizeof(CFloat3), m_TotalParticleCount * sizeof(CFloat3), true))
			return false;
		// Second billboarding axis (necessary for PlaneAligned)
		if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("Axis1s Buffer"), (viewIndependentInputs & Drawers::GenInput_ParticleAxis1) != 0, manager, m_Axis1s, RHI::RawBuffer, m_TotalParticleCount_OverEstimated * sizeof(CFloat3), m_TotalParticleCount * sizeof(CFloat3), true))
			return false;
	}
	return true;
}

//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy_Vertex::_AllocateBuffers_ViewDependent(const RHI::PApiManager &manager, const SGeneratedInputs &toGenerate)
{
	PK_ASSERT(!m_GPUStorage);

	// View dependent indices (required for sorted particles)
	// Indices are either view dependent or view independent for the entire batch

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
		if (!_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("View Sorted Indices Buffer"), (viewGeneratedInputs & Drawers::GenInput_Indices) != 0, manager, viewBuffers.m_Indices, RHI::RawBuffer, m_TotalParticleCount_OverEstimated * m_IndexSize, m_TotalParticleCount * m_IndexSize, true))
			return false;
	}
	return true;
}

//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy_Vertex::_AllocateBuffers_AdditionalInputs(const RHI::PApiManager &manager, const SGeneratedInputs &toGenerate)
{
	//----------------------------------------------------------------------------
	// Additional inputs:
	// Input streams not required by billboarding (Color, TextureID, AlphaRemapCursor, Additional shader inputs, ..)

	const u32	additionalInputCount = toGenerate.m_AdditionalGeneratedInputs.Count();
	if (m_GPUStorage)
	{
#if (PK_PARTICLES_UPDATER_USE_GPU != 0)
		if (!PK_VERIFY(m_SimStreamOffsets_AdditionalInputs.Resize(additionalInputCount)) ||
			!PK_VERIFY(m_MappedSimStreamOffsets_AdditionalInputs.Resize(additionalInputCount)))
			return false;

		m_ColorStreamId = CGuid::INVALID; // Used for debug drawing
		const u32	offsetsSizeInBytes = kMaxDrawRequestCount * sizeof(u32); // u32 offsets
		for (u32 i = 0; i < additionalInputCount; ++i)
		{
			if (!PK_VERIFY(_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("AdditionalInputs Buffer"), true, manager, m_SimStreamOffsets_AdditionalInputs[i], RHI::RawBuffer, offsetsSizeInBytes, offsetsSizeInBytes, true)))
				return false;
			// Editor only: for debugging purposes, we'll remove that from samples code later
			if (toGenerate.m_AdditionalGeneratedInputs[i].m_Type == PopcornFX::EBaseTypeID::BaseType_Float4 &&
				toGenerate.m_AdditionalGeneratedInputs[i].m_Name == BasicRendererProperties::SID_Diffuse_Color())
				m_ColorStreamId = i;
		}
#endif // (PK_PARTICLES_UPDATER_USE_GPU != 0)
	}
	else
	{
		if (!PK_VERIFY(m_AdditionalFields.Resize(additionalInputCount)))
			return false;

		// 2 possible debug color in order:
		CGuid		diffuseColor;
		CGuid		distortionColor;

		for (u32 i = 0; i < additionalInputCount; ++i)
		{
			// Create a gpu buffer for each additional field that your engine supports:
			// Those gpu buffers will be filled with particle data from the matching streams, per vertex, ie:
			// Particle stream with 2 particles, ViewposAligned so 4 vertices per particle :[Color0][Color1][TextureID0][TextureID1]
			// Dst gpu buffers															   :[Color0][Color1][TextureID0][TextureID1]
			// This policy creates a gpu buffer per additional input, but you could choose to only copy out specific particle fields

			const u32	typeSize = CBaseTypeTraits::Traits(toGenerate.m_AdditionalGeneratedInputs[i].m_Type).Size;
			m_AdditionalFields[i].m_ByteSize = typeSize;
			m_AdditionalFields[i].m_AdditionalInputIndex = i; // Important, we need that index internally in copy stream tasks

			// Here is where you can discard specific additional inputs if your engine does not support them
			// Ie. If the target engine doesn't support additional inputs, don't allocate gpu buffers for it
			if (toGenerate.m_AdditionalGeneratedInputs[i].m_Type == PopcornFX::EBaseTypeID::BaseType_Float4 &&
				toGenerate.m_AdditionalGeneratedInputs[i].m_Name == BasicRendererProperties::SID_Diffuse_Color())
				diffuseColor = i;
			else if (toGenerate.m_AdditionalGeneratedInputs[i].m_Type == PopcornFX::EBaseTypeID::BaseType_Float4 &&
				toGenerate.m_AdditionalGeneratedInputs[i].m_Name == BasicRendererProperties::SID_Distortion_Color())
				distortionColor = i;
			if (!PK_VERIFY(_CreateOrResizeGpuBufferIf(RHI::SRHIResourceInfos("AdditionalInputs Buffer"), true, manager, m_AdditionalFields[i].m_Buffer, RHI::RawBuffer, m_TotalParticleCount_OverEstimated * typeSize, m_TotalParticleCount * typeSize, true)))
				return false;
		}

		if (diffuseColor.Valid())
			m_AdditionalFields[diffuseColor].m_Semantic = SRHIDrawCall::DebugDrawGPUBuffer_Color; // Flag that additional field for debug draw (editor only)
		else if (distortionColor.Valid())
			m_AdditionalFields[distortionColor].m_Semantic = SRHIDrawCall::DebugDrawGPUBuffer_Color; // Flag that additional field for debug draw (editor only)
	}
	return true;
}

//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy_Vertex::MapBuffers(SRenderContext &ctx, const TMemoryView<SSceneView> &views, SGPUBillboardBatchJobs *billboardVertexBatch, const SGeneratedInputs &toMap)
{
	(void)views;
	PK_NAMEDSCOPEDPROFILE("CRHIBillboardingBatchPolicy_Vertex::MapBuffers (vertex billboard)");

	RHI::PApiManager	manager = ctx.ApiManager();

	if (billboardVertexBatch == null)
	{
		PK_ASSERT(m_GPUStorage);
#if (PK_PARTICLES_UPDATER_USE_GPU != 0)
		PK_NAMEDSCOPEDPROFILE("Map GPU streams offsets buffers");

		// If the billboard batch is null, we have GPU storage:
		m_MappedIndirectBuffer = static_cast<RHI::SDrawIndexedIndirectArgs*>(manager->MapCpuView(m_IndirectDraw.m_Buffer));

		// Mandatory streams
		m_MappedSimStreamOffsets_Enableds = static_cast<u32*>(manager->MapCpuView(m_SimStreamOffsets_Enableds.m_Buffer));
		m_MappedSimStreamOffsets_Positions = static_cast<u32*>(manager->MapCpuView(m_SimStreamOffsets_Positions.m_Buffer));
		m_MappedSimStreamOffsets_Sizes = m_SimStreamOffsets_Sizes.Used() ? static_cast<u32*>(manager->MapCpuView(m_SimStreamOffsets_Sizes.m_Buffer)) : null;
		m_MappedSimStreamOffsets_Size2s = m_SimStreamOffsets_Size2s.Used() ? static_cast<u32*>(manager->MapCpuView(m_SimStreamOffsets_Size2s.m_Buffer)) : null;

		// Optional streams
		m_MappedSimStreamOffsets_Rotations = m_SimStreamOffsets_Rotations.Used() ? static_cast<u32*>(manager->MapCpuView(m_SimStreamOffsets_Rotations.m_Buffer)) : null;
		m_MappedSimStreamOffsets_Axis0s = m_SimStreamOffsets_Axis0s.Used() ? static_cast<u32*>(manager->MapCpuView(m_SimStreamOffsets_Axis0s.m_Buffer)) : null;
		m_MappedSimStreamOffsets_Axis1s = m_SimStreamOffsets_Axis1s.Used() ? static_cast<u32*>(manager->MapCpuView(m_SimStreamOffsets_Axis1s.m_Buffer)) : null;

		if (!PK_VERIFY(!m_SimStreamOffsets_Rotations.Used() || m_MappedSimStreamOffsets_Rotations != null) ||
			!PK_VERIFY(!m_SimStreamOffsets_Axis0s.Used() || m_MappedSimStreamOffsets_Axis0s != null) ||
			!PK_VERIFY(!m_SimStreamOffsets_Axis1s.Used() || m_MappedSimStreamOffsets_Axis1s != null))
			return false;

		// Map all
		for (u32 i = 0; i < m_SimStreamOffsets_AdditionalInputs.Count(); ++i)
		{
			m_MappedSimStreamOffsets_AdditionalInputs[i] = static_cast<u32*>(manager->MapCpuView(m_SimStreamOffsets_AdditionalInputs[i].m_Buffer));
			if (!PK_VERIFY(m_MappedSimStreamOffsets_AdditionalInputs[i] != null))
				return false;
		}

		// Camera sort optional stream offsets
		if (m_NeedGPUSort && !m_SortByCameraDistance)
		{
			m_MappedCustomSortKeysOffsets = static_cast<u32*>(manager->MapCpuView(m_CustomSortKeysOffsets.m_Buffer));
			if (!PK_VERIFY(m_MappedCustomSortKeysOffsets != null))
				return false;
		}

		return PK_VERIFY(m_MappedIndirectBuffer != null) &&
				PK_VERIFY(m_MappedSimStreamOffsets_Enableds != null) &&
				PK_VERIFY(m_MappedSimStreamOffsets_Positions != null) &&
				PK_VERIFY(m_MappedSimStreamOffsets_Sizes != null || m_MappedSimStreamOffsets_Size2s != null);
#endif // (PK_PARTICLES_UPDATER_USE_GPU != 0)
	}
	PK_ASSERT(!m_GPUStorage);

	// Assign mapped gpu buffers on CPU tasks that will run async on worker threads
	// All buffers used below have been allocated in AllocBuffers during this BeginCollectingDrawCalls
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
		billboardVertexBatch->m_Exec_Indices.m_IndexStream.Setup(mappedValue, m_TotalParticleCount, m_IndexSize == sizeof(u32));
	}
	if (toMap.m_GeneratedInputs & Drawers::GenInput_ParticlePosition)
	{
		PK_ASSERT(m_Positions.Used());
		void	*mappedValue = manager->MapCpuView(m_Positions.m_Buffer, 0, m_TotalParticleCount * sizeof(CFloat4));
		if (!PK_VERIFY(mappedValue != null))
			return false;
		billboardVertexBatch->m_Exec_CopyBillboardingStreams.m_PositionsDrIds = TMemoryView<Drawers::SVertex_PositionDrId>(static_cast<Drawers::SVertex_PositionDrId*>(mappedValue), m_TotalParticleCount);

		PK_ASSERT(m_DrawRequests.Used());
		void	*mappedDrawRequests = manager->MapCpuView(m_DrawRequests.m_Buffer, 0, kMaxDrawRequestCount * sizeof(Drawers::SBillboardDrawRequest));
		if (!PK_VERIFY(mappedDrawRequests != null))
			return false;

		billboardVertexBatch->m_Exec_GeomBillboardDrawRequests.m_GeomDrawRequests = TMemoryView<Drawers::SBillboardDrawRequest>(static_cast<Drawers::SBillboardDrawRequest*>(mappedDrawRequests), kMaxDrawRequestCount);
	}
	if (toMap.m_GeneratedInputs & Drawers::GenInput_ParticleSize)
	{
		PK_ASSERT(m_Sizes.Used());
		void	*mappedValue = manager->MapCpuView(m_Sizes.m_Buffer, 0, m_TotalParticleCount * sizeof(float));
		if (!PK_VERIFY(mappedValue != null))
			return false;
		billboardVertexBatch->m_Exec_CopyBillboardingStreams.m_Sizes = TMemoryView<float>(static_cast<float*>(mappedValue), m_TotalParticleCount);
	}
	else if (toMap.m_GeneratedInputs & Drawers::GenInput_ParticleSize2)
	{
		PK_ASSERT(m_Sizes2.Used());
		void	*mappedValue = manager->MapCpuView(m_Sizes2.m_Buffer, 0, m_TotalParticleCount * sizeof(CFloat2));
		if (!PK_VERIFY(mappedValue != null))
			return false;
		billboardVertexBatch->m_Exec_CopyBillboardingStreams.m_Sizes2 = TMemoryView<CFloat2>(static_cast<CFloat2*>(mappedValue), m_TotalParticleCount);
	}
	if (toMap.m_GeneratedInputs & Drawers::GenInput_ParticleRotation)
	{
		PK_ASSERT(m_Rotations.Used());
		void	*mappedValue = manager->MapCpuView(m_Rotations.m_Buffer, 0, m_TotalParticleCount * sizeof(float));
		if (!PK_VERIFY(mappedValue != null))
			return false;
		billboardVertexBatch->m_Exec_CopyBillboardingStreams.m_Rotations = TMemoryView<float>(static_cast<float*>(mappedValue), m_TotalParticleCount);
	}
	if (toMap.m_GeneratedInputs & Drawers::GenInput_ParticleAxis0)
	{
		PK_ASSERT(m_Axis0s.Used());
		void	*mappedValue = manager->MapCpuView(m_Axis0s.m_Buffer, 0, m_TotalParticleCount * sizeof(CFloat3));
		if (!PK_VERIFY(mappedValue != null))
			return false;
		billboardVertexBatch->m_Exec_CopyBillboardingStreams.m_Axis0 = TMemoryView<CFloat3>(static_cast<CFloat3*>(mappedValue), m_TotalParticleCount);
	}
	if (toMap.m_GeneratedInputs & Drawers::GenInput_ParticleAxis1)
	{
		PK_ASSERT(m_Axis1s.Used());
		void	*mappedValue = manager->MapCpuView(m_Axis1s.m_Buffer, 0, m_TotalParticleCount * sizeof(CFloat3));
		if (!PK_VERIFY(mappedValue != null))
			return false;
		billboardVertexBatch->m_Exec_CopyBillboardingStreams.m_Axis1 = TMemoryView<CFloat3>(static_cast<CFloat3*>(mappedValue), m_TotalParticleCount);
	}

	// View dependent inputs:
	PK_ASSERT(m_PerViewBuffers.Count() == billboardVertexBatch->m_PerView.Count());
	for (u32 i = 0; i < m_PerViewBuffers.Count(); ++i)
	{
		const u32	viewGeneratedInputs = toMap.m_PerViewGeneratedInputs[i].m_GeneratedInputs;

		PK_ASSERT(m_PerViewBuffers[i].m_ViewIdx == billboardVertexBatch->m_PerView[i].m_ViewIndex);
		if (viewGeneratedInputs & Drawers::GenInput_Indices)
		{
			PK_ASSERT(m_PerViewBuffers[i].m_Indices.Used());
			void	*mappedValue = manager->MapCpuView(m_PerViewBuffers[i].m_Indices.m_Buffer, 0, m_TotalParticleCount * m_IndexSize);
			if (!PK_VERIFY(mappedValue != null))
				return false;
			billboardVertexBatch->m_PerView[i].m_Exec_Indices.m_IndexStream.Setup(mappedValue, m_TotalParticleCount, m_IndexSize == sizeof(u32));
		}
	}

	// Additional inputs:
	if (!toMap.m_AdditionalGeneratedInputs.Empty())
	{
		PK_ASSERT(toMap.m_AdditionalGeneratedInputs.Count() == m_AdditionalFields.Count());
		if (!PK_VERIFY(m_MappedAdditionalFields.Resize(m_AdditionalFields.Count())))
			return false;
		for (u32 i = 0; i < m_AdditionalFields.Count(); ++i)
		{
			if (m_AdditionalFields[i].m_Buffer.Used())
			{
				void	*mappedValue = manager->MapCpuView(m_AdditionalFields[i].m_Buffer.m_Buffer, 0, m_TotalParticleCount * m_AdditionalFields[i].m_ByteSize);
				if (!PK_VERIFY(mappedValue != null))
					return false;

				m_MappedAdditionalFields[i].m_AdditionalInputIndex = m_AdditionalFields[i].m_AdditionalInputIndex;
				m_MappedAdditionalFields[i].m_Storage.m_Count = m_TotalParticleCount;
				m_MappedAdditionalFields[i].m_Storage.m_RawDataPtr = static_cast<u8*>(mappedValue);
				m_MappedAdditionalFields[i].m_Storage.m_Stride = m_AdditionalFields[i].m_ByteSize;
			}
		}
		billboardVertexBatch->m_Exec_CopyAdditionalFields.m_FieldsToCopy = m_MappedAdditionalFields.View();
	}
	return true;
}

//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy_Vertex::MapBuffers(SRenderContext &ctx, const TMemoryView<SSceneView> &views, SGPUTriangleBatchJobs *triangleVertexBatch, const SGeneratedInputs &toMap)
{
	(void)views;
	PK_NAMEDSCOPEDPROFILE("CRHIBillboardingBatchPolicy_Vertex::MapBuffers (vertex triangle)");

	PK_ASSERT(!m_GPUStorage);

	RHI::PApiManager	manager = ctx.ApiManager();

	// Assign mapped gpu buffers on CPU tasks that will run async on worker threads
	// All buffers used below have been allocated in AllocBuffers during this BeginCollectingDrawCalls
	// They could also have been allocated in a previous BeginCollectingDrawCalls call, see documentation for more detail
	// CCopyStream_Exec_Indices will generate indices and sort particles based on the renderer sort metric if necessary
	// CCopyStream_Exec_Triangle_Std will copy particle sim data necessary for billboarding (Positions)
	// This tasks will also write in Position's W component the associated draw request ID
	// CCopyStream_Exec_GPUTriangleDrawRequests will write draw request datas packed
	// CCopyStream_Exec_AdditionalField will do a straight copy of specified particle data into the vertex buffers

	if (toMap.m_GeneratedInputs & Drawers::GenInput_Indices)
	{
		PK_ASSERT(m_Indices.Used());
		void	*mappedValue = manager->MapCpuView(m_Indices.m_Buffer, 0, m_IndexSize * m_TotalParticleCount);
		if (!PK_VERIFY(mappedValue != null))
			return false;
		triangleVertexBatch->m_Exec_Indices.m_IndexStream.Setup(mappedValue, m_TotalParticleCount, m_IndexSize == sizeof(u32));
	}
	if (toMap.m_GeneratedInputs & Drawers::GenInput_ParticlePosition0)
	{
		PK_ASSERT(toMap.m_GeneratedInputs & Drawers::GenInput_ParticlePosition1);
		PK_ASSERT(toMap.m_GeneratedInputs & Drawers::GenInput_ParticlePosition2);
		PK_ASSERT(m_VertexPositions0.Used());
		PK_ASSERT(m_VertexPositions1.Used());
		PK_ASSERT(m_VertexPositions2.Used());
		void	*mappedValue0 = manager->MapCpuView(m_VertexPositions0.m_Buffer, 0, m_TotalParticleCount * sizeof(CFloat4));
		void	*mappedValue1 = manager->MapCpuView(m_VertexPositions1.m_Buffer, 0, m_TotalParticleCount * sizeof(CFloat3));
		void	*mappedValue2 = manager->MapCpuView(m_VertexPositions2.m_Buffer, 0, m_TotalParticleCount * sizeof(CFloat3));
		if (!PK_VERIFY(mappedValue0 != null && mappedValue1 != null && mappedValue2 != null))
			return false;
		// PositionsDrIds contains the positions of the triangles first vertex in its XYZ components and the draw request ID for each triangle in the W component
		// Positions1 and Positions2 only contains the positions of the triangles second and third vertices
		triangleVertexBatch->m_Exec_CopyBillboardingStreams.m_PositionsDrIds = TMemoryView<Drawers::SVertex_PositionDrId>(static_cast<Drawers::SVertex_PositionDrId*>(mappedValue0), m_TotalParticleCount);
		triangleVertexBatch->m_Exec_CopyBillboardingStreams.m_Positions1 = TMemoryView<CFloat3>(static_cast<CFloat3*>(mappedValue1), m_TotalParticleCount);
		triangleVertexBatch->m_Exec_CopyBillboardingStreams.m_Positions2 = TMemoryView<CFloat3>(static_cast<CFloat3*>(mappedValue2), m_TotalParticleCount);

		PK_ASSERT(m_DrawRequests.Used());
		void	*mappedDrawRequests = manager->MapCpuView(m_DrawRequests.m_Buffer, 0, kMaxDrawRequestCount * sizeof(Drawers::STriangleDrawRequest));
		if (!PK_VERIFY(mappedDrawRequests != null))
			return false;
		triangleVertexBatch->m_Exec_GPUTriangleDrawRequests.m_GPUDrawRequests = TMemoryView<Drawers::STriangleDrawRequest>(static_cast<Drawers::STriangleDrawRequest*>(mappedDrawRequests), kMaxDrawRequestCount);
	}

	// View dependent inputs:
	PK_ASSERT(m_PerViewBuffers.Count() == triangleVertexBatch->m_PerView.Count());
	for (u32 i = 0; i < m_PerViewBuffers.Count(); ++i)
	{
		const u32	viewGeneratedInputs = toMap.m_PerViewGeneratedInputs[i].m_GeneratedInputs;

		PK_ASSERT(m_PerViewBuffers[i].m_ViewIdx == triangleVertexBatch->m_PerView[i].m_ViewIndex);
		if (viewGeneratedInputs & Drawers::GenInput_Indices)
		{
			PK_ASSERT(m_PerViewBuffers[i].m_Indices.Used());
			void	*mappedValue = manager->MapCpuView(m_PerViewBuffers[i].m_Indices.m_Buffer, 0, m_TotalParticleCount * m_IndexSize);
			if (!PK_VERIFY(mappedValue != null))
				return false;
			triangleVertexBatch->m_PerView[i].m_Exec_Indices.m_IndexStream.Setup(mappedValue, m_TotalParticleCount, m_IndexSize == sizeof(u32));
		}
	}

	// Additional inputs:
	if (!toMap.m_AdditionalGeneratedInputs.Empty())
	{
		PK_ASSERT(toMap.m_AdditionalGeneratedInputs.Count() == m_AdditionalFields.Count());
		if (!PK_VERIFY(m_MappedAdditionalFields.Resize(m_AdditionalFields.Count())))
			return false;
		for (u32 i = 0; i < m_AdditionalFields.Count(); ++i)
		{
			if (m_AdditionalFields[i].m_Buffer.Used())
			{
				void	*mappedValue = manager->MapCpuView(m_AdditionalFields[i].m_Buffer.m_Buffer, 0, m_TotalParticleCount * m_AdditionalFields[i].m_ByteSize);
				if (!PK_VERIFY(mappedValue != null))
					return false;

				m_MappedAdditionalFields[i].m_AdditionalInputIndex = m_AdditionalFields[i].m_AdditionalInputIndex;
				m_MappedAdditionalFields[i].m_Storage.m_Count = m_TotalParticleCount;
				m_MappedAdditionalFields[i].m_Storage.m_RawDataPtr = static_cast<u8*>(mappedValue);
				m_MappedAdditionalFields[i].m_Storage.m_Stride = m_AdditionalFields[i].m_ByteSize;
			}
		}
		triangleVertexBatch->m_Exec_CopyAdditionalFields.m_FieldsToCopy = m_MappedAdditionalFields.View();
	}
	return true;
}

//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy_Vertex::MapBuffers(SRenderContext &ctx, const TMemoryView<SSceneView> &views, SGPURibbonBatchJobs *billboardVertexBatch, const SGeneratedInputs &toMap)
{
	(void)views;
	(void)toMap;
	PK_NAMEDSCOPEDPROFILE("CRHIBillboardingBatchPolicy_Vertex::MapBuffers (ribbon vertex billboard)");

	RHI::PApiManager	manager = ctx.ApiManager();

	if (billboardVertexBatch == null)
	{
		PK_ASSERT(m_GPUStorage);
#if (PK_PARTICLES_UPDATER_USE_GPU != 0)
		PK_NAMEDSCOPEDPROFILE("Map GPU streams offsets buffers");

		// If the billboard batch is null, we have GPU storage:
		m_MappedIndirectBuffer = static_cast<RHI::SDrawIndexedIndirectArgs*>(manager->MapCpuView(m_IndirectDraw.m_Buffer));

		// Mandatory streams
		m_MappedSimStreamOffsets_Enableds = static_cast<u32*>(manager->MapCpuView(m_SimStreamOffsets_Enableds.m_Buffer));
		m_MappedSimStreamOffsets_Positions = static_cast<u32*>(manager->MapCpuView(m_SimStreamOffsets_Positions.m_Buffer));
		m_MappedSimStreamOffsets_Sizes = m_SimStreamOffsets_Sizes.Used() ? static_cast<u32*>(manager->MapCpuView(m_SimStreamOffsets_Sizes.m_Buffer)) : null;

		// Optional streams
		m_MappedSimStreamOffsets_Rotations = m_SimStreamOffsets_Rotations.Used() ? static_cast<u32*>(manager->MapCpuView(m_SimStreamOffsets_Rotations.m_Buffer)) : null;
		m_MappedSimStreamOffsets_Axis0s = m_SimStreamOffsets_Axis0s.Used() ? static_cast<u32*>(manager->MapCpuView(m_SimStreamOffsets_Axis0s.m_Buffer)) : null;
		m_MappedSimStreamOffsets_ParentIDs = m_SimStreamOffsets_ParentIDs.Used() ? static_cast<u32*>(manager->MapCpuView(m_SimStreamOffsets_ParentIDs.m_Buffer)) : null;
		m_MappedSimStreamOffsets_SelfIDs = m_SimStreamOffsets_SelfIDs.Used() ? static_cast<u32*>(manager->MapCpuView(m_SimStreamOffsets_SelfIDs.m_Buffer)) : null;

		if (!PK_VERIFY(!m_SimStreamOffsets_Rotations.Used() || m_MappedSimStreamOffsets_Rotations != null) ||
			!PK_VERIFY(!m_SimStreamOffsets_Axis0s.Used() || m_MappedSimStreamOffsets_Axis0s != null) ||
			!PK_VERIFY(!m_SimStreamOffsets_ParentIDs.Used() || m_MappedSimStreamOffsets_ParentIDs != null) ||
			!PK_VERIFY(!m_SimStreamOffsets_SelfIDs.Used() || m_MappedSimStreamOffsets_SelfIDs != null))
			return false;

		// Map all
		for (u32 i = 0; i < m_SimStreamOffsets_AdditionalInputs.Count(); ++i)
		{
			m_MappedSimStreamOffsets_AdditionalInputs[i] = static_cast<u32*>(manager->MapCpuView(m_SimStreamOffsets_AdditionalInputs[i].m_Buffer));
			if (!PK_VERIFY(m_MappedSimStreamOffsets_AdditionalInputs[i] != null))
				return false;
		}

		// Camera sort optional stream offsets
		if (m_NeedGPUSort && !m_SortByCameraDistance)
		{
			m_MappedCustomSortKeysOffsets = static_cast<u32*>(manager->MapCpuView(m_CustomSortKeysOffsets.m_Buffer));
			if (!PK_VERIFY(m_MappedCustomSortKeysOffsets != null))
				return false;
		}

		return	PK_VERIFY(m_MappedIndirectBuffer != null) &&
				PK_VERIFY(m_MappedSimStreamOffsets_Enableds != null) &&
				PK_VERIFY(m_MappedSimStreamOffsets_Positions != null) &&
				PK_VERIFY(m_MappedSimStreamOffsets_Sizes != null);
#endif // (PK_PARTICLES_UPDATER_USE_GPU != 0)
	}

	// Vertex ribbon billboarding not implemented for CPU simulation
	PK_ASSERT_NOT_REACHED();

	return false;
}
//----------------------------------------------------------------------------

#if (PK_PARTICLES_UPDATER_USE_GPU != 0)
bool CRHIBillboardingBatchPolicy_Vertex::_WriteGPUStreamsOffsets(const TMemoryView<const Drawers::SBase_DrawRequest * const> &drawRequests)
{
	PK_ASSERT(m_MappedIndirectBuffer != null);
	const u32	streamCount = m_GPUBuffers.Count(); // Stream offsets used by the vertex shader.
	const u32	drCount = drawRequests.Count();
	u32			GPUSortAdditionnalOffsetsCount = 0; // Additionnal offsets used by sort compute only.
	// TODO: replace count of offsets buffer by a proper tracking of
	// of those needed by compute but not by vertex shader, like m_GPUBuffers,
	// and rename m_GPUBuffers.

	if (m_NeedGPUSort && !m_SortByCameraDistance)
		GPUSortAdditionnalOffsetsCount += 1; // Custom sort key offsets

	if (m_RendererType == Renderer_Ribbon) // Ribbon sort: selfIDs stream (parent ID is contained in m_GPUBuffers count)
		GPUSortAdditionnalOffsetsCount += 1;

	PK_STACKALIGNEDMEMORYVIEW(u32, streamsOffsets, drCount * (streamCount + GPUSortAdditionnalOffsetsCount), 0x10);

	// Store offsets in a stack view ([PosOffsetDr0][PosOffsetDr1][SizeOffsetDr0][SizeOffsetDr1]..)
	for (u32 iDr = 0; iDr < drCount; ++iDr)
	{
		// Indirect buffer
		m_MappedIndirectBuffer[iDr].m_InstanceCount = 0;
		m_MappedIndirectBuffer[iDr].m_IndexOffset = 0;
		m_MappedIndirectBuffer[iDr].m_VertexOffset = 0;
		m_MappedIndirectBuffer[iDr].m_InstanceOffset = 0;

		// Fill in stream offsets for each draw request
		const CParticleStreamToRender_GPU				*streamToRender = drawRequests[iDr]->StreamToRender_GPU();
		PK_ASSERT(streamToRender != null);
		const Drawers::SBase_DrawRequest			*dr = drawRequests[iDr];
		const Drawers::SBase_BillboardingRequest	baseBr = dr->BaseBillboardingRequest();

		u32		offset = 0;

		// Mandatory streams
		streamsOffsets[offset++ * drCount + iDr] = streamToRender->StreamOffset(baseBr.m_EnabledStreamId);
		streamsOffsets[offset++ * drCount + iDr] = streamToRender->StreamOffset(baseBr.m_PositionStreamId);

		// Billboard streams
		if (m_RendererType == Renderer_Billboard)
		{
			const Drawers::SBillboard_DrawRequest	*billboardDr = static_cast<const Drawers::SBillboard_DrawRequest *>(dr);

			m_MappedIndirectBuffer[iDr].m_IndexCount = m_CapsulesDC ? 12 : 6;

			// Mandatory
			streamsOffsets[offset++ * drCount + iDr] = streamToRender->StreamOffset(billboardDr->m_BB.m_SizeStreamId);

			// Optional
			if (m_MappedSimStreamOffsets_Rotations != null)
				streamsOffsets[offset++ * drCount + iDr] = streamToRender->StreamOffset(billboardDr->m_BB.m_RotationStreamId);
			if (m_MappedSimStreamOffsets_Axis0s != null)
				streamsOffsets[offset++ * drCount + iDr] = streamToRender->StreamOffset(billboardDr->m_BB.m_Axis0StreamId);
			if (m_MappedSimStreamOffsets_Axis1s != null)
				streamsOffsets[offset++ * drCount + iDr] = streamToRender->StreamOffset(billboardDr->m_BB.m_Axis1StreamId);
		}
		else if (m_RendererType == Renderer_Ribbon)
		{
			const Drawers::SRibbon_DrawRequest	*ribbonDr = static_cast<const Drawers::SRibbon_DrawRequest *>(dr);

			m_MappedIndirectBuffer[iDr].m_IndexCount = m_ParticleQuadCount * 6;

			// Mandatory
			streamsOffsets[offset++ * drCount + iDr] = streamToRender->StreamOffset(ribbonDr->m_BB.m_WidthStreamId);
			streamsOffsets[offset++ * drCount + iDr] = streamToRender->StreamOffset(ribbonDr->m_BB.m_ParentIDStreamId);
			streamsOffsets[offset++ * drCount + iDr] = streamToRender->StreamOffset(ribbonDr->m_BB.m_SelfIDStreamId);

			// Optional
			if (m_MappedSimStreamOffsets_Axis0s != null)
				streamsOffsets[offset++ * drCount + iDr] = streamToRender->StreamOffset(ribbonDr->m_BB.m_AxisStreamId);
		}
		else
		{
			PK_ASSERT_NOT_REACHED_MESSAGE("Writing GPU streams offset from vertex policy, but draw request is neither a billboard or a ribbon one.");
			return false;
		}

		// Add all non-virtual stream additional inputs
		const u32	additionalInputCount = baseBr.m_AdditionalInputs.Count();
		for (u32 iInput = 0; iInput < additionalInputCount; ++iInput)
			streamsOffsets[offset++ * drCount + iDr] = streamToRender->StreamOffset(baseBr.m_AdditionalInputs[iInput].m_StreamId);

		if (m_NeedGPUSort && !m_SortByCameraDistance)
			streamsOffsets[offset++ * drCount + iDr] = streamToRender->StreamOffset(baseBr.m_SortKeyStreamId);

		PK_ASSERT(offset == streamCount + GPUSortAdditionnalOffsetsCount);
	}

	// Non temporal writes to gpu mem, aligned and contiguous
	u32	streamOffset = 0;
	Mem::Copy_Uncached(m_MappedSimStreamOffsets_Enableds, &streamsOffsets[streamOffset++ * drCount], sizeof(u32) * drCount);
	Mem::Copy_Uncached(m_MappedSimStreamOffsets_Positions, &streamsOffsets[streamOffset++ * drCount], sizeof(u32) * drCount);

	if (m_SimStreamOffsets_Size2s.Used())
		Mem::Copy_Uncached(m_MappedSimStreamOffsets_Size2s, &streamsOffsets[streamOffset++ * drCount], sizeof(u32) * drCount);
	else if (m_SimStreamOffsets_Sizes.Used())
		Mem::Copy_Uncached(m_MappedSimStreamOffsets_Sizes, &streamsOffsets[streamOffset++ * drCount], sizeof(u32) * drCount);

	if (m_MappedSimStreamOffsets_ParentIDs != null)
		Mem::Copy_Uncached(m_MappedSimStreamOffsets_ParentIDs, &streamsOffsets[streamOffset++ * drCount], sizeof(u32) * drCount);
	if (m_MappedSimStreamOffsets_SelfIDs != null)
		Mem::Copy_Uncached(m_MappedSimStreamOffsets_SelfIDs, &streamsOffsets[streamOffset++ * drCount], sizeof(u32) * drCount);

	if (m_MappedSimStreamOffsets_Rotations != null)
		Mem::Copy_Uncached(m_MappedSimStreamOffsets_Rotations, &streamsOffsets[streamOffset++ * drCount], sizeof(u32) * drCount);
	if (m_MappedSimStreamOffsets_Axis0s != null)
		Mem::Copy_Uncached(m_MappedSimStreamOffsets_Axis0s, &streamsOffsets[streamOffset++ * drCount], sizeof(u32) * drCount);
	if (m_MappedSimStreamOffsets_Axis1s != null)
		Mem::Copy_Uncached(m_MappedSimStreamOffsets_Axis1s, &streamsOffsets[streamOffset++ * drCount], sizeof(u32) * drCount);

	for (u32 iInput = 0; iInput < m_MappedSimStreamOffsets_AdditionalInputs.Count(); ++iInput)
		Mem::Copy_Uncached(m_MappedSimStreamOffsets_AdditionalInputs[iInput], &streamsOffsets[streamOffset++ * drCount], sizeof(u32) * drCount);

	if (m_MappedCustomSortKeysOffsets != null && !m_SortByCameraDistance)
		Mem::Copy_Uncached(m_MappedCustomSortKeysOffsets, &streamsOffsets[streamOffset++ * drCount], sizeof(u32) * drCount);

	return true;
}
#endif // (PK_PARTICLES_UPDATER_USE_GPU != 0)

//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy_Vertex::LaunchCustomTasks(SRenderContext &ctx, const TMemoryView<const Drawers::SBillboard_DrawRequest * const> &drawRequests, Drawers::CCopyStream_CPU *billboardVertexBatch)
{
	(void)ctx;
	// If the billboard batch is null, we have GPU storage:
	if (billboardVertexBatch == null)
	{
		PK_NAMEDSCOPEDPROFILE("Write GPU streams offsets");

		PK_ASSERT(m_GPUStorage);

#if (PK_PARTICLES_UPDATER_USE_GPU != 0)
		return PK_VERIFY(_WriteGPUStreamsOffsets(static_cast<const TMemoryView<const Drawers::SBase_DrawRequest * const>>(drawRequests)));
#endif // (PK_PARTICLES_UPDATER_USE_GPU != 0)
	}
	PK_ASSERT(!m_GPUStorage);

	(void)drawRequests;
	(void)billboardVertexBatch;

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	// Editor only: for debugging purposes, we'll remove that from samples code later
	if (ctx.Selection().HasParticlesSelected() || ctx.Selection().HasRendersSelected())
	{
		// If we have a CPU storage, we can render the particles that are selected as wire-frame
		m_GeomBillboardCustomParticleSelectTask.Clear();
		PK_ASSERT(m_Selections.Used());
		void	*mappedValue = ctx.ApiManager()->MapCpuView(m_Selections.m_Buffer, 0, sizeof(float) * m_TotalParticleCount);
		if (!PK_VERIFY(mappedValue != null))
			return false;
		m_GeomBillboardCustomParticleSelectTask.m_DstSelectedParticles = TStridedMemoryView<float>(static_cast<float*>(mappedValue), m_TotalParticleCount, sizeof(float));
		m_GeomBillboardCustomParticleSelectTask.m_SrcParticleSelected = ctx.Selection();
		billboardVertexBatch->AddExecAsyncPage(&m_GeomBillboardCustomParticleSelectTask);
	}
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)
	return true;
}

//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy_Vertex::LaunchCustomTasks(SRenderContext &ctx, const TMemoryView<const Drawers::STriangle_DrawRequest * const> &drawRequests, Drawers::CCopyStream_CPU *triangleVertexBatch)
{
	(void)ctx;
	(void)drawRequests;
	(void)triangleVertexBatch;

	PK_ASSERT(!m_GPUStorage);

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	// Editor only: for debugging purposes, we'll remove that from samples code later
	if (ctx.Selection().HasParticlesSelected() || ctx.Selection().HasRendersSelected())
	{
		// If we have a CPU storage, we can render the particles that are selected as wire-frame
		m_GeomBillboardCustomParticleSelectTask.Clear();
		PK_ASSERT(m_Selections.Used());
		void	*mappedValue = ctx.ApiManager()->MapCpuView(m_Selections.m_Buffer, 0, sizeof(float) * m_TotalParticleCount);
		if (!PK_VERIFY(mappedValue != null))
			return false;
		m_GeomBillboardCustomParticleSelectTask.m_DstSelectedParticles = TStridedMemoryView<float>(static_cast<float*>(mappedValue), m_TotalParticleCount, sizeof(float));
		m_GeomBillboardCustomParticleSelectTask.m_SrcParticleSelected = ctx.Selection();
		triangleVertexBatch->AddExecAsyncPage(&m_GeomBillboardCustomParticleSelectTask);
	}
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)

	return true;
}

//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy_Vertex::LaunchCustomTasks(SRenderContext &ctx, const TMemoryView<const Drawers::SRibbon_DrawRequest * const> &drawRequests, Drawers::CCopyStream_CPU *ribbonVertexBatch)
{
	(void)ctx;
	// If the billboard batch is null, we have GPU storage:
	if (ribbonVertexBatch == null)
	{
		PK_NAMEDSCOPEDPROFILE("Write GPU streams offsets");

		PK_ASSERT(m_GPUStorage);

#if (PK_PARTICLES_UPDATER_USE_GPU != 0)
		return PK_VERIFY(_WriteGPUStreamsOffsets(static_cast<const TMemoryView<const Drawers::SBase_DrawRequest * const>>(drawRequests)));
#endif // (PK_PARTICLES_UPDATER_USE_GPU != 0)
	}
	(void)drawRequests;
	PK_ASSERT_NOT_REACHED_MESSAGE("Ribbon vertex billboarding is only implemented for GPU sim");

	return true;
}

//----------------------------------------------------------------------------

u32	CRHIBillboardingBatchPolicy_Vertex::_GetVertexBillboardShaderOptions(const Drawers::SBillboard_BillboardingRequest &bbRequest)
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
#if (PK_PARTICLES_UPDATER_USE_GPU != 0)
	if (m_NeedGPUSort)
		shaderOptions |= Option_GPUSort;
#endif // (PK_PARTICLES_UPDATER_USE_GPU != 0)
	return shaderOptions;
}

//----------------------------------------------------------------------------

u32	CRHIBillboardingBatchPolicy_Vertex::_GetVertexRibbonShaderOptions(const Drawers::SRibbon_BillboardingRequest &bbRequest)
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
#if (PK_PARTICLES_UPDATER_USE_GPU != 0)
	if (m_NeedGPUSort)
		shaderOptions |= Option_GPUSort;
	if (m_GPUStorage)
		shaderOptions |= Option_GPUStorage;
#endif // (PK_PARTICLES_UPDATER_USE_GPU != 0)

	return shaderOptions;
}

//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy_Vertex::UnmapBuffers(SRenderContext &ctx)
{
	PK_NAMEDSCOPEDPROFILE("CRHIBillboardingBatchPolicy_Vertex::UnmpBuffers");

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	// Editor only: for debugging purposes, we'll remove that from samples code later
	m_Selections.UnmapIFN(ctx.ApiManager());
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)

	// UnmapBuffers is called once all CPU tasks (if any) has finished.
	// This gets called prior to issuing draw calls
	// Do not clear your vertex buffers here, just unmap

	RHI::PApiManager	manager = ctx.ApiManager();

	m_Indices.UnmapIFN(manager);
	m_DrawRequests.UnmapIFN(manager);
	m_Positions.UnmapIFN(manager);
	m_Sizes.UnmapIFN(manager);
	m_Sizes2.UnmapIFN(manager);
	m_Rotations.UnmapIFN(manager);
	m_Axis0s.UnmapIFN(manager);
	m_Axis1s.UnmapIFN(manager);

	m_VertexPositions0.UnmapIFN(manager);
	m_VertexPositions1.UnmapIFN(manager);
	m_VertexPositions2.UnmapIFN(manager);

#if (PK_PARTICLES_UPDATER_USE_GPU != 0)
	m_IndirectDraw.UnmapIFN(manager);
	m_SimStreamOffsets_Enableds.UnmapIFN(manager);
	m_SimStreamOffsets_Positions.UnmapIFN(manager);
	m_SimStreamOffsets_Sizes.UnmapIFN(manager);
	m_SimStreamOffsets_Size2s.UnmapIFN(manager);
	m_SimStreamOffsets_Rotations.UnmapIFN(manager);
	m_SimStreamOffsets_Axis0s.UnmapIFN(manager);
	m_SimStreamOffsets_Axis1s.UnmapIFN(manager);
	m_SimStreamOffsets_ParentIDs.UnmapIFN(manager);
	m_SimStreamOffsets_SelfIDs.UnmapIFN(manager);

	for (u32 i = 0; i < m_SimStreamOffsets_AdditionalInputs.Count(); ++i)
		m_SimStreamOffsets_AdditionalInputs[i].UnmapIFN(manager);

	m_CustomSortKeysOffsets.UnmapIFN(manager);
#endif // (PK_PARTICLES_UPDATER_USE_GPU != 0)

	// Additional inputs:
	for (u32 i = 0; i < m_AdditionalFields.Count(); ++i)
		m_AdditionalFields[i].m_Buffer.UnmapIFN(manager);

	// View dependent inputs:
	for (u32 i = 0; i < m_PerViewBuffers.Count(); ++i)
		m_PerViewBuffers[i].m_Indices.UnmapIFN(manager);
	return true;
}

//----------------------------------------------------------------------------

#if (PK_PARTICLES_UPDATER_USE_GPU != 0)
bool	CRHIBillboardingBatchPolicy_Vertex::_UpdateConstantSetsIFN_GPU(const RHI::PApiManager &manager, const SDrawCallDesc &toEmit, const RHI::PGpuBuffer &drStreamBuffer, const RHI::PGpuBuffer &drCameraSortIndirection, const RHI::PGpuBuffer &drRibbonSortIndirection, u32 shaderOptions)
{
	CRendererCacheInstance_UpdateThread		*renderCacheInstance = static_cast<CRendererCacheInstance_UpdateThread*>(toEmit.m_RendererCaches.First().Get());
	if (!PK_VERIFY(renderCacheInstance != null))
	{
		CLog::Log(PK_ERROR, "Invalid renderer cache instance");
		return false;
	}

	// Two constant sets:
	// - One re-created each frame per draw call (sim data SRV)
	// - One created once, valid for the entire batch, offset via push constant

	PK_ASSERT(renderCacheInstance != null);
	const PCRendererCacheInstance	cacheInstance = renderCacheInstance->RenderThread_GetCacheInstance();
	if (cacheInstance == null)
		return false;

	const RHI::SConstantSetLayout	*simDataConstantSetLayout = null;
	const RHI::SConstantSetLayout	*offsetsConstantSetLayout = null;
	if (!PK_VERIFY(cacheInstance->m_Cache->GetGPUStorageConstantSets(static_cast<PKSample::EShaderOptions>(shaderOptions), simDataConstantSetLayout, offsetsConstantSetLayout)) ||
		!PK_VERIFY(simDataConstantSetLayout != null) ||
		!PK_VERIFY(offsetsConstantSetLayout != null) ||
		simDataConstantSetLayout->m_Constants.Empty() ||
		offsetsConstantSetLayout->m_Constants.Empty())
		return false;

	m_VertexBBSimDataConstantSet = manager->CreateConstantSet(RHI::SRHIResourceInfos("Sim Data Constant Set"), *simDataConstantSetLayout);
	if (!PK_VERIFY(m_VertexBBSimDataConstantSet != null))
		return false;

	PK_ASSERT(drStreamBuffer != null);
	// Sim data constant set are:
	// raw stream + camera sort indirection buffer if needed + ribbon sort indirection and indirect draw buffers if needed
	PK_ASSERT(m_VertexBBSimDataConstantSet->GetConstantValues().Count() == (1u + (m_NeedGPUSort ? 1u : 0u) + ((m_RendererType == Renderer_Ribbon) ? 2u : 0u)));
	u32	constantSetLocation = 0;
	if (!PK_VERIFY(m_VertexBBSimDataConstantSet->SetConstants(drStreamBuffer, constantSetLocation++)))
		return false;
	if (m_NeedGPUSort)
	{
		if (!PK_VERIFY(drCameraSortIndirection != null) ||
			!PK_VERIFY(m_VertexBBSimDataConstantSet->SetConstants(drCameraSortIndirection, constantSetLocation++)))
			return false;
	}
	if (m_RendererType == Renderer_Ribbon)
	{
		if (!PK_VERIFY(drRibbonSortIndirection != null) ||
			!PK_VERIFY(m_VertexBBSimDataConstantSet->SetConstants(drRibbonSortIndirection, constantSetLocation++)) ||
			!PK_VERIFY(m_VertexBBSimDataConstantSet->SetConstants(m_IndirectDraw.m_Buffer, constantSetLocation++)))
			return false;
	}
	m_VertexBBSimDataConstantSet->UpdateConstantValues();

	// Lazy, once
	if (m_GpuBufferResizedOrCreated)
	{
		m_VertexBBOffsetsConstantSet = manager->CreateConstantSet(RHI::SRHIResourceInfos("Offsets Constant Set"), *offsetsConstantSetLayout);
		if (!PK_VERIFY(m_VertexBBOffsetsConstantSet != null))
			return false;

		// Fill offsets constant sets. Also contains SortIndirection offsets buffer if needed.
		PK_ASSERT(!m_GPUBuffers.Empty());
		PK_ASSERT(m_VertexBBOffsetsConstantSet->GetConstantValues().Count() == m_GPUBuffers.Count());
		for (u32 i = 0; i < m_VertexBBOffsetsConstantSet->GetConstantValues().Count(); ++i)
		{
			if (!PK_VERIFY(m_VertexBBOffsetsConstantSet->SetConstants(m_GPUBuffers[i], i)))
				return false;
		}
		m_VertexBBOffsetsConstantSet->UpdateConstantValues();

		m_ShaderOptions = shaderOptions;
		m_GpuBufferResizedOrCreated = false;
	}
	return true;
}
#endif // (PK_PARTICLES_UPDATER_USE_GPU != 0)

//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy_Vertex::_UpdateConstantSetsIFN_CPU(const RHI::PApiManager &manager, const SDrawCallDesc &toEmit, u32 shaderOptions)
{
	CRendererCacheInstance_UpdateThread		*renderCacheInstance = static_cast<CRendererCacheInstance_UpdateThread*>(toEmit.m_RendererCaches.First().Get());
	if (!PK_VERIFY(renderCacheInstance != null))
	{
		CLog::Log(PK_ERROR, "Invalid renderer cache instance");
		return false;
	}
	if (m_GpuBufferResizedOrCreated ||
		m_ShaderOptions != shaderOptions)
	{
		PK_ASSERT(renderCacheInstance != null);
		const PCRendererCacheInstance	cacheInstance = renderCacheInstance->RenderThread_GetCacheInstance();
		if (cacheInstance == null)
			return false;

		const RHI::SConstantSetLayout	*simDataConstantSetLayout = null;
		const RHI::SConstantSetLayout	*offsetsConstantSetLayout = null;
		if (!PK_VERIFY(cacheInstance->m_Cache->GetGPUStorageConstantSets(static_cast<PKSample::EShaderOptions>(shaderOptions), simDataConstantSetLayout, offsetsConstantSetLayout)) ||
			!PK_VERIFY(simDataConstantSetLayout != null) ||
			simDataConstantSetLayout->m_Constants.Empty())
			return false;

		m_VertexBBSimDataConstantSet = manager->CreateConstantSet(RHI::SRHIResourceInfos("Sim Data Constant Set"), *simDataConstantSetLayout);
		if (!PK_VERIFY(m_VertexBBSimDataConstantSet != null))
			return false;

		PK_ASSERT(!m_GPUBuffers.Empty());
		PK_ASSERT(m_VertexBBSimDataConstantSet->GetConstantValues().Count() == m_GPUBuffers.Count());
		for (u32 i = 0; i < m_VertexBBSimDataConstantSet->GetConstantValues().Count(); ++i)
		{
			if (!PK_VERIFY(m_VertexBBSimDataConstantSet->SetConstants(m_GPUBuffers[i], i)))
				return false;
		}
		m_VertexBBSimDataConstantSet->UpdateConstantValues();

		m_ShaderOptions = shaderOptions;
		m_GpuBufferResizedOrCreated = false;
	}
#if	(PK_HAS_PARTICLES_SELECTION != 0)
	if (m_SelectionsResizedOrCreated)
	{
		// TODO: This can be global
		RHI::SConstantSetLayout	selectionSetLayout(RHI::VertexShaderMask);
		selectionSetLayout.AddConstantsLayout(RHI::SRawBufferDesc("Selections"));

		m_SelectionConstantSet = manager->CreateConstantSet(RHI::SRHIResourceInfos("Selection Constant Set"), selectionSetLayout);
		if (!PK_VERIFY(m_SelectionConstantSet != null) ||
			!PK_VERIFY(m_SelectionConstantSet->SetConstants(m_Selections.m_Buffer, 0)))
			return false;
		m_SelectionConstantSet->UpdateConstantValues();
		m_SelectionsResizedOrCreated = false;
	}
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)

	return true;
}

//----------------------------------------------------------------------------

SRHIDrawCall	*CRHIBillboardingBatchPolicy_Vertex::_CreateDrawCall(SRenderContext &ctx, const SDrawCallDesc &toEmit, SRHIDrawOutputs &output, SRHIDrawCall::EDrawCallType drawCallType, u32 shaderOptions)
{
	(void)ctx;
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
	outDrawCall->m_ShaderOptions = shaderOptions;
	outDrawCall->m_RendererType = m_RendererType;
	outDrawCall->m_GPUStorageSimDataConstantSet = m_VertexBBSimDataConstantSet;

	// Editor only: for debugging purposes, we'll remove that from samples code later
	{
		outDrawCall->m_BBox = toEmit.m_BBox;
		outDrawCall->m_TotalBBox = m_TotalBBox;
		outDrawCall->m_SlicedDC = toEmit.m_TotalIndexCount != m_TotalIndexCount;
	}

#if (PK_PARTICLES_UPDATER_USE_GPU != 0)
	PK_ASSERT(m_VertexBBOffsetsConstantSet == null || m_GPUStorage);
	outDrawCall->m_GPUStorageOffsetsConstantSet = m_VertexBBOffsetsConstantSet;
#endif // (PK_PARTICLES_UPDATER_USE_GPU != 0)

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	if ((ctx.Selection().HasParticlesSelected() || ctx.Selection().HasRendersSelected()) && m_SelectionConstantSet != null)
		outDrawCall->m_SelectionConstantSet = m_SelectionConstantSet;
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)

	outDrawCall->m_Valid =	renderCacheInstance != null &&
							renderCacheInstance->RenderThread_GetCacheInstance() != null &&
							renderCacheInstance->RenderThread_GetCacheInstance()->m_Cache != null &&
							renderCacheInstance->RenderThread_GetCacheInstance()->m_Cache->GetRenderState(static_cast<EShaderOptions>(shaderOptions)) != null;

	PK_ASSERT(	(outDrawCall->m_ShaderOptions & Option_VertexBillboarding) ||
				(outDrawCall->m_ShaderOptions & Option_TriangleVertexBillboarding) ||
				(outDrawCall->m_ShaderOptions & Option_RibbonVertexBillboarding));
	return outDrawCall;
}

//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy_Vertex::_CreateOrResizeGpuBufferIf(const RHI::SRHIResourceInfos &infos, bool condition, const RHI::PApiManager &manager, SGpuBuffer &buffer, RHI::EBufferType type, u32 sizeToAlloc, u32 requiredSize, bool simDataField)
{
	PK_ASSERT(sizeToAlloc >= requiredSize);
	if (!condition)
		return true;
	bool	bufferModified = false;
	if (buffer.m_Buffer == null || buffer.m_Buffer->GetByteSize() < requiredSize)
	{
		RHI::PGpuBuffer	gpuBuffer = manager->CreateGpuBuffer(infos, type, sizeToAlloc);
		buffer.SetGpuBuffer(gpuBuffer);
		if (!PK_VERIFY(gpuBuffer != null) ||
			!PK_VERIFY(buffer.Used()))
			return false;
		bufferModified = true;
	}
	else
		buffer.Use();

	// Add the buffer to m_GPUBuffers array (order matters)
	if (type == RHI::EBufferType::RawBuffer)
	{
		if (simDataField)
		{
			m_GpuBufferResizedOrCreated |= bufferModified;
			if (!PK_VERIFY(m_GPUBuffers.PushBack(buffer.m_Buffer).Valid()))
				return false;
		}
#if	(PK_HAS_PARTICLES_SELECTION != 0)
		else
			m_SelectionsResizedOrCreated = bufferModified;
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)
	}
	return true;
}

//----------------------------------------------------------------------------

#if (PK_PARTICLES_UPDATER_USE_GPU != 0)
bool	CRHIBillboardingBatchPolicy_Vertex::_IssueDrawCall_Billboard_GPU(SRenderContext &ctx, const SDrawCallDesc &toEmit, SRHIDrawOutputs &output)
{
	PK_ASSERT(m_GPUStorage);
	PK_ASSERT(m_DrawIndices.Used());

	PK_NAMEDSCOPEDPROFILE("CRHIBillboardingBatchPolicy_Vertex::EmitDrawCall Billboard (GPU)");

	// Draw calls are not issued inline here, but later in PK-SampleLib's render loop
	// _CreateDrawCalls outputs a new pushed draw call that will be processed later (in SRHIDrawOutputs)
	RHI::PApiManager	manager = ctx.ApiManager();

	// !Currently, there is no batching for gpu particles!
	// So we'll emit one draw call per draw request
	const u32	drCount = toEmit.m_DrawRequests.Count();

	if (m_NeedGPUSort)
	{
		// Create compute sort key constant set
		if (drCount != m_ComputeCameraSortKeysConstantSets.Count())
		{
			if (!PK_VERIFY(m_ComputeCameraSortKeysConstantSets.Resize(drCount)))
				return false;
			RHI::SConstantSetLayout	layout;
			PKSample::CreateComputeSortKeysConstantSetLayout(layout, m_SortByCameraDistance, false);
			for (auto &constantSet : m_ComputeCameraSortKeysConstantSets)
				constantSet = manager->CreateConstantSet(RHI::SRHIResourceInfos("Camera Sort Keys Constant Set"), layout);
		}
		const u32	computePerDrawRequest = 3 * ((16 + 3) / 4) + 1; // 3 steps 4 bits radix sort, 16 bits key, plus 1 dispatch to generate the sort key

		if (!PK_VERIFY(output.m_ComputeDispatchs.Reserve(drCount * computePerDrawRequest)))
			return false;
	}

	for (u32 dri = 0; dri < drCount; ++dri)
	{
		PK_ASSERT(toEmit.m_DrawRequests[dri] != null);
		const Drawers::SBillboard_DrawRequest			*dr = static_cast<const Drawers::SBillboard_DrawRequest*>(toEmit.m_DrawRequests[dri]);
		const Drawers::SBillboard_BillboardingRequest	&bbRequest = dr->m_BB;
		const CParticleStreamToRender					*streamToRender = &dr->StreamToRender();

		RHI::PGpuBuffer		drStreamBuffer;
		RHI::PGpuBuffer		drStreamSizeBuffer;

#if	(PK_PARTICLES_UPDATER_USE_D3D11 != 0)
		if (streamToRender->StorageClass() == CParticleStorageManager_D3D11::DefaultStorageClass())
		{
			const CParticleStreamToRender_D3D11	*streamsToRender = static_cast<const CParticleStreamToRender_D3D11*>(streamToRender);
			const SBuffer_D3D11					&streamBuffer = streamsToRender->StreamBuffer();
			const SBuffer_D3D11					&streamSizeBuffer = streamsToRender->StreamSizeBuf();

			if (PK_VERIFY(!streamBuffer.Empty()) && PK_VERIFY(!streamSizeBuffer.Empty())) // We have valid sim data and particle count buffers
			{
				drStreamBuffer = CastD3D11(manager)->D3D11GetGpuBufferFromExisting(RHI::SRHIResourceInfos("Stream Data Buffer"), streamBuffer.m_Buffer, RHI::RawBuffer);
				drStreamSizeBuffer = CastD3D11(manager)->D3D11GetGpuBufferFromExisting(RHI::SRHIResourceInfos("Stream Infos Buffer"), streamSizeBuffer.m_Buffer, RHI::RawVertexBuffer);
			}
		}
#endif
#if	(PK_PARTICLES_UPDATER_USE_D3D12 != 0)
		if (streamToRender->StorageClass() == CParticleStorageManager_D3D12::DefaultStorageClass())
		{
			const CParticleStreamToRender_D3D12	*streamsToRender = static_cast<const CParticleStreamToRender_D3D12*>(streamToRender);
			const SBuffer_D3D12					&streamBuffer = streamsToRender->StreamBuffer();
			const SBuffer_D3D12					&streamSizeBuffer = streamsToRender->StreamSizeBuf();

			if (PK_VERIFY(!streamBuffer.Empty()) && PK_VERIFY(!streamSizeBuffer.Empty())) // We have valid sim data and particle count buffers
			{
				drStreamBuffer = CastD3D12(manager)->D3D12GetGpuBufferFromExisting(RHI::SRHIResourceInfos("Stream Data Buffer"), streamBuffer.m_Resource, RHI::RawBuffer, streamBuffer.m_State);
				drStreamSizeBuffer = CastD3D12(manager)->D3D12GetGpuBufferFromExisting(RHI::SRHIResourceInfos("Stream Infos Buffer"), streamSizeBuffer.m_Resource, RHI::RawVertexBuffer, streamSizeBuffer.m_State);
			}
		}
#endif
#if	(PK_PARTICLES_UPDATER_USE_UNKNOWN2 != 0)
		if (streamToRender->StorageClass() == CParticleStorageManager_UNKNOWN2::DefaultStorageClass())
		{
			const CParticleStreamToRender_UNKNOWN2	*streamsToRender = static_cast<const CParticleStreamToRender_UNKNOWN2*>(streamToRender);
			const SBuffer_UNKNOWN2					&streamBuffer = streamsToRender->StreamBuffer();
			const SBuffer_UNKNOWN2					&streamSizeBuffer = streamsToRender->StreamSizeBuf();

			if (PK_VERIFY(!streamBuffer.Empty()) && PK_VERIFY(!streamSizeBuffer.Empty())) // We have valid sim data and particle count buffers
			{
				drStreamBuffer = CastUNKNOWN2(manager)->UNKNOWN2GetGpuBufferFromExisting(RHI::SRHIResourceInfos("Stream Data Buffer"), streamBuffer.m_Resource, streamBuffer.m_ByteSize, RHI::RawBuffer);
				drStreamSizeBuffer = CastUNKNOWN2(manager)->UNKNOWN2GetGpuBufferFromExisting(RHI::SRHIResourceInfos("Stream Infos Buffer"), streamSizeBuffer.m_Resource, streamBuffer.m_ByteSize, RHI::RawVertexBuffer);
			}
		}
#endif
		if (!PK_VERIFY(drStreamBuffer != null) ||
			!PK_VERIFY(drStreamSizeBuffer != null))
			return false;

		// Camera GPU sort
		if (m_NeedGPUSort)
		{
			CRendererCacheInstance_UpdateThread		*renderCacheInstance = static_cast<CRendererCacheInstance_UpdateThread*>(toEmit.m_RendererCaches.First().Get());
			PKSample::PCRendererCacheInstance		rCacheInstance = renderCacheInstance->RenderThread_GetCacheInstance();
			if (rCacheInstance == null)
				return false;

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

				output.m_ComputeDispatchs.PushBack(computeDispatch);
			}

			// Compute: Sort computes
			{
				m_CameraGPUSorters[dri].SetInOutBuffers(m_CameraSortKeys[dri].m_Buffer, m_CameraSortIndirection[dri].m_Buffer);
				m_CameraGPUSorters[dri].AppendDispatchs(rCacheInstance, output.m_ComputeDispatchs);
			}
		}

		// Emit draw call
		{
			const u32				shaderOptions = PKSample::Option_VertexPassThrough | PKSample::Option_GPUStorage | _GetVertexBillboardShaderOptions(bbRequest);
			const RHI::PGpuBuffer	sortIndirectionBuffer = m_NeedGPUSort ? m_CameraSortIndirection[dri].m_Buffer : null;
			if (!_UpdateConstantSetsIFN_GPU(ctx.ApiManager(), toEmit, drStreamBuffer, sortIndirectionBuffer, null, shaderOptions))
				return false;

			SRHIDrawCall	*_outDrawCall = _CreateDrawCall(ctx, toEmit, output, SRHIDrawCall::DrawCall_IndexedInstancedIndirect, shaderOptions);
			if (!PK_VERIFY(_outDrawCall != null))
			{
				CLog::Log(PK_ERROR, "Failed to create a draw-call");
				return false;
			}

			SRHIDrawCall		&outDrawCall = *_outDrawCall;

			// A single vertex buffer is used for the instanced draw: the texcoords buffer, contains the direction in which vertices should be expanded
			PK_ASSERT(m_TexCoords.Used());
			if (!PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(m_TexCoords.m_Buffer).Valid()))
				return false;

			// Editor only: for debugging purposes, we'll remove that from samples code later
			{
				outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Texcoords] = m_TexCoords.m_Buffer;

#if	(PK_HAS_PARTICLES_SELECTION != 0)
				RHI::PGpuBuffer		bufferIsSelected = ctx.Selection().HasGPUParticlesSelected() ? GetIsSelectedBuffer(ctx.Selection(), *dr) : null;
				if (bufferIsSelected != null)
				{
					// TODO: This can be global
					RHI::SConstantSetLayout	selectionSetLayout(RHI::VertexShaderMask);
					selectionSetLayout.AddConstantsLayout(RHI::SRawBufferDesc("Selections"));

					// Constant set re-created each frame, we don't know if the buffer was re-created
					m_SelectionConstantSet = manager->CreateConstantSet(RHI::SRHIResourceInfos("Selection Constant Set"), selectionSetLayout);
					if (!PK_VERIFY(m_SelectionConstantSet != null) ||
						!PK_VERIFY(m_SelectionConstantSet->SetConstants(bufferIsSelected, 0)))
						return false;
					m_SelectionConstantSet->UpdateConstantValues();
					outDrawCall.m_SelectionConstantSet = m_SelectionConstantSet;
				}
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)

				outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Enabled] = m_SimStreamOffsets_Enableds.m_Buffer;
				outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Position] = m_SimStreamOffsets_Positions.m_Buffer;
				outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Size] = m_SimStreamOffsets_Sizes.Used() ? m_SimStreamOffsets_Sizes.m_Buffer : m_SimStreamOffsets_Size2s.m_Buffer;
				outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Rotation] = m_SimStreamOffsets_Rotations.m_Buffer;
				outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Axis0] = m_SimStreamOffsets_Axis0s.m_Buffer;
				outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Axis1] = m_SimStreamOffsets_Axis1s.m_Buffer;

				if (m_ColorStreamId.Valid())
					outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Color] = m_SimStreamOffsets_AdditionalInputs[m_ColorStreamId].m_Buffer;
			}

			outDrawCall.m_IndexOffset = 0;
			outDrawCall.m_IndexSize = RHI::IndexBuffer16Bit;
			outDrawCall.m_IndexBuffer = m_DrawIndices.m_Buffer;

			outDrawCall.m_IndirectBuffer = m_IndirectDraw.m_Buffer;
			outDrawCall.m_IndirectBufferOffset = m_DrawCallCurrentOffset;
			outDrawCall.m_EstimatedParticleCount = dr->RenderedParticleCount();

			// Now we need to create the indirect buffer to store the draw informations
			// We do not want to read-back the exact particle count from the GPU, so we are creating an
			// indirect buffer and send a copy command to copy the exact particle count to this buffer:
			if (!PK_VERIFY(output.m_CopyCommands.PushBack().Valid()))
				return false;

			SRHICopyCommand		&copyCommand = output.m_CopyCommands.Last();

			// We retrieve the particles info buffer:
			copyCommand.m_SrcBuffer = drStreamSizeBuffer;
			PK_TODO("Retrieve the live count offset from the D3D11_ParticleStream");
			copyCommand.m_SrcOffset = 0;
			copyCommand.m_DstBuffer = m_IndirectDraw.m_Buffer;
			copyCommand.m_DstOffset = m_DrawCallCurrentOffset + PK_MEMBER_OFFSET(RHI::SDrawIndexedIndirectArgs, m_InstanceCount);
			copyCommand.m_SizeToCopy = sizeof(u32);

			m_DrawCallCurrentOffset += sizeof(RHI::SDrawIndexedIndirectArgs);

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
#endif // (PK_PARTICLES_UPDATER_USE_GPU != 0)

//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy_Vertex::_IssueDrawCall_Billboard_CPU(SRenderContext &ctx, const SDrawCallDesc &toEmit, SRHIDrawOutputs &output)
{
	PK_ASSERT(!m_GPUStorage);

	PK_NAMEDSCOPEDPROFILE("CRHIBillboardingBatchPolicy_Vertex::EmitDrawCall Billboard (CPU)");

	// Draw calls are not issued inline here, but later in PK-SampleLib's render loop
	// _CreateDrawCalls outputs a new pushed draw call that will be processed later (in SRHIDrawOutputs)

	// All draw requests are compatible, we can take the first one as reference
	const Drawers::SBillboard_DrawRequest	*dr = static_cast<const Drawers::SBillboard_DrawRequest*>(toEmit.m_DrawRequests.First());
	const u32								shaderOptions = PKSample::Option_VertexPassThrough | _GetVertexBillboardShaderOptions(dr->m_BB);

	if (!_UpdateConstantSetsIFN_CPU(ctx.ApiManager(), toEmit, shaderOptions))
	{
		CLog::Log(PK_ERROR, "Failed to update constant sets for draw call");
		return false;
	}

	SRHIDrawCall	*_outDrawCall = _CreateDrawCall(ctx, toEmit, output, SRHIDrawCall::DrawCall_IndexedInstanced, shaderOptions);
	if (!PK_VERIFY(_outDrawCall != null))
	{
		CLog::Log(PK_ERROR, "Failed to create a draw-call");
		return false;
	}

	SRHIDrawCall	&outDrawCall = *_outDrawCall;

	// A single vertex buffer is used for the instanced draw: the texcoords buffer, contains the direction in which vertices should be expanded
	PK_ASSERT(m_TexCoords.Used());
	if (!PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(m_TexCoords.m_Buffer).Valid()))
		return false;
	outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Texcoords] = m_TexCoords.m_Buffer;

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	if (m_Selections.Used() && (ctx.Selection().HasParticlesSelected() || ctx.Selection().HasRendersSelected()))
		outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_IsParticleSelected] = m_Selections.m_Buffer;
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)

	// Editor only: for debugging purposes, we'll remove that from samples code later
	{
		if (m_DrawRequests.Used())
			outDrawCall.m_UBSemanticsPtr[SRHIDrawCall::UBSemantic_GPUBillboard] = m_DrawRequests.m_Buffer;
	}

	// Setup the draw call description
	{
		outDrawCall.m_IndexOffset = 0;
		outDrawCall.m_VertexCount = m_CapsulesDC ? 6 : 4;
		outDrawCall.m_IndexCount = m_CapsulesDC ? 12 : 6;
		outDrawCall.m_InstanceCount = toEmit.m_TotalParticleCount;
		outDrawCall.m_IndexSize = RHI::IndexBuffer16Bit;
		outDrawCall.m_IndexBuffer = m_DrawIndices.m_Buffer;

		// GPUBillboardPushConstants
		if (!PK_VERIFY(outDrawCall.m_PushConstants.PushBack().Valid()))
			return false;
		u32		&indexOffset = *reinterpret_cast<u32*>(&outDrawCall.m_PushConstants.Last());
		indexOffset = toEmit.m_IndexOffset;

		PK_ASSERT(m_DrawIndices.Used());

		// Editor only: for debugging purposes, we'll remove that from samples code later
		{
			// TODO: We only support a single view right now
			PK_ASSERT(m_Indices.Used() || (!m_PerViewBuffers.Empty() && m_PerViewBuffers[0].m_Indices.Used())); // Currently, we must have one valid indices raw buffer
			outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Indices] = m_Indices.Used() ? m_Indices.m_Buffer : m_PerViewBuffers[0].m_Indices.m_Buffer;

			outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Position] = m_Positions.m_Buffer;
			outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Size] = m_Sizes.Used() ? m_Sizes.m_Buffer : m_Sizes2.m_Buffer;
			outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Rotation] = m_Rotations.m_Buffer;
			outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Axis0] = m_Axis0s.m_Buffer;
			outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Axis1] = m_Axis1s.m_Buffer;

			for (u32 i = 0; i < m_AdditionalFields.Count(); ++i)
			{
				if (!m_AdditionalFields[i].m_Buffer.Used())
					continue;

				// Editor only: for debugging purposes, we'll remove that from samples code later
				if (m_AdditionalFields[i].m_Semantic == SRHIDrawCall::DebugDrawGPUBuffer_Color)
				{
					PK_ASSERT(outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Color] == null);
					outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Color] = m_AdditionalFields[i].m_Buffer.m_Buffer;
				}
			}
		}
	}
	return true;
}

//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy_Vertex::_IssueDrawCall_Triangle_CPU(SRenderContext &ctx, const SDrawCallDesc &toEmit, SRHIDrawOutputs &output)
{
	PK_ASSERT(!m_GPUStorage);

	PK_NAMEDSCOPEDPROFILE("CRHIBillboardingBatchPolicy_Vertex::_IssueDrawCall_Triangle_CPU Triangle (CPU)");

	const u32	shaderOptions = PKSample::Option_VertexPassThrough | PKSample::Option_TriangleVertexBillboarding;

	if (!_UpdateConstantSetsIFN_CPU(ctx.ApiManager(), toEmit, shaderOptions))
	{
		CLog::Log(PK_ERROR, "Failed to update constant sets for draw call");
		return false;
	}

	// TODO : Use DrawCall_Instanced when RHI supports it
	SRHIDrawCall	*_outDrawCall = _CreateDrawCall(ctx, toEmit, output, SRHIDrawCall::DrawCall_IndexedInstanced, shaderOptions);
	if (!PK_VERIFY(_outDrawCall != null))
	{
		CLog::Log(PK_ERROR, "Failed to create a draw-call");
		return false;
	}

	SRHIDrawCall	&outDrawCall = *_outDrawCall;

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	if (m_Selections.Used() && (ctx.Selection().HasParticlesSelected() || ctx.Selection().HasRendersSelected()))
		outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_IsParticleSelected] = m_Selections.m_Buffer;
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)

	if (m_DrawRequests.Used())
		outDrawCall.m_UBSemanticsPtr[SRHIDrawCall::UBSemantic_GPUBillboard] = m_DrawRequests.m_Buffer;

	// Setup the draw call description
	{
		outDrawCall.m_IndexOffset = 0;
		outDrawCall.m_VertexCount = 3;
		outDrawCall.m_IndexCount = 3;
		outDrawCall.m_InstanceCount = toEmit.m_TotalParticleCount;

		// FIXME: Remove this when the RHI supports DrawCall_Instanced
		outDrawCall.m_IndexSize = RHI::IndexBuffer16Bit;
		outDrawCall.m_IndexBuffer = m_DrawIndices.m_Buffer;

		// GPUBillboardPushConstants
		if (!PK_VERIFY(outDrawCall.m_PushConstants.PushBack().Valid()))
			return false;
		u32		&indexOffset = *reinterpret_cast<u32*>(&outDrawCall.m_PushConstants.Last());
		indexOffset = toEmit.m_IndexOffset;

		PK_ASSERT(m_DrawIndices.Used());

		// Editor only: for debugging purposes, we'll remove that from samples code later
		{
			// TODO: We only support a single view right now
			PK_ASSERT(m_Indices.Used() || (!m_PerViewBuffers.Empty() && m_PerViewBuffers[0].m_Indices.Used())); // Currently, we must have one valid indices raw buffer
			outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Indices] = m_Indices.Used() ? m_Indices.m_Buffer : m_PerViewBuffers[0].m_Indices.m_Buffer;

			outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_VertexPosition0] = m_VertexPositions0.m_Buffer;
			outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_VertexPosition1] = m_VertexPositions1.m_Buffer;
			outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_VertexPosition2] = m_VertexPositions2.m_Buffer;

			for (u32 i = 0; i < m_AdditionalFields.Count(); ++i)
			{
				if (!m_AdditionalFields[i].m_Buffer.Used())
					continue;

				// Editor only: for debugging purposes, we'll remove that from samples code later
				if (m_AdditionalFields[i].m_Semantic == SRHIDrawCall::DebugDrawGPUBuffer_Color)
				{
					PK_ASSERT(outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Color] == null);
					outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Color] = m_AdditionalFields[i].m_Buffer.m_Buffer;
				}
			}
		}
	}
	return true;
}

//----------------------------------------------------------------------------

#if (PK_PARTICLES_UPDATER_USE_GPU != 0)
bool	CRHIBillboardingBatchPolicy_Vertex::_IssueDrawCall_Ribbon_GPU(SRenderContext &ctx, const SDrawCallDesc &toEmit, SRHIDrawOutputs &output)
{
	PK_ASSERT(m_GPUStorage);
	PK_ASSERT(m_DrawIndices.Used());

	PK_NAMEDSCOPEDPROFILE("CRHIBillboardingBatchPolicy_Vertex::EmitDrawCall Ribbon (GPU)");

	// Draw calls are not issued inline here, but later in PK-SampleLib's render loop
	// _CreateDrawCalls outputs a new pushed draw call that will be processed later (in SRHIDrawOutputs)
	RHI::PApiManager	manager = ctx.ApiManager();

	// !Currently, there is no batching for gpu particles!
	// So we'll emit one draw call per draw request
	const u32	drCount = toEmit.m_DrawRequests.Count();

	u32	computePerDrawRequest = 0;

	// Create compute sort key constant sets
	{
		// For camera sort, if needed
		if (m_NeedGPUSort)
		{
			// Create compute sort key constant set
			if (drCount != m_ComputeCameraSortKeysConstantSets.Count())
			{
				if (!PK_VERIFY(m_ComputeCameraSortKeysConstantSets.Resize(drCount)))
					return false;
				RHI::SConstantSetLayout	layout;
				PKSample::CreateComputeSortKeysConstantSetLayout(layout, m_SortByCameraDistance, true);
				for (auto &constantSet : m_ComputeCameraSortKeysConstantSets)
					constantSet = manager->CreateConstantSet(RHI::SRHIResourceInfos("Camera Sort Keys Constant Set"), layout);
			}
			computePerDrawRequest += 3 * 16 / 4 + 1; // 3 steps 4 bits radix sort, 16 bits key, plus 1 dispatch to generate the sort key
		}
		// For ribbon sort
		{
			if (drCount != m_ComputeRibbonSortKeysConstantSets.Count())
			{
				if (!PK_VERIFY(m_ComputeRibbonSortKeysConstantSets.Resize(drCount)))
					return false;
				RHI::SConstantSetLayout	layout;
				PKSample::CreateComputeRibbonSortKeysConstantSetLayout(layout);
				for (auto &constantSet : m_ComputeRibbonSortKeysConstantSets)
					constantSet = manager->CreateConstantSet(RHI::SRHIResourceInfos("Ribbon Sort Keys Constant Set"), layout);
			}
			computePerDrawRequest += 3 * 44 / 4 + 1; // 3 steps base 16 radix sort, 44 bits key, plus 1 dispatch to generate the sort key
		}
		if (!PK_VERIFY(output.m_ComputeDispatchs.Reserve(drCount * computePerDrawRequest)))
			return false;
	}

	for (u32 dri = 0; dri < drCount; ++dri)
	{
		PK_ASSERT(toEmit.m_DrawRequests[dri] != null);
		const Drawers::SRibbon_DrawRequest			*dr = static_cast<const Drawers::SRibbon_DrawRequest*>(toEmit.m_DrawRequests[dri]);
		const Drawers::SRibbon_BillboardingRequest	&bbRequest = dr->m_BB;
		const CParticleStreamToRender					*streamToRender = &dr->StreamToRender();

		RHI::PGpuBuffer		drStreamBuffer;
		RHI::PGpuBuffer		drStreamSizeBuffer;

#if	(PK_PARTICLES_UPDATER_USE_D3D11 != 0)
		if (streamToRender->StorageClass() == CParticleStorageManager_D3D11::DefaultStorageClass())
		{
			const CParticleStreamToRender_D3D11	*streamsToRender = static_cast<const CParticleStreamToRender_D3D11*>(streamToRender);
			const SBuffer_D3D11					&streamBuffer = streamsToRender->StreamBuffer();
			const SBuffer_D3D11					&streamSizeBuffer = streamsToRender->StreamSizeBuf();

			if (PK_VERIFY(!streamBuffer.Empty()) && PK_VERIFY(!streamSizeBuffer.Empty())) // We have valid sim data and particle count buffers
			{
				drStreamBuffer = CastD3D11(manager)->D3D11GetGpuBufferFromExisting(RHI::SRHIResourceInfos("Stream Data Buffer"), streamBuffer.m_Buffer, RHI::RawBuffer);
				drStreamSizeBuffer = CastD3D11(manager)->D3D11GetGpuBufferFromExisting(RHI::SRHIResourceInfos("Stream Infos Buffer"), streamSizeBuffer.m_Buffer, RHI::RawVertexBuffer);
			}
		}
#endif
#if	(PK_PARTICLES_UPDATER_USE_D3D12 != 0)
		if (streamToRender->StorageClass() == CParticleStorageManager_D3D12::DefaultStorageClass())
		{
			const CParticleStreamToRender_D3D12	*streamsToRender = static_cast<const CParticleStreamToRender_D3D12*>(streamToRender);
			const SBuffer_D3D12					&streamBuffer = streamsToRender->StreamBuffer();
			const SBuffer_D3D12					&streamSizeBuffer = streamsToRender->StreamSizeBuf();

			if (PK_VERIFY(!streamBuffer.Empty()) && PK_VERIFY(!streamSizeBuffer.Empty())) // We have valid sim data and particle count buffers
			{
				drStreamBuffer = CastD3D12(manager)->D3D12GetGpuBufferFromExisting(RHI::SRHIResourceInfos("Stream Data Buffer"), streamBuffer.m_Resource, RHI::RawBuffer, streamBuffer.m_State);
				drStreamSizeBuffer = CastD3D12(manager)->D3D12GetGpuBufferFromExisting(RHI::SRHIResourceInfos("Stream Infos Buffer"), streamSizeBuffer.m_Resource, RHI::RawVertexBuffer, streamSizeBuffer.m_State);
			}
		}
#endif
#if	(PK_PARTICLES_UPDATER_USE_UNKNOWN2 != 0)
		if (streamToRender->StorageClass() == CParticleStorageManager_UNKNOWN2::DefaultStorageClass())
		{
			const CParticleStreamToRender_UNKNOWN2	*streamsToRender = static_cast<const CParticleStreamToRender_UNKNOWN2*>(streamToRender);
			const SBuffer_UNKNOWN2					&streamBuffer = streamsToRender->StreamBuffer();
			const SBuffer_UNKNOWN2					&streamSizeBuffer = streamsToRender->StreamSizeBuf();

			if (PK_VERIFY(!streamBuffer.Empty()) && PK_VERIFY(!streamSizeBuffer.Empty())) // We have valid sim data and particle count buffers
			{
				drStreamBuffer = CastUNKNOWN2(manager)->UNKNOWN2GetGpuBufferFromExisting(RHI::SRHIResourceInfos("Stream Data Buffer"), streamBuffer.m_Resource, streamBuffer.m_ByteSize, RHI::RawBuffer);
				drStreamSizeBuffer = CastUNKNOWN2(manager)->UNKNOWN2GetGpuBufferFromExisting(RHI::SRHIResourceInfos("Stream Infos Buffer"), streamSizeBuffer.m_Resource, streamBuffer.m_ByteSize, RHI::RawVertexBuffer);
			}
		}
#endif
		if (!PK_VERIFY(drStreamBuffer != null) ||
			!PK_VERIFY(drStreamSizeBuffer != null))
			return false;

		// Ribbon GPU sort
		{
			CRendererCacheInstance_UpdateThread		*renderCacheInstance = static_cast<CRendererCacheInstance_UpdateThread*>(toEmit.m_RendererCaches.First().Get());
			PKSample::PCRendererCacheInstance		rCacheInstance = renderCacheInstance->RenderThread_GetCacheInstance();
			if (rCacheInstance == null)
				return false;

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

				if (!PK_VERIFY(output.m_ComputeDispatchs.PushBack(computeDispatch).Valid()))
					return false;
			}

			// Compute: Sort computes
			{
				m_RibbonGPUSorters[dri].SetInOutBuffers(m_RibbonSortKeys[dri].m_Buffer, m_RibbonSortIndirection[dri].m_Buffer);
				m_RibbonGPUSorters[dri].AppendDispatchs(rCacheInstance, output.m_ComputeDispatchs);
			}
		}

		// Camera GPU sort (must be done after the ribbon GPU sort)
		if (m_NeedGPUSort)
		{
			CRendererCacheInstance_UpdateThread		*renderCacheInstance = static_cast<CRendererCacheInstance_UpdateThread*>(toEmit.m_RendererCaches.First().Get());
			PKSample::PCRendererCacheInstance		rCacheInstance = renderCacheInstance->RenderThread_GetCacheInstance();
			if (rCacheInstance == null)
				return false;

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

				output.m_ComputeDispatchs.PushBack(computeDispatch);
			}

			// Compute: Sort computes
			{
				m_CameraGPUSorters[dri].SetInOutBuffers(m_CameraSortKeys[dri].m_Buffer, m_CameraSortIndirection[dri].m_Buffer);
				m_CameraGPUSorters[dri].AppendDispatchs(rCacheInstance, output.m_ComputeDispatchs);
			}
		}

		// Emit draw call
		{
			const u32				shaderOptions = _GetVertexRibbonShaderOptions(bbRequest);
			const RHI::PGpuBuffer	ribbonSortIndirection = m_RibbonSortIndirection[dri].m_Buffer;
			const RHI::PGpuBuffer	cameraSortIndirection = m_NeedGPUSort ? m_CameraSortIndirection[dri].m_Buffer : null;

			if (!_UpdateConstantSetsIFN_GPU(ctx.ApiManager(), toEmit, drStreamBuffer, cameraSortIndirection, ribbonSortIndirection, shaderOptions))
				return false;

			SRHIDrawCall	*_outDrawCall = _CreateDrawCall(ctx, toEmit, output, SRHIDrawCall::DrawCall_IndexedInstancedIndirect, shaderOptions);
			if (!PK_VERIFY(_outDrawCall != null))
			{
				CLog::Log(PK_ERROR, "Failed to create a draw-call");
				return false;
			}

			SRHIDrawCall		&outDrawCall = *_outDrawCall;

			// A single vertex buffer is used for the instanced draw: the texcoords buffer, contains the direction in which vertices should be expanded
			PK_ASSERT(m_TexCoords.Used());
			if (!PK_VERIFY(outDrawCall.m_VertexBuffers.PushBack(m_TexCoords.m_Buffer).Valid()))
				return false;

			// Editor only: for debugging purposes, we'll remove that from samples code later
			{
				outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Texcoords] = m_TexCoords.m_Buffer;
				// Unused by debug rendering:
				outDrawCall.m_DebugDrawGPUBuffers[SRHIDrawCall::DebugDrawGPUBuffer_Color] = m_ColorStreamId.Valid() ? m_TexCoords.m_Buffer : null;
			}

			outDrawCall.m_IndexOffset = 0;
			outDrawCall.m_IndexSize = RHI::IndexBuffer16Bit;
			outDrawCall.m_IndexBuffer = m_DrawIndices.m_Buffer;

			outDrawCall.m_IndirectBuffer = m_IndirectDraw.m_Buffer;
			outDrawCall.m_IndirectBufferOffset = m_DrawCallCurrentOffset;
			outDrawCall.m_EstimatedParticleCount = dr->RenderedParticleCount();

			if (!PK_VERIFY(output.m_CopyCommands.PushBack().Valid()))
				return false;

			SRHICopyCommand		&copyCommand = output.m_CopyCommands.Last();

			// We retrieve the particles info buffer:
			copyCommand.m_SrcBuffer = drStreamSizeBuffer;
			PK_TODO("Retrieve the live count offset from the D3D12_ParticleStream");
			copyCommand.m_SrcOffset = 0;
			copyCommand.m_DstBuffer = m_IndirectDraw.m_Buffer;
			copyCommand.m_DstOffset = m_DrawCallCurrentOffset + PK_MEMBER_OFFSET(RHI::SDrawIndexedIndirectArgs, m_InstanceCount);
			copyCommand.m_SizeToCopy = sizeof(u32);

			m_DrawCallCurrentOffset += sizeof(RHI::SDrawIndexedIndirectArgs);

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
#endif // (PK_PARTICLES_UPDATER_USE_GPU != 0)

//----------------------------------------------------------------------------

bool	CRHIBillboardingBatchPolicy_Vertex::EmitDrawCall(SRenderContext &ctx, const SDrawCallDesc &toEmit, SRHIDrawOutputs &output)
{
	PK_NAMEDSCOPEDPROFILE("CRHIBillboardingBatchPolicy_Vertex::EmitDrawCall");

	PK_ASSERT(toEmit.m_TotalParticleCount <= m_TotalParticleCount); // <= if slicing is enabled
	PK_ASSERT(toEmit.m_TotalIndexCount <= m_TotalParticleCount);
	PK_ASSERT(!toEmit.m_DrawRequests.Empty());
	PK_ASSERT(toEmit.m_DrawRequests.First() != null);

#if (PK_PARTICLES_UPDATER_USE_GPU != 0)
	const bool	gpuStorage = toEmit.m_DrawRequests.First()->StreamToRender_MainMemory() == null;
	PK_ASSERT(gpuStorage == m_GPUStorage);
#endif // (PK_PARTICLES_UPDATER_USE_GPU != 0)

	const u32	dcCount = output.m_DrawCalls.Count();
	bool		success = false;

	switch (toEmit.m_Renderer)
	{
	case	Renderer_Billboard:
		PK_ASSERT(toEmit.m_TotalVertexCount > 0 && toEmit.m_TotalIndexCount > 0);

#if (PK_PARTICLES_UPDATER_USE_GPU != 0)
		if (gpuStorage)
			success = _IssueDrawCall_Billboard_GPU(ctx, toEmit, output);
		else
#endif // (PK_PARTICLES_UPDATER_USE_GPU != 0)
			success = _IssueDrawCall_Billboard_CPU(ctx, toEmit, output);
		break;
	case	Renderer_Triangle:
		PK_ASSERT(toEmit.m_TotalVertexCount > 0);
		success = _IssueDrawCall_Triangle_CPU(ctx, toEmit, output);
		break;
#if (PK_PARTICLES_UPDATER_USE_GPU != 0)
	case	Renderer_Ribbon:
		PK_ASSERT(gpuStorage);
		success = _IssueDrawCall_Ribbon_GPU(ctx, toEmit, output);
	break;
#endif // (PK_PARTICLES_UPDATER_USE_GPU != 0)
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

void	CRHIBillboardingBatchPolicy_Vertex::ClearBuffers(SRenderContext &ctx)
{
	(void)ctx;
	// This only gets called when a new frame has been collected (so before starting billboarding)
	// Clear here only resets the m_UsedThisFrame flags, it is not a proper clear as we want to avoid vbuffer resizing/allocations:
	// Batches (and their policy) can be reused for various renderers (no matter the layer),
	// so we ensure to bind the correct gpu buffers for draw calls.
	// It is up to you to find a proper gpu buffer pooling solution for particles and how/when to clear them

	m_Indices.Clear();
	m_DrawRequests.Clear();
	m_Positions.Clear();
	m_Sizes.Clear();
	m_Sizes2.Clear();
	m_Rotations.Clear();
	m_Axis0s.Clear();
	m_Axis1s.Clear();

#if (PK_PARTICLES_UPDATER_USE_GPU != 0)
	m_IndirectDraw.Clear();

	m_SimStreamOffsets_Enableds.Clear();
	m_SimStreamOffsets_Positions.Clear();
	m_SimStreamOffsets_Sizes.Clear();
	m_SimStreamOffsets_Size2s.Clear();
	m_SimStreamOffsets_Rotations.Clear();
	m_SimStreamOffsets_Axis0s.Clear();
	m_SimStreamOffsets_Axis1s.Clear();
	m_SimStreamOffsets_ParentIDs.Clear();
	m_SimStreamOffsets_SelfIDs.Clear();

	for (SGpuBuffer &buffer : m_SimStreamOffsets_AdditionalInputs)
		buffer.Clear();

	m_CustomSortKeysOffsets.Clear();
#endif // (PK_PARTICLES_UPDATER_USE_GPU != 0)

	// Additional inputs:
	for (SAdditionalInputs &input : m_AdditionalFields)
		input.m_Buffer.Clear();

	// View dependent inputs:
	for (SPerView &view : m_PerViewBuffers)
		view.m_Indices.Clear();
}

//----------------------------------------------------------------------------

void	CRHIBillboardingBatchPolicy_Vertex::_ClearFrame(u32 activeViewCount)
{
	// Here we only clear the values that are changing from one frame to another.
	// Most of the gpu buffers will stay the same
	m_CapsulesDC = false;
	m_TotalParticleCount = 0;
	m_TotalParticleCount_OverEstimated = 0;
	m_TotalIndexCount = 0;
	m_DrawRequestCount = 0;
	m_IndexSize = 0;
	m_TotalBBox = CAABB::DEGENERATED;
	PK_VERIFY(m_PerViewBuffers.Resize(activeViewCount));

#if (PK_PARTICLES_UPDATER_USE_GPU != 0)
	m_MappedIndirectBuffer = null;
	m_MappedSimStreamOffsets_Enableds = null;
	m_MappedSimStreamOffsets_Positions = null;
	m_MappedSimStreamOffsets_Sizes = null;
	m_MappedSimStreamOffsets_Size2s = null;
	m_MappedSimStreamOffsets_Rotations = null;
	m_MappedSimStreamOffsets_Axis0s = null;
	m_MappedSimStreamOffsets_Axis1s = null;
	for (u32 i = 0; i < m_MappedSimStreamOffsets_AdditionalInputs.Count(); ++i)
		m_MappedSimStreamOffsets_AdditionalInputs[i] = null;
	m_MappedSimStreamOffsets_ParentIDs = null;
	m_MappedSimStreamOffsets_SelfIDs = null;
	m_MappedCustomSortKeysOffsets = null;
#endif // (PK_PARTICLES_UPDATER_USE_GPU != 0)

	m_GPUBuffers.Clear();
	PK_VERIFY(m_GPUBuffers.Reserve(0x10)); // Reserved, memory not cleared
}

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
