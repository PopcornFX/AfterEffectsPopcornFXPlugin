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

#include <pk_render_helpers/include/batches/rh_particle_batch_policy_data.h>

#include <pk_render_helpers/include/batch_jobs/rh_batch_jobs_billboard_gpu.h>
#include <pk_render_helpers/include/batch_jobs/rh_batch_jobs_ribbon_gpu.h>
#include <pk_render_helpers/include/batch_jobs/rh_batch_jobs_mesh_gpu.h>
#include <pk_render_helpers/include/batch_jobs/rh_batch_jobs_decal_gpu.h>
#include <pk_render_helpers/include/batch_jobs/rh_batch_jobs_triangle_gpu.h>

#include "PK-SampleLib/RenderIntegrationRHI/RendererCache.h"
#include "PK-SampleLib/RenderIntegrationRHI/RHITypePolicy.h"
#include "PK-SampleLib/RenderIntegrationRHI/RHIGPUSorter.h"

#include "RHICustomTasks.h"

#include <pk_rhi/include/FwdInterfaces.h>

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

struct	SRHIAdditionalFieldBatchGPU // To hold the SimStreamOffset for the additional fields
{
	TArray<SAdditionalInputs>		m_Fields;
	TArray<Drawers::SCopyFieldDesc>	m_MappedFields;

	//bool	Setup(); // Setup is done inline by the owner. Fill 'm_Fields' with any fields that you need to copy.
	bool	AllocBuffers(u32 vCount, RHI::PApiManager manager);
	bool	MapBuffers(u32 vCount, RHI::PApiManager manager);
	void	UnmapBuffers(RHI::PApiManager manager);
};

//----------------------------------------------------------------------------

class	CRHIRendererBatch_BillboardGPU_GeomBB : public CRendererBatchJobs_Billboard_GPUBB
{
public:
	CRHIRendererBatch_BillboardGPU_GeomBB(RHI::PApiManager apiManager)
	:	m_ApiManager(apiManager)
	{
	}

	virtual bool	AreRenderersCompatible(const CRendererDataBase *rendererA, const CRendererDataBase *rendererB) const override;

	virtual bool	AllocBuffers(SRenderContext &ctx) override;
	virtual bool	MapBuffers(SRenderContext &ctx) override;
	virtual bool	LaunchCustomTasks(SRenderContext &ctx) override;
	virtual bool	UnmapBuffers(SRenderContext &ctx) override;

	virtual bool	EmitDrawCall(SRenderContext &ctx, const SDrawCallDesc &toEmit) override;

private:
	RHI::PApiManager			m_ApiManager;

	SGpuBuffer					m_IndirectDraw;
	RHI::SDrawIndirectArgs		*m_MappedIndirectBuffer = null;
};

//----------------------------------------------------------------------------

class	CRHIRendererBatch_BillboardGPU_VertexBB : public CRendererBatchJobs_Billboard_GPUBB
{
public:
	CRHIRendererBatch_BillboardGPU_VertexBB(RHI::PApiManager apiManager)
	:	m_ApiManager(apiManager)
	{
	}

	virtual bool	Setup(const CRendererDataBase *renderer, const CParticleRenderMedium *owner, const CFrameCollector *fc, const CStringId &storageClass) override;
	virtual bool	AreRenderersCompatible(const CRendererDataBase *rendererA, const CRendererDataBase *rendererB) const override;

	virtual bool	AllocBuffers(SRenderContext &ctx) override;
	virtual bool	MapBuffers(SRenderContext &ctx) override;
	virtual bool	LaunchCustomTasks(SRenderContext &ctx) override;
	virtual bool	UnmapBuffers(SRenderContext &ctx) override;

	virtual bool	EmitDrawCall(SRenderContext &ctx, const SDrawCallDesc &toEmit) override;

private:
	RHI::PApiManager	m_ApiManager;

