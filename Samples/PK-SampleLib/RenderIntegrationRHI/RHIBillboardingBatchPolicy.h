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

#include "RHICustomTasks.h"

#include <pk_rhi/include/FwdInterfaces.h>

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

class	CRHIParticleRenderDataFactory;

class	CRHIBillboardingBatchPolicy
{
public:
	CRHIBillboardingBatchPolicy();
	~CRHIBillboardingBatchPolicy();

	// Return true if this draw request can be rendered (this gets called per batches when the frame collector is in BeginCollectingDrawCalls() and EndCollectingDrawCalls())
	// Return false if you want to discard this draw request and handle it later
	// request and renderer cache are the firsts from their batch
	static bool		CanRender(const Drawers::SBillboard_DrawRequest *request, const PRendererCacheBase &rendererCache, SRenderContext &ctx);
	static bool		CanRender(const Drawers::SRibbon_DrawRequest *request, const PRendererCacheBase &rendererCache, SRenderContext &ctx);
	static bool		CanRender(const Drawers::SMesh_DrawRequest *request, const PRendererCacheBase &rendererCache, SRenderContext &ctx);
	static bool		CanRender(const Drawers::SDecal_DrawRequest *request, const PRendererCacheBase &rendererCache, SRenderContext &ctx);
	static bool		CanRender(const Drawers::STriangle_DrawRequest *request, const PRendererCacheBase &rendererCache, SRenderContext &ctx);
	static bool		CanRender(const Drawers::SLight_DrawRequest *request, const PRendererCacheBase &rendererCache, SRenderContext &ctx);
	static bool		CanRender(const Drawers::SSound_DrawRequest *request, const PRendererCacheBase &rendererCache, SRenderContext &ctx);

	// Whether this billboarding batch policy can be re-used by incompatible renderer caches (see CHM documentation for more detail)
	// For example: light renderer billboarding batch policies could return true
	static bool		IsStateless() { return false; }

	// Return false when this billboarding batch policy should be destroyed (ie. nothing was drawn after 10 ticks)
	bool		Tick(SRenderContext &ctx, const TMemoryView<SSceneView> &views);

	// Called via BeginCollectingDrawCalls() for all render data batches
	bool		AllocBuffers(SRenderContext &ctx, const SBuffersToAlloc &allocBuffers, const TMemoryView<SSceneView> &views, ERendererClass rendererType);

	// Called to map necessary buffers for tasks
	// Only map buffers necessary buffers descripted in SGeneratedInputs
	bool		MapBuffers(SRenderContext &ctx, const TMemoryView<SSceneView> &views, SBillboardBatchJobs *billboardBatch, const SGeneratedInputs &toMap);
	bool		MapBuffers(SRenderContext &ctx, const TMemoryView<SSceneView> &views, SGPUBillboardBatchJobs *billboardBatch, const SGeneratedInputs &toMap);
	bool		MapBuffers(SRenderContext &ctx, const TMemoryView<SSceneView> &views, SRibbonBatchJobs *billboardBatch, const SGeneratedInputs &toMap);
	bool		MapBuffers(SRenderContext &ctx, const TMemoryView<SSceneView> &views, SMeshBatchJobs *billboardBatch, const SGeneratedInputs &toMap);
	bool		MapBuffers(SRenderContext &ctx, const TMemoryView<SSceneView> &views, SDecalBatchJobs *billboardBatch, const SGeneratedInputs &toMap);
	bool		MapBuffers(SRenderContext &ctx, const TMemoryView<SSceneView> &views, STriangleBatchJobs *billboardBatch, const SGeneratedInputs &toMap);
	bool		MapBuffers(SRenderContext &ctx, const TMemoryView<SSceneView> &views, SGPUTriangleBatchJobs *billboardBatch, const SGeneratedInputs &toMap);
	bool		MapBuffers(SRenderContext &ctx, const TMemoryView<SSceneView> &views, SGPURibbonBatchJobs *RibbonBatch, const SGeneratedInputs &toMap);

