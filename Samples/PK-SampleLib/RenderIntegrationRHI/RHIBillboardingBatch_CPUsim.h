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

#include <pk_render_helpers/include/batch_jobs/rh_batch_jobs_billboard_cpu.h>
#include <pk_render_helpers/include/batch_jobs/rh_batch_jobs_billboard_gpu.h>
#include <pk_render_helpers/include/batch_jobs/rh_batch_jobs_ribbon_cpu.h>
#include <pk_render_helpers/include/batch_jobs/rh_batch_jobs_mesh_cpu.h>
#include <pk_render_helpers/include/batch_jobs/rh_batch_jobs_decal_cpu.h>
#include <pk_render_helpers/include/batch_jobs/rh_batch_jobs_triangle_cpu.h>
#include <pk_render_helpers/include/batch_jobs/rh_batch_jobs_triangle_gpu.h>
#include <pk_render_helpers/include/batch_jobs/rh_batch_jobs_light_std.h>

#include "PK-SampleLib/RenderIntegrationRHI/RendererCache.h"
#include "PK-SampleLib/RenderIntegrationRHI/RHITypePolicy.h"

#include "RHICustomTasks.h"

#include <pk_rhi/include/FwdInterfaces.h>

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

struct	SRHICommonCPUBillboardBuffers
{
	// View independent buffers:
	SGpuBuffer				m_Indices;
	u32						m_IndexSize = 0;

	SGpuBuffer				m_Positions;
	SGpuBuffer				m_Normals;
	SGpuBuffer				m_Tangents;
	SGpuBuffer				m_TexCoords0;
	SGpuBuffer				m_TexCoords1;
	SGpuBuffer				m_RawTexCoords0;
	SGpuBuffer				m_UVFactors;
	SGpuBuffer				m_UVRemap;

	// View dependent buffers:
	struct	SPerView
	{
		SGpuBuffer			m_Indices;
		SGpuBuffer			m_Positions;
		SGpuBuffer			m_Normals;
		SGpuBuffer			m_Tangents;
		SGpuBuffer			m_UVFactors;
	};
	TArray<SPerView>		m_PerViewBuffers;

	bool	AllocBuffers(u32 indexCount, u32 vertexCount, const SGeneratedInputs &toGenerate, RHI::PApiManager manager);
	//bool	MapBuffers() is made inline by the owner.
	void	UnmapBuffers(RHI::PApiManager manager);
};

//----------------------------------------------------------------------------

struct	SRHIAdditionalFieldBatch
{
	TArray<SAdditionalInputs>			m_Fields;
	TArray<Drawers::SCopyFieldDesc>		m_MappedFields; // We need this to hold data while the tasks are running. Filled by "MapBuffers".

	//bool	Setup(); // Setup is done inline by the owner. Fill 'm_Fields' with any fields that you need to copy.
	bool	AllocBuffers(u32 vCount, RHI::PApiManager manager, RHI::EBufferType type = RHI::VertexBuffer);
	bool	MapBuffers(u32 vCount, RHI::PApiManager manager);
	void	UnmapBuffers(RHI::PApiManager manager);
};

//----------------------------------------------------------------------------

class	CRHIRendererBatch_Billboard_CPUBB : public CRendererBatchJobs_Billboard_CPUBB
{
public:
	CRHIRendererBatch_Billboard_CPUBB(RHI::PApiManager apiManager) : m_ApiManager(apiManager) { }

	virtual bool	Setup(const CRendererDataBase *renderer, const CParticleRenderMedium *owner, const CFrameCollector *fc, const CStringId &storageClass) override;
	virtual bool	AreRenderersCompatible(const CRendererDataBase *rendererA, const CRendererDataBase *rendererB) const override;

	virtual bool	AllocBuffers(SRenderContext &ctx) override;
	virtual bool	MapBuffers(SRenderContext &ctx) override;
	virtual bool	LaunchCustomTasks(SRenderContext &ctx) override;
	virtual bool	UnmapBuffers(SRenderContext &ctx) override;

	virtual bool	EmitDrawCall(SRenderContext &ctx, const SDrawCallDesc &toEmit) override;

private:
	RHI::PApiManager			m_ApiManager;

	SRHICommonCPUBillboardBuffers	m_CommonBuffers;

