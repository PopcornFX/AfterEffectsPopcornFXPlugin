//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#include "ae_precompiled.h"

#include "AEGP_UpdateAEState.h"

#include "AEGP_World.h"
#include "AEGP_LayerHolder.h"
#include "AEGP_AEPKConversion.h"

#include <A.h>

#include <pk_maths/include/pk_maths_transforms.h>

__AEGP_PK_BEGIN


int		CAEUpdater::s_AttributeIndexes[__Attribute_Parameters_Count];
int		CAEUpdater::s_EmitterIndexes[__Effect_Parameters_Count];
int		CAEUpdater::s_SamplerIndexes[__AttributeSamplerType_Parameters_Count];

//----------------------------------------------------------------------------

CAEUpdater::CAEUpdater()
{
}

//----------------------------------------------------------------------------

CAEUpdater::~CAEUpdater()
{
}

//----------------------------------------------------------------------------

A_Err	CAEUpdater::UpdateLayerAtTime(SLayerHolder *targetLayer, float time, bool isSeeking /*=false*/)
{
	s32				targetTime = (s32)(time * (float)targetLayer->m_TimeScale);
	s32				toEndOfFrame = targetTime % targetLayer->m_TimeStep;
	A_Time			AETime;

	if (targetTime < 0)
		AETime.value = 0;
	else if ((u32)toEndOfFrame > (targetLayer->m_TimeStep / 2))
		AETime.value = targetTime + (targetLayer->m_TimeStep - toEndOfFrame); // Round to next frame
	else
		AETime.value = targetTime - toEndOfFrame; // Round to previous frame
	AETime.scale = targetLayer->m_TimeScale;
	return _UpdateLayerAtTime(targetLayer, AETime, isSeeking);
}

//----------------------------------------------------------------------------

A_Err	CAEUpdater::_UpdateLayerAtTime(SLayerHolder *targetLayer, A_Time &AETime, bool isSeeking /*=false*/)
{
	PK_ASSERT(targetLayer != null);

	PK_SCOPEDPROFILE();

	A_Err				frameAborted = A_Err_NONE;
	CFloat4x4			viewMatrix;
	CFloat4				cameraPos;
	CPopcornFXWorld		&PKFXWorld = CPopcornFXWorld::Instance();
	AEGP_SuiteHandler	suites(PKFXWorld.GetAESuites());
	
	float				cameraZoom = 0.f;
	GetCameraViewMatrixAtTime(targetLayer, viewMatrix, cameraPos, AETime, cameraZoom);
	targetLayer->m_Scene->SetCameraViewMatrix(viewMatrix, cameraPos, cameraZoom);

	A_long				effectCount = 0;

	frameAborted |= suites.EffectSuite4()->AEGP_GetLayerNumEffects(targetLayer->m_EffectLayer, &effectCount);

	for (A_long j = effectCount - 1; j >= 0; --j)
	{
		AEGP_EffectRefH				effectRef = null;
		AEGP_InstalledEffectKey		installedKey;

		frameAborted |= suites.EffectSuite4()->AEGP_GetLayerEffectByIndex(PKFXWorld.GetPluginID(), targetLayer->m_EffectLayer, j, &effectRef);
		frameAborted |= suites.EffectSuite4()->AEGP_GetInstalledKeyFromLayerEffect(effectRef, &installedKey);

		if (installedKey == PKFXWorld.GetPluginEffectKey(EPKChildPlugins::EMITTER))
		{
			frameAborted |= _UpdateEmitterAtTime(targetLayer, effectRef, AETime, isSeeking);
			if (frameAborted != A_Err_NONE)
				return frameAborted;
		}
		else if (installedKey == PKFXWorld.GetPluginEffectKey(EPKChildPlugins::ATTRIBUTE))
		{
			CStringId id = PKFXWorld.GetAttributeID(effectRef);
			if (targetLayer->m_SpawnedAttributes.Contains(id))
			{
				SPendingAttribute *attribute = targetLayer->m_SpawnedAttributes[id];

				PK_ASSERT(attribute != null);
				frameAborted |= _UpdateAttributeAtTime(targetLayer, attribute, effectRef, AETime, isSeeking);
				if (frameAborted != A_Err_NONE)
					return frameAborted;
			}
		}
		else if (installedKey == PKFXWorld.GetPluginEffectKey(EPKChildPlugins::SAMPLER))
		{
			CStringId id = PKFXWorld.GetAttributeSamplerID(effectRef);
			if (targetLayer->m_SpawnedAttributesSampler.Contains(id))
			{
				SPendingAttribute *smplr = targetLayer->m_SpawnedAttributesSampler[id];

				PK_ASSERT(smplr != null);
				PK_ASSERT(smplr->m_Desc != null);

				SAttributeSamplerDesc	*samplerDescriptor = static_cast<SAttributeSamplerDesc*>(smplr->m_Desc);

				if (smplr->m_PKDesc == null)
				{
					switch (samplerDescriptor->m_Type)
					{
					case AttributeSamplerType_Geometry:
						smplr->m_PKDesc = PK_NEW(SSamplerShape);
						break;
					case AttributeSamplerType_Text:
						smplr->m_PKDesc = PK_NEW(SSamplerText);
						break;
					case AttributeSamplerType_Image:
						smplr->m_PKDesc = PK_NEW(SSamplerImage);
						break;
					case AttributeSamplerType_Audio:
					{
						SSamplerAudio	*sampler = PK_NEW(SSamplerAudio);
						smplr->m_PKDesc = sampler;
						sampler->m_Name = CStringId(((SAudioSamplerDescriptor*)samplerDescriptor->m_Descriptor)->m_ChannelGroup.c_str());
						break;
					}
					case AttributeSamplerType_VectorField:
						smplr->m_PKDesc = PK_NEW(SSamplerVectorField);
						break;
					default:
						break;
					}
				}
				PK_ASSERT(smplr->m_PKDesc != null);
				frameAborted |= _UpdateSamplerAtTime(targetLayer, smplr, effectRef, AETime, isSeeking);
				if (frameAborted != A_Err_NONE)
					return frameAborted;
			}
		}
		frameAborted |= suites.EffectSuite4()->AEGP_DisposeEffect(effectRef);
	}
	if (!PK_VERIFY(frameAborted == A_Err_NONE))
		return frameAborted;
	if (!targetLayer->m_Scene->UpdateAttributes(targetLayer))
		return A_Err_GENERIC;
	return A_Err_NONE;
}

//----------------------------------------------------------------------------

bool	CAEUpdater::GetCameraViewMatrixAtTime(SLayerHolder *layer, CFloat4x4 &view, CFloat4 &pos, A_Time &AETime, float &cameraZoom)
{
	PK_SCOPEDPROFILE();

	CPopcornFXWorld		&PKFXWorld = CPopcornFXWorld::Instance();
	AEGP_SuiteHandler	suites(PKFXWorld.GetAESuites());
	AEGP_LayerH			camera_layerH = null;
	A_Matrix4			matrix;
	PF_Err				result = A_Err_NONE;
	CFloat4x4			viewMatrix = CFloat4x4::IDENTITY;
	SEmitterDesc		*emitter = layer->m_SpawnedEmitter.m_Desc;

	camera_layerH = layer->m_CameraLayer;
	if (!result && camera_layerH)
	{
		result |= suites.LayerSuite5()->AEGP_GetLayerToWorldXform(camera_layerH, &AETime, &matrix);
		if (!result)
		{
			emitter->m_Camera.m_AECameraPresent = true;
			AEGP_StreamVal	stream_val;
			AEFX_CLR_STRUCT(stream_val);

			result |= suites.StreamSuite2()->AEGP_GetLayerStreamValue(camera_layerH,
				AEGP_LayerStream_ZOOM,
				AEGP_LTimeMode_CompTime,
				&AETime,
				FALSE,
				&stream_val,
				null);
			cameraZoom = (float)stream_val.one_d;
			AAEToPK(matrix, viewMatrix);

			pos = viewMatrix.WAxis();
#if defined(PK_SCALE_DOWN)
			pos.xyz() = pos.xyz() / layer->m_ScaleFactor;
			viewMatrix.WAxis() = pos;
#endif
			viewMatrix.Invert();
			view = viewMatrix;
		}
	}
	if (!PK_VERIFY(result == A_Err_NONE))
		return false;
	return true;
}

//----------------------------------------------------------------------------

