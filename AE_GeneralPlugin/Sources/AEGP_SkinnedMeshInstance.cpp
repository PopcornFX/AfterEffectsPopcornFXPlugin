//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#include "ae_precompiled.h"

#include "AEGP_SkinnedMeshInstance.h"
#include "AEGP_SkinnedMesh.h"

#include <pk_maths/include/pk_maths_transforms.h>
#include <pk_particles/include/ps_samplers_shape.h>
#include <pk_particles/include/ps_samplers_event_stream.h>
#include <pk_geometrics/include/ge_matrix_tools.h>		// for random XForms tests
#include <pk_maths/include/pk_maths_simd_matrix.h>		// for 'Skin_PostProcess'
#include <pk_geometrics/include/ge_mesh_sampler_accel.h>	// for CMeshSurfaceSamplerStructuresRandom && CMeshVolumeSamplerStructuresRandom
// For skinned mesh backdrops
#include <pk_particles_toolbox/include/pt_skeleton_anim.h>
#include <pk_particles_toolbox/include/pt_mesh_deformers_skin.h>

__AEGP_PK_BEGIN

//----------------------------------------------------------------------------

CSkinnedMeshInstance::CSkinnedMeshInstance()
	: m_SkeletonAnimationInstance(null)
	, m_CurrentAnimationPath(CString::EmptyString)
	, m_SkinnedMesh(null)
	, m_CurMeshTransformScaled(CFloat4x4::IDENTITY)
	, m_CurMeshTransform(CFloat4x4::IDENTITY)
	, m_PrevMeshTransform(CFloat4x4::IDENTITY)
{

}

//----------------------------------------------------------------------------

CSkinnedMeshInstance::~CSkinnedMeshInstance()
{
	if (m_SurfaceSamplingStruct != null)
		PK_DELETE(m_SurfaceSamplingStruct);
	if (m_VolumeSamplingStruct != null)
		PK_DELETE(m_VolumeSamplingStruct);

	// order matters
	m_SkeletonAnimationInstance = null;
	m_SkinnedMesh = null;

}

//----------------------------------------------------------------------------

CSkinnedMeshInstance	&CSkinnedMeshInstance::operator=(const CSkinnedMeshInstance &other)
{
	if (m_SurfaceSamplingStruct != null)
		PK_SAFE_DELETE(m_SurfaceSamplingStruct);
	if (m_VolumeSamplingStruct != null)
		PK_SAFE_DELETE(m_VolumeSamplingStruct);

	m_CurrentAnimationPath = other.m_CurrentAnimationPath;
	m_SkinnedMesh = other.m_SkinnedMesh;
	m_ShapeDescOverride = other.m_ShapeDescOverride;

	m_CurMeshTransformScaled = other.m_CurMeshTransformScaled;
	m_CurMeshTransform = other.m_CurMeshTransform;
	m_PrevMeshTransform = other.m_PrevMeshTransform;

	PK_ASSERT(other.m_SurfaceSamplingStruct == null &&
			  other.m_VolumeSamplingStruct == null);
	return *this;
}

//----------------------------------------------------------------------------

void	CSkinnedMeshInstance::ResetXForms(const CFloat4x4 &backdropXForm)
{
	SetBackdropXForms(backdropXForm);

}

//----------------------------------------------------------------------------

void	CSkinnedMeshInstance::SetBackdropXForms(const CFloat4x4 &backdropXForm)
{
	m_CurMeshTransform = backdropXForm;
	m_PrevMeshTransform = backdropXForm;
	m_CurMeshTransformScaled = m_CurMeshTransform;

	// Fix #5336: Scaled mesh backdrops also scale their sampled normals. Should not
	// Here we need to remove the scale of the transforms and instead apply it in the mesh descriptor with 'SetScale'
	const CFloat3	scale = m_CurMeshTransform.ScalingFactors();
	m_CurMeshTransform.RemoveScale();
	m_PrevMeshTransform.RemoveScale();

	// Apply scale
	if (m_ShapeDescOverride != null)
	{
		CParticleSamplerDescriptor_Shape_Default	*def = checked_cast<CParticleSamplerDescriptor_Shape_Default *>(m_ShapeDescOverride.Get());
		if (PK_VERIFY(def->m_Shape != null && def->m_Shape->ShapeType() == CShapeDescriptor::ShapeMesh))
		{
			CShapeDescriptor_Mesh	*shapeDescMesh = const_cast<CShapeDescriptor_Mesh*>(checked_cast<const CShapeDescriptor_Mesh*>(def->m_Shape.Get()));
			if (PK_VERIFY(shapeDescMesh != null))
				shapeDescMesh->SetScale(scale);
		}
	}
}

//----------------------------------------------------------------------------

void	CSkinnedMeshInstance::ResetAnimationCursor()
{
	if (m_SkeletonAnimationInstance != null)
		m_SkeletonAnimationInstance->SeekTo(0.0f);
}

//----------------------------------------------------------------------------

void	CSkinnedMeshInstance::Update(float dt)
{
	if (m_SkinnedMesh != null)
		m_SkinnedMesh->Update(dt);
}

//----------------------------------------------------------------------------

void	CSkinnedMeshInstance::StartAsyncUpdateSkin(float dt)
{
	if (m_SkinnedMesh != null)
		m_SkinnedMesh->StartAsyncUpdateSkin(dt);
}

