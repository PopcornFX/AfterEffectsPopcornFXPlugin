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

#include <pk_render_helpers/include/batch_jobs/rh_batch_jobs_sound_std.h>

#include "PK-SampleLib/SoundIntegrationFMod/SoundPoolCache.h"

//#include "RHICustomTasks.h"

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

class	CFModRendererBatch_CPU : public CRendererBatchJobs_Sound_Std
{
public:
	CFModRendererBatch_CPU(CSoundPoolCache *soundPoolCache) : m_SoundPlayer(soundPoolCache) { }

	virtual bool	AreRenderersCompatible(const CRendererDataBase *, const CRendererDataBase *) const override { return true; }

	virtual bool	AllocBuffers(PopcornFX::SRenderContext &ctx) override;

	virtual bool	EmitDrawCall(PopcornFX::SRenderContext &ctx, const SDrawCallDesc &toEmit) override;

private:
	CSoundPoolCache	*m_SoundPlayer;
};

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