bool	CAEUpdater::_SetupAudioSampler(SLayerHolder *targetLayer, AEGP_LayerIDVal layerID, A_Time &AETime, SSamplerAudio *samplerAudio, bool isSeeking)
{
	(void)isSeeking;
	CPopcornFXWorld			&PKFXWorld = CPopcornFXWorld::Instance();
	AEGP_SuiteHandler		suites(PKFXWorld.GetAESuites());
	A_Err					frameAborted = A_Err_NONE;

	if (layerID != AEGP_LayerIDVal_NONE)
	{
		AEGP_LayerH				layer;
		AEGP_CompH				compH;
		AEGP_ItemH				layerItem = null;
		AEGP_SoundDataFormat	soundFormat;
		AEGP_SoundDataH			soundData = null;
		A_Time					duration;
		A_Time					layerTime;

		frameAborted |= suites.LayerSuite5()->AEGP_GetLayerParentComp(targetLayer->m_EffectLayer, &compH);
		frameAborted |= suites.LayerSuite7()->AEGP_GetLayerFromLayerID(compH, layerID, &layer);
		frameAborted |= suites.LayerSuite5()->AEGP_GetLayerSourceItem(layer, &layerItem);
		frameAborted |= suites.LayerSuite5()->AEGP_ConvertCompToLayerTime(layer, &AETime, &layerTime);

		if (layerItem == null)
			return false;

		soundFormat.encoding = AEGP_SoundEncoding_FLOAT;
		soundFormat.num_channelsL = 1;
		soundFormat.sample_rateF = 48000.0;

		// We compute the expected timestep to get 2048 values (We are using 2x the number of samples in the PopcornFX editor to get proper spectrum analysis):
		A_FpLong	secondsToSample = 2048.0 / soundFormat.sample_rateF;

		duration.value = static_cast<A_long>(secondsToSample * static_cast<A_FpLong>(targetLayer->m_TimeScale)) + 1;
		duration.scale = targetLayer->m_TimeScale;
		frameAborted |= suites.RenderSuite5()->AEGP_RenderNewItemSoundData(layerItem, &layerTime, &duration, &soundFormat, null, null, &soundData);

		if (soundData != null)
		{
			void	*audioSamples;
			A_long	numSamples = 0;

			frameAborted |= suites.SoundDataSuite1()->AEGP_LockSoundDataSamples(soundData, &audioSamples);
			frameAborted |= suites.SoundDataSuite1()->AEGP_GetNumSamples(soundData, &numSamples);

			if (!PK_VERIFY(frameAborted == A_Err_NONE))
			{
				frameAborted |= suites.SoundDataSuite1()->AEGP_UnlockSoundDataSamples(soundData);
				frameAborted |= suites.SoundDataSuite1()->AEGP_DisposeSoundData(soundData);
			}
			else
			{
				samplerAudio->m_SoundData = soundData;
				// Align to the previous power of 2:
				const int alignedSampleCount = 1 << IntegerTools::Log2(numSamples);
				samplerAudio->m_InputSampleCount = alignedSampleCount;
				samplerAudio->m_Waveform = (float*)audioSamples;
				samplerAudio->m_Dirty = true;
				return true;
			}
		}
	}
	samplerAudio->m_SoundData = null;
	samplerAudio->m_InputSampleCount = 0;
	samplerAudio->m_Waveform = null;
	return false;
}

//----------------------------------------------------------------------------

