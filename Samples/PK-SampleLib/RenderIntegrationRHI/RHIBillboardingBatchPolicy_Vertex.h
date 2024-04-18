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

#include "PK-SampleLib/PKSample.h"

#include <pk_render_helpers/include/frame_collector/rh_billboarding_batch_interface.h>
#include <pk_render_helpers/include/batches/rh_particle_batch_policy_data.h>

#include "PK-SampleLib/RenderIntegrationRHI/RendererCache.h"
#include "PK-SampleLib/RenderIntegrationRHI/RHITypePolicy.h"
#if (PK_PARTICLES_UPDATER_USE_GPU != 0)
#include "PK-SampleLib/RenderIntegrationRHI/RHIGPUSorter.h"
#endif // (PK_PARTICLES_UPDATER_USE_GPU != 0)

#include "RHICustomTasks.h"

#include <pk_rhi/include/FwdInterfaces.h>

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

class	CRHIParticleRenderDataFactory;

class	CRHIBillboardingBatchPolicy_Vertex
{
public:
	CRHIBillboardingBatchPolicy_Vertex();
	~CRHIBillboardingBatchPolicy_Vertex();

	// Return true if this draw request can be rendered (this gets called per batches when the frame collector is in BeginCollectingDrawCalls() and EndCollectingDrawCalls())
	// Return false if you want to discard this draw request and handle it later
	// request and renderer cache are the firsts from their batch
	static bool		CanRender(const Drawers::SBillboard_DrawRequest *request, const PRendererCacheBase &rendererCache, SRenderContext &ctx);
	static bool		CanRender(const Drawers::SRibbon_DrawRequest *request, const PRendererCacheBase &rendererCache, SRenderContext &ctx);
	static bool		CanRender(const Drawers::SMesh_DrawRequest *request, const PRendererCacheBase &rendererCache, SRenderContext &ctx) { (void)request; (void)rendererCache; (void)ctx; PK_ASSERT_NOT_REACHED(); return false; }
	static bool		CanRender(const Drawers::STriangle_DrawRequest *request, const PRendererCacheBase &rendererCache, SRenderContext & ctx);
	static bool		CanRender(const Drawers::SLight_DrawRequest *request, const PRendererCacheBase &rendererCache, SRenderContext &ctx) { (void)request; (void)rendererCache; (void)ctx; PK_ASSERT_NOT_REACHED(); return false; }
	static bool		CanRender(const Drawers::SSound_DrawRequest *request, const PRendererCacheBase &rendererCache, SRenderContext &ctx) { (void)request; (void)rendererCache; (void)ctx; PK_ASSERT_NOT_REACHED(); return false; }

	// Whether this billboarding batch policy can be re-used by incompatible renderer caches (see CHM documentation for more detail)
	// For example: light renderer billboarding batch policies could return true
	static bool		IsStateless() { return false; }

	// Return false when this billboarding batch policy should be destroyed (ie. nothing was drawn after 10 ticks)
	bool			Tick(SRenderContext &ctx, const TMemoryView<SSceneView> &views);

	// Called via BeginCollectingDrawCalls() for all render data batches
	bool			AllocBuffers(SRenderContext &ctx, const SBuffersToAlloc &allocBuffers, const TMemoryView<SSceneView> &views, ERendererClass rendererType);

	// Called to map necessary buffers for tasks
	// Only map buffers necessary buffers descripted in SGeneratedInputs
	bool			MapBuffers(SRenderContext &ctx, const TMemoryView<SSceneView> &views, SBillboardBatchJobs *billboardBatch, const SGeneratedInputs &toMap) { (void)ctx; (void)views; (void)billboardBatch; (void)toMap; PK_ASSERT_NOT_REACHED(); return false; }
	bool			MapBuffers(SRenderContext &ctx, const TMemoryView<SSceneView> &views, SGPUBillboardBatchJobs *billboardVertexBatch, const SGeneratedInputs &toMap);
	bool			MapBuffers(SRenderContext &ctx, const TMemoryView<SSceneView> &views, SGPURibbonBatchJobs *billboardBatch, const SGeneratedInputs &toMap);
	bool			MapBuffers(SRenderContext &ctx, const TMemoryView<SSceneView> &views, SRibbonBatchJobs *billboardBatch, const SGeneratedInputs &toMap) { (void)ctx; (void)views; (void)billboardBatch; (void)toMap; PK_ASSERT_NOT_REACHED(); return false; }
	bool			MapBuffers(SRenderContext &ctx, const TMemoryView<SSceneView> &views, SMeshBatchJobs *billboardBatch, const SGeneratedInputs &toMap) { (void)ctx; (void)views; (void)billboardBatch; (void)toMap; PK_ASSERT_NOT_REACHED(); return false; }
	bool			MapBuffers(SRenderContext &ctx, const TMemoryView<SSceneView> &views, STriangleBatchJobs *billboardBatch, const SGeneratedInputs &toMap) { (void)ctx; (void)views; (void)billboardBatch; (void)toMap; PK_ASSERT_NOT_REACHED(); return false; }
	bool			MapBuffers(SRenderContext &ctx, const TMemoryView<SSceneView> &views, SGPUTriangleBatchJobs *billboardBatch, const SGeneratedInputs &toMap);