	bool		LaunchCustomTasks(SRenderContext &ctx, const TMemoryView<const Drawers::SBillboard_DrawRequest * const> &drawRequests, Drawers::CCopyStream_CPU *geomBillboardBatch);
	bool		LaunchCustomTasks(SRenderContext &ctx, const TMemoryView<const Drawers::SBillboard_DrawRequest * const> &drawRequests, Drawers::CBillboard_CPU *billboardBatch);
	bool		LaunchCustomTasks(SRenderContext &ctx, const TMemoryView<const Drawers::SRibbon_DrawRequest * const> &drawRequests, Drawers::CCopyStream_CPU *geomBillboardBatch);
	bool		LaunchCustomTasks(SRenderContext &ctx, const TMemoryView<const Drawers::SRibbon_DrawRequest * const> &drawRequests, Drawers::CRibbon_CPU *ribbonBatch);
	bool		LaunchCustomTasks(SRenderContext &ctx, const TMemoryView<const Drawers::SMesh_DrawRequest * const> &drawRequests, Drawers::CMesh_CPU *meshBatch);
	bool		LaunchCustomTasks(SRenderContext &ctx, const TMemoryView<const Drawers::SDecal_DrawRequest * const> &drawRequests, Drawers::CDecal_CPU *decalBatch);
	bool		LaunchCustomTasks(SRenderContext &ctx, const TMemoryView<const Drawers::SLight_DrawRequest * const> &drawRequests, Drawers::CBillboard_CPU *batch);
	bool		LaunchCustomTasks(SRenderContext &ctx, const TMemoryView<const Drawers::SSound_DrawRequest * const> &drawRequests, Drawers::CBillboard_CPU *batch);
	bool		LaunchCustomTasks(SRenderContext &ctx, const TMemoryView<const Drawers::STriangle_DrawRequest * const> &drawRequests, Drawers::CTriangle_CPU *batch);
	bool		LaunchCustomTasks(SRenderContext &ctx, const TMemoryView<const Drawers::STriangle_DrawRequest * const> &drawRequests, Drawers::CCopyStream_CPU *batch);

	bool		WaitForCustomTasks(SRenderContext &ctx);

	bool		UnmapBuffers(SRenderContext &ctx);
	void		ClearBuffers(SRenderContext &ctx);

	bool		AreBillboardingBatchable(const PCRendererCacheBase &firstCache, const PCRendererCacheBase &secondCache) const;

	// By default, draw calls are submit back to front based on their bounding boxes (see EDrawCallSortMethod), for each view
	// You can setup the frame collector to sort draw calls differently (ie. by sorting sliced draw calls or you can also disable sorting entirely)
	bool		EmitDrawCall(SRenderContext &ctx, const SDrawCallDesc &toEmit, SRHIDrawOutputs &output);

private:
	bool				_AllocateBuffers_Main(const RHI::PApiManager &manager, const SBuffersToAlloc &allocBuffers);
	bool				_AllocateBuffers_ViewDependent(const RHI::PApiManager &manager, const SGeneratedInputs &toGenerate);
	bool				_AllocateBuffers_AdditionalInputs(const RHI::PApiManager &manager, const SGeneratedInputs &toGenerate);
	bool				_AllocateBuffers_GeomShaderBillboarding(const RHI::PApiManager &manager, u32 viewIndependentInputs);
	bool				_AllocateBuffers_GPU(const RHI::PApiManager &manager, const SBuffersToAlloc &allocBuffers);
	bool				_AllocateBuffers_Lights(const RHI::PApiManager &manager, const SGeneratedInputs &toGenerate);

	u32					_GetGeomBillboardShaderOptions(const Drawers::SBillboard_BillboardingRequest &bbRequest);
	bool				_CreateOrResizeGpuBufferIf(const RHI::SRHIResourceInfos &infos, bool condition, const RHI::PApiManager &manager, SGpuBuffer &buffer, RHI::EBufferType type, u32 sizeToAlloc, u32 requiredSize);
	RHI::PGpuBuffer		_RetrieveStorageBuffer(const RHI::PApiManager &manager, const CParticleStreamToRender *streams, CGuid streamIdx, u32 &storageOffset);
	RHI::PGpuBuffer		_RetrieveParticleInfoBuffer(const RHI::PApiManager &manager, const CParticleStreamToRender *streams);
	void				_ClearFrame(u32 activeViewCount = 0); // Reset the batch for a new frame