A_Err	CAEUpdater::_UpdateSamplerAtTime(SLayerHolder *targetLayer, SPendingAttribute *sampler, AEGP_EffectRefH effectRef, A_Time &AETime, bool isSeeking /*=false*/)
{
	if (sampler == null || sampler->m_Desc == null)
		return A_Err_NONE;
	PK_SCOPEDPROFILE();
	A_Err					frameAborted = A_Err_NONE;
	CPopcornFXWorld			&PKFXWorld = CPopcornFXWorld::Instance();
	AEGP_SuiteHandler		suites(PKFXWorld.GetAESuites());
	SAttributeSamplerDesc	*samplerDescriptor = static_cast<SAttributeSamplerDesc*>(sampler->m_Desc);
	SBaseSamplerDescriptor	*AEdescriptor = samplerDescriptor->m_Descriptor;
	SSamplerBase			*PKdescriptor = sampler->m_PKDesc;

	if (!PK_VERIFY(AEdescriptor != null))
		return A_Err_GENERIC;

	switch (samplerDescriptor->m_Type)
	{
	case AttributeSamplerType_Geometry:
	{
		SShapeSamplerDescriptor		*shapeDescriptor = static_cast<SShapeSamplerDescriptor*>(AEdescriptor);
		ESamplerShapeType			shapeType;
		double						value;
		double						dimension[3] = { 0.0, 0.0, 0.0 };

		frameAborted |= _GetParamsStreamValueAtTime(AttributeSamplerType_Parameters_Shapes, effectRef, AETime, value);
		shapeType = (ESamplerShapeType)(int)(value - 1); // Because AE Popup are weird.

		if (shapeType == SamplerShapeType_Box)
		{
			frameAborted |= _GetParamsStreamValueAtTime(AttributeSamplerType_Parameters_Box_Size_X, effectRef, AETime, dimension[0]);
			frameAborted |= _GetParamsStreamValueAtTime(AttributeSamplerType_Parameters_Box_Size_Y, effectRef, AETime, dimension[1]);
			frameAborted |= _GetParamsStreamValueAtTime(AttributeSamplerType_Parameters_Box_Size_Z, effectRef, AETime, dimension[2]);
		}
		else if (shapeType == SamplerShapeType_Sphere)
		{
			frameAborted |= _GetParamsStreamValueAtTime(AttributeSamplerType_Parameters_Sphere_Radius, effectRef, AETime, dimension[0]);
			frameAborted |= _GetParamsStreamValueAtTime(AttributeSamplerType_Parameters_Sphere_InnerRadius, effectRef, AETime, dimension[1]);
		}
		else if (shapeType == SamplerShapeType_Ellipsoid)
		{
			frameAborted |= _GetParamsStreamValueAtTime(AttributeSamplerType_Parameters_Ellipsoid_Radius, effectRef, AETime, dimension[0]);
			frameAborted |= _GetParamsStreamValueAtTime(AttributeSamplerType_Parameters_Ellipsoid_InnerRadius, effectRef, AETime, dimension[1]);
		}
		else if (shapeType == SamplerShapeType_Cylinder)
		{
			frameAborted |= _GetParamsStreamValueAtTime(AttributeSamplerType_Parameters_Cylinder_Radius, effectRef, AETime, dimension[0]);
			frameAborted |= _GetParamsStreamValueAtTime(AttributeSamplerType_Parameters_Cylinder_Height, effectRef, AETime, dimension[1]);
			frameAborted |= _GetParamsStreamValueAtTime(AttributeSamplerType_Parameters_Cylinder_InnerRadius, effectRef, AETime, dimension[2]);
		}
		else if (shapeType == SamplerShapeType_Capsule)
		{
			frameAborted |= _GetParamsStreamValueAtTime(AttributeSamplerType_Parameters_Capsule_Radius, effectRef, AETime, dimension[0]);
			frameAborted |= _GetParamsStreamValueAtTime(AttributeSamplerType_Parameters_Capsule_Height, effectRef, AETime, dimension[1]);
			frameAborted |= _GetParamsStreamValueAtTime(AttributeSamplerType_Parameters_Capsule_InnerRadius, effectRef, AETime, dimension[2]);
		}
		else if (shapeType == SamplerShapeType_Cone)
		{
			frameAborted |= _GetParamsStreamValueAtTime(AttributeSamplerType_Parameters_Cone_Radius, effectRef, AETime, dimension[0]);
			frameAborted |= _GetParamsStreamValueAtTime(AttributeSamplerType_Parameters_Cone_Height, effectRef, AETime, dimension[1]);
		}
		else if (shapeType == SamplerShapeType_Mesh)
		{
			frameAborted |= _GetParamsStreamValueAtTime(AttributeSamplerType_Parameters_Mesh_Scale, effectRef, AETime, dimension[0]);
		}
		shapeDescriptor->m_Type = shapeType;
		shapeDescriptor->m_Dimension[0] = (float)dimension[0];
		shapeDescriptor->m_Dimension[1] = (float)dimension[1];
		shapeDescriptor->m_Dimension[2] = (float)dimension[2];
		if (shapeType == SamplerShapeType_Mesh)
		{
			if (shapeDescriptor->m_Path.compare(samplerDescriptor->m_ResourcePath))
			{
				shapeDescriptor->m_Path = samplerDescriptor->m_ResourcePath;
				PKdescriptor->m_Dirty = true;
			}

			double	doubleValue;
			frameAborted |= _GetParamsStreamValueAtTime(AttributeSamplerType_Parameters_Mesh_Bind_Backdrop, effectRef, AETime, doubleValue);
			shapeDescriptor->m_BindToBackdrop = (bool)doubleValue;
			if (shapeDescriptor->m_BindToBackdrop)
			{
				frameAborted |= _GetParamsStreamValueAtTime(AttributeSamplerType_Parameters_Mesh_Bind_Backdrop_Weighted_Enabled, effectRef, AETime, doubleValue);
				shapeDescriptor->m_WeightedSampling = (bool)doubleValue;

				frameAborted |= _GetParamsStreamValueAtTime(AttributeSamplerType_Parameters_Mesh_Bind_Backdrop_ColorStreamID, effectRef, AETime, doubleValue);
				shapeDescriptor->m_ColorStreamID = (unsigned int)doubleValue;

				frameAborted |= _GetParamsStreamValueAtTime(AttributeSamplerType_Parameters_Mesh_Bind_Backdrop_WeightStreamID, effectRef, AETime, doubleValue);
				shapeDescriptor->m_WeightStreamID = (unsigned int)doubleValue;
			}
			targetLayer->m_Scene->SetSkinnedBackdropParams(shapeDescriptor->m_BindToBackdrop, shapeDescriptor->m_WeightedSampling, shapeDescriptor->m_ColorStreamID, shapeDescriptor->m_WeightStreamID);

		}
		else
			PKdescriptor->m_Dirty = true;
		break;
	}
	case AttributeSamplerType_Text:
	{
		STextSamplerDescriptor	*textDesc = static_cast<STextSamplerDescriptor*>(AEdescriptor);

		AEGP_LayerIDVal		layerID;
		frameAborted |= _GetParamsStreamValueAtTime<AEGP_LayerIDVal>(AttributeSamplerType_Layer_Pick, effectRef, AETime, layerID);

		if (layerID != AEGP_LayerIDVal_NONE)
		{
			AEGP_LayerH			textLayer;
			AEGP_CompH			compH;

			frameAborted |= suites.LayerSuite5()->AEGP_GetLayerParentComp(targetLayer->m_EffectLayer, &compH);
			frameAborted |= suites.LayerSuite7()->AEGP_GetLayerFromLayerID(compH, layerID, &textLayer);

			AEGP_StreamValue2	streamValue;
			AEGP_StreamType		streamType;
			AEGP_ObjectType		layerType;
			AEGP_StreamRefH		textStream = null;

			frameAborted |= suites.LayerSuite8()->AEGP_GetLayerObjectType(textLayer, &layerType);
			if (!PK_VERIFY(frameAborted == A_Err_NONE) || layerType != AEGP_ObjectType_TEXT)
				break;

			frameAborted |= suites.StreamSuite5()->AEGP_GetNewLayerStream(PKFXWorld.GetPluginID(), textLayer, AEGP_LayerStream_SOURCE_TEXT, &textStream);
			frameAborted |= suites.StreamSuite5()->AEGP_GetNewStreamValue(PKFXWorld.GetPluginID(), textStream, AEGP_LTimeMode_LayerTime, &AETime, TRUE, &streamValue);
			frameAborted |= suites.StreamSuite2()->AEGP_GetStreamType(textStream, &streamType);
			if (!PK_VERIFY(frameAborted == A_Err_NONE) || streamType != AEGP_StreamType_TEXT_DOCUMENT)
				break;

			AEGP_MemHandle	textHandle = null;
			frameAborted |= suites.TextDocumentSuite1()->AEGP_GetNewText(PKFXWorld.GetPluginID(), streamValue.val.text_documentH, &textHandle);

			aechar_t			*retrievedText;
			CString				text = "";

			if (textHandle != null)
			{
				frameAborted |= suites.MemorySuite1()->AEGP_LockMemHandle(textHandle, reinterpret_cast<void **>(&retrievedText));

				WCharToCString(retrievedText, &text);

				frameAborted |= suites.MemorySuite1()->AEGP_UnlockMemHandle(textHandle);
				frameAborted |= suites.MemorySuite1()->AEGP_FreeMemHandle(textHandle);

				textDesc->m_Data = text.Data();
				PKdescriptor->m_Dirty = true;
			}
			frameAborted |= suites.StreamSuite5()->AEGP_DisposeStreamValue(&streamValue);
			frameAborted |= suites.StreamSuite5()->AEGP_DisposeStream(textStream);
		}
		else
			textDesc->m_Data = "";
		break;
	}
	case AttributeSamplerType_Image:
	{
		SSamplerImage			*pkImageDesc = static_cast<SSamplerImage*>(PKdescriptor);

		if (!PK_VERIFY(pkImageDesc != null))
			break;

		double	boolValue;
		bool	sampleOnSeek = false, sampleOnce = false;
		frameAborted |= _GetParamsStreamValueAtTime(AttributeSamplerType_Layer_Sample_Once, effectRef, AETime, boolValue);
		sampleOnce = (bool)boolValue;
		frameAborted |= _GetParamsStreamValueAtTime(AttributeSamplerType_Layer_Sample_Seeking, effectRef, AETime, boolValue);
		sampleOnSeek = (bool)boolValue;

		double	intValue;
		int	downSampleX;
		int	downSampleY;
		frameAborted |= _GetParamsStreamValueAtTime(AttributeSamplerType_Layer_Sample_Downsampling_X, effectRef, AETime, intValue);
		downSampleX = (int)intValue;
		frameAborted |= _GetParamsStreamValueAtTime(AttributeSamplerType_Layer_Sample_Downsampling_Y, effectRef, AETime, intValue);
		downSampleY = (int)intValue;

		if ((!sampleOnSeek && isSeeking) ||
			(sampleOnce && pkImageDesc->m_TextureData != null && pkImageDesc->m_TextureData->DataSizeInBytes() != 0))
		{
			pkImageDesc->m_Dirty = false;
			break;
		}

		AEGP_LayerIDVal		layerID;
		frameAborted |= _GetParamsStreamValueAtTime<AEGP_LayerIDVal>(AttributeSamplerType_Layer_Pick, effectRef, AETime, layerID);

		if (layerID != AEGP_LayerIDVal_NONE)
		{
			AEGP_LayerH					imageLayer;
			AEGP_CompH					compH;
			AEGP_LayerRenderOptionsH	renderOptions = null;
			AEGP_WorldType				worldType = AEGP_WorldType_NONE;
			AEGP_FrameReceiptH			inputFrame = null;
			AEGP_WorldH					inputworld = null;
			A_Time						layerDuration, layerInPoint;

			frameAborted |= suites.LayerSuite5()->AEGP_GetLayerParentComp(targetLayer->m_EffectLayer, &compH);
			frameAborted |= suites.LayerSuite7()->AEGP_GetLayerFromLayerID(compH, layerID, &imageLayer);

			frameAborted |= suites.LayerSuite8()->AEGP_GetLayerInPoint(imageLayer, AEGP_LTimeMode_CompTime, &layerInPoint);
			frameAborted |= suites.LayerSuite8()->AEGP_GetLayerDuration(imageLayer, AEGP_LTimeMode_CompTime, &layerDuration);

			if (!PK_VERIFY(frameAborted == A_Err_NONE))
				return frameAborted;

			double	aeTime = (double)AETime.value / (double)AETime.scale;
			double	layInPtTime = (double)layerInPoint.value / (double)layerInPoint.scale;
			double	layDurTime = (double)layerDuration.value / (double)layerDuration.scale;

			if ((aeTime >= layInPtTime) &&
				(aeTime < (layInPtTime + layDurTime)))
			{
				A_Time layerTime;

				layerTime.scale = AETime.scale;
				layerTime.value = AETime.value - layerInPoint.value;
				frameAborted |= suites.LayerRenderOptionsSuite2()->AEGP_NewFromLayer(PKFXWorld.GetPluginID(), imageLayer, &renderOptions);
				frameAborted |= suites.LayerRenderOptionsSuite2()->AEGP_GetWorldType(renderOptions, &worldType);

				frameAborted |= suites.LayerRenderOptionsSuite2()->AEGP_SetTime(renderOptions, layerTime);

				frameAborted |= suites.LayerRenderOptionsSuite2()->AEGP_SetDownsampleFactor(renderOptions, (A_short)downSampleX, (A_short)downSampleY);

				if (frameAborted != A_Err_NONE)
					return frameAborted;

				// This fails if the frame is cancelled:
				frameAborted |= suites.RenderSuite5()->AEGP_RenderAndCheckoutLayerFrame(renderOptions, null, null, &inputFrame);

				if (frameAborted != A_Err_NONE)
					return frameAborted;

				frameAborted |= suites.RenderSuite5()->AEGP_GetReceiptWorld(inputFrame, &inputworld);
				if (inputworld != null)
				{
					A_long		width, height;
					A_u_long	rowbyte;

					frameAborted |= suites.WorldSuite3()->AEGP_GetSize(inputworld, &width, &height);
					frameAborted |= suites.WorldSuite3()->AEGP_GetRowBytes(inputworld, &rowbyte);

					if (worldType == AEGP_WorldType_32)
					{
						PF_Pixel32	*worldData = null;

						pkImageDesc->m_PixelFormat = CImage::EFormat::Format_Fp32RGBA;
						pkImageDesc->m_Width = width;
						pkImageDesc->m_Height = height;
						pkImageDesc->m_SizeInBytes = width * height * sizeof(PF_Pixel32);
						pkImageDesc->m_TextureData = CRefCountedMemoryBuffer::AllocAligned(pkImageDesc->m_SizeInBytes, 0x10);

						frameAborted |= suites.WorldSuite3()->AEGP_GetBaseAddr32(inputworld, &worldData);

						if (worldData != null)
						{
							//input is ARGB;
							u8		*sptr = (u8*)worldData;
							u8		*dptr = (u8*)pkImageDesc->m_TextureData->Data<u8*>();

							for (s32 i = 0; i < height; ++i)
							{
								for (s32 j = 0; j < width; ++j)
								{
									PF_Pixel32 *dst = (PF_Pixel32*)&(dptr[j * sizeof(PF_Pixel32) + i * width * sizeof(PF_Pixel32)]);
									PF_Pixel32 *src = (PF_Pixel32*)&(sptr[j * sizeof(PF_Pixel32) + i * rowbyte]);

									CFloat4 value = CFloat4(PKSample::ConvertSRGBToLinear(CFloat3(src->red, src->green, src->blue)), src->alpha);

									value = PKSaturate(value);

									/*0*/dst->alpha = value.x();
									/*1*/dst->red = value.y();
									/*2*/dst->green = value.z();
									/*3*/dst->blue = value.w();
								}
							}
						}
					}
					else if (worldType == AEGP_WorldType_16)
					{
						PF_Pixel16	*worldData = null;

						pkImageDesc->m_PixelFormat = CImage::EFormat::Format_Fp16RGBA;
						pkImageDesc->m_Width = width;
						pkImageDesc->m_Height = height;
						pkImageDesc->m_SizeInBytes = width * height * sizeof(PF_Pixel16);
						pkImageDesc->m_TextureData = CRefCountedMemoryBuffer::AllocAligned(pkImageDesc->m_SizeInBytes, 0x10);

						frameAborted |= suites.WorldSuite3()->AEGP_GetBaseAddr16(inputworld, &worldData);

						if (worldData != null)
						{
							//input is ARGB;
							u8		*sptr = (u8*)worldData;
							u8		*dptr = (u8*)pkImageDesc->m_TextureData->Data<u8*>();

							for (s32 i = 0; i < height; ++i)
							{
								for (s32 j = 0; j < width; ++j)
								{
									PF_Pixel16 *dst = (PF_Pixel16*)&(dptr[j * sizeof(PF_Pixel16) + i * width * sizeof(PF_Pixel16)]);
									PF_Pixel16 *src = (PF_Pixel16*)&(sptr[j * sizeof(PF_Pixel16) + i * rowbyte]);

									CFloat4 value = CFloat4(PKSample::ConvertSRGBToLinear(CFloat3(src->red, src->green, src->blue)), src->alpha);

									value = PKSaturate(value);
									/*0*/dst->alpha = (A_u_short)value.x();
									/*1*/dst->red = (A_u_short)value.y();
									/*2*/dst->green = (A_u_short)value.z();
									/*3*/dst->blue = (A_u_short)value.w();
								}
							}
						}
					}
					else if (worldType == AEGP_WorldType_8)
					{
						PF_Pixel8	*worldData = null;

						pkImageDesc->m_PixelFormat = CImage::EFormat::Format_BGRA8;
						pkImageDesc->m_Width = width;
						pkImageDesc->m_Height = height;
						pkImageDesc->m_SizeInBytes = width * height * sizeof(PF_Pixel8);
						pkImageDesc->m_TextureData = CRefCountedMemoryBuffer::AllocAligned(pkImageDesc->m_SizeInBytes, 0x10);

						frameAborted |= suites.WorldSuite3()->AEGP_GetBaseAddr8(inputworld, &worldData);

						if (worldData != null)
						{
							//input is ARGB;
							u8		*sptr = (u8*)worldData;
							u8		*dptr = (u8*)pkImageDesc->m_TextureData->Data<u8*>();

							for (s32 i = 0; i < height; ++i)
							{
								for (s32 j = 0; j < width; ++j)
								{
									PF_Pixel8 *dst = (PF_Pixel8*)&(dptr[j * sizeof(PF_Pixel8) + i * width * sizeof(PF_Pixel8)]);
									PF_Pixel8 *src = (PF_Pixel8*)&(sptr[j * sizeof(PF_Pixel8) + i * rowbyte]);

									CUbyte4 value = CUbyte4(/*PKSample::ConvertSRGBToLinear*/(CUbyte3(src->blue, src->green, src->red)), src->alpha);

									value = PKClamp(value, CUbyte4::ZERO, CUbyte4(255));
									/*0*/dst->alpha = value.x();
									/*1*/dst->red = value.y();
									/*2*/dst->green = value.z();
									/*3*/dst->blue = value.w();
								}
							}
						}
					}
				}

				frameAborted |= suites.RenderSuite5()->AEGP_CheckinFrame(inputFrame);
				frameAborted |= suites.LayerRenderOptionsSuite2()->AEGP_Dispose(renderOptions);
			}
			else if (pkImageDesc->m_Width != 1 || pkImageDesc->m_Height != 1)
			{
				pkImageDesc->m_PixelFormat = CImage::EFormat::Format_BGRA8;
				pkImageDesc->m_Width = 1;
				pkImageDesc->m_Height = 1;
				pkImageDesc->m_SizeInBytes = sizeof(PF_Pixel8);
				pkImageDesc->m_TextureData = CRefCountedMemoryBuffer::AllocAligned(pkImageDesc->m_SizeInBytes, 0x10);

				u8		*dptr = (u8*)pkImageDesc->m_TextureData->Data<u8*>();
				PF_Pixel8 *dst = (PF_Pixel8*)&(dptr[0]);
				/*0*/dst->alpha = 0;
				/*1*/dst->red = 0;
				/*2*/dst->green = 0;
				/*3*/dst->blue = 0;
			}

			if (!PK_VERIFY(frameAborted == A_Err_NONE))
				break;
			pkImageDesc->m_Dirty = true;
		}
		else if (pkImageDesc->m_Width != 1 || pkImageDesc->m_Height != 1)
		{
			pkImageDesc->m_PixelFormat = CImage::EFormat::Format_BGRA8;
			pkImageDesc->m_Width = 1;
			pkImageDesc->m_Height = 1;
			pkImageDesc->m_SizeInBytes = sizeof(PF_Pixel8);
			pkImageDesc->m_TextureData = CRefCountedMemoryBuffer::AllocAligned(pkImageDesc->m_SizeInBytes, 0x10);

			u8			*dptr = (u8*)pkImageDesc->m_TextureData->Data<u8*>();
			PF_Pixel8	*dst = (PF_Pixel8*)&(dptr[0]);
			/*0*/dst->alpha = 0;
			/*1*/dst->red = 0;
			/*2*/dst->green = 0;
			/*3*/dst->blue = 0;

			pkImageDesc->m_Dirty = true;
		}
		break;
	}
	case AttributeSamplerType_Audio:
	{
		SSamplerAudio				*pkAudioDesc = static_cast<SSamplerAudio*>(PKdescriptor);

		if (!PK_VERIFY(pkAudioDesc != null))
			break;

		double	value;
		bool	sampleOnSeek = false;

		frameAborted |= _GetParamsStreamValueAtTime(AttributeSamplerType_Layer_Sample_Seeking, effectRef, AETime, value);
		sampleOnSeek = (bool)value;

		if ((!sampleOnSeek && isSeeking))
		{
			pkAudioDesc->m_Dirty = false;
			break;
		}
		AEGP_LayerIDVal		layerID;
		frameAborted |= _GetParamsStreamValueAtTime<AEGP_LayerIDVal>(AttributeSamplerType_Layer_Pick, effectRef, AETime, layerID);
		_SetupAudioSampler(targetLayer, layerID, AETime, pkAudioDesc, isSeeking);
		break;
	}
	case AttributeSamplerType_VectorField:
	{
		SVectorFieldSamplerDescriptor		*vfDescriptor = static_cast<SVectorFieldSamplerDescriptor*>(AEdescriptor);
		{
			AEGP_StreamRefH		streamHandler;
			AEGP_StreamValue2 	value;

			frameAborted |= suites.StreamSuite3()->AEGP_GetNewEffectStreamByIndex(PKFXWorld.GetPluginID(), effectRef, AttributeSamplerType_Parameters_VectorField_Position, &streamHandler);
			AEFX_CLR_STRUCT(value);
			frameAborted |= suites.StreamSuite3()->AEGP_GetNewStreamValue(PKFXWorld.GetPluginID(), streamHandler, AEGP_LTimeMode_CompTime, &AETime, false, &value);
			vfDescriptor->m_Position = A_FloatPoint3{ value.val.three_d.x, value.val.three_d.y, value.val.three_d.z };
			frameAborted |= suites.StreamSuite3()->AEGP_DisposeStreamValue(&value);
			frameAborted |= suites.StreamSuite3()->AEGP_DisposeStream(streamHandler);
		}
		double	value;
		frameAborted |= _GetParamsStreamValueAtTime(AttributeSamplerType_Parameters_VectorField_Strength, effectRef, AETime, value);
		vfDescriptor->m_Strength = (float)value;

		frameAborted |= _GetParamsStreamValueAtTime(AttributeSamplerType_Parameters_VectorField_Interpolation, effectRef, AETime, value);
		EInterpolationType	interpolation = (EInterpolationType)(int)value;
		if (vfDescriptor->m_Interpolation != interpolation)
		{
			vfDescriptor->m_Interpolation = interpolation;
			vfDescriptor->m_ResourceUpdate = true;
		}

		std::string		vectorfieldPath = samplerDescriptor->m_ResourcePath.c_str();
		if (vectorfieldPath != vfDescriptor->m_Path)
			vfDescriptor->m_Path = vectorfieldPath;
		PKdescriptor->m_Dirty = true;
		break;
	}
	default:
		break;
	}
	return frameAborted;
}