	bool			LaunchCustomTasks(SRenderContext &ctx, const TMemoryView<const Drawers::SBillboard_DrawRequest * const> &drawRequests, Drawers::CCopyStream_CPU *billboardVertexBatch);
	bool			LaunchCustomTasks(SRenderContext &ctx, const TMemoryView<const Drawers::SBillboard_DrawRequest * const> &drawRequests, Drawers::CBillboard_CPU *billboardBatch) { (void)ctx; (void)drawRequests; (void)billboardBatch; PK_ASSERT_NOT_REACHED(); return false; }
	bool			LaunchCustomTasks(SRenderContext &ctx, const TMemoryView<const Drawers::SRibbon_DrawRequest * const> &drawRequests, Drawers::CCopyStream_CPU *ribbonVertexBatch);
	bool			LaunchCustomTasks(SRenderContext &ctx, const TMemoryView<const Drawers::SRibbon_DrawRequest * const> &drawRequests, Drawers::CRibbon_CPU *ribbonBatch) { (void)ctx; (void)drawRequests; (void)ribbonBatch; PK_ASSERT_NOT_REACHED(); return false; }
	bool			LaunchCustomTasks(SRenderContext &ctx, const TMemoryView<const Drawers::SMesh_DrawRequest * const> &drawRequests, Drawers::CMesh_CPU *meshBatch) { (void)ctx; (void)drawRequests; (void)meshBatch; PK_ASSERT_NOT_REACHED(); return false; }
	bool			LaunchCustomTasks(SRenderContext &ctx, const TMemoryView<const Drawers::STriangle_DrawRequest * const> &drawRequests, Drawers::CTriangle_CPU *triangleBatch) { (void)ctx; (void)drawRequests; (void)triangleBatch; PK_ASSERT_NOT_REACHED(); return false; }
	bool			LaunchCustomTasks(SRenderContext &ctx, const TMemoryView<const Drawers::STriangle_DrawRequest * const> &drawRequests, Drawers::CCopyStream_CPU *triangleVertexBatch);
	bool			LaunchCustomTasks(SRenderContext &ctx, const TMemoryView<const Drawers::SLight_DrawRequest * const> &drawRequests, Drawers::CBillboard_CPU *batch) { (void)ctx; (void)drawRequests; (void)batch; PK_ASSERT_NOT_REACHED(); return false; }
	bool			LaunchCustomTasks(SRenderContext &ctx, const TMemoryView<const Drawers::SSound_DrawRequest * const> &drawRequests, Drawers::CBillboard_CPU *batch) { (void)ctx; (void)drawRequests; (void)batch; PK_ASSERT_NOT_REACHED(); return false; }

	bool			WaitForCustomTasks(SRenderContext &ctx) { (void)ctx; return true; }

	bool			UnmapBuffers(SRenderContext &ctx);
	void			ClearBuffers(SRenderContext &ctx);

	bool			AreBillboardingBatchable(const PCRendererCacheBase &firstCache, const PCRendererCacheBase &secondCache) const;

	// By default, draw calls are submit back to front based on their bounding boxes (see EDrawCallSortMethod), for each view
	// You can setup the frame collector to sort draw calls differently (ie. by sorting sliced draw calls or you can also disable sorting entirely)
	bool			EmitDrawCall(SRenderContext &ctx, const SDrawCallDesc &toEmit, SRHIDrawOutputs &output);

private:
	bool			_AllocateVertexBBConstantSet(const RHI::PApiManager &manager);
	bool			_AllocateBuffers_Main(const RHI::PApiManager &manager, const SBuffersToAlloc &allocBuffers);
	bool			_AllocateBuffers_ViewDependent(const RHI::PApiManager &manager, const SGeneratedInputs &toGenerate);
	bool			_AllocateBuffers_AdditionalInputs(const RHI::PApiManager &manager, const SGeneratedInputs &toGenerate);

