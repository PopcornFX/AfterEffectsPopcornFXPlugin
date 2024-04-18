//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#pragma once

#ifndef __FX_AAESCENE_H__
#define __FX_AAESCENE_H__

#include "AEGP_Define.h"

#include "AEGP_RenderContext.h"
#include "AEGP_FrameCollector.h"

#include <pk_base_object/include/hbo_object.h>

#include <pk_kernel/include/kr_refptr.h>

#include <pk_rhi/include/FwdInterfaces.h>

#include <PK-SampleLib/RenderIntegrationRHI/RHIParticleRenderDataFactory.h>
#include <pk_particles/include/Updaters/D3D12/updater_d3d12.h>
#include <PK-SampleLib/Camera.h>

#include "AEGP_Attribute.h"
#include "AEGP_ParticleScene.h"

#include <PopcornFX_Define.h>

//----------------------------------------------------------------------------

__PK_API_BEGIN
PK_FORWARD_DECLARE(SkeletonState);
__PK_API_END

__AAEPK_BEGIN

struct SEmitterDesc;

__AAEPK_END


__AEGP_PK_BEGIN

PK_FORWARD_DECLARE(AAERenderContext);
PK_FORWARD_DECLARE(SkinnedMeshInstance);

// Forward declare our simple scene definition
struct	SSimpleSceneDef;
struct	SLayerHolder;

struct	SSamplerAudio;
struct	SPendingAttribute;

//----------------------------------------------------------------------------

struct SAAEWorld
{
	CFloat3				m_WorldOrigin;
	float				m_WorldWidth = -1.0f;
	float				m_WorldHeight = -1.0f;
};

//----------------------------------------------------------------------------

struct SRendererProperties
{
	CString		m_Name;
	CString		m_Value;

	CString		m_EffectLayerName;
	u32			m_EffectLayerUID;

	CStringId	m_LayerID;

	u32			m_RendererUID;
	u32			m_PropertyUID;


	SRendererProperties(const CString &name, const CString &value, const CString &effectLayerName, u32 effectLayerUID, CStringId layerID, u32 rdrUID, u32 propUID)
		: m_Name(name)
		, m_Value(value)
		, m_EffectLayerName(effectLayerName)
		, m_EffectLayerUID(effectLayerUID)
		, m_LayerID(layerID)
		, m_RendererUID(rdrUID)
		, m_PropertyUID(propUID)
	{
		
	}
};

PK_FORWARD_DECLARE(AAEScene);

//----------------------------------------------------------------------------

class	CAAEScene : public CRefCountedObject
{
public:

	struct	SSkinnedDataSimple
	{
		struct	SSkinnedDataSubMesh
		{
			u32									m_SubMeshID;
			TStridedMemoryView<const CFloat3>	m_Positions;
			TStridedMemoryView<const CFloat3>	m_Normals;
			SSkinnedDataSubMesh() : m_SubMeshID(0) {}
		};

		bool										m_Valid;
		TSemiDynamicArray<SSkinnedDataSubMesh, 4>	m_SubMeshes;

		SSkinnedDataSimple() : m_Valid(false) {}
	};
public:
	static u32	s_SceneID;
	u32			m_ID;
public:
	CAAEScene();
	virtual	~CAAEScene();

	bool									Init(SAAEIOData &AAEData);
	bool									Quit();
	static void								ShutdownPopcornFX();

	bool									Update(SAAEIOData &AAEData);
	bool									UpdateLight(SLayerHolder *layer);
	bool									UpdateAttributes(SLayerHolder *layer);
	
	void									UpdateBackdropTransform(SEmitterDesc *desc);
	bool									UpdateBackdrop(SLayerHolder *layer, SEmitterDesc *desc);
	bool									Render(SAAEIOData &AAEData);

	bool									ResetEffect(bool unload);

	TArray<SRendererProperties*>			&GetRenderers() { return m_OverridableProperties; }
	HBO::CContext							*GetContext() { return m_HBOContext;  }
	bool									GetEmitterBounds(CAABB &bounds);