//----------------------------------------------------------------------------

A_Err	CAEUpdater::_UpdateAttributeAtTime(SLayerHolder *targetLayer, SPendingAttribute *attribute, AEGP_EffectRefH effectRef, A_Time &AETime, bool isSeeking /*=false*/)
{
	PK_SCOPEDPROFILE();
	(void)targetLayer;
	(void)isSeeking;

	A_Err				result = A_Err_NONE;
	CPopcornFXWorld		&PKFXWorld = CPopcornFXWorld::Instance();
	AEGP_SuiteHandler	suites(PKFXWorld.GetAESuites());

	if (attribute->m_Desc && attribute->m_Desc->m_IsAttribute)
	{
		SAttributeDesc *desc = static_cast<SAttributeDesc*>(attribute->m_Desc);
		if (desc->m_AttributeSemantic == AttributeSemantic_Color)
		{
			float	floatValues[4];

			AEGP_StreamRefH		streamHandler;
			AEGP_StreamValue2 	value;

			result |= suites.StreamSuite3()->AEGP_GetNewEffectStreamByIndex(PKFXWorld.GetPluginID(), effectRef, Attribute_Parameters_Color_RGB, &streamHandler);
			AEFX_CLR_STRUCT(value);
			result |= suites.StreamSuite3()->AEGP_GetNewStreamValue(PKFXWorld.GetPluginID(), streamHandler, AEGP_LTimeMode_CompTime, &AETime, false, &value);
			floatValues[0] = (float)value.val.color.redF;
			floatValues[1] = (float)value.val.color.greenF;
			floatValues[2] = (float)value.val.color.blueF;

			result |= suites.StreamSuite3()->AEGP_DisposeStreamValue(&value);
			result |= suites.StreamSuite3()->AEGP_DisposeStream(streamHandler);

			if (desc->m_Type == AttributeType_Float4 ||
				desc->m_Type == AttributeType_Int4)
			{
				double	floatValue;
				result |= _GetParamsStreamValueAtTime(Attribute_Parameters_Color_A, effectRef, AETime, floatValue);
				floatValues[3] = (float)(floatValue / 100.0f);
			}
			if (desc->m_Type >= AttributeType_Int1 && desc->m_Type <= AttributeType_Int4)
			{
				for (u32 i = 0; i < 4; ++i)
					floatValues[i] *= 255;
			}
			desc->SetValue(&floatValues);
		}
		else
		{
			switch (desc->m_Type)
			{
			case AttributeType_Bool1:
			case AttributeType_Bool2:
			case AttributeType_Bool3:
			case AttributeType_Bool4:
			{
				bool boolValues[4];
				for (s32 i = 0; i < (desc->m_Type + 1) - AttributeType_Bool1; ++i)
				{
					double	boolValue;

					result |= _GetParamsStreamValueAtTime(AttributeType_Bool1 + i, effectRef, AETime, boolValue);
					boolValues[i] = (bool)boolValue;
				}
				desc->SetValue(&boolValues);
				break;
			}
			case AttributeType_Int1:
			case AttributeType_Int2:
			case AttributeType_Int3:
			case AttributeType_Int4:
			{
				int intValues[4];
				for (s32 i = 0; i < (desc->m_Type + 1) - AttributeType_Int1; ++i)
				{
					double	intValue;

					result |= _GetParamsStreamValueAtTime(AttributeType_Int1 + i, effectRef, AETime, intValue);
					intValues[i] = (int)intValue;
				}
				desc->SetValue(&intValues);
				break;
			}
			case AttributeType_Float1:
			case AttributeType_Float2:
			case AttributeType_Float3:
			case AttributeType_Float4:
			{
				float floatValues[4];
				for (s32 i = 0; i < (desc->m_Type + 1) - AttributeType_Float1; ++i)
				{
					double	floatValue;

					result |= _GetParamsStreamValueAtTime(AttributeType_Float1 + i, effectRef, AETime, floatValue);
					floatValues[i] = (float)floatValue;
				}
				desc->SetValue(&floatValues);
				break;
			}
			default:
				break;
			}
		}
	}
	if (!PK_VERIFY(result == A_Err_NONE))
		return false;
	return result;
}