	u32				_GetVertexBillboardShaderOptions(const Drawers::SBillboard_BillboardingRequest &bbRequest);
	u32				_GetVertexRibbonShaderOptions(const Drawers::SRibbon_BillboardingRequest &bbRequest);
	bool			_CreateOrResizeGpuBufferIf(const RHI::SRHIResourceInfos &infos, bool condition, const RHI::PApiManager &manager, SGpuBuffer &buffer, RHI::EBufferType type, u32 sizeToAlloc, u32 requiredSize, bool simDataField);
	void			_ClearFrame(u32 activeViewCount = 0); // Reset the batch for a new frame

	SRHIDrawCall	*_CreateDrawCall(SRenderContext &ctx, const SDrawCallDesc &toEmit, SRHIDrawOutputs &output, SRHIDrawCall::EDrawCallType drawCallType, u32 shaderOptions);
	bool			_UpdateConstantSetsIFN_CPU(const RHI::PApiManager &manager, const SDrawCallDesc &toEmit, u32 shaderOptions);
	bool			_IssueDrawCall_Billboard_CPU(SRenderContext &ctx, const SDrawCallDesc &toEmit, SRHIDrawOutputs &output);

#if (PK_PARTICLES_UPDATER_USE_GPU != 0)
	bool			_AllocateBuffers_GPU(const RHI::PApiManager &manager, const SBuffersToAlloc &allocBuffers);
	bool			_UpdateConstantSetsIFN_GPU(const RHI::PApiManager &manager, const SDrawCallDesc &toEmit, const RHI::PGpuBuffer &drStreamBuffer, const RHI::PGpuBuffer &drCameraSortIndirection, const RHI::PGpuBuffer &drRibbonSortIndirection, u32 shaderOptions);
	bool			_IssueDrawCall_Billboard_GPU(SRenderContext &ctx, const SDrawCallDesc &toEmit, SRHIDrawOutputs &output);
	bool			_IssueDrawCall_Ribbon_GPU(SRenderContext &ctx, const SDrawCallDesc &toEmit, SRHIDrawOutputs &output);
	bool			_WriteGPUStreamsOffsets(const TMemoryView<const Drawers::SBase_DrawRequest * const> & drawRequests);
#endif // (PK_PARTICLES_UPDATER_USE_GPU != 0)

	bool			_IssueDrawCall_Triangle_CPU(SRenderContext &ctx, const SDrawCallDesc &toEmit, SRHIDrawOutputs &output);

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	// Job to set a boolean as vertex input if the particle is selected:
	CCopyStream_Exec_WireframeDiscard		m_GeomBillboardCustomParticleSelectTask;
	SGpuBuffer								m_Selections;
	RHI::PConstantSet						m_SelectionConstantSet;
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)

	//----------------------------------------------------------------------------
	// RHI buffers data:
	//----------------------------------------------------------------------------

	ERendererClass		m_RendererType;

	bool				m_Initialized;
	bool				m_CapsulesDC;
	bool				m_TubesDC;
	bool				m_MultiPlanesDC;
	bool				m_GpuBufferResizedOrCreated;
	bool				m_GPUStorage;
	u32					m_ParticleQuadCount;
#if	(PK_HAS_PARTICLES_SELECTION != 0)
	bool				m_SelectionsResizedOrCreated;
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)
#if (PK_PARTICLES_UPDATER_USE_GPU != 0)
	CGuid				m_ColorStreamId;
	bool				m_NeedGPUSort;
	bool				m_SortByCameraDistance;

	// Camera sort
	TArray<CGPUSorter>	m_CameraGPUSorters;

	// Ribbon sort
	TArray<CGPUSorter>	m_RibbonGPUSorters;