	void									SetLayerHolder(SLayerHolder *parent);
	void									SetCameraViewMatrix(const CFloat4x4 &viewMatrix, const CFloat4 &pos, const float cameraZoom);

	void									SetWorldSize(SAAEIOData &AAEData);
	void									SetEmitterPosition(const CFloat3 &position, ETransformType type);
	void									SetEmitterRotation(const CFloat3 &rotation);
	void									SetEmitterTeleport(bool teleport = true);

	bool									SetupScene(bool seeking, bool refresh);

	//Pack&Effect Management
	bool									SetPack(PFilePack pack, bool unload);
	bool									SetSelectedEffect(CString name, bool refresh);

	void									SetEffectDescriptor(SEmitterDesc* desc);

	bool									RefreshAssetList();

	SSamplerAudio							*GetAudioSamplerDescriptor(CStringId name, SSamplerAudio::SamplingType type);

	void									SetSeeking(bool seekingEnabled) { m_SeekingEnabled = seekingEnabled; }
	void									SetSkinnedBackdropParams(bool enabled, bool weightedSampling,  u32 colorStreamID, u32 weightStreamID);

protected:
	bool									_LateInitializeIFN();
	bool									_SetupMediumCollection();
	void									_FastForwardSimulation(SAAEIOData &AAEData, float timeTarget);
	bool									_UpdateShapeSampler(SPendingAttribute &samplerData, SAttributeSamplerDesc* descriptor);
	bool									_UpdateTextSampler(SPendingAttribute &samplerData, SAttributeSamplerDesc* descriptor);
	bool									_UpdateImageSampler(SPendingAttribute &samplerData, SAttributeSamplerDesc* descriptor);
	bool									_UpdateAudioSampler(SPendingAttribute &samplerData, SAttributeSamplerDesc* descriptor);
	bool									_UpdateVectorFieldSampler(SPendingAttribute &samplerData, SAttributeSamplerDesc* descriptor);
	bool									_SeekingUpdateEffect(float dt, float elapsedTime, float timeTarget, u32 curUpdateIdx, u32 totalUpdatesCount);
	void									_SeekingLoadAndRunEffect(CParticleMediumCollection *mediumCollection,
																	const CParticleEffect *effect,
																	const CFloat4x4 &transform,
																	float timeFromStartOfFrame,
																	float timeToEndOfFrame,
																	float elapsedTime);
	bool									_SeekingWaitForUpdateFence(CTimer *waitTimer, float customMaxUpdateTime = -1.0f);
	u32										_PickNewEffectSeed();

	//	Scene
	bool									_LoadScene(const SSimpleSceneDef &fxPath);
	void									_CollectCurrentFrame();
	void									_RenderLastCollectedFrame();

	void									_UpdateEmitter(float dt);
	void									_UpdateCamera();
	void									_UpdateMediumCollectionView();

	void									_OnUpdateComplete(CParticleMediumCollection *collection);

	//	Effects
	bool									_LoadEffectIFN(const SSimpleSceneDef &sceneDef);
	PParticleEffectInstance					_InstantiateEffect();
	PParticleEffectInstance					_InstantiateEffect(float timeFromStartOfFrame, float timeToEndOfFrame);
	void									_OnInstanceDeath(const PParticleEffectInstance &instance);

	bool									_RebuildAttributes(const SSimpleSceneDef &sceneDef);

	void									_ExtractAEFrameInfo(SAAEIOData &AAEData);
	bool									_CheckRenderAbort(SAAEIOData *AAEData);
	// Camera
	void									_SetProj(float fovxDegrees, const CFloat2 &winDimPixel, float zNear, float zFar);