	bool				m_CapsulesDC = false;
	bool				m_NeedGPUSort = false;
	bool				m_SortByCameraDistance = false;

	SGpuBuffer						m_IndirectDraw;
	RHI::SDrawIndexedIndirectArgs	*m_MappedIndirectBuffer = null;

	RHI::PConstantSet				m_VertexBBOffsetsConstantSet; // (PositionsOffset, SizesOffset, ..)

	// Offsets for accessing each draw request gpu sim data streams
	SGpuBuffer					m_SimStreamOffsets_Enableds;
	SGpuBuffer					m_SimStreamOffsets_Positions;
	SGpuBuffer					m_SimStreamOffsets_Sizes;
	SGpuBuffer					m_SimStreamOffsets_Size2s;
	SGpuBuffer					m_SimStreamOffsets_Rotations;
	SGpuBuffer					m_SimStreamOffsets_Axis0s;
	SGpuBuffer					m_SimStreamOffsets_Axis1s;
	TStaticArray<u32*, 6>		m_MappedSimStreamOffsets;
	SRHIAdditionalFieldBatchGPU	m_AdditionalFieldsSimStreamOffsets;
	CGuid						m_ColorStreamIdx;

	// Sorting
	TArray<CGPUSorter>			m_CameraGPUSorters;
	TArray<SGpuBuffer>			m_CameraSortIndirection;
	TArray<SGpuBuffer>			m_CameraSortKeys;
	SGpuBuffer					m_CustomSortKeysOffsets;
	volatile u32				*m_MappedCustomSortKeysOffsets = null;
	TArray<RHI::PConstantSet>	m_ComputeCameraSortKeysConstantSets;

	// Static buffers
	SGpuBuffer		m_DrawIndices;	// DrawIndexed indices
	SGpuBuffer		m_TexCoords;	// DrawIndexed texcoords
	bool			m_Initialized = false;
	bool			_InitStaticBuffers();
};

//----------------------------------------------------------------------------

class	CRHIRendererBatch_Ribbon_GPU : public CRendererBatchJobs_Ribbon_GPUBB
{
public:
	CRHIRendererBatch_Ribbon_GPU(RHI::PApiManager apiManager) : m_ApiManager(apiManager) { }

	virtual bool	Setup(const CRendererDataBase *renderer, const CParticleRenderMedium *owner, const CFrameCollector *fc, const CStringId &storageClass) override;
	virtual bool	AreRenderersCompatible(const CRendererDataBase *rendererA, const CRendererDataBase *rendererB) const override;

	virtual bool	AllocBuffers(SRenderContext &ctx) override;
	virtual bool	MapBuffers(SRenderContext &ctx) override;
	virtual bool	LaunchCustomTasks(SRenderContext &ctx) override;
	virtual bool	UnmapBuffers(SRenderContext &ctx) override;

	virtual bool	EmitDrawCall(SRenderContext &ctx, const SDrawCallDesc &toEmit) override;

private:
	RHI::PApiManager		m_ApiManager;

	bool					m_TubesDC = false;
	bool					m_MultiPlanesDC = false;
	u32						m_ParticleQuadCount = 0;

	bool					m_NeedGPUSort = false;
	bool					m_SortByCameraDistance = false;

	SGpuBuffer						m_IndirectDraw;
	RHI::SDrawIndexedIndirectArgs	*m_MappedIndirectBuffer = null;

	RHI::PConstantSet						m_VertexBBOffsetsConstantSet; // (PositionsOffset, SizesOffset, ..)

	// Offsets for accessing each draw request gpu sim data streams
	SGpuBuffer					m_SimStreamOffsets_Enableds;
	SGpuBuffer					m_SimStreamOffsets_Positions;
	SGpuBuffer					m_SimStreamOffsets_Sizes;
	SGpuBuffer					m_SimStreamOffsets_Axis0s;
	SGpuBuffer					m_SimStreamOffsets_ParentIDs;
	SGpuBuffer					m_SimStreamOffsets_SelfIDs;
	TStaticArray<u32*, 6>		m_MappedSimStreamOffsets;
	SRHIAdditionalFieldBatchGPU	m_AdditionalFieldsSimStreamOffsets;
	CGuid						m_ColorStreamIdx;

