//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#include "ae_precompiled.h"

#include "AEGP_ParticleScene.h"

#include <PopcornFX_Suite.h>

#include "AEGP_Scene.h"
#include <pk_kernel/include/kr_containers_onstack.h>

__AEGP_PK_BEGIN

//----------------------------------------------------------------------------

void	CAAEParticleScene::RayTracePacket(	const Colliders::STraceFilter	&traceFilter,
											const Colliders::SRayPacket		&packet,
											const Colliders::STracePacket	&results)
{
	(void)traceFilter;
	PK_NAMEDSCOPEDPROFILE_C("CParticleSceneInterface: RayTracePacket", CFloat3(0.0f, 0.6f, 1.0f));
	// pre-conditions
	PK_ASSERT(packet.m_RayOrigins_Aligned16.Count() == packet.m_RayDirectionsAndLengths_Aligned16.Count());
	PK_ASSERT(results.Count() == packet.m_RayOrigins_Aligned16.Count());

	const s32						kMaskTrue = -1;
	TStridedMemoryView<const s32>	enableMasks = packet.m_RayMasks_Aligned16;
	if (enableMasks.Empty())
		enableMasks = TStridedMemoryView<const s32>(&kMaskTrue, results.Count(), 0);

	if (!m_BackdropMeshes.Empty())
	{
		// we've got a mesh to collide against. Store the previous ray lengths so we may
		// detect which rays intersect the mesh:
		PK_STACKALIGNEDMEMORYVIEW(float, prevHitTimes, results.Count(), 0x10);
		Mem::Copy(prevHitTimes.Data(), results.m_HitTimes_Aligned16, results.Count() * sizeof(float));

		const CFloat4x4		&meshXForms = m_BackdropMeshTransforms;
		const CFloat4x4		meshXFormsInv = meshXForms.Inverse();

		for (u32 i = 0; i < m_BackdropMeshes.Count(); ++i)
		{
			// trace the mesh:
			m_BackdropMeshes[i]->TracePacket(CMeshNew::STraceFlags(), packet, results, &meshXForms, &meshXFormsInv);

			if (results.m_ContactObjects_Aligned16 != null)  // the caller wants us to report contact objects
			{
				for (u32 j = 0; j < results.Count(); j++)
				{
					PK_ASSERT(results.m_HitTimes_Aligned16[j] <= prevHitTimes[j]);

					// if the hit time is different from the original hit-time,
					// this ray has hit the mesh, set the contact object:
					if (results.m_HitTimes_Aligned16[j] != prevHitTimes[j])
						results.m_ContactObjects_Aligned16[j] = CollidableObject::DEFAULT;
				}
			}
		}
	}
}

//----------------------------------------------------------------------------

void	CAAEParticleScene::ClearBackdropMesh()
{
	m_BackdropMeshes.Clear();
}

//----------------------------------------------------------------------------

void	CAAEParticleScene::SetBackdropMeshTransform(const CFloat4x4 &transforms)
{
	m_BackdropMeshTransforms = transforms;
}

//----------------------------------------------------------------------------

void	CAAEParticleScene::SetBackdropMesh(const TResourcePtr<CResourceMesh> &resourceMesh, const CFloat4x4 &transforms)
{
	m_BackdropMeshes.Clear();
	if (resourceMesh == null || resourceMesh->Empty())
		return;
	for (const auto &staticBatch : resourceMesh->BatchList())
	{
		m_BackdropMeshes.PushBack(staticBatch->RawMesh());
	}
	m_BackdropMeshTransforms = transforms;
}

//----------------------------------------------------------------------------

TMemoryView<const float * const> CAAEParticleScene::GetAudioSpectrum(CStringId channelGroup, u32 &outBaseCount) const
{
	PK_ASSERT(m_Parent != null);
	SSamplerAudio *samplerAudio = m_Parent->GetAudioSamplerDescriptor(channelGroup, SSamplerAudio::SamplingType_Spectrum);

	if (samplerAudio == null)
		return TMemoryView<const float*const>();

	samplerAudio->m_SamplingType = SSamplerAudio::SamplingType_Spectrum;

	samplerAudio->BuildAudioPyramidIFN();

	outBaseCount = samplerAudio->m_SampleCount;
	if (samplerAudio->m_SampleCount == 0)
		return TMemoryView<const float*const>();
	return TMemoryView<const float * const>(samplerAudio->m_WaveformPyramid.RawDataPointer(), samplerAudio->m_WaveformPyramid.Count());
}

//----------------------------------------------------------------------------

TMemoryView<const float * const> CAAEParticleScene::GetAudioWaveform(CStringId channelGroup, u32 &outBaseCount) const
{
	PK_ASSERT(m_Parent != null);
	SSamplerAudio *samplerAudio = m_Parent->GetAudioSamplerDescriptor(channelGroup, SSamplerAudio::SamplingType_WaveForm);

	if (samplerAudio == null)
		return TMemoryView<const float*const>();

	samplerAudio->m_SamplingType = SSamplerAudio::SamplingType_WaveForm;

	samplerAudio->BuildAudioPyramidIFN();

	outBaseCount = samplerAudio->m_SampleCount;
	if (samplerAudio->m_SampleCount == 0)
		return TMemoryView<const float*const>();
	return TMemoryView<const float * const>(samplerAudio->m_WaveformPyramid.RawDataPointer(), samplerAudio->m_WaveformPyramid.Count());
	
}

//----------------------------------------------------------------------------

__AEGP_PK_END