	void									_FillAdditionnalDataForRender();
public:
	// GPU Sim
#if (PK_PARTICLES_UPDATER_USE_D3D12 != 0 || PK_PARTICLES_UPDATER_USE_D3D11 != 0)
	static bool								SimDispatchMask(const PopcornFX::CParticleDescriptor *descriptor, PopcornFX::SSimDispatchHint &outHint);
	void									D3D12_EnqueueTask(const PopcornFX::PParticleUpdaterTaskD3D12 &task);
#endif

protected:
	//World
	SAAEWorld								m_SAAEWorldData;
	PF_ProgPtr								m_EffectRef;
	SEmitterDesc							*m_EffectDesc;

	SLayerHolder							*m_LayerHolder;

	CFloat3									m_EmitterDefaultPosition;
	CFloat3									m_EmitterDefaultOrientation;

	ETransformType							m_EmitterTransformType;
	CFloat4x4								m_EmitterTransformsCurrent;
	CFloat4x4								m_EmitterTransformsPrevious;
	CFloat3									m_EmitterVelCurrent;
	CFloat3									m_EmitterVelPrevious;

	CUint2									m_OriginViewport;

	bool									m_Paused;

	//AE Interface
	CFloat3									m_AEEmitterPosition;
	CFloat3									m_AEEmitterRotation;
	u32										m_AEEmitterSeed;

	CFloat4x4								m_AEViewMatrix;
	CFloat4									m_AECameraPos;
	float									m_AECameraZoom;
	bool									m_TeleportEmitter;

	u32										m_FrameNumber;
	float									m_DT;
	float									m_PreviousTimeSec;
	float									m_CurrentTimeSec;

	bool									m_Initialized;
	bool									m_SeekingEnabled;

	A_Err									m_FrameAbortedDuringSeeking;

	PKSample::SCamera						m_Camera;

	PParticleEffect							m_Effect;

	PBaseObjectFile							m_EffectFile;
	CString									m_EffectPath;
	PParticleEffectInstance					m_EffectLastInstance;

	PParticleAttributeList					m_AttributesList;

	TArray<SSamplerAudio*>					m_ActiveAudioSamplers;

	CFloat4									m_EmitterPositionNormalized_Debug;
	CFloat4x4								m_LastInstanceXForms;
	float									m_ElapsedTimeSinceLastSpawn;

	PKSample::CRHIParticleRenderDataFactory	m_ParticleRenderDataFactory;
	CFrameCollector							m_FrameCollector;
	PKSample::SRHIDrawOutputs				m_DrawOutputs;
	EDrawCallSortMethod						m_DCSortMethod;

	PKSample::SSceneInfoData				m_SceneInfoData;
	RHI::PConstantSet						m_SceneInfoConstantSet;
	RHI::PGpuBuffer							m_SceneInfoConstantBuffer;

	CString									m_SelectedEffect;

	PFilePack								m_LoadedPack;

	HBO::CContext							*m_HBOContext;
	CAAEParticleScene						*m_ParticleScene;
	CParticleMediumCollection				*m_ParticleMediumCollection;
	float									m_Stats_SimulationTime;

	CGuid									m_ViewSlotInMediumCollection;
	PKSample::SBackdropsData				m_BackdropData;

	TResourcePtr<CResourceMesh>				m_ResourceMesh;
	PSkinnedMeshInstance					m_SkinnedMeshInstance;
	PSkeletonState							m_SkeletonState;
	TArray<SSkinnedDataSimple>				m_FXInstancesSkinnedData;

	CResourceManager						*m_ResourceManager;

	//Skinned Mesh Sampling
	bool									m_HasBoundBackdrop;
	bool									m_IsWeightedSampling;
	u32										m_ColorStreamID;
	u32										m_WeightStreamID;
	SAAEIOData								*m_AAEDataForSeeking;

private:
	TArray<SRendererProperties*>			m_OverridableProperties;
	bool									m_ForceRestartSeeking;
};
PK_DECLARE_REFPTRCLASS(AAEScene);

//----------------------------------------------------------------------------

__AEGP_PK_END

#endif
