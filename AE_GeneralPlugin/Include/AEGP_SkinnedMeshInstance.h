//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#pragma once

#ifndef	__AEGP_SKINNEDMESHINSTANCE_H__
#define	__AEGP_SKINNEDMESHINSTANCE_H__

#include "AEGP_Define.h"

#include <pk_kernel/include/kr_refptr.h>

#include <pk_particles_toolbox/include/pt_helpers.h>
#include <pk_geometrics/include/ge_mesh_resource.h>
#include <pk_particles_toolbox/include/pt_skeleton_anim.h>
#include <pk_particles_toolbox/include/pt_mesh_deformers_skin.h>

//----------------------------------------------------------------------------

__PK_API_BEGIN

PK_FORWARD_DECLARE(SkeletonAnimationInstance);
PK_FORWARD_DECLARE(ParticleSamplerDescriptor_Shape);
PK_FORWARD_DECLARE(MeshSurfaceSamplerStructuresRandom);
PK_FORWARD_DECLARE(MeshVolumeSamplerStructuresRandom);
PK_FORWARD_DECLARE(ResourceMesh);

namespace	HBO {
	class	CContext;
}
__PK_API_END

__AEGP_PK_BEGIN

PK_FORWARD_DECLARE(SkinnedMesh);

//----------------------------------------------------------------------------

class	CSkinnedMeshInstance : public CRefCountedObject
{
public:
	PSkeletonAnimationInstance			m_SkeletonAnimationInstance;
	CString								m_CurrentAnimationPath;
	PSkinnedMesh						m_SkinnedMesh;
	PParticleSamplerDescriptor_Shape	m_ShapeDescOverride;

	CMeshSurfaceSamplerStructuresRandom		*m_SurfaceSamplingStruct = null;
	CMeshVolumeSamplerStructuresRandom		*m_VolumeSamplingStruct = null;

	CFloat4x4					m_CurMeshTransformScaled;
	CFloat4x4					m_CurMeshTransform;
	CFloat4x4					m_PrevMeshTransform;

public:
	CSkinnedMeshInstance();
	~CSkinnedMeshInstance();

	CSkinnedMeshInstance	&operator = (const CSkinnedMeshInstance &other);


	void	ResetXForms(const CFloat4x4 &backdropXForm);
	void	SetBackdropXForms(const CFloat4x4 &backdropXForm);

	CFloat3	PredictVelocity(float dt);

	void	ResetAnimationCursor();

	void	Update(float dt);
	void	StartAsyncUpdateSkin(float dt);
	bool	WaitAsyncUpdateSkin();

	void	ClearSkinnedMesh();
	bool	LoadSkinnedMeshIFN(const TResourcePtr<CResourceMesh> &meshResource, u32 samplingChannels, const PSkeletonState &scrSkeletonState);
	bool	LoadAnimationIFN(HBO::CContext *context, const CString &pksaPath, bool forceReload);
	bool	SetupAttributeSampler(CMeshNew *srcMesh, bool weightedSampling, u32 weightedSamplingColorStreamId, u32 weightedSamplingChannelId);
};

//----------------------------------------------------------------------------

__AEGP_PK_END

#endif