	SRHIDrawCall		*_CreateDrawCall(const SDrawCallDesc &toEmit, SRHIDrawOutputs &output, SRHIDrawCall::EDrawCallType drawCallType, u32 baseShaderOptions);

	bool				_IssueDrawCall_Billboard_GPU(SRenderContext &ctx, const SDrawCallDesc &toEmit, SRHIDrawOutputs &output);
	bool				_IssueDrawCall_Billboard_CPU(SRenderContext &ctx, const SDrawCallDesc &toEmit, SRHIDrawOutputs &output);
	bool				_IssueDrawCall_Ribbon(SRenderContext &ctx, const SDrawCallDesc &toEmit, SRHIDrawOutputs &output);
	bool				_IssueDrawCall_Mesh_CPU(SRenderContext &ctx, const SDrawCallDesc &toEmit, SRHIDrawOutputs &output);
#if (PK_PARTICLES_UPDATER_USE_GPU != 0)
	u32					_SubmeshIDToLOD(CRendererCacheInstance_UpdateThread &refCacheInstance, u32 lodCount, u32 iSubMesh);
	bool				_IssueDrawCall_Mesh_GPU(SRenderContext &ctx, const SDrawCallDesc &toEmit, SRHIDrawOutputs &output);
#endif // (PK_PARTICLES_UPDATER_USE_GPU != 0)
	bool				_IssueDrawCall_Decal(SRenderContext &ctx, const SDrawCallDesc &toEmit, SRHIDrawOutputs &output);
	bool				_IssueDrawCall_Triangle(SRenderContext &ctx, const SDrawCallDesc &toEmit, SRHIDrawOutputs &output);
	bool				_IssueDrawCall_Light(SRenderContext &ctx, const SDrawCallDesc &toEmit, SRHIDrawOutputs &output);
	bool				_IssueDrawCall_Sound(SRenderContext &ctx, const SDrawCallDesc &toEmit, SRHIDrawOutputs &output);

	bool				_SetupGeomBillboardVertexBuffers(SRHIDrawCall &outDrawCall);
	bool				_SetupCommonBillboardVertexBuffers(SRHIDrawCall &outDrawCall, bool hasAtlas);
	bool				_SetupBillboardDrawCall(SRenderContext &ctx, const SDrawCallDesc &toEmit, SRHIDrawCall &outDrawCall);