	SRHIAdditionalFieldBatch		m_AdditionalFieldsBatch;
	CGuid							m_ColorStreamIdx;

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	CBillboard_Exec_WireframeDiscard		m_BillboardCustomParticleSelectTask;
	SGpuBuffer								m_IsParticleSelected;
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)
};

//----------------------------------------------------------------------------

class	CRHIRendererBatch_Billboard_GeomBB : public CRendererBatchJobs_Billboard_GPUBB
{
public:
	CRHIRendererBatch_Billboard_GeomBB(RHI::PApiManager apiManager)
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
	RHI::PApiManager			m_ApiManager;

	SGpuBuffer					m_GeomConstants;
	SGpuBuffer					m_GeomPositions;
	SGpuBuffer					m_GeomSizes;
	SGpuBuffer					m_GeomSizes2;
	SGpuBuffer					m_GeomRotations;
	SGpuBuffer					m_GeomAxis0;
	SGpuBuffer					m_GeomAxis1;

	SGpuBuffer					m_Indices; // the RHI draw-call always needs the 'indices' buffer, even for non-sorted particles
	u32							m_IndexSize = 0; // u16-indices or u32-indices
	TArray<SGpuBuffer>			m_PerViewIndicesBuffers;

	SRHIAdditionalFieldBatch	m_AdditionalFieldsBatch;
	CGuid						m_ColorStreamIdx;

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	CCopyStream_Exec_WireframeDiscard		m_CopyCustomParticleSelectTask;
	SGpuBuffer								m_IsParticleSelected;
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)
};

//----------------------------------------------------------------------------

class	CRHIRendererBatch_Billboard_VertexBB : public CRendererBatchJobs_Billboard_GPUBB
{
public:
	CRHIRendererBatch_Billboard_VertexBB(RHI::PApiManager apiManager)
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
	RHI::PApiManager		m_ApiManager;

	bool					m_CapsulesDC = false;

	SGpuBuffer				m_Indices; // Generated indices for sorted particles
	TArray<SGpuBuffer>		m_PerViewIndicesBuffers;

	SGpuBuffer				m_DrawIndices;	// DrawIndexed indices
	SGpuBuffer				m_TexCoords;	// DrawIndexed texcoords

	SGpuBuffer				m_DrawRequests;	// Draw requests buffer, for draw calls batching

	SGpuBuffer				m_Positions;
	SGpuBuffer				m_Sizes;
	SGpuBuffer				m_Sizes2;
	SGpuBuffer				m_Rotations;
	SGpuBuffer				m_Axis0s;
	SGpuBuffer				m_Axis1s;

	RHI::PConstantSet		m_VertexBBSimDataConstantSet; // SRVs

	SRHIAdditionalFieldBatch			m_AdditionalFieldsBatch;
	CGuid								m_ColorStreamIdx;

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	CCopyStream_Exec_WireframeDiscard		m_GeomBillboardCustomParticleSelectTask;
	SGpuBuffer								m_IsParticleSelected;
	RHI::PConstantSet						m_SelectionConstantSet;
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)

	bool		m_Initialized = false;
	bool		_InitStaticBuffers();
};

//----------------------------------------------------------------------------

class	CRHIRendererBatch_Ribbon_CPU : public CRendererBatchJobs_Ribbon_CPUBB
{
public:
	CRHIRendererBatch_Ribbon_CPU(RHI::PApiManager apiManager) : m_ApiManager(apiManager) { }

	virtual bool	Setup(const CRendererDataBase *renderer, const CParticleRenderMedium *owner, const CFrameCollector *fc, const CStringId &storageClass) override;
	virtual bool	AreRenderersCompatible(const CRendererDataBase *rendererA, const CRendererDataBase *rendererB) const override;

	virtual bool	AllocBuffers(SRenderContext &ctx) override;
	virtual bool	MapBuffers(SRenderContext &ctx) override;
	virtual bool	LaunchCustomTasks(SRenderContext &ctx) override;
	virtual bool	UnmapBuffers(SRenderContext &ctx) override;

	virtual bool	EmitDrawCall(SRenderContext &ctx, const SDrawCallDesc &toEmit) override;

private:
	RHI::PApiManager			m_ApiManager;

	SRHICommonCPUBillboardBuffers	m_CommonBuffers;

	SRHIAdditionalFieldBatch		m_AdditionalFieldsBatch;
	CGuid							m_ColorStreamIdx;

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	CRibbon_Exec_WireframeDiscard			m_RibbonCustomParticleSelectTask;
	SGpuBuffer								m_IsParticleSelected;
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)
};