//----------------------------------------------------------------------------

A_Err	CAEUpdater::_UpdateEmitterAtTime(SLayerHolder *layer, AEGP_EffectRefH effectRef, A_Time &AETime, bool isSeeking)
{
	PK_SCOPEDPROFILE();
	(void)isSeeking;
	CPopcornFXWorld		&PKFXWorld = CPopcornFXWorld::Instance();
	AEGP_SuiteHandler	suites(PKFXWorld.GetAESuites());
	A_Err				result = A_Err_NONE;
	AEGP_StreamRefH		streamHandler;
	AEGP_StreamValue2 	value;
	SPendingEmitter		&emitter = layer->m_SpawnedEmitter;

	if (!PK_VERIFY(effectRef != null))
	{
		return A_Err_GENERIC;
	}
	{ //Emitter
		{
			result |= suites.StreamSuite3()->AEGP_GetNewEffectStreamByIndex(PKFXWorld.GetPluginID(), effectRef, s_EmitterIndexes[Effect_Parameters_TransformType], &streamHandler);
			PK_ASSERT(result == A_Err_NONE);
			AEFX_CLR_STRUCT(value);
			result |= suites.StreamSuite3()->AEGP_GetNewStreamValue(PKFXWorld.GetPluginID(), streamHandler, AEGP_LTimeMode_CompTime, &AETime, false, &value);
			PK_ASSERT(result == A_Err_NONE);
			emitter.m_Desc->m_TransformType = ((ETransformType)(int)value.val.one_d);
			result |= suites.StreamSuite3()->AEGP_DisposeStreamValue(&value);
			PK_ASSERT(result == A_Err_NONE);
			result |= suites.StreamSuite3()->AEGP_DisposeStream(streamHandler);
			PK_ASSERT(result == A_Err_NONE);
			if (result != A_Err_NONE)
				return result;

			if (emitter.m_Desc->m_TransformType == ETransformType_3D)
			{
				result |= suites.StreamSuite3()->AEGP_GetNewEffectStreamByIndex(PKFXWorld.GetPluginID(), effectRef, s_EmitterIndexes[Effect_Parameters_Position], &streamHandler);
				PK_ASSERT(result == A_Err_NONE);
				AEFX_CLR_STRUCT(value);
				result |= suites.StreamSuite3()->AEGP_GetNewStreamValue(PKFXWorld.GetPluginID(), streamHandler, AEGP_LTimeMode_CompTime, &AETime, false, &value);
				PK_ASSERT(result == A_Err_NONE);
				emitter.m_Desc->m_Position = A_FloatPoint3{ value.val.three_d.x, value.val.three_d.y, value.val.three_d.z };
#if defined(PK_SCALE_DOWN)
				emitter.m_Desc->m_Position.x = emitter.m_Desc->m_Position.x / layer->m_ScaleFactor;
				emitter.m_Desc->m_Position.y = emitter.m_Desc->m_Position.y / layer->m_ScaleFactor;
				emitter.m_Desc->m_Position.z = emitter.m_Desc->m_Position.z / layer->m_ScaleFactor;
#endif
				layer->m_Scene->SetEmitterPosition(AAEToPK(emitter.m_Desc->m_Position), emitter.m_Desc->m_TransformType);
				result |= suites.StreamSuite3()->AEGP_DisposeStreamValue(&value);
				PK_ASSERT(result == A_Err_NONE);
				result |= suites.StreamSuite3()->AEGP_DisposeStream(streamHandler);
				PK_ASSERT(result == A_Err_NONE);
				if (result != A_Err_NONE)
					return result;
			}
			else if (emitter.m_Desc->m_TransformType == ETransformType_2D)
			{
				result |= suites.StreamSuite3()->AEGP_GetNewEffectStreamByIndex(PKFXWorld.GetPluginID(), effectRef, s_EmitterIndexes[Effect_Parameters_Position_2D], &streamHandler);
				PK_ASSERT(result == A_Err_NONE);
				AEFX_CLR_STRUCT(value);
				result |= suites.StreamSuite3()->AEGP_GetNewStreamValue(PKFXWorld.GetPluginID(), streamHandler, AEGP_LTimeMode_CompTime, &AETime, false, &value);
				PK_ASSERT(result == A_Err_NONE);

				emitter.m_Desc->m_Position.x = value.val.two_d.x;
				emitter.m_Desc->m_Position.y = value.val.two_d.y;

				result |= suites.StreamSuite3()->AEGP_GetNewEffectStreamByIndex(PKFXWorld.GetPluginID(), effectRef, s_EmitterIndexes[Effect_Parameters_Position_2D_Distance], &streamHandler);
				PK_ASSERT(result == A_Err_NONE);
				AEFX_CLR_STRUCT(value);
				result |= suites.StreamSuite3()->AEGP_GetNewStreamValue(PKFXWorld.GetPluginID(), streamHandler, AEGP_LTimeMode_CompTime, &AETime, false, &value);
				PK_ASSERT(result == A_Err_NONE);

				emitter.m_Desc->m_Position.z = value.val.one_d;
				layer->m_Scene->SetEmitterPosition(AAEToPK(emitter.m_Desc->m_Position), emitter.m_Desc->m_TransformType);
			}
		}
		{
			float rotation[3];
			for (u32 i = 0; i < 3; ++i)
			{
				result |= suites.StreamSuite3()->AEGP_GetNewEffectStreamByIndex(PKFXWorld.GetPluginID(), effectRef, s_EmitterIndexes[Effect_Parameters_Rotation_X + i], &streamHandler);
				PK_ASSERT(result == A_Err_NONE);
				AEFX_CLR_STRUCT(value);
				result |= suites.StreamSuite3()->AEGP_GetNewStreamValue(PKFXWorld.GetPluginID(), streamHandler, AEGP_LTimeMode_CompTime, &AETime, false, &value);
				PK_ASSERT(result == A_Err_NONE);
				rotation[i] = (float)value.val.one_d;
				result |= suites.StreamSuite3()->AEGP_DisposeStreamValue(&value);
				PK_ASSERT(result == A_Err_NONE);
				result |= suites.StreamSuite3()->AEGP_DisposeStream(streamHandler);
				PK_ASSERT(result == A_Err_NONE);
				if (result != A_Err_NONE)
					return result;
			}
			emitter.m_Desc->m_Rotation = A_FloatPoint3{ rotation[0], rotation[1], rotation[2] };
			layer->m_Scene->SetEmitterRotation(AngleAAEToPK(emitter.m_Desc->m_Rotation));
		}
	}

	{ //Camera

		result |= suites.StreamSuite3()->AEGP_GetNewEffectStreamByIndex(PKFXWorld.GetPluginID(), effectRef, s_EmitterIndexes[Effect_Parameters_Camera], &streamHandler);
		PK_ASSERT(result == A_Err_NONE);
		AEFX_CLR_STRUCT(value);
		result |= suites.StreamSuite3()->AEGP_GetNewStreamValue(PKFXWorld.GetPluginID(), streamHandler, AEGP_LTimeMode_CompTime, &AETime, false, &value);
		PK_ASSERT(result == A_Err_NONE);
		result |= suites.StreamSuite3()->AEGP_DisposeStreamValue(&value);
		PK_ASSERT(result == A_Err_NONE);
		result |= suites.StreamSuite3()->AEGP_DisposeStream(streamHandler);
		PK_ASSERT(result == A_Err_NONE);
		if (result != A_Err_NONE)
			return result;

		result |= suites.StreamSuite3()->AEGP_GetNewEffectStreamByIndex(PKFXWorld.GetPluginID(), effectRef, s_EmitterIndexes[Effect_Parameters_Camera_Near], &streamHandler);
		PK_ASSERT(result == A_Err_NONE);
		AEFX_CLR_STRUCT(value);
		result |= suites.StreamSuite3()->AEGP_GetNewStreamValue(PKFXWorld.GetPluginID(), streamHandler, AEGP_LTimeMode_CompTime, &AETime, false, &value);
		PK_ASSERT(result == A_Err_NONE);
		emitter.m_Desc->m_Camera.m_Near = (float)value.val.one_d;
		result |= suites.StreamSuite3()->AEGP_DisposeStreamValue(&value);
		PK_ASSERT(result == A_Err_NONE);
		result |= suites.StreamSuite3()->AEGP_DisposeStream(streamHandler);
		PK_ASSERT(result == A_Err_NONE);
		if (result != A_Err_NONE)
			return result;

		result |= suites.StreamSuite3()->AEGP_GetNewEffectStreamByIndex(PKFXWorld.GetPluginID(), effectRef, s_EmitterIndexes[Effect_Parameters_Camera_Far], &streamHandler);
		PK_ASSERT(result == A_Err_NONE);
		AEFX_CLR_STRUCT(value);
		result |= suites.StreamSuite3()->AEGP_GetNewStreamValue(PKFXWorld.GetPluginID(), streamHandler, AEGP_LTimeMode_CompTime, &AETime, false, &value);
		PK_ASSERT(result == A_Err_NONE);
		emitter.m_Desc->m_Camera.m_Far = (float)value.val.one_d;
		result |= suites.StreamSuite3()->AEGP_DisposeStreamValue(&value);
		PK_ASSERT(result == A_Err_NONE);
		result |= suites.StreamSuite3()->AEGP_DisposeStream(streamHandler);
		PK_ASSERT(result == A_Err_NONE);
		if (result != A_Err_NONE)
			return result;
	}

	//Background Override
	{
		result |= suites.StreamSuite3()->AEGP_GetNewEffectStreamByIndex(PKFXWorld.GetPluginID(), effectRef, s_EmitterIndexes[Effect_Parameters_Background_Toggle], &streamHandler);
		PK_ASSERT(result == A_Err_NONE);
		AEFX_CLR_STRUCT(value);
		result |= suites.StreamSuite3()->AEGP_GetNewStreamValue(PKFXWorld.GetPluginID(), streamHandler, AEGP_LTimeMode_CompTime, &AETime, false, &value);
		PK_ASSERT(result == A_Err_NONE);
		emitter.m_Desc->m_IsAlphaBGOverride = (bool)value.val.one_d;
		result |= suites.StreamSuite3()->AEGP_DisposeStreamValue(&value);
		PK_ASSERT(result == A_Err_NONE);
		result |= suites.StreamSuite3()->AEGP_DisposeStream(streamHandler);
		PK_ASSERT(result == A_Err_NONE);
		if (result != A_Err_NONE)
			return result;

		result |= suites.StreamSuite3()->AEGP_GetNewEffectStreamByIndex(PKFXWorld.GetPluginID(), effectRef, s_EmitterIndexes[Effect_Parameters_Background_Opacity], &streamHandler);
		PK_ASSERT(result == A_Err_NONE);
		AEFX_CLR_STRUCT(value);
		result |= suites.StreamSuite3()->AEGP_GetNewStreamValue(PKFXWorld.GetPluginID(), streamHandler, AEGP_LTimeMode_CompTime, &AETime, false, &value);
		PK_ASSERT(result == A_Err_NONE);
		emitter.m_Desc->m_AlphaBGOverride = (float)value.val.one_d / 100.0f;
		result |= suites.StreamSuite3()->AEGP_DisposeStreamValue(&value);
		PK_ASSERT(result == A_Err_NONE);
		result |= suites.StreamSuite3()->AEGP_DisposeStream(streamHandler);
		PK_ASSERT(result == A_Err_NONE);
		if (result != A_Err_NONE)
			return result;
	}
	{ //BackdropMesh
		result |= suites.StreamSuite3()->AEGP_GetNewEffectStreamByIndex(PKFXWorld.GetPluginID(), effectRef, s_EmitterIndexes[Effect_Parameters_BackdropMesh_Enable_Rendering], &streamHandler);
		PK_ASSERT(result == A_Err_NONE);
		AEFX_CLR_STRUCT(value);
		result |= suites.StreamSuite3()->AEGP_GetNewStreamValue(PKFXWorld.GetPluginID(), streamHandler, AEGP_LTimeMode_CompTime, &AETime, false, &value);
		PK_ASSERT(result == A_Err_NONE);
		emitter.m_Desc->m_BackdropMesh.m_EnableRendering = (int)value.val.one_d;
		result |= suites.StreamSuite3()->AEGP_DisposeStreamValue(&value);
		PK_ASSERT(result == A_Err_NONE);
		result |= suites.StreamSuite3()->AEGP_DisposeStream(streamHandler);
		PK_ASSERT(result == A_Err_NONE);
		if (result != A_Err_NONE)
			return result;

		result |= suites.StreamSuite3()->AEGP_GetNewEffectStreamByIndex(PKFXWorld.GetPluginID(), effectRef, s_EmitterIndexes[Effect_Parameters_BackdropMesh_Enable_Collisions], &streamHandler);
		PK_ASSERT(result == A_Err_NONE);
		AEFX_CLR_STRUCT(value);
		result |= suites.StreamSuite3()->AEGP_GetNewStreamValue(PKFXWorld.GetPluginID(), streamHandler, AEGP_LTimeMode_CompTime, &AETime, false, &value);
		PK_ASSERT(result == A_Err_NONE);
		emitter.m_Desc->m_BackdropMesh.m_EnableCollisions = (int)value.val.one_d;
		result |= suites.StreamSuite3()->AEGP_DisposeStreamValue(&value);
		PK_ASSERT(result == A_Err_NONE);
		result |= suites.StreamSuite3()->AEGP_DisposeStream(streamHandler);
		PK_ASSERT(result == A_Err_NONE);
		if (result != A_Err_NONE)
			return result;

		result |= suites.StreamSuite3()->AEGP_GetNewEffectStreamByIndex(PKFXWorld.GetPluginID(), effectRef, s_EmitterIndexes[Effect_Parameters_BackdropMesh_Enable_Animation], &streamHandler);
		PK_ASSERT(result == A_Err_NONE);
		AEFX_CLR_STRUCT(value);
		result |= suites.StreamSuite3()->AEGP_GetNewStreamValue(PKFXWorld.GetPluginID(), streamHandler, AEGP_LTimeMode_CompTime, &AETime, false, &value);
		PK_ASSERT(result == A_Err_NONE);
		emitter.m_Desc->m_BackdropMesh.m_EnableAnimations = (int)value.val.one_d;
		result |= suites.StreamSuite3()->AEGP_DisposeStreamValue(&value);
		PK_ASSERT(result == A_Err_NONE);
		result |= suites.StreamSuite3()->AEGP_DisposeStream(streamHandler);
		PK_ASSERT(result == A_Err_NONE);
		if (result != A_Err_NONE)
			return result;

		if (emitter.m_Desc->m_BackdropMesh.m_EnableCollisions)
		{
			{
				result |= suites.StreamSuite3()->AEGP_GetNewEffectStreamByIndex(PKFXWorld.GetPluginID(), effectRef, s_EmitterIndexes[Effect_Parameters_BackdropMesh_Position], &streamHandler);
				PK_ASSERT(result == A_Err_NONE);
				AEFX_CLR_STRUCT(value);
				result |= suites.StreamSuite3()->AEGP_GetNewStreamValue(PKFXWorld.GetPluginID(), streamHandler, AEGP_LTimeMode_CompTime, &AETime, false, &value);
				PK_ASSERT(result == A_Err_NONE);
				emitter.m_Desc->m_BackdropMesh.m_Position = A_FloatPoint3{ value.val.three_d.x, value.val.three_d.y, value.val.three_d.z };
#if defined(PK_SCALE_DOWN)
				emitter.m_Desc->m_BackdropMesh.m_Position.x = emitter.m_Desc->m_BackdropMesh.m_Position.x / layer->m_ScaleFactor;
				emitter.m_Desc->m_BackdropMesh.m_Position.y = emitter.m_Desc->m_BackdropMesh.m_Position.y / layer->m_ScaleFactor;
				emitter.m_Desc->m_BackdropMesh.m_Position.z = emitter.m_Desc->m_BackdropMesh.m_Position.z / layer->m_ScaleFactor;
#endif
				layer->m_Scene->UpdateBackdropTransform(emitter.m_Desc);
				result |= suites.StreamSuite3()->AEGP_DisposeStreamValue(&value);
				PK_ASSERT(result == A_Err_NONE);
				result |= suites.StreamSuite3()->AEGP_DisposeStream(streamHandler);
				PK_ASSERT(result == A_Err_NONE);
				if (result != A_Err_NONE)
					return result;
			}
			{
				float rotation[3];
				for (u32 i = 0; i < 3; ++i)
				{
					result |= suites.StreamSuite3()->AEGP_GetNewEffectStreamByIndex(PKFXWorld.GetPluginID(), effectRef, s_EmitterIndexes[Effect_Parameters_BackdropMesh_Rotation_X + i], &streamHandler);
					PK_ASSERT(result == A_Err_NONE);
					AEFX_CLR_STRUCT(value);
					result |= suites.StreamSuite3()->AEGP_GetNewStreamValue(PKFXWorld.GetPluginID(), streamHandler, AEGP_LTimeMode_CompTime, &AETime, false, &value);
					PK_ASSERT(result == A_Err_NONE);
					rotation[i] = DegToRad((float)value.val.one_d);
					result |= suites.StreamSuite3()->AEGP_DisposeStreamValue(&value);
					PK_ASSERT(result == A_Err_NONE);
					result |= suites.StreamSuite3()->AEGP_DisposeStream(streamHandler);
					PK_ASSERT(result == A_Err_NONE);
					if (result != A_Err_NONE)
						return result;
				}
				emitter.m_Desc->m_BackdropMesh.m_Rotation = A_FloatPoint3{ rotation[0], rotation[1], rotation[2] };
			}
			{
				float scale[3];
				for (u32 i = 0; i < 3; ++i)
				{
					result |= suites.StreamSuite3()->AEGP_GetNewEffectStreamByIndex(PKFXWorld.GetPluginID(), effectRef, s_EmitterIndexes[Effect_Parameters_BackdropMesh_Scale_X + i], &streamHandler);
					PK_ASSERT(result == A_Err_NONE);
					AEFX_CLR_STRUCT(value);
					result |= suites.StreamSuite3()->AEGP_GetNewStreamValue(PKFXWorld.GetPluginID(), streamHandler, AEGP_LTimeMode_CompTime, &AETime, true, &value);
					PK_ASSERT(result == A_Err_NONE);
					scale[i] = (float)value.val.one_d;
					result |= suites.StreamSuite3()->AEGP_DisposeStreamValue(&value);
					PK_ASSERT(result == A_Err_NONE);
					result |= suites.StreamSuite3()->AEGP_DisposeStream(streamHandler);
					PK_ASSERT(result == A_Err_NONE);
					if (result != A_Err_NONE)
						return result;
				}
				emitter.m_Desc->m_BackdropMesh.m_Scale = A_FloatPoint3{ scale[0], scale[1], scale[2] };
			}
		}
		result |= suites.StreamSuite3()->AEGP_GetNewEffectStreamByIndex(PKFXWorld.GetPluginID(), effectRef, s_EmitterIndexes[Effect_Parameters_Simulation_State], &streamHandler);
		PK_ASSERT(result == A_Err_NONE);
		AEFX_CLR_STRUCT(value);
		result |= suites.StreamSuite3()->AEGP_GetNewStreamValue(PKFXWorld.GetPluginID(), streamHandler, AEGP_LTimeMode_CompTime, &AETime, false, &value);
		PK_ASSERT(result == A_Err_NONE);
		emitter.m_Desc->m_SimStatePrev = emitter.m_Desc->m_SimState;
		emitter.m_Desc->m_SimState = (int)value.val.one_d;

		result |= suites.StreamSuite3()->AEGP_DisposeStreamValue(&value);
		PK_ASSERT(result == A_Err_NONE);
		result |= suites.StreamSuite3()->AEGP_DisposeStream(streamHandler);
		PK_ASSERT(result == A_Err_NONE);
		if (result != A_Err_NONE)
			return result;
	}

	// Update audio backdrop:
	AEGP_LayerIDVal		layerID;
	CAEUpdater::_GetParamsStreamValueAtTime<AEGP_LayerIDVal>(s_EmitterIndexes[Effect_Parameters_Audio], effectRef, AETime, layerID);

	if (layer->m_BackdropAudioSpectrum == null)
		layer->m_BackdropAudioSpectrum = PK_NEW(SSamplerAudio); 
	if (layer->m_BackdropAudioWaveform == null)
		layer->m_BackdropAudioWaveform = PK_NEW(SSamplerAudio);

	if (!_SetupAudioSampler(layer, layerID, AETime, layer->m_BackdropAudioSpectrum, isSeeking))
		PK_SAFE_DELETE(layer->m_BackdropAudioSpectrum);
	if (!_SetupAudioSampler(layer, layerID, AETime, layer->m_BackdropAudioWaveform, isSeeking))
		PK_SAFE_DELETE(layer->m_BackdropAudioWaveform);
	if (!PK_VERIFY(result == A_Err_NONE))
		return result;
	return A_Err_NONE;
}

