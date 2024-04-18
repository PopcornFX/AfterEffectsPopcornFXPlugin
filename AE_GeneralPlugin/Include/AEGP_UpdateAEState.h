//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#pragma once

#ifndef __AEGP_UPDATEAESTATE_H__
#define __AEGP_UPDATEAESTATE_H__

#include "AEGP_Define.h"
#include "PopcornFX_Suite.h"

#include "AEGP_World.h"

#include <AE_GeneralPlug.h>
#include <AE_GeneralPlugPanels.h>
#include <A.h>
#include <SPSuites.h>
#include <AE_Macros.h>

__AEGP_PK_BEGIN

//----------------------------------------------------------------------------

struct SLayerHolder;
struct SPendingAttribute;

//----------------------------------------------------------------------------

class CAEUpdater
{
public:
	static A_Err	UpdateLayerAtTime(SLayerHolder *targetLayer, float time, bool isSeeking);

	static bool		GetLightsAtTime(SLayerHolder *layer, A_Time &AETime, TArray<SLightDesc> &lights);
	static bool		GetCameraViewMatrixAtTime(SLayerHolder *layer, CFloat4x4 &view, CFloat4 &pos, A_Time &AETime, float &cameraZoom);

	static int		s_AttributeIndexes[__Attribute_Parameters_Count];
	static int		s_EmitterIndexes[__Effect_Parameters_Count];
	static int		s_SamplerIndexes[__AttributeSamplerType_Parameters_Count];
private:
	CAEUpdater();
	~CAEUpdater();

	static A_Err	_UpdateLayerAtTime(SLayerHolder *targetLayer, A_Time &AETime, bool isSeeking);

	static A_Err	_UpdateSamplerAtTime(SLayerHolder *targetLayer, SPendingAttribute *sampler, AEGP_EffectRefH effect, A_Time &AETime, bool isSeeking);
	static A_Err	_UpdateAttributeAtTime(SLayerHolder *targetLayer, SPendingAttribute *attribute, AEGP_EffectRefH effect, A_Time &AETime, bool isSeeking);
	static A_Err	_UpdateEmitterAtTime(SLayerHolder *layer, AEGP_EffectRefH effectRef, A_Time &AETime, bool isSeeking);
	static bool		_SetupAudioSampler(SLayerHolder *targetLayer, AEGP_LayerIDVal layerID, A_Time &AETime, SSamplerAudio *samplerAudio, bool isSeeking);

	template<class T>
	static A_Err	_GetParamsStreamValueAtTime(s32 idx, AEGP_EffectRefH effectRef, A_Time &AETime, T &out)
	{
		A_Err				result = A_Err_NONE;
		CPopcornFXWorld		&PKFXWorld = CPopcornFXWorld::Instance();
		AEGP_SuiteHandler	suites(PKFXWorld.GetAESuites());
		AEGP_StreamRefH		streamHandler;
		AEGP_StreamValue2 	value;
		AEGP_StreamType		stream_type;

		result |= suites.StreamSuite5()->AEGP_GetNewEffectStreamByIndex(PKFXWorld.GetPluginID(), effectRef, idx, &streamHandler);

		AEFX_CLR_STRUCT(value);
		if (!PK_VERIFY(streamHandler != null))
			return result;

		result |= suites.StreamSuite5()->AEGP_GetStreamType(streamHandler, &stream_type);
		if (AEGP_StreamType_NO_DATA == stream_type)
		{
			PK_ASSERT_NOT_REACHED();
			result |= suites.StreamSuite5()->AEGP_DisposeStream(streamHandler);
			return result;
		}
		result |= suites.StreamSuite5()->AEGP_GetNewStreamValue(PKFXWorld.GetPluginID(), streamHandler, AEGP_LTimeMode_CompTime, &AETime, false, &value);

		out = *reinterpret_cast<T*>(&value.val.one_d);

		result |= suites.StreamSuite5()->AEGP_DisposeStreamValue(&value);
		result |= suites.StreamSuite5()->AEGP_DisposeStream(streamHandler);

		return result;
	}
};

//----------------------------------------------------------------------------


__AEGP_PK_END

#endif
