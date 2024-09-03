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
#include "FModBillboardingBatchPolicy.h"

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

bool	CFModRendererBatch_CPU::AllocBuffers(PopcornFX::SRenderContext &ctx)
{
	(void)ctx;
	PK_ASSERT(m_DrawPass != null);
	PK_ASSERT(!m_DrawPass->m_Views.Empty());
	return true;
}

//----------------------------------------------------------------------------

bool	CFModRendererBatch_CPU::EmitDrawCall(PopcornFX::SRenderContext &ctx, const SDrawCallDesc &toEmit)
{
	PK_NAMEDSCOPEDPROFILE("CFModRendererBatch_CPU::EmitDrawCall Sound");

	const SAudioRenderContext	&ctxAudio = *static_cast<SAudioRenderContext*>(&ctx);

	if (m_SoundPlayer == null)
		return false;

	const CFloat4x4	invViewmatrix = !m_DrawPass->m_Views.Empty() ? m_DrawPass->m_Views.First().m_InvViewMatrix : CFloat4x4::IDENTITY;

	// Sounds have no tasks to setup, and no vertex buffers.
	// Iterate on all draw requests
	const u32	drCount = toEmit.m_DrawRequests.Count();
	for (u32 dri = 0; dri < drCount; ++dri)
	{
		PK_ASSERT(toEmit.m_DrawRequests[dri] != null);
		const Drawers::SSound_DrawRequest			*dr = static_cast<const Drawers::SSound_DrawRequest*>(toEmit.m_DrawRequests[dri]);
		const Drawers::SSound_BillboardingRequest	&br = dr->m_BB;

		CSoundResource			*sCache = static_cast<CSoundResource*>(toEmit.m_RendererCaches[dri].Get());
		if (!PK_VERIFY(sCache != null))
		{
			CLog::Log(PK_ERROR, "Invalid sound cache instance");
			return false;
		}

		if (sCache->m_SoundData == null)
		{
			// The sound failed to load, but don't assert and don't spam the log
			continue;
		}

		const CParticleStreamToRender_MainMemory	*streamToRender = dr->StreamToRender_MainMemory();
		PK_ASSERT(streamToRender != null);

		// Map the sound-pool with particles
		for (u32 pageIdx = 0; pageIdx < streamToRender->PageCount(); ++pageIdx)
		{
			const CParticlePageToRender_MainMemory	*page = streamToRender->Page(pageIdx);
			PK_ASSERT(page != null);
			if (page->Culled())
				continue;

			const u32	partCount = page->InputParticleCount();
			const u8	enabledTrue = u8(-1);

			PK_ASSERT(br.m_InvLifeStreamId.Valid());
			PK_ASSERT(br.m_LifeRatioStreamId.Valid());
			PK_ASSERT(br.m_PositionStreamId.Valid());
			TStridedMemoryView<const float>		bufLRt = page->StreamForReading<float>(br.m_LifeRatioStreamId);
			TStridedMemoryView<const float>		bufLif = page->StreamForReading<float>(br.m_InvLifeStreamId);
			TStridedMemoryView<const CFloat3>	bufPos = page->StreamForReading<CFloat3>(br.m_PositionStreamId);

			TStridedMemoryView<const CFloat3>	bufVel = (br.m_VelocityStreamId.Valid()) ? page->StreamForReading<CFloat3>(br.m_VelocityStreamId) : TStridedMemoryView<const CFloat3>(&CFloat4::ZERO.xyz(), partCount, 0);
			TStridedMemoryView<const float>		bufVol = (br.m_VolumeStreamId.Valid()) ? page->StreamForReading<float>(br.m_VolumeStreamId) : TStridedMemoryView<const float>(&CFloat4::ZERO.x(), partCount, 0);
			TStridedMemoryView<const float>		bufRad = (br.m_RangeStreamId.Valid()) ? page->StreamForReading<float>(br.m_RangeStreamId) : TStridedMemoryView<const float>(&CFloat4::ZERO.x(), partCount, 0);
			TStridedMemoryView<const u8>		bufEna = (br.m_EnabledStreamId.Valid()) ? page->StreamForReading<bool>(br.m_EnabledStreamId) : TStridedMemoryView<const u8>(&enabledTrue, partCount, 0);

			for (u32 idx = 0; idx < partCount; ++idx)
			{
				if (!bufEna[idx])
					continue;

				const float		ptime = bufLRt[idx] / bufLif[idx];
				if (ptime > sCache->m_Length)
					continue;

				const CFloat3	posRel = invViewmatrix.TransformVector(bufPos[idx]);

				const float		dist = posRel.Length();

				float	volumePerceived = bufVol[idx];
				if (br.m_AttenuationMode == 0)
				{
					volumePerceived *= (bufRad[idx] < 1.e-4) ? 0.f : 1.f - dist / bufRad[idx];
				}
				else if (br.m_AttenuationMode == 1)
				{
					volumePerceived *= (bufRad[idx] < 1.e-4) ? 0.f : 1.1f / (dist / bufRad[idx] + 0.1f) - 1.f;
				}
				if (volumePerceived < 0.001f)
					continue;

				const CFloat3	velRel = invViewmatrix.RotateVector(bufVel[idx]);

				// We have a sound !
				// -> find the closest unused sound-element in the pool
				{
					const s32	sPoolMatchIdx = m_SoundPlayer->FindBestMatchingSoundSlot(sCache, 0.200f * ctxAudio.m_SimSpeed, ptime);
					if (sPoolMatchIdx != -1)
					{
						SSoundElement	&matchSound = (*m_SoundPlayer)[sPoolMatchIdx];
						const float		deltaTime = matchSound.m_PlayTime - ptime;
						const float		deltaTimeAbs = PKAbs(deltaTime);
						matchSound.m_Used = true;
						matchSound.m_Position = posRel;
						matchSound.m_Velocity = velRel;
						matchSound.m_PerceivedVolume = volumePerceived;
						matchSound.m_DopplerLevel = br.m_DopplerFactor;
						// Soft-sync method
						if (deltaTimeAbs > 0.060f * ctxAudio.m_SimSpeed)
						{
							//matchSound.m_PlaySpeed = (1.f - deltaTime / 5.f);
							//CLog::Log(PK_ERROR, "FMOD Soft-sync sound %d by %f s - playspeed = %f", sPoolMatchIdx, deltaTime, matchSound.m_PlaySpeed);
						}
						continue; // next particle
					}
					//if (sPoolMatchIdx == -1 && ptime > 0.050f)
					//	CLog::Log(PK_ERROR, "Sound-Renderer ; fail to get matching sound for particle %d (vol=%f, time=%f, minDt=%f)", idx, volumePerceived, ptime, sPoolMatchDtAbs);
				}

				// Not found, then get a free slot in the pool
				const CGuid	slotId = m_SoundPlayer->GetFreeSoundSlot(sCache);
				if (slotId.Valid())
				{
					SSoundElement	&repSound = (*m_SoundPlayer)[slotId];
					repSound.m_Used = true;
					repSound.m_NeedResync = true;
					repSound.m_Position = posRel;
					repSound.m_Velocity = velRel;
					repSound.m_PlayTime = ptime;
					repSound.m_PerceivedVolume = volumePerceived;
					repSound.m_DopplerLevel = br.m_DopplerFactor;
					repSound.m_PlaySpeed = 1.f;
					repSound.m_Resource = sCache;
				}
			} // end loop on particles
		} // end loop on pages
	} // end loop on draw-request
	return true;
}

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