	bool				_IsAdditionalInputIgnored(const SRendererFeatureFieldDefinition &input);

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	// Job to set a boolean as vertex input if the particle is selected:
	CBillboard_Exec_WireframeDiscard		m_BillboardCustomParticleSelectTask;
	CRibbon_Exec_WireframeDiscard			m_RibbonCustomParticleSelectTask;
	CCopyStream_Exec_WireframeDiscard		m_CopyStreamCustomParticleSelectTask;
	CMesh_Exec_WireframeDiscard				m_MeshCustomParticleSelectTask;
	CTriangle_Exec_WireframeDiscard			m_TriangleCustomParticleSelectTask;
	SGpuBuffer								m_IsParticleSelected;
	RHI::PConstantSet						m_GPUSelectionConstantSet;
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)

	//----------------------------------------------------------------------------
	// RHI buffers data:
	//----------------------------------------------------------------------------

	bool						m_GPUStorage;
	ERendererClass				m_RendererType;

	u32							m_UnusedCounter;
	u32							m_DrawRequestCount;
	u32							m_TotalParticleCount;
	u32							m_TotalParticleCount_OverEstimated;
	u32							m_TotalVertexCount;
	u32							m_TotalVertexCount_OverEstimated;
	u32							m_TotalIndexCount;
	u32							m_TotalIndexCount_OverEstimated;
	u32							m_IndexSize;

	CAABB						m_TotalBBox;

	TMemoryView<const u32>		m_PerMeshParticleCount;
	TMemoryView<const u32>		m_PerMeshBufferOffset;
	bool						m_HasMeshIDs;
	bool						m_HasMeshLODs;
	u32							m_MeshCount;
	TArray<u32>					m_PerMeshIndexCount;

	struct	SPerView
	{
		u32					m_ViewIdx;

		SGpuBuffer			m_Indices;
		SGpuBuffer			m_Positions;
		SGpuBuffer			m_Normals;
		SGpuBuffer			m_Tangents;
		SGpuBuffer			m_UVFactors;
	};

	// View dependent buffers:
	TArray<SPerView>		m_PerViewBuffers;

	SGpuBuffer				m_Indices;

	SGpuBuffer				m_Positions;
	SGpuBuffer				m_Normals;
	SGpuBuffer				m_Tangents;

	SGpuBuffer				m_TexCoords0;
	SGpuBuffer				m_TexCoords1;
	SGpuBuffer				m_AtlasIDs;

	SGpuBuffer				m_UVFactors;
	SGpuBuffer				m_UVRemap;

	// Meshes and Decals:
	SGpuBuffer				m_Matrices;

	// For GPU meshes :
	SGpuBuffer				m_Indirection;

	SGpuBuffer				m_IndirectionOffsets;
	volatile u32			*m_MappedIndirectionOffsets;

	SGpuBuffer				m_MatricesOffsets;
	volatile u32			*m_MappedMatricesOffsets;

	// Constant buffer data used by GPU mesh draw calls, holds mesh LOD infos
	SGpuBuffer				m_LODsConstantBuffer;

	SGpuBuffer				m_SimStreamOffsets_Positions;
	SGpuBuffer				m_SimStreamOffsets_Scales;
	SGpuBuffer				m_SimStreamOffsets_Orientations;
	SGpuBuffer				m_SimStreamOffsets_Enableds;
	SGpuBuffer				m_SimStreamOffsets_MeshIDs;
	SGpuBuffer				m_SimStreamOffsets_MeshLODs;
	TArray<SGpuBuffer>		m_SimStreamOffsets_AdditionalInputs;
	bool					m_HasColorStream;

	volatile u32			*m_MappedSimStreamOffsets_Positions;
	volatile u32			*m_MappedSimStreamOffsets_Scales;
	volatile u32			*m_MappedSimStreamOffsets_Orientations;
	volatile u32			*m_MappedSimStreamOffsets_Enableds;
	volatile u32			*m_MappedSimStreamOffsets_MeshIDs;
	volatile u32			*m_MappedSimStreamOffsets_LODs;
	TArray<volatile u32*>	m_MappedSimStreamOffsets_AdditionalInputs;

	RHI::PConstantSet		m_SimDataConstantSet;
	RHI::PConstantSet		m_OffsetsConstantSet;

	// For decals, we can generate the invert matrice:
	SGpuBuffer				m_InvMatrices;

	// Geom billboard buffers:
	SGpuBuffer				m_GeomConstants;
	SGpuBuffer				m_GeomPositions;
	SGpuBuffer				m_GeomSizes;
	SGpuBuffer				m_GeomSizes2;
	SGpuBuffer				m_GeomRotations;
	SGpuBuffer				m_GeomAxis0;
	SGpuBuffer				m_GeomAxis1;

	TArray<SAdditionalInputs>	m_AdditionalFields;

	SGpuBuffer					m_LightsPositions;

	// Indirect draw buffer
	RHI::SDrawIndirectArgs					*m_MappedIndirectBuffer;
	SGpuBuffer								m_IndirectDraw; // We might need multiple indirect draws for mesh atlases.
	volatile RHI::SDrawIndexedIndirectArgs	*m_MappedIndexedIndirectBuffer;
	u32										m_DrawCallCurrentOffset;

	// We need those temporary arrays to map data:
	TArray<Drawers::SCopyFieldDesc>				m_MappedAdditionalFields;

	// Position of the main camera (used for sound to define the position/rotation of the listener)
	CFloat4x4									m_MainViewMatrix;

	// Job used to copy additional fields once per particle (used for lights and decals):
	Drawers::CCopyStream_Exec_AdditionalField	m_CopyAdditionalFieldsTask;
	Drawers::CCopyStream_CPU					m_CopyTasks;

	// Here we use a custom task to copy the positions of the lights
	// We could have just used the CCopyStream_Exec_AdditionalField as all the light fields are just copied:
	class	CCopyStream_Exec_LightsPositions
	{
	public:
		TMemoryView<CFloat3>								m_Positions;

		void	Clear() { Mem::Reinit(*this); }
		void	operator()(const Drawers::SCopyStream_ExecPage &execPage);
	};

	CCopyStream_Exec_LightsPositions			m_LightDataCopyTask;
};

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
