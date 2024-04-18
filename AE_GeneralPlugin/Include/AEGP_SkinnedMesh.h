//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#pragma once

#ifndef	__AEGP_SKINNEDMESH_H__
#define	__AEGP_SKINNEDMESH_H__

#include "AEGP_Define.h"

#include <pk_kernel/include/kr_refptr.h>
#include <pk_particles_toolbox/include/pt_legacy_timeline.h>

// For skinned mesh backdrops
#include <pk_particles_toolbox/include/pt_skeleton_anim.h>
#include <pk_particles_toolbox/include/pt_mesh_deformers_skin.h>
#include <pk_geometrics/include/ge_mesh_resource.h>

//----------------------------------------------------------------------------

namespace PopcornFX
{
	PK_FORWARD_DECLARE(SkeletonState);
	PK_FORWARD_DECLARE(ResourceMeshBatch);
}

__AEGP_PK_BEGIN

//----------------------------------------------------------------------------

enum	EMeshChannels
{
	Channel_Position = 0x1,
	Channel_Normal = 0x2,
	Channel_Tangent = 0x4,
	Channel_Velocity = 0x8,
};

//----------------------------------------------------------------------------

class	CSkinnedMesh : public CRefCountedObject
{
protected:
	PSkeletonState					m_SkeletonState;
	CTimeline						m_Timeline;

	struct	SSubMesh
	{
		PCResourceMeshBatch				m_SkeletalMesh;
		PSkeletonState					m_SkeletonState;
		CSkinAsyncContext				m_SkinUpdateContext;
		SSamplerSourceOverride			m_SampleSourceOverride;
		void							*m_RawSkinnedData = null;
		u32								m_RawSkinnedDataElementCount = 0;
		TStridedMemoryView<CFloat3>		m_Positions;	// view inside 'm_RawSkinnedData'
		TStridedMemoryView<CFloat3>		m_Normals;		// view inside 'm_RawSkinnedData'
		TStridedMemoryView<CFloat4>		m_Tangents;		// view inside 'm_RawSkinnedData'
		TStridedMemoryView<CFloat3>		m_OldPositions;	// view inside 'm_RawSkinnedData'
		TStridedMemoryView<CFloat3>		m_Velocities;	// view inside 'm_RawSkinnedData'
		float							m_SkinDt;		// accessed by skinning jobs
		bool							m_FirstFrame;

		~SSubMesh()
		{
			PK_FREE(m_RawSkinnedData);
		}

		bool							Empty() const { return m_SkeletalMesh == null; }

		void							Skin_PreProcess(u32 vertexStart, u32 vertexCount, const SSkinContext &ctx);
		void							Skin_PostProcess(u32 vertexStart, u32 vertexCount, const SSkinContext &ctx);
	};

	TArray<SSubMesh>				m_SubMeshes;

	float							m_SkinDt;		// accessed by skinning jobs
	bool							m_FirstFrame;
	u32								m_SamplingChannels;

	bool							_Init_Impl(const TResourcePtr<CResourceMesh> &meshResource, u32 samplingChannels, const PSkeletonState &srcSkeletonState);

public:
	CSkinnedMesh();
	~CSkinnedMesh();

	void							Reset();
	bool							Init(const TResourcePtr<CResourceMesh> &meshResource, u32 samplingChannels, const PSkeletonState &srcSkeletonState);
	bool							Play(const PSkeletonAnimationInstance &animInstance);
	void							Stop();
	void							Update(float dt);
	bool							Valid() const { return m_SkeletonState != null; }
	//	void							Render();
	void							StartAsyncUpdateSkin(float dt);
	bool							WaitAsyncUpdateSkin();
	void							ClearVelocities();

	u32								SubMeshCount() const { return m_SubMeshes.Count(); }
	TStridedMemoryView<const CFloat3>	Positions(u32 batchId) const { return m_SubMeshes[batchId].m_Positions; }
	TStridedMemoryView<const CFloat3>	Normals(u32 batchId) const { return m_SubMeshes[batchId].m_Normals; }

	u32								SamplingChannels() const { return m_SamplingChannels; }

	//	CMeshAnimRenderer				&Renderer() { return m_Renderer; }
	PSkeletonState					SkeletonState() { return m_SkeletonState; }
	bool							HasGeometry() const
	{
		for (const auto &mesh : m_SubMeshes)
		{
			if (mesh.m_SkeletalMesh != null && mesh.m_SkeletalMesh->RawMesh() != null && !mesh.m_SkeletalMesh->RawMesh()->TriangleBatch().Empty())
				return true;;
		}
		return false;
	}
	SSamplerSourceOverride			*SamplerSourceOverride(u32 batchId) { return &m_SubMeshes[batchId].m_SampleSourceOverride; }
};
PK_DECLARE_REFPTRCLASS(SkinnedMesh);

//----------------------------------------------------------------------------

__AEGP_PK_END

#endif