//----------------------------------------------------------------------------

bool	CSkinnedMeshInstance::WaitAsyncUpdateSkin()
{
	if (m_SkinnedMesh != null)
		return m_SkinnedMesh->WaitAsyncUpdateSkin();
	return true;
}

//----------------------------------------------------------------------------

void	CSkinnedMeshInstance::ClearSkinnedMesh()
{
	m_SkeletonAnimationInstance = null;
	if (m_SkinnedMesh != null)
		m_SkinnedMesh->Reset();
}

//----------------------------------------------------------------------------

bool	CSkinnedMeshInstance::LoadSkinnedMeshIFN(const TResourcePtr<CResourceMesh> &meshResource, u32 samplingChannels, const PSkeletonState &scrSkeletonState)
{
	m_SkeletonAnimationInstance = null;
	if (meshResource == null)
	{
		if (m_SkinnedMesh != null)
			m_SkinnedMesh->Reset();
		return true;
	}

	if (m_SkinnedMesh == null)
	{
		m_SkinnedMesh = PK_NEW(CSkinnedMesh);
		if (m_SkinnedMesh == null)
			return false;
	}

	if (!m_SkinnedMesh->Init(meshResource, samplingChannels, scrSkeletonState))
	{
		m_SkinnedMesh->Reset();
		return false;
	}
	return true;
}

//----------------------------------------------------------------------------

bool	CSkinnedMeshInstance::LoadAnimationIFN(HBO::CContext *context, const CString &pksaPath, bool forceReload)
{
	m_SkeletonAnimationInstance = null;
	if (m_SkinnedMesh == null || !m_SkinnedMesh->HasGeometry())
		return true;

	if (pksaPath.Empty())
	{
		m_SkeletonAnimationInstance = null;
		m_CurrentAnimationPath = null;
		m_SkinnedMesh->Stop();
		return true;
	}

	if (m_SkeletonAnimationInstance == null || m_CurrentAnimationPath != pksaPath)	// if not already loaded or different anim
	{
		m_SkeletonAnimationInstance = CSkeletonAnimationInstance::NewInstance(context, pksaPath, m_SkinnedMesh->SkeletonState().Get(), forceReload);
		if (m_SkeletonAnimationInstance == null)
		{
			CLog::Log(PK_ERROR, "failed loading animation");
			m_SkinnedMesh->Stop();
			return false;
		}

		m_CurrentAnimationPath = pksaPath;
	}

	// reset playback parameters
	m_SkeletonAnimationInstance->SetPlaybackMode(CSkeletonAnimationInstance::Playback_Loop);
	m_SkeletonAnimationInstance->SeekTo(0.0f);
	m_SkeletonAnimationInstance->SetSpeed(1.0f);
	m_SkinnedMesh->Play(m_SkeletonAnimationInstance);
	return true;
}

//----------------------------------------------------------------------------

bool	CSkinnedMeshInstance::SetupAttributeSampler(CMeshNew *srcMesh, bool weightedSampling, u32 weightedSamplingColorStreamId, u32 weightedSamplingChannelId)
{
	PK_ASSERT(m_SkinnedMesh != null);
	CShapeDescriptor_Mesh	*shapeDescMesh = PK_NEW(CShapeDescriptor_Mesh);
	if (!PK_VERIFY(shapeDescMesh != null))
		return false;

	SSamplerSourceOverride	*samplerSourceOverride = null;
	const u32	samplingSubMeshId = 0;
	if (samplingSubMeshId < m_SkinnedMesh->SubMeshCount())
		samplerSourceOverride = m_SkinnedMesh->SamplerSourceOverride(samplingSubMeshId);
	shapeDescMesh->SetMesh(srcMesh, samplerSourceOverride);

	shapeDescMesh->SetScale(m_CurMeshTransformScaled.ScalingFactors());

	if (weightedSampling)
	{
		if (m_SurfaceSamplingStruct == null)
			m_SurfaceSamplingStruct = PK_NEW(CMeshSurfaceSamplerStructuresRandom);
		if (m_SurfaceSamplingStruct != null)
			srcMesh->SetupSurfaceSamplingAccelStructs(weightedSamplingColorStreamId, weightedSamplingChannelId, *m_SurfaceSamplingStruct);
	
		if (m_VolumeSamplingStruct == null)
			m_VolumeSamplingStruct = PK_NEW(CMeshVolumeSamplerStructuresRandom);
		if (m_VolumeSamplingStruct != null)
			srcMesh->SetupVolumeSamplingAccelStructs(*m_VolumeSamplingStruct);
	
		shapeDescMesh->SetSamplingStructs(m_SurfaceSamplingStruct, m_VolumeSamplingStruct);
	}

	PParticleSamplerDescriptor_Shape_Default	desc = PK_NEW(CParticleSamplerDescriptor_Shape_Default(shapeDescMesh));
	if (!PK_VERIFY(desc != null))
	{
		PK_DELETE(shapeDescMesh);
		return false;
	}
	desc->m_Angular_Velocity = &CFloat3::ZERO;
	desc->m_Linear_Velocity = &CFloat3::ZERO;
	desc->m_WorldTr_Current = &m_CurMeshTransform;
	desc->m_WorldTr_Previous = &m_PrevMeshTransform;
	m_ShapeDescOverride = desc;
	return true;

}

//----------------------------------------------------------------------------

__AEGP_PK_END