//----------------------------------------------------------------------------

bool	CAEUpdater::GetLightsAtTime(SLayerHolder *layer, A_Time &AETime, TArray<SLightDesc> &lights)
{
	CPopcornFXWorld		&PKFXWorld = CPopcornFXWorld::Instance();
	AEGP_SuiteHandler	suites(PKFXWorld.GetAESuites());
	A_Err				result = A_Err_NONE;
	AEGP_CompH			compH = null;
	AEGP_LayerH			layerH = null;
	A_long				layerNbr = 0;

	A_Boolean			layerActive = false;

	result |= suites.LayerSuite5()->AEGP_GetLayerParentComp(layer->m_EffectLayer, &compH);
	result |= suites.LayerSuite5()->AEGP_GetCompNumLayers(compH, &layerNbr);

	for (s32 i = 0; i < layerNbr; ++i)
	{
		AEGP_ObjectType		layerType = AEGP_ObjectType_NONE;

		result |= suites.LayerSuite5()->AEGP_GetCompLayerByIndex(compH, i, &layerH);
		result |= suites.LayerSuite5()->AEGP_GetLayerObjectType(layerH, &layerType);

		result |= suites.LayerSuite5()->AEGP_IsVideoActive(layerH, AEGP_LTimeMode_CompTime, &AETime, &layerActive);
		if (layerType == AEGP_ObjectType_LIGHT && layerActive == (A_Boolean)TRUE)
		{
			SLightDesc		light;
			AEGP_LightType	type;
			AEGP_StreamVal	streamValColor, streamValIntensity;

			result |= suites.LightSuite2()->AEGP_GetLightType(layerH, &type);
			result |= suites.StreamSuite2()->AEGP_GetLayerStreamValue(layerH, AEGP_LayerStream_COLOR, AEGP_LTimeMode_CompTime, &AETime, false, &streamValColor, null);
			result |= suites.StreamSuite2()->AEGP_GetLayerStreamValue(layerH, AEGP_LayerStream_INTENSITY, AEGP_LTimeMode_CompTime, &AETime, false, &streamValIntensity, null);

			light.m_Type = type;
			light.m_Color.x = streamValColor.color.redF;
			light.m_Color.y = streamValColor.color.greenF;
			light.m_Color.z = streamValColor.color.blueF;
			light.m_Intensity = (float)(streamValIntensity.one_d) / 100.0f;

			if (light.m_Type == AEGP_LightType_PARALLEL)
			{
				AEGP_StreamVal		streamValPos, streamValAnchor;

				result |= suites.StreamSuite2()->AEGP_GetLayerStreamValue(layerH, AEGP_LayerStream_POSITION, AEGP_LTimeMode_CompTime, &AETime, false, &streamValPos, null);
				result |= suites.StreamSuite2()->AEGP_GetLayerStreamValue(layerH, AEGP_LayerStream_ANCHORPOINT, AEGP_LTimeMode_CompTime, &AETime, false, &streamValAnchor, null);

				CFloat3	direction = { (float)(streamValAnchor.three_d.x - streamValPos.three_d.x), (float)(streamValAnchor.three_d.y - streamValPos.three_d.y), (float)(streamValAnchor.three_d.z - streamValPos.three_d.z) };
				direction.Normalize();
				light.m_Direction = PKToAAE(direction);
			}
			else if (light.m_Type == AEGP_LightType_SPOT)
			{
				AEGP_StreamVal	streamValPos, streamValAnchor, streamValConeAngle, streamValConeFeather;

				result |= suites.StreamSuite2()->AEGP_GetLayerStreamValue(layerH, AEGP_LayerStream_POSITION, AEGP_LTimeMode_CompTime, &AETime, false, &streamValPos, null);
				CFloat3			position = { (float)(streamValPos.three_d.x), (float)(streamValPos.three_d.y), (float)(streamValPos.three_d.z) };
				light.m_Position = PKToAAE(position);
#if defined(PK_SCALE_DOWN)
				light.m_Position.x = light.m_Position.x / layer->m_ScaleFactor;
				light.m_Position.y = light.m_Position.y / layer->m_ScaleFactor;
				light.m_Position.z = light.m_Position.z / layer->m_ScaleFactor;
#endif
				result |= suites.StreamSuite2()->AEGP_GetLayerStreamValue(layerH, AEGP_LayerStream_ANCHORPOINT, AEGP_LTimeMode_CompTime, &AETime, false, &streamValAnchor, null);
				result |= suites.StreamSuite2()->AEGP_GetLayerStreamValue(layerH, AEGP_LayerStream_CONE_ANGLE, AEGP_LTimeMode_CompTime, &AETime, false, &streamValConeAngle, null);
				result |= suites.StreamSuite2()->AEGP_GetLayerStreamValue(layerH, AEGP_LayerStream_CONE_FEATHER, AEGP_LTimeMode_CompTime, &AETime, false, &streamValConeFeather, null);

				CFloat3			direction = { (float)(streamValAnchor.three_d.x - streamValPos.three_d.x), (float)(streamValAnchor.three_d.y - streamValPos.three_d.y), (float)(streamValAnchor.three_d.z - streamValPos.three_d.z) };
				direction.Normalize();
				light.m_Direction = PKToAAE(direction);

				light.m_Angle = (float)streamValConeAngle.one_d;
				light.m_Feather = (float)(streamValConeFeather.one_d / 100.0f);
			}
			else if (light.m_Type == AEGP_LightType_POINT)
			{
				AEGP_StreamVal		streamValPos;
				result |= suites.StreamSuite2()->AEGP_GetLayerStreamValue(layerH, AEGP_LayerStream_POSITION, AEGP_LTimeMode_CompTime, &AETime, false, &streamValPos, null);

				CFloat3	position = { (float)(streamValPos.three_d.x), (float)(streamValPos.three_d.y), (float)(streamValPos.three_d.z) };
				light.m_Position = PKToAAE(position);
#if defined(PK_SCALE_DOWN)
				light.m_Position.x = light.m_Position.x / layer->m_ScaleFactor;
				light.m_Position.y = light.m_Position.y / layer->m_ScaleFactor;
				light.m_Position.z = light.m_Position.z / layer->m_ScaleFactor;
#endif
			}
			if (!lights.PushBack(light).Valid())
				return false;
		}
	}
	if (!PK_VERIFY(result == A_Err_NONE))
		return false;
	return true;
}

__AEGP_PK_END