//----------------------------------------------------------------------------

class	CRHIRendererBatch_Mesh_CPU : public CRendererBatchJobs_Mesh_CPUBB
{
public:
	CRHIRendererBatch_Mesh_CPU(RHI::PApiManager apiManager) : m_ApiManager(apiManager) { }

	virtual bool	Setup(const CRendererDataBase *renderer, const CParticleRenderMedium *owner, const CFrameCollector *fc, const CStringId &storageClass) override;
	virtual bool	AreRenderersCompatible(const CRendererDataBase *rendererA, const CRendererDataBase *rendererB) const override;

	virtual bool	AllocBuffers(SRenderContext &ctx) override;
	virtual bool	MapBuffers(SRenderContext &ctx) override;
	virtual bool	LaunchCustomTasks(SRenderContext &ctx) override;
	virtual bool	UnmapBuffers(SRenderContext &ctx) override;

	virtual bool	EmitDrawCall(SRenderContext &ctx, const SDrawCallDesc &toEmit) override;

private:
	RHI::PApiManager		m_ApiManager;

	SGpuBuffer				m_Matrices;

	SRHIAdditionalFieldBatch	m_AdditionalFieldsBatch;
	CGuid						m_ColorStreamIdx;

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	CMesh_Exec_WireframeDiscard				m_MeshCustomParticleSelectTask;
	SGpuBuffer								m_IsParticleSelected;
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)
};

//----------------------------------------------------------------------------

class	CRHIRendererBatch_Decal_CPU : public CRendererBatchJobs_Decal_CPUBB
{
public:
	CRHIRendererBatch_Decal_CPU(RHI::PApiManager apiManager) : m_ApiManager(apiManager) { }

	virtual bool	Setup(const CRendererDataBase *renderer, const CParticleRenderMedium *owner, const CFrameCollector *fc, const CStringId &storageClass) override;
	virtual bool	AreRenderersCompatible(const CRendererDataBase *rendererA, const CRendererDataBase *rendererB) const override;

	virtual bool	AllocBuffers(SRenderContext &ctx) override;
	virtual bool	MapBuffers(SRenderContext &ctx) override;
	virtual bool	LaunchCustomTasks(SRenderContext &ctx) override;
	virtual bool	WaitForCustomTasks(SRenderContext &ctx) override;
	virtual bool	UnmapBuffers(SRenderContext &ctx) override;

	virtual bool	EmitDrawCall(SRenderContext &ctx, const SDrawCallDesc &toEmit) override;

private:
	RHI::PApiManager		m_ApiManager;

	SGpuBuffer				m_Matrices;
	SGpuBuffer				m_InvMatrices;

	SRHIAdditionalFieldBatch	m_AdditionalFieldsBatch;
	CGuid						m_ColorStreamIdx;

	Drawers::CCopyStream_Exec_AdditionalField	m_CopyAdditionalFieldsTask;
	Drawers::CCopyStream_CPU					m_CopyTasks;

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	CCopyStream_Exec_WireframeDiscard		m_CopyStreamCustomParticleSelectTask;
	SGpuBuffer								m_IsParticleSelected;
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)
};

//----------------------------------------------------------------------------

class	CRHIRendererBatch_Triangle_CPUBB : public CRendererBatchJobs_Triangle_CPUBB
{
public:
	CRHIRendererBatch_Triangle_CPUBB(RHI::PApiManager apiManager) : m_ApiManager(apiManager), m_IndexSize(0) { }

	virtual bool	Setup(const CRendererDataBase *renderer, const CParticleRenderMedium *owner, const CFrameCollector *fc, const CStringId &storageClass) override;
	virtual bool	AreRenderersCompatible(const CRendererDataBase *rendererA, const CRendererDataBase *rendererB) const override;

	virtual bool	AllocBuffers(SRenderContext &ctx) override;
	virtual bool	MapBuffers(SRenderContext &ctx) override;
	virtual bool	LaunchCustomTasks(SRenderContext &ctx) override;
	virtual bool	UnmapBuffers(SRenderContext &ctx) override;

	virtual bool	EmitDrawCall(SRenderContext &ctx, const SDrawCallDesc &toEmit) override;

private:
	RHI::PApiManager		m_ApiManager;