	// Sorting (draw-order-sort + ribbon-sort)

	TArray<CGPUSorter>			m_CameraGPUSorters;
	TArray<CGPUSorter>			m_RibbonGPUSorters;

	TArray<SGpuBuffer>			m_CameraSortIndirection;
	TArray<SGpuBuffer>			m_CameraSortKeys;
	volatile u32				*m_MappedCustomSortKeysOffsets = null;

	TArray<SGpuBuffer>			m_RibbonSortIndirection;
	TArray<SGpuBuffer>			m_RibbonSortKeys;

	SGpuBuffer					m_CustomSortKeysOffsets;

	TArray<RHI::PConstantSet>	m_ComputeCameraSortKeysConstantSets;
	TArray<RHI::PConstantSet>	m_ComputeRibbonSortKeysConstantSets;

	// Static buffers
	SGpuBuffer		m_DrawIndices;	// DrawIndexed indices
	SGpuBuffer		m_TexCoords;	// DrawIndexed texcoords
	bool			m_Initialized = false;
	bool			_InitStaticBuffers();
};

//----------------------------------------------------------------------------

class	CRHIRendererBatch_Triangle_GPU : public CRendererBatchJobs_Triangle_GPUBB
{
public:
	CRHIRendererBatch_Triangle_GPU(RHI::PApiManager apiManager)
	:	m_ApiManager(apiManager)
	{
	}

	virtual bool	Setup(const CRendererDataBase *renderer, const CParticleRenderMedium *owner, const CFrameCollector *fc, const CStringId &storageClass) override;
	virtual bool	AreRenderersCompatible(const CRendererDataBase *rendererA, const CRendererDataBase *rendererB) const override;

	virtual bool	AllocBuffers(SRenderContext &ctx) override;
	virtual bool	MapBuffers(SRenderContext &ctx) override;
	virtual bool	LaunchCustomTasks(SRenderContext &ctx) override;
	virtual bool	UnmapBuffers(SRenderContext &ctx) override;

	virtual bool	EmitDrawCall(SRenderContext &ctx, const SDrawCallDesc &toEmit) override;

private:
	RHI::PApiManager	m_ApiManager;

	bool				m_NeedGPUSort = false;
	bool				m_SortByCameraDistance = false;

	SGpuBuffer						m_IndirectDraw;
	RHI::SDrawIndexedIndirectArgs	*m_MappedIndirectBuffer = null;

	RHI::PConstantSet				m_VertexBBOffsetsConstantSet; // (Positions0Offset, Positions1Offset, ..)

	// Offsets for accessing each draw request gpu sim data streams
	SGpuBuffer					m_SimStreamOffsets_Enableds;
	SGpuBuffer					m_SimStreamOffsets_Positions0;
	SGpuBuffer					m_SimStreamOffsets_Positions1;
	SGpuBuffer					m_SimStreamOffsets_Positions2;

	TStaticArray<u32*, 4>		m_MappedSimStreamOffsets;
	SRHIAdditionalFieldBatchGPU	m_AdditionalFieldsSimStreamOffsets;
	CGuid						m_ColorStreamIdx;

	// Sorting
	TArray<CGPUSorter>			m_CameraGPUSorters;
	TArray<SGpuBuffer>			m_CameraSortIndirection;
	TArray<SGpuBuffer>			m_CameraSortKeys;
	SGpuBuffer					m_CustomSortKeysOffsets;
	volatile u32				*m_MappedCustomSortKeysOffsets = null;
	TArray<RHI::PConstantSet>	m_ComputeCameraSortKeysConstantSets;