#endif // (PK_PARTICLES_UPDATER_USE_GPU != 0)

	u32					m_UnusedCounter;
	u32					m_TotalParticleCount;
	u32					m_TotalIndexCount;
	u32					m_TotalParticleCount_OverEstimated;
	u32					m_DrawRequestCount;
	u32					m_IndexSize;
	u32					m_ShaderOptions;

	CAABB				m_TotalBBox;

	struct	SPerView
	{
		u32			m_ViewIdx;
		SGpuBuffer	m_Indices;
	};

	// View dependent buffers:
	TArray<SPerView>		m_PerViewBuffers;

	SGpuBuffer				m_DrawIndices;	// DrawIndexed indices
	SGpuBuffer				m_TexCoords;	// DrawIndexed texcoords
	SGpuBuffer				m_DrawRequests;	// Draw requests buffer, for draw calls batching
	SGpuBuffer				m_Indices;		// Generated indices for sorted particles

	// Collection of particle data of all active draw requests
	SGpuBuffer				m_Positions;
	SGpuBuffer				m_Sizes;
	SGpuBuffer				m_Sizes2;
	SGpuBuffer				m_Rotations;
	SGpuBuffer				m_Axis0s;
	SGpuBuffer				m_Axis1s;

	// for triangles
	SGpuBuffer				m_VertexPositions0;
	SGpuBuffer				m_VertexPositions1;
	SGpuBuffer				m_VertexPositions2;

	RHI::PConstantSet		m_VertexBBSimDataConstantSet; // SRVs: Positions/Sizes/.. for CPU particles, GPUSimData for GPU particles

#if (PK_PARTICLES_UPDATER_USE_GPU != 0)
	// Indirect draw buffer for GPU simulated particles
	volatile RHI::SDrawIndexedIndirectArgs	*m_MappedIndirectBuffer;
	SGpuBuffer								m_IndirectDraw;
	u32										m_DrawCallCurrentOffset;
	RHI::PConstantSet						m_VertexBBOffsetsConstantSet; // (PositionsOffset, SizesOffset, ..)

	// Offsets for accessing each draw request gpu sim data streams
	SGpuBuffer								m_SimStreamOffsets_Enableds;
	SGpuBuffer								m_SimStreamOffsets_Positions;
	SGpuBuffer								m_SimStreamOffsets_Sizes;
	SGpuBuffer								m_SimStreamOffsets_Size2s;
	SGpuBuffer								m_SimStreamOffsets_Rotations;
	SGpuBuffer								m_SimStreamOffsets_Axis0s;
	SGpuBuffer								m_SimStreamOffsets_Axis1s;
	TArray<SGpuBuffer>						m_SimStreamOffsets_AdditionalInputs;
	SGpuBuffer								m_SimStreamOffsets_ParentIDs;
	SGpuBuffer								m_SimStreamOffsets_SelfIDs;

	// Mapped data
	volatile u32							*m_MappedSimStreamOffsets_Enableds;
	volatile u32							*m_MappedSimStreamOffsets_Positions;
	volatile u32							*m_MappedSimStreamOffsets_Sizes;
	volatile u32							*m_MappedSimStreamOffsets_Size2s;
	volatile u32							*m_MappedSimStreamOffsets_Rotations;
	volatile u32							*m_MappedSimStreamOffsets_Axis0s;
	volatile u32							*m_MappedSimStreamOffsets_Axis1s;
	TArray<volatile u32*>					m_MappedSimStreamOffsets_AdditionalInputs;
	volatile u32							*m_MappedSimStreamOffsets_ParentIDs;
	volatile u32							*m_MappedSimStreamOffsets_SelfIDs;
	volatile u32							*m_MappedCustomSortKeysOffsets;

	TArray<SGpuBuffer>						m_CameraSortIndirection;
	TArray<SGpuBuffer>						m_CameraSortKeys;

	TArray<SGpuBuffer>						m_RibbonSortIndirection;
	TArray<SGpuBuffer>						m_RibbonSortKeys;

	SGpuBuffer								m_CustomSortKeysOffsets;

	TArray<RHI::PConstantSet>				m_ComputeCameraSortKeysConstantSets;
	TArray<RHI::PConstantSet>				m_ComputeRibbonSortKeysConstantSets;

#endif // (PK_PARTICLES_UPDATER_USE_GPU != 0)

	TArray<RHI::PGpuBuffer>					m_GPUBuffers; // Used gpu buffers this frame
	TArray<SAdditionalInputs>				m_AdditionalFields;

	// We need those temporary arrays to map data:
	TArray<Drawers::SCopyFieldDesc>			m_MappedAdditionalFields;
};

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