	SGpuBuffer				m_Indices;
	u32						m_IndexSize;

	SGpuBuffer				m_Positions;
	SGpuBuffer				m_Normals;
	SGpuBuffer				m_Tangents;
	SGpuBuffer				m_TexCoords0;

	SRHIAdditionalFieldBatch	m_AdditionalFieldsBatch;
	CGuid						m_ColorStreamIdx;

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	CTriangle_Exec_WireframeDiscard			m_TriangleCustomParticleSelectTask;
	SGpuBuffer								m_IsParticleSelected;
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)
};

//----------------------------------------------------------------------------

class	CRHIRendererBatch_Triangle_VertexBB : public CRendererBatchJobs_Triangle_GPUBB
{
public:
	CRHIRendererBatch_Triangle_VertexBB(RHI::PApiManager apiManager) : m_ApiManager(apiManager) { }

	virtual bool	Setup(const CRendererDataBase *renderer, const CParticleRenderMedium *owner, const CFrameCollector *fc, const CStringId &storageClass) override;
	virtual bool	AreRenderersCompatible(const CRendererDataBase *rendererA, const CRendererDataBase *rendererB) const override;

	virtual bool	AllocBuffers(SRenderContext &ctx) override;
	virtual bool	MapBuffers(SRenderContext &ctx) override;
	virtual bool	LaunchCustomTasks(SRenderContext &ctx) override;
	virtual bool	UnmapBuffers(SRenderContext &ctx) override;

	virtual bool	EmitDrawCall(SRenderContext &ctx, const SDrawCallDesc &toEmit) override;

private:
	RHI::PApiManager		m_ApiManager;

	SGpuBuffer				m_Indices; // Generated indices for sorted particles
	TArray<SGpuBuffer>		m_PerViewIndicesBuffers;

	SGpuBuffer				m_DrawIndices;	// DrawIndexed indices

	SGpuBuffer				m_DrawRequests;	// Draw requests buffer, for draw calls batching

	SGpuBuffer				m_VertexPositions0;
	SGpuBuffer				m_VertexPositions1;
	SGpuBuffer				m_VertexPositions2;

	RHI::PConstantSet		m_VertexBBSimDataConstantSet; // SRVs

	SRHIAdditionalFieldBatch			m_AdditionalFieldsBatch;
	CGuid								m_ColorStreamIdx;

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	CCopyStream_Exec_WireframeDiscard		m_GeomBillboardCustomParticleSelectTask;
	SGpuBuffer								m_IsParticleSelected;
	RHI::PConstantSet						m_SelectionConstantSet;
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)

	bool		m_Initialized = false;
	bool		_InitStaticBuffers();
};

//----------------------------------------------------------------------------

class	CRHIRendererBatch_Light_CPU : public CRendererBatchJobs_Light_Std
{
public:
	CRHIRendererBatch_Light_CPU(RHI::PApiManager apiManager) : m_ApiManager(apiManager) { }

	virtual bool	Setup(const CRendererDataBase *renderer, const CParticleRenderMedium *owner, const CFrameCollector *fc, const CStringId &storageClass) override;
	virtual bool	AreRenderersCompatible(const CRendererDataBase *rendererA, const CRendererDataBase *rendererB) const override;

	virtual bool	AllocBuffers(SRenderContext &ctx) override;
	virtual bool	MapBuffers(SRenderContext &ctx) override;
	virtual bool	LaunchCustomTasks(SRenderContext &ctx) override;
	virtual bool	WaitForCustomTasks(SRenderContext &ctx) override;
	virtual bool	UnmapBuffers(SRenderContext &ctx) override;

	virtual bool	EmitDrawCall(SRenderContext &ctx, const SDrawCallDesc &toEmit) override;

private:
	RHI::PApiManager			m_ApiManager;

	SGpuBuffer					m_LightsPositions;

	SRHIAdditionalFieldBatch	m_AdditionalFieldsBatch;
	CGuid						m_ColorStreamIdx;
	CGuid						m_RangeStreamIdx;

	Drawers::CCopyStream_Exec_AdditionalField	m_CopyAdditionalFieldsTask;
	Drawers::CCopyStream_CPU					m_CopyTasks;

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	CCopyStream_Exec_WireframeDiscard		m_CopyStreamCustomParticleSelectTask;
	SGpuBuffer								m_IsParticleSelected;
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)
};

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