	// Static buffers
	SGpuBuffer		m_DrawIndices;	// DrawIndexed indices
	bool			m_Initialized = false;
	bool			_InitStaticBuffers();
};

//----------------------------------------------------------------------------

class	CRHIRendererBatch_Mesh_GPU : public CRendererBatchJobs_Mesh_GPUBB
{
public:
	CRHIRendererBatch_Mesh_GPU(RHI::PApiManager apiManager) : m_ApiManager(apiManager) { }

	virtual bool	Setup(const CRendererDataBase *renderer, const CParticleRenderMedium *owner, const CFrameCollector *fc, const CStringId &storageClass) override;
	virtual bool	AreRenderersCompatible(const CRendererDataBase *rendererA, const CRendererDataBase *rendererB) const override;

	virtual bool	AllocBuffers(SRenderContext &ctx) override;
	virtual bool	MapBuffers(SRenderContext &ctx) override;
	virtual bool	LaunchCustomTasks(SRenderContext &ctx) override;
	virtual bool	UnmapBuffers(SRenderContext &ctx) override;

	virtual bool	EmitDrawCall(SRenderContext &ctx, const SDrawCallDesc &toEmit) override;

private:
	RHI::PApiManager			m_ApiManager;

	TMemoryView<const u32>		m_PerMeshParticleCount;
	TMemoryView<const u32>		m_PerMeshBufferOffset;
	u32							m_MeshCount = 0;
	u32							m_MeshLODCount = 0;
	TArray<u32>					m_PerMeshIndexCount;

	SGpuBuffer				m_Matrices;
	SGpuBuffer				m_Indirection;

	SGpuBuffer				m_IndirectionOffsets;
	volatile u32			*m_MappedIndirectionOffsets = null;

	SGpuBuffer				m_MatricesOffsets;
	volatile u32			*m_MappedMatricesOffsets = null;

	SGpuBuffer				m_LODsConstantBuffer;
	//volatile u32			*m_MappedLODsConstant = null;

	SGpuBuffer				m_SimStreamOffsets_Positions;
	SGpuBuffer				m_SimStreamOffsets_Scales;
	SGpuBuffer				m_SimStreamOffsets_Orientations;
	SGpuBuffer				m_SimStreamOffsets_Enableds;
	SGpuBuffer				m_SimStreamOffsets_MeshIDs;
	SGpuBuffer				m_SimStreamOffsets_MeshLODs;

	TStaticArray<u32*, 6>	m_MappedSimStreamOffsets;

	RHI::PConstantSet		m_SimDataConstantSet;
	RHI::PConstantSet		m_OffsetsConstantSet;

	SRHIAdditionalFieldBatchGPU	m_AdditionalFieldsSimStreamOffsets;
	CGuid						m_ColorStreamIdx;

	// Indirect draw buffer
	SGpuBuffer								m_IndirectDraw; // We might need multiple indirect draws for mesh atlases.
	volatile RHI::SDrawIndexedIndirectArgs	*m_MappedIndexedIndirectBuffer = null;

	u32					_SubmeshIDToLOD(CRendererCacheInstance_UpdateThread &refCacheInstance, u32 lodCount, u32 iSubMesh);
};

//----------------------------------------------------------------------------

class	CRHIRendererBatch_Decal_GPU : public CRendererBatchJobs_Decal_GPUBB
{
public:
	CRHIRendererBatch_Decal_GPU(RHI::PApiManager apiManager) : m_ApiManager(apiManager) { }

	//virtual bool	AreRenderersCompatible(const CRendererDataBase *rendererA, const CRendererDataBase *rendererB) const override { return CRendererBatchJobs_Decal_GPUBB::AreRenderersCompatible(rendererA, rendererB); }

	virtual bool	AllocBuffers(SRenderContext &ctx) override;

	virtual bool	EmitDrawCall(SRenderContext &ctx, const SDrawCallDesc &toEmit) override;

private:
	RHI::PApiManager			m_ApiManager;
};

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
