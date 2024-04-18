//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#pragma once

#ifndef __FX_AAEPARTICLESCENE_H__
#define __FX_AAEPARTICLESCENE_H__

#include "AEGP_Define.h"

#include <pk_kernel/include/kr_refptr.h>

#include <pk_particles/include/ps_scene.h>
#include <pk_geometrics/include/ge_mesh_resource.h>
#include <pk_geometrics/include/ge_collidable_object.h>

#include <PopcornFX_Define.h>

__AEGP_PK_BEGIN

class	CAAEScene;

//----------------------------------------------------------------------------

class	CAAEParticleScene : public IParticleScene
{
public:
	CAAEParticleScene() {}
	virtual ~CAAEParticleScene() {}

	virtual void	RayTracePacket(	const Colliders::STraceFilter	&traceFilter,
									const Colliders::SRayPacket		&packet,
									const Colliders::STracePacket	&results) override;

	void				ClearBackdropMesh();
	void				SetBackdropMeshTransform(const CFloat4x4 &transforms);
	void				SetBackdropMesh(const TResourcePtr<CResourceMesh> &resourceMesh, const CFloat4x4 &transforms);
	const CFloat4x4		&BackdropMeshTransforms() const { return m_BackdropMeshTransforms; }

	virtual TMemoryView<const float * const>	GetAudioSpectrum(CStringId channelGroup, u32 &outBaseCount) const override;
	virtual TMemoryView<const float * const>	GetAudioWaveform(CStringId channelGroup, u32 &outBaseCount) const override;

	CAAEScene			*m_Parent = null;
private:
	// Mesh backdrop
	TArray<PMeshNew>	m_BackdropMeshes;
	CFloat4x4			m_BackdropMeshTransforms = CFloat4x4::IDENTITY;
};

//----------------------------------------------------------------------------

__AEGP_PK_END

#endif
