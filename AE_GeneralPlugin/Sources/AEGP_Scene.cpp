//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#include "ae_precompiled.h"

#include "AEGP_Scene.h"

#include "AEGP_World.h"
#include "AEGP_AEPKConversion.h"

#include "AEGP_RenderContext.h"
#include "RenderApi/AEGP_BaseContext.h"
#include "RenderApi/AEGP_D3D12Context.h"
#include "RenderApi/AEGP_D3D11Context.h"

#include "AEGP_Attribute.h"
#include "AEGP_Log.h"
#include "AEGP_PackExplorer.h"

#include "AEGP_Main.h"
#include "AEGP_Attribute.h"

#include "AEGP_AssetBaker.h"
#include "AEGP_SkinnedMeshInstance.h"
#include "AEGP_SkinnedMesh.h"

#include <PopcornFX_Suite.h>


#include <pk_kernel/include/kr_string_id.h>

#include <pk_particles/include/ps_system.h>
#include <pk_particles/include/ps_effect.h>
#include <pk_particles/include/ps_stats.h>

#include <pk_particles/include/ps_mediums.h>
#include <pk_particles/include/ps_samplers.h>
#include <pk_particles/include/ps_samplers_image.h>
#include <pk_particles/include/ps_samplers_text.h>
#include <pk_particles/include/ps_samplers_curve.h>
#include <pk_particles/include/ps_samplers_shape.h>
#include <pk_particles/include/ps_samplers_audio.h>
#include <pk_particles/include/ps_samplers_vectorfield.h>
#include <pk_particles/include/ps_attributes.h>

#include <pk_particles/include/Updaters/GPU/updater_gpu.h>
#include <pk_particles/include/Updaters/Auto/updater_auto.h>
#include <pk_particles/include/Updaters/D3D12/updater_d3d12.h>
#include <pk_particles/include/Updaters/D3D11/updater_d3d11.h>

#include <pk_render_helpers/include/frame_collector/rh_frame_collector.h>

#include <pk_engine_utils/include/eu_random.h>		// for Random::DefaultGenerator()
#include <pk_maths/include/pk_maths_random.h>		// for PRNG
#include <pk_maths/include/pk_maths_transforms.h>	// for CPlane in CParticleSceneBasic

#include <pk_particles_toolbox/include/pt_helpers.h>
#include <pk_particles_toolbox/include/pt_seek_interface.h>

#include <PK-SampleLib/ShaderDefinitions/SampleLibShaderDefinitions.h>

#include <pk_kernel/include/kr_memoryviews_utils.h>
#include <pk_kernel/include/kr_sort.h>

#include <pk_particles/include/ps_descriptor.h>
#include <pk_particles/include/ps_descriptor_cache.h>

#include <AEGP_UpdateAEState.h>

__AEGP_PK_BEGIN

//----------------------------------------------------------------------------

struct	SSimpleSceneDef
{
	CString					m_EffectPath;
	CString					m_BackdropMeshPath;

	CUint3		m_SpawnGridResolution;	// defined in side-up-forward coordinates
	float		m_SpawnGridRange;

	float		m_RespawnDelay;

	bool		m_Seeking = false;
	bool		m_Refresh = false;
	struct
	{
		float		m_Distance;
		CFloat3		m_Angles;		// { pitch, yaw, roll }
		float		m_Height;
	} m_CameraStartParameters;
};

//----------------------------------------------------------------------------


u32		CAAEScene::s_SceneID = 0;

//----------------------------------------------------------------------------

CAAEScene::CAAEScene()
:	m_EffectRef(null)
,	m_EffectDesc(null)
,	m_LayerHolder(null)
,	m_EmitterDefaultPosition(CFloat3::ZERO)
,	m_EmitterDefaultOrientation(CFloat3::ZERO)
,	m_EmitterTransformType(ETransformType::ETransformType_3D)
,	m_EmitterTransformsCurrent(CFloat4x4::IDENTITY)
,	m_EmitterTransformsPrevious(CFloat4x4::IDENTITY)
,	m_EmitterVelCurrent(CFloat3::ZERO)
,	m_EmitterVelPrevious(CFloat3::ZERO)
,	m_OriginViewport(CUint2::ZERO)
,	m_Paused(false)
,	m_AEEmitterPosition(CFloat3::ZERO)
,	m_AEEmitterRotation(CFloat3::ZERO)
,	m_AEEmitterSeed(0)
,	m_AEViewMatrix(CFloat4x4::IDENTITY)
,	m_AECameraPos(CFloat4::ZERO)
,	m_AECameraZoom(1.0f)
,	m_TeleportEmitter(false)
,	m_FrameNumber(0)
,	m_DT(0.0f)
,	m_PreviousTimeSec(0.0f)
,	m_CurrentTimeSec(0.0f)
,	m_Initialized(false)
,	m_FrameAbortedDuringSeeking(A_Err_NONE)
,	m_Effect(null)
,	m_EffectFile(null)
,	m_EffectLastInstance(null)
,	m_AttributesList(null)
,	m_EmitterPositionNormalized_Debug(CFloat4::ZERO)
,	m_LastInstanceXForms(CFloat4x4::IDENTITY)
,	m_DCSortMethod(EDrawCallSortMethod::Sort_DrawCalls)
,	m_SceneInfoConstantSet(null)
,	m_SceneInfoConstantBuffer(null)
,	m_LoadedPack(null)
,	m_HBOContext(null)
,	m_ParticleScene(null)
,	m_ParticleMediumCollection(null)
,	m_Stats_SimulationTime(0.0f)
,	m_ViewSlotInMediumCollection(CGuid::INVALID)
,	m_SkinnedMeshInstance(null)
,	m_SkeletonState(null)
,	m_ResourceManager(null)
,	m_HasBoundBackdrop(false)
,	m_IsWeightedSampling(false)
,	m_ColorStreamID(0)
,	m_WeightStreamID(0)
,	m_ForceRestartSeeking(true)
{
	m_ID = s_SceneID++;
}

//----------------------------------------------------------------------------

CAAEScene::~CAAEScene()
{
	Quit();
}

//----------------------------------------------------------------------------

bool	CAAEScene::Init(SAAEIOData &AAEData)
{
	if (AAEData.m_InData)
		m_EffectRef = AAEData.m_InData->effect_ref;
	else
		m_EffectRef = null;

	m_ResourceManager = Resource::DefaultManager();
	if (!PK_VERIFY(m_ResourceManager != null))
		return CAELog::LogErrorWindows(&AAEData, "Could not allocate PopcornFX ResourceManager");
	if (m_HBOContext != null)
		PK_SAFE_DELETE(m_HBOContext);
	m_HBOContext = PK_NEW(HBO::CContext);
	if (m_ParticleScene != null)
		PK_SAFE_DELETE(m_ParticleScene);
	m_ParticleScene = PK_NEW(CAAEParticleScene);
	if (!PK_VERIFY(m_ParticleScene != null))
		return false;
	m_ParticleScene->m_Parent = this;
	//AAE INFO
	SetWorldSize(AAEData);
	// Medium collection
	if (m_ParticleMediumCollection == null)
	{
		if (_SetupMediumCollection() == false)
			return false;
	}

	return true;
}

//----------------------------------------------------------------------------

void	CAAEScene::SetWorldSize(SAAEIOData &AAEData)
{
	if (AAEData.m_InData)
	{
		m_SAAEWorldData.m_WorldWidth = (float)AAEData.m_InData->width;
		m_SAAEWorldData.m_WorldHeight = (float)AAEData.m_InData->height;
		m_SAAEWorldData.m_WorldOrigin = CFloat3{ m_SAAEWorldData.m_WorldWidth / 2, m_SAAEWorldData.m_WorldHeight / 2, 0 };
	}
}

//----------------------------------------------------------------------------

void	CAAEScene::SetEmitterPosition(const CFloat3 &position, ETransformType type)
{
	m_EmitterTransformType = type;
	m_AEEmitterPosition = position;
}

//----------------------------------------------------------------------------

void	CAAEScene::SetEmitterRotation(const CFloat3 &rotation)
{
	m_AEEmitterRotation = rotation;
}

//----------------------------------------------------------------------------

void CAAEScene::SetEmitterTeleport(bool teleport)
{
	m_TeleportEmitter = teleport;
}

//----------------------------------------------------------------------------

void CAAEScene::SetLayerHolder(SLayerHolder *parent)
{
	m_LayerHolder = parent;
}

//----------------------------------------------------------------------------

void CAAEScene::SetCameraViewMatrix(const CFloat4x4 &viewMatrix, const CFloat4 &pos, const float cameraZoom)
{
	m_AECameraZoom = cameraZoom;
	m_AECameraPos = pos;
	m_AEViewMatrix = viewMatrix;
}

//----------------------------------------------------------------------------

bool	CAAEScene::RefreshAssetList()
{
	m_LoadedPack = null;
	PK_ASSERT(m_LayerHolder != null);

	if (m_LayerHolder->m_BakedPack == null)
		return false;

	m_LoadedPack = m_LayerHolder->m_BakedPack;
	return true;
}

//----------------------------------------------------------------------------

SSamplerAudio	*CAAEScene::GetAudioSamplerDescriptor(CStringId name, SSamplerAudio::SamplingType type)
{
	if (!name.Valid() || name.Empty())
		return null;

	for (u32 i = 0; i < m_ActiveAudioSamplers.Count(); ++i)
	{
		if (name == m_ActiveAudioSamplers[i]->m_Name && type == m_ActiveAudioSamplers[i]->m_SamplingType)
		{
			return m_ActiveAudioSamplers[i];
		}
	}
	for (u32 i = 0; i < m_ActiveAudioSamplers.Count(); ++i)
	{
		if (name == m_ActiveAudioSamplers[i]->m_Name && m_ActiveAudioSamplers[i]->m_SamplingType == SSamplerAudio::SamplingType::SamplingType_Unknown)
		{
			return m_ActiveAudioSamplers[i];
		}
	}
	for (u32 i = 0; i < m_ActiveAudioSamplers.Count(); ++i)
	{
		if (!m_ActiveAudioSamplers[i]->m_Name.Valid() || m_ActiveAudioSamplers[i]->m_Name.Empty())
		{
			m_ActiveAudioSamplers[i]->m_Name = name;
			return m_ActiveAudioSamplers[i];
		}
	}
	return null;
}
//----------------------------------------------------------------------------

bool	CAAEScene::GetEmitterBounds(CAABB &bounds)
{
	if (m_EffectLastInstance == null)
		return false;
	const CParticleMediumCollection::SEffectMediums *mediums = m_ParticleMediumCollection->MapEffectMediumLookup(m_Effect.Get());

	for (auto medium : mediums->m_Mediums)
	{
		bounds.Add(medium->ExactBounds());
	}
	return true;
}

//----------------------------------------------------------------------------

bool	 CAAEScene::SetPack(PFilePack pack, bool unload)
{
	if (pack == null)
		return false;

	ResetEffect(unload);

	m_LoadedPack = pack;
	return true;
}

//----------------------------------------------------------------------------

bool CAAEScene::SetSelectedEffect(CString name, bool refresh)
{
	if (name == null || name.Empty())
		return false;
	CBakedPackExplorer	packExplorer(m_LoadedPack->Path(), File::DefaultFileSystem());
	CString				bakedName = CFilePath::StripExtension(name);

	bakedName += ".pkb";

	packExplorer.Explore();
	TMemoryView<const CString>	effectPaths = packExplorer.EffectPaths();

	if (effectPaths.IndexOf(bakedName).Valid())
	{
		m_SelectedEffect = bakedName;
		return SetupScene(false, refresh);
	}
	else
	{
		CAELog::TryLogInfoWindows("Trying to load " + bakedName + " Missing file in " + m_LoadedPack->Path() + " Reimport effect");
	}
	return false;
}

//----------------------------------------------------------------------------

void CAAEScene::SetEffectDescriptor(SEmitterDesc *desc)
{
	m_EffectDesc = desc;
}

//----------------------------------------------------------------------------

bool	CAAEScene::SetupScene(bool seeking, bool refresh)
{
	m_PreviousTimeSec = 0.0f;
	m_CurrentTimeSec = 0.0f;
	m_DT = 0.0f;

	PK_ASSERT(m_ParticleMediumCollection != null);
	m_ParticleMediumCollection->Clear();

	SetEmitterTeleport();

	SSimpleSceneDef	def;
	def.m_Seeking = seeking;
	def.m_Refresh = refresh;
	if (m_SelectedEffect == null)
	{
		if (!m_LayerHolder->m_SpawnedEmitter.m_Desc->m_PathSource.empty())
			CPopcornFXWorld::Instance().SetDestinationPackFromPath(*m_LayerHolder, CString(m_LayerHolder->m_SpawnedEmitter.m_Desc->m_PathSource.c_str()));
		SetPack(m_LayerHolder->m_BakedPack, true);
		SetEffectDescriptor(m_LayerHolder->m_SpawnedEmitter.m_Desc);
		SetSelectedEffect(CString(m_LayerHolder->m_SpawnedEmitter.m_Desc->m_Name.c_str()), false);
	}
	def.m_EffectPath = m_SelectedEffect;

	if (!_LoadScene(def))
		return false;
	CPopcornFXWorld::Instance().OnEndSetupScene();
	return true;
}

//----------------------------------------------------------------------------

bool	CAAEScene::Quit()
{
	m_EffectRef = null;
	m_EffectDesc = null;

	m_LayerHolder = null;
	if (m_EffectLastInstance != null)
	{
		// Not strictly necessary to -= the callback, the instance is getting destroyed anyway.
		m_EffectLastInstance->m_DeathNotifier -= FastDelegate<void(const PParticleEffectInstance &)>(this, &CAAEScene::_OnInstanceDeath);
		//m_EffectLastInstance->KillImmediate();
		m_EffectLastInstance = null;
	}
	m_AttributesList = null;

	m_EffectPath = null;
	if (m_Effect != null)
		m_Effect = null;
	if (m_EffectFile != null)
	{
		m_EffectFile->Unload();
		m_EffectFile = null;
	}

	m_ActiveAudioSamplers.Clear();

	m_ParticleRenderDataFactory.UpdateThread_Release();

	if (m_SkinnedMeshInstance != null)
	{
		m_SkinnedMeshInstance->WaitAsyncUpdateSkin();
		m_SkinnedMeshInstance->ClearSkinnedMesh();
	}
	m_SkinnedMeshInstance = null;

	m_DrawOutputs.Clear();
	m_SceneInfoConstantSet = null;
	m_SceneInfoConstantBuffer = null;

	m_LoadedPack = null;

	PK_SAFE_DELETE(m_ParticleScene);

	if (m_Initialized)
	{
		m_FrameCollector.UninstallFromMediumCollection(m_ParticleMediumCollection);
		m_FrameCollector.DeleteUnloadedRenderMediums();
		m_FrameCollector.Destroy();

		if (m_ParticleMediumCollection)
		{
			m_ParticleMediumCollection->Clear();
			PK_SAFE_DELETE(m_ParticleMediumCollection);
		}
	}

	m_ResourceManager = null;

	for (u32 i = 0; i < m_OverridableProperties.Count(); ++i)
		PK_SAFE_DELETE(m_OverridableProperties[i]);
	m_OverridableProperties.Clean();

	PK_SAFE_DELETE(m_HBOContext);

	m_Initialized = false;
	return true;
}

//----------------------------------------------------------------------------

void	CAAEScene::ShutdownPopcornFX()
{

}

//----------------------------------------------------------------------------

bool	CAAEScene::Update(SAAEIOData &AAEData)
{
	PK_SCOPEDPROFILE();

	if (m_LayerHolder == null || m_EffectDesc == null) //Waiting for AE to process new effects
		return true;

	SetWorldSize(AAEData);
	if (!PK_VERIFY(_LateInitializeIFN()))
		return false;

	m_EffectRef = AAEData.m_InData->effect_ref;

	if (!m_Paused)
	{
		if (m_EffectLastInstance == null)
		{
			if (!SetupScene(false, false))
				return false;
		}
	}
	/// Update ------------------------------------------------------------------------------------------
	if (!m_Paused)
	{
		PK_NAMEDSCOPEDPROFILE("Scene Update");
		_ExtractAEFrameInfo(AAEData);
		m_FrameCollector.ReleaseRenderedFrame();
		m_FrameAbortedDuringSeeking = A_Err_NONE;
		_FastForwardSimulation(AAEData, m_CurrentTimeSec + m_DT);
		if (m_FrameAbortedDuringSeeking != A_Err_NONE)			
		{
			AAEData.m_ReturnCode = m_FrameAbortedDuringSeeking;
			m_ParticleMediumCollection->Clear();
			return true;
		}
		if (_CheckRenderAbort(&AAEData))
		{
			m_ParticleMediumCollection->Clear();
			return true;
		}
		_CollectCurrentFrame();
	}

	_FillAdditionnalDataForRender();

#if _DEBUG
	auto v = m_ParticleMediumCollection->Stats();
#endif
	return true;
}

//----------------------------------------------------------------------------

bool	CAAEScene::UpdateAttributes(SLayerHolder *layer)
{
	PK_SCOPEDPROFILE();
	if (m_EffectLastInstance == null || layer == null)
		return true;

	for (auto it = layer->m_SpawnedAttributes.Begin(); it != layer->m_SpawnedAttributes.End(); ++it)
	{
		SAttributeDesc	*descriptor = static_cast<SAttributeDesc*>(it->m_Desc);
		if (descriptor == null) // Corrupt attribute on AE Side
			it->m_Deleted = true;
		else if (descriptor->m_IsDirty)
		{
			//Update Attributes Values
#if defined(PK_SCALE_DOWN)
			if (descriptor->m_IsAffectedByScale)
			{
				void	*attributeValue = descriptor->GetRawValue();
				CInt4	*intData = null;
				CFloat4	*floatData = null;

				switch (descriptor->m_Type)
				{
					case (AttributeType_Int1):
					case (AttributeType_Int2):
					case (AttributeType_Int3):
					case (AttributeType_Int4):
						intData = static_cast<CInt4*>(attributeValue);
						*((CFloat4*)intData) /= layer->m_ScaleFactor;
						break;
					case (AttributeType_Float1):
					case (AttributeType_Float2):
					case (AttributeType_Float3):
					case (AttributeType_Float4):
						floatData = static_cast<CFloat4*>(attributeValue);
						*floatData /= layer->m_ScaleFactor;
						break;
					default:
						break;
				}
				if (!PK_VERIFY(m_EffectLastInstance->SetRawAttribute(descriptor->m_Name.data(), AttributeAAEToPK(descriptor->m_Type), attributeValue)))
					return false;
			}
			else
			{
				if (!PK_VERIFY(m_EffectLastInstance->SetRawAttribute(descriptor->m_Name.data(), AttributeAAEToPK(descriptor->m_Type), descriptor->GetRawValue())))
					return false;
			}
#else
			if (!PK_VERIFY(m_EffectLastInstance->SetRawAttribute(descriptor->m_Name.data(), AttributeAAEToPK(descriptor->m_Type), descriptor->GetRawValue())))
				return false;
#endif
			descriptor->m_IsDirty = false;
		}
	}
	for (auto it = layer->m_SpawnedAttributesSampler.Begin(); it != layer->m_SpawnedAttributesSampler.End(); ++it)
	{
		SAttributeSamplerDesc	*descriptor = static_cast<SAttributeSamplerDesc*>(it->m_Desc);
		if (descriptor == null) // Corrupt attribute on AE Side
		{
			it->m_Deleted = true;
			continue;
		}

		switch (descriptor->m_Type)
		{
		case AttributeSamplerType_Geometry:
			_UpdateShapeSampler(*it, descriptor);
			break;
		case AttributeSamplerType_Text:
			_UpdateTextSampler(*it, descriptor);
			break;
		case AttributeSamplerType_Image:
			_UpdateImageSampler(*it, descriptor);
			break;
		case AttributeSamplerType_Audio:
			_UpdateAudioSampler(*it, descriptor);
			break;
		case AttributeSamplerType_VectorField:
			_UpdateVectorFieldSampler(*it, descriptor);
			break;
		default:
			break;
		}
		if ((*it).m_PKDesc != null)
			(*it).m_PKDesc->m_Dirty = false;
	}
	if (layer->m_BackdropAudioWaveform != null)
	{
		layer->m_BackdropAudioWaveform->m_BuiltThisFrame = false;
		if (!PK_VERIFY(m_ActiveAudioSamplers.PushBack(layer->m_BackdropAudioWaveform).Valid()))
			return false;
		layer->m_BackdropAudioWaveform->m_Dirty = false;
	}
	if (layer->m_BackdropAudioSpectrum != null)
	{
		layer->m_BackdropAudioSpectrum->m_BuiltThisFrame = false;
		if (!PK_VERIFY(m_ActiveAudioSamplers.PushBack(layer->m_BackdropAudioSpectrum).Valid()))
			return false;
		layer->m_BackdropAudioSpectrum->m_Dirty = false;
	}
	return true;
}

//----------------------------------------------------------------------------

static CString	_GetAnimPathFromMesh(const CResourceManager *resourceManager, const CString &meshPath)
{
	PK_ASSERT(resourceManager != null);
	if (meshPath.Empty())
		return null;

	CString		remappedPath = meshPath;
	bool		remappedPathNotVirtual = false;
	resourceManager->RemapAndPurifyPathIFN(remappedPath, remappedPathNotVirtual);
	PK_ASSERT(!remappedPathNotVirtual);
	return CFilePath::StripExtension(remappedPath) + ".pksa";
}

//----------------------------------------------------------------------------

static bool		_PreloadMeshResource(CResourceManager *resourceManager, const CString &path, TResourcePtr<CResourceMesh> &dstMeshResource, PSkeletonState &dstBindPose)
{
	dstMeshResource = resourceManager->Load<CResourceMesh>(path, false, SResourceLoadCtl(false, true));
	if (dstMeshResource == null)
	{
		CLog::Log(PK_ERROR, "Failed loading mesh resource \"%s\"", path.Data());
		return false;
	}

	for (const auto &batch : dstMeshResource->BatchList())
	{
		batch->RawMesh()->BuildTangentsIFN();
	}
	
	PSkeleton	skeleton = dstMeshResource->Skeleton();
	if (skeleton != null)
	{
		dstBindPose = PK_NEW(CSkeletonState(skeleton));
		if (dstBindPose == null)
			return false;
	}
	return true;
}

//----------------------------------------------------------------------------

void	CAAEScene::UpdateBackdropTransform(SEmitterDesc *desc)
{
	const CQuaternion	quat = Transforms::Quaternion::FromEuler(CFloat3((float)desc->m_BackdropMesh.m_Rotation.x, (float)desc->m_BackdropMesh.m_Rotation.y, (float)desc->m_BackdropMesh.m_Rotation.z));
	const CFloat3		pos((float)desc->m_BackdropMesh.m_Position.x, (float)desc->m_BackdropMesh.m_Position.y, (float)desc->m_BackdropMesh.m_Position.z);
	const CFloat4		scale((float)desc->m_BackdropMesh.m_Scale.x, (float)desc->m_BackdropMesh.m_Scale.y, (float)desc->m_BackdropMesh.m_Scale.z, 1.0f);

	m_BackdropData.m_MeshBackdropTransforms = Transforms::Matrix::FromQuaternion(quat, CFloat4(pos, 1.0f));
	m_BackdropData.m_MeshBackdropTransforms.Scale(scale);
	if (desc->m_BackdropMesh.m_EnableCollisions)
		m_ParticleScene->SetBackdropMeshTransform(m_BackdropData.m_MeshBackdropTransforms);
}

//----------------------------------------------------------------------------

bool	CAAEScene::UpdateBackdrop(SLayerHolder *layer,SEmitterDesc *desc)
{
	if (!desc->m_BackdropMesh.m_Path.empty())
	{
		const CQuaternion	quat = Transforms::Quaternion::FromEuler(CFloat3((float)desc->m_BackdropMesh.m_Rotation.x, (float)desc->m_BackdropMesh.m_Rotation.y, (float)desc->m_BackdropMesh.m_Rotation.z));
		const CFloat3		pos((float)desc->m_BackdropMesh.m_Position.x, (float)desc->m_BackdropMesh.m_Position.y, (float)desc->m_BackdropMesh.m_Position.z);
		const CFloat4		scale((float)desc->m_BackdropMesh.m_Scale.x, (float)desc->m_BackdropMesh.m_Scale.y, (float)desc->m_BackdropMesh.m_Scale.z, 1.0f);

		m_BackdropData.m_MeshBackdropTransforms = Transforms::Matrix::FromQuaternion(quat, CFloat4(pos, 1.0f));
		m_BackdropData.m_MeshBackdropTransforms.Scale(scale);
		if (m_BackdropData.m_MeshBackdropTransforms == CFloat4x4::IDENTITY && desc->m_LoadBackdrop)
		{
			CPopcornFXWorld					&pkfxWorld = CPopcornFXWorld::Instance();
			m_BackdropData.m_MeshBackdropTransforms = m_EmitterTransformsCurrent;
			pkfxWorld.SetBackdropMeshDefaultTransform(layer);
		}

		m_BackdropData.m_MeshMetalness = desc->m_BackdropMesh.m_Metalness;
		m_BackdropData.m_MeshRoughness = desc->m_BackdropMesh.m_Roughness;

		m_BackdropData.m_ShowMesh = desc->m_BackdropMesh.m_EnableRendering;
	}
	if (!desc->m_BackdropEnvironmentMap.m_Path.empty())
	{
		CFilePackPath	filePackPath = CFilePackPath::FromPhysicalPath(desc->m_BackdropEnvironmentMap.m_Path.data(), File::DefaultFileSystem());

		m_BackdropData.m_EnvironmentMapPath = filePackPath.Path();
		m_BackdropData.m_EnvironmentMapColor = CFloat3((float)desc->m_BackdropEnvironmentMap.m_Color.x, (float)desc->m_BackdropEnvironmentMap.m_Color.y, (float)desc->m_BackdropEnvironmentMap.m_Color.z);
		m_BackdropData.m_EnvironmentMapIntensity = desc->m_BackdropEnvironmentMap.m_Intensity;
		m_BackdropData.m_BackgroundUsesEnvironmentMap = desc->m_BackdropEnvironmentMap.m_EnableRendering;
		m_BackdropData.m_EnvironmentMapAffectsAmbient = true;
	}
	else
	{
		m_BackdropData.m_EnvironmentMapPath = null;
	}

	if (!desc->m_UpdateBackdrop && !desc->m_LoadBackdrop)
		return true;

	desc->m_LoadBackdrop = false;
	desc->m_UpdateBackdrop = false;
	if (!desc->m_BackdropMesh.m_Path.empty())
	{
		CFilePackPath	filePackPath = CFilePackPath::FromPhysicalPath(desc->m_BackdropMesh.m_Path.data(), File::DefaultFileSystem());

		m_BackdropData.m_MeshPath = filePackPath.Path();

		_PreloadMeshResource(m_ResourceManager, m_BackdropData.m_MeshPath, m_ResourceMesh, m_SkeletonState);

		if (desc->m_BackdropMesh.m_EnableCollisions)
			m_ParticleScene->SetBackdropMesh(m_ResourceMesh, m_BackdropData.m_MeshBackdropTransforms);
		else
			m_ParticleScene->ClearBackdropMesh();
		if (desc->m_BackdropMesh.m_EnableAnimations)
		{
			const CString	animPath = (!m_BackdropData.m_MeshPath.Empty()) ? _GetAnimPathFromMesh(m_ResourceManager, m_BackdropData.m_MeshPath) : CString::EmptyString;
			m_SkinnedMeshInstance = PK_NEW(CSkinnedMeshInstance);
			if (!m_SkinnedMeshInstance->LoadSkinnedMeshIFN(m_ResourceMesh, EMeshChannels::Channel_Tangent | EMeshChannels::Channel_Velocity | EMeshChannels::Channel_Normal | EMeshChannels::Channel_Position, m_SkeletonState) ||
				!m_SkinnedMeshInstance->LoadAnimationIFN(m_HBOContext, animPath, false))
				m_SkinnedMeshInstance = null;
		}
	}
	else
	{
		m_SkinnedMeshInstance = null;
		m_SkeletonState = null;
		m_BackdropData.m_MeshPath = "";
		m_ParticleScene->ClearBackdropMesh();
	}
	return true;
}

//----------------------------------------------------------------------------

bool	CAAEScene::Render(SAAEIOData &AAEData)
{
	if (_CheckRenderAbort(&AAEData))
		return true;

	CPopcornFXWorld					&pkfxWorld = CPopcornFXWorld::Instance();

	// our render specific context (one per thread)
	PAAERenderContext currentRenderContext = pkfxWorld.GetCurrentRenderContext();
	if (!PK_VERIFY(currentRenderContext != null))
		return false;

	RHI::PApiManager		apiManager = currentRenderContext->GetAEGraphicContext()->GetApiManager();

	if (_CheckRenderAbort(&AAEData))
		return true;

	if (!(currentRenderContext->AERenderFrameBegin(AAEData, !m_BackdropData.m_BackgroundUsesEnvironmentMap)))
		return true;

	if (_CheckRenderAbort(&AAEData))
	{
		PK_VERIFY(currentRenderContext->AERenderFrameEnd(AAEData));
		return true;
	}

	_RenderLastCollectedFrame();

	// Update scene info
	{
		PKSample::Utils::SetupSceneInfoData(m_SceneInfoData, m_Camera, CCoordinateFrame::GlobalFrame());
		if (!PK_VERIFY(currentRenderContext->GetCurrentSceneRenderer()->SetSceneInfo(m_SceneInfoData, CCoordinateFrame::GlobalFrame())))
		{
			PK_VERIFY(currentRenderContext->AERenderFrameEnd(AAEData));
			return false;
		}
	}

	//// Flush all graphical resources
	PKSample::CRendererCacheInstance_UpdateThread::RenderThread_FlushAllResources();

	PKSample::SParticleSceneOptions	sceneOptions;

	AAEToPK(m_EffectDesc->m_Rendering, sceneOptions);
	currentRenderContext->SetPostFXOptions(sceneOptions);

	currentRenderContext->SetBackgroundOptions(m_EffectDesc->m_IsAlphaBGOverride, m_EffectDesc->m_AlphaBGOverride);

	// Render ------------------------------------------------------------------------------------------
	{
		PKSample::CRHIParticleSceneRenderHelper		*renderHelper = currentRenderContext->GetCurrentSceneRenderer();
		CUint2										vpSize = currentRenderContext->GetViewportSize();
		//
		PK_ASSERT(renderHelper != null);
		PK_ASSERT(vpSize.x() != 0 || vpSize.y() != 0);

		RHI::PCommandBuffer	preRenderCmdBuff = apiManager->CreateCommandBuffer(RHI::SRHIResourceInfos("PK-RHI Pre Render command buffer"));
		RHI::PCommandBuffer	postRenderCmdBuff = apiManager->CreateCommandBuffer(RHI::SRHIResourceInfos("PK-RHI Post Render command buffer"));
		
		renderHelper->SetupPostFX_Bloom(sceneOptions.m_Bloom, false, false);
		renderHelper->SetupPostFX_Distortion(sceneOptions.m_Distortion, false);
		renderHelper->SetupPostFX_ToneMapping(sceneOptions.m_ToneMapping, sceneOptions.m_Vignetting, sceneOptions.m_Dithering, false, false);
		renderHelper->SetupPostFX_FXAA(sceneOptions.m_FXAA, false, false);
		renderHelper->SetupShadows();
		renderHelper->SetCurrentPackResourceManager(m_ResourceManager);
		renderHelper->SetBackdropInfo(m_BackdropData, ECoordinateFrame::Frame_User1);

		// Fill the command buffer
		preRenderCmdBuff->Start();

		if (!m_BackdropData.m_EnvironmentMapPath.Empty())
		{
			renderHelper->GetEnvironmentMap().UpdateCubemap(preRenderCmdBuff, m_ResourceManager);
		
			PKSample::SBackdropsData	&data = renderHelper->GetBackDropsData();
			data.m_BackgroundUsesEnvironmentMap = m_BackdropData.m_BackgroundUsesEnvironmentMap;
			data.m_EnvironmentMapAffectsAmbient = true;
		}
		
		// Copy the particle count for the indirect draws:
		PK_FOREACH(copy, m_DrawOutputs.m_CopyCommands)
		{
			if (!PK_VERIFY(preRenderCmdBuff->CopyBuffer(copy->m_SrcBuffer, copy->m_DstBuffer, copy->m_SrcOffset, copy->m_DstOffset, copy->m_SizeToCopy)))
			{
				CLog::Log(PK_ERROR, "preRenderCmdBuff->CopyBuffer() failed.");
				PK_VERIFY(currentRenderContext->AERenderFrameEnd(AAEData));
				return false;
			}
		}
		preRenderCmdBuff->SetViewport({ 0, 0 }, vpSize, { 0, 1 });
		preRenderCmdBuff->SetScissor({ 0, 0 }, vpSize);

		preRenderCmdBuff->Stop();
		apiManager->SubmitCommandBufferDirect(preRenderCmdBuff);
		postRenderCmdBuff->Start();
		if (!PK_VERIFY(renderHelper->RenderScene(AAEToPK(m_EffectDesc->m_Rendering.m_Type), m_DrawOutputs, 0)))
		{
			CLog::Log(PK_ERROR, "renderHelper->RenderScene() failed.");
			PK_VERIFY(currentRenderContext->AERenderFrameEnd(AAEData));
			return false;
		}

		if (!PK_VERIFY(postRenderCmdBuff->Stop()))
		{
			CLog::Log(PK_ERROR, "postRenderCmdBuff->Stop() failed.");
			PK_VERIFY(currentRenderContext->AERenderFrameEnd(AAEData));
			return false;
		}
		apiManager->SubmitCommandBufferDirect(postRenderCmdBuff);
		if (!PK_VERIFY(currentRenderContext->AERenderFrameEnd(AAEData)))
			return false;
	}
	return true;
}

//----------------------------------------------------------------------------

bool	CAAEScene::ResetEffect(bool unload)
{
	bool hasUnloaded = false;

	m_EffectLastInstance = null;
	if (m_ParticleMediumCollection != null)
		m_ParticleMediumCollection->Clear();
	m_Effect = null;
	if (unload && m_EffectFile != null)
	{
		m_EffectFile->Unload();
		hasUnloaded = true;
		m_EffectFile = null;
	}
	return hasUnloaded;
}

//----------------------------------------------------------------------------
//
//	RenderHelpers integration
//
//----------------------------------------------------------------------------

PKSample::CRendererBatchDrawer	*CAAEScene::NewBatchDrawer(ERendererClass rendererType, const PRendererCacheBase &rendererCache, bool gpuStorage)
{
	return m_ParticleRenderDataFactory.CreateBillboardingBatch2(rendererType, rendererCache, gpuStorage);
}

//----------------------------------------------------------------------------

PRendererCacheBase	CAAEScene::NewRendererCache(const PRendererDataBase &renderer, const CParticleDescriptor *particleDesc)
{
	return m_ParticleRenderDataFactory.UpdateThread_CreateRendererCache(renderer, particleDesc);
}

//----------------------------------------------------------------------------

void	CAAEScene::SetSkinnedBackdropParams(bool enabled, bool weightedSampling, u32 colorStreamID, u32 weightStreamID)
{
	bool update = false;
	if (m_HasBoundBackdrop != enabled || m_IsWeightedSampling != weightedSampling || m_ColorStreamID != colorStreamID || m_WeightStreamID != weightStreamID)
		update = true;
	m_HasBoundBackdrop = enabled;
	m_IsWeightedSampling = weightedSampling;
	m_ColorStreamID = colorStreamID;
	m_WeightStreamID = weightStreamID;

	if (update && m_SkinnedMeshInstance != null && m_ResourceMesh != null && m_ResourceMesh->BatchList().Count() > 0)
	{
		m_SkinnedMeshInstance->WaitAsyncUpdateSkin();

		m_SkinnedMeshInstance->SetupAttributeSampler(m_ResourceMesh->BatchList()[0]->RawMesh(),
			m_IsWeightedSampling,
			m_ColorStreamID,
			m_WeightStreamID);
	}

}

//----------------------------------------------------------------------------

bool	CAAEScene::UpdateLight(SLayerHolder *layer)
{
	PK_ASSERT(layer != null);
	PK_ASSERT(layer->m_SpawnedEmitter.m_Desc != null);

	SLightDesc	&light = layer->m_SpawnedEmitter.m_Desc->m_Light;

	if (light.m_Internal)
	{
		if (m_BackdropData.m_Lights.Count() == 0)
		{
			m_BackdropData.m_Lights.Resize(2);
		}
		m_BackdropData.m_Lights[0].SetAmbient(AAEToPK(light.m_Ambient), light.m_Intensity);
		m_BackdropData.m_Lights[1].SetDirectional(AAEToPK(light.m_Direction), AAEToPK(light.m_Color), light.m_Intensity);
	}
	if (!light.m_Internal)
	{
		TArray<SLightDesc>	lights;
		A_Time				time;

		time.value = layer->m_CurrentTime;
		time.scale = layer->m_TimeScale;
		if (!CAEUpdater::GetLightsAtTime(layer, time, lights))
			return false;

		if (lights.Count() != m_BackdropData.m_Lights.Count())
			m_BackdropData.m_Lights.Resize(lights.Count());
		for (u32 i = 0; i < lights.Count(); ++i)
		{
			if (lights[i].m_Type == AEGP_LightType_PARALLEL)
			{
				m_BackdropData.m_Lights[i].SetDirectional(AAEToPK(lights[i].m_Direction), AAEToPK(lights[i].m_Color), lights[i].m_Intensity);
			}
			else if (lights[i].m_Type == AEGP_LightType_AMBIENT)
			{
				m_BackdropData.m_Lights[i].SetAmbient(AAEToPK(lights[i].m_Color), lights[i].m_Intensity);
			}
			else if (lights[i].m_Type == AEGP_LightType_SPOT)
			{
				m_BackdropData.m_Lights[i].SetSpot(AAEToPK(lights[i].m_Position), AAEToPK(lights[i].m_Direction), lights[i].m_Angle, lights[i].m_Feather, AAEToPK(lights[i].m_Color), lights[i].m_Intensity);
			}
			else if (lights[i].m_Type == AEGP_LightType_POINT)
			{
				m_BackdropData.m_Lights[i].SetPoint(AAEToPK(lights[i].m_Position), AAEToPK(lights[i].m_Color), lights[i].m_Intensity);
			}
		}
	}

	return false;
}

//----------------------------------------------------------------------------

bool	CAAEScene::_LateInitializeIFN()
{
	if (m_Initialized == true)
		return true;
	// Medium collection
	if (m_ParticleMediumCollection == null)
	{
		if (_SetupMediumCollection() == false)
			return false;
	}
	// Render data factory
	{
		CPopcornFXWorld					&pkfxWorld = CPopcornFXWorld::Instance();

		PK_SCOPEDLOCK(pkfxWorld.GetRenderLock());
		RHI::PApiManager	apiManager = pkfxWorld.GetCurrentRenderContext()->GetAEGraphicContext()->GetApiManager();
		const RHI::SGPUCaps	&caps = apiManager->GetApiContext()->m_GPUCaps; // We get the GPU caps to know what is supported by the GPU and fallback IFN
		m_ParticleRenderDataFactory.UpdateThread_Initialize(apiManager, m_HBOContext, caps, Drawers::BillboardingLocation_CPU);
	}
	// Frame collector
	{
		const u32	enabledRenderers =	(1U << ERendererClass::Renderer_Billboard) | (1U << ERendererClass::Renderer_Ribbon) | (1U << ERendererClass::Renderer_Mesh) | (1U << ERendererClass::Renderer_Light)  | (1U << ERendererClass::Renderer_Triangle);

		// Initialize the frame collector with the factory and required renderers (see CPopcornScene::CollectCurrentFrame() for more detail)
		CFrameCollector::SFrameCollectorInit	init(enabledRenderers,
													 CbNewBatchDrawer(this, &CAAEScene::NewBatchDrawer),
													 CbNewRendererCache(this, &CAAEScene::NewRendererCache));
		if (!m_FrameCollector.Initialize(init))
		{
			return CAELog::TryLogErrorWindows("Failed to initialize the frame collector.");
		}
		// Hook this frame collector to the medium collection. It will "listen" to each medium insertion (see rh_frame_collector.inl)
		m_FrameCollector.InstallToMediumCollection(m_ParticleMediumCollection);

		m_FrameCollector.SetDrawCallsSortMethod(m_DCSortMethod);
	}
	m_Initialized = true;
	return true;
}

//----------------------------------------------------------------------------

bool	CAAEScene::_SetupMediumCollection()
{
	PK_ASSERT(m_ParticleMediumCollection == null);

	CPopcornFXWorld		&pkfxWorld = CPopcornFXWorld::Instance();

	PK_SCOPEDLOCK(pkfxWorld.GetRenderLock());

	PAAERenderContext	renderContext = pkfxWorld.GetCurrentRenderContext();

	CParticleUpdateManager_Auto			*updateManager = null;

#if PK_PARTICLES_UPDATER_USE_D3D12 != 0
	if (pkfxWorld.GetRenderApi() == RHI::GApi_D3D12)
		updateManager = PopcornFX::CParticleUpdateManager_Auto::New(EGPUContext::ContextD3D12);
#endif

#if PK_PARTICLES_UPDATER_USE_D3D11 != 0
	if (pkfxWorld.GetRenderApi() == RHI::GApi_D3D11)
		updateManager = PopcornFX::CParticleUpdateManager_Auto::New(EGPUContext::ContextD3D11);
#endif

	m_ParticleMediumCollection = PK_NEW(CParticleMediumCollection(m_ParticleScene, updateManager));
	if (!PK_VERIFY(m_ParticleMediumCollection != null))
		return false;

	m_ViewSlotInMediumCollection = m_ParticleMediumCollection->RegisterView();
	if (!PK_VERIFY(m_ViewSlotInMediumCollection.Valid()))
		return false;

	m_ParticleMediumCollection->m_OnUpdateComplete += FastDelegate<void(CParticleMediumCollection*)>(this, &CAAEScene::_OnUpdateComplete);

	m_ParticleMediumCollection->EnableBounds(true);
	m_ParticleMediumCollection->EnableLocalizedPages(true, false);

#if 0
#if PK_PARTICLES_UPDATER_USE_D3D12 != 0
	if (pkfxWorld.GetRenderApi() == RHI::GApi_D3D12)
	{
		CAAED3D12Context	*D3D12Context = static_cast<CAAED3D12Context*>(renderContext->GetAEGraphicContext());

		if (D3D12Context != null)
		{
			m_OriginViewport = renderContext->GetContextSize();
			// Small struct describing the D3D12 device used to build command lists, the root signature and the callback for submitting tasks to the engine
			// g_D3D12SerializeRootSignature is PFN_D3D12_SERIALIZE_ROOT_SIGNATURE
			PopcornFX::SD3D12Context			context(D3D12Context->m_D3D12Context->m_Device, D3D12Context->m_D3D12Context->m_SerializeRootSignatureFunc, PopcornFX::CParticleUpdateManager_D3D12::CbEnqueueTask(this, &CAAEScene::D3D12_EnqueueTask), D3D12_COMMAND_LIST_TYPE_DIRECT);

			if (updateManager->GetUpdateManager_GPU()->BindContext(context))
			{
				// Enables GPU simulation but does not force it

				m_ParticleMediumCollection->SetSimDispatchMaskCallback(CParticleMediumCollection::CbSimDispatchMask(&SimDispatchMask));
			}
		}
	}
#endif

#if PK_PARTICLES_UPDATER_USE_D3D11 != 0
	if (pkfxWorld.GetRenderApi() == RHI::GApi_D3D11)
	{
		CAAED3D11Context	*D3D11Context = static_cast<CAAED3D11Context*>(renderContext->GetAEGraphicContext());

		if (D3D11Context != null)
		{
			m_OriginViewport = renderContext->GetContextSize();
			PopcornFX::SD3D11Context			context(D3D11Context->m_D3D11Context->m_Device, D3D11Context->m_D3D11Context->m_ImmediateDeviceContext);

			CParticleUpdateManager_D3D11			*updateManagerD3D11 = checked_cast<CParticleUpdateManager_D3D11*>(updateManager->GetUpdateManager_GPU());
			if (updateManagerD3D11)
			{
				updateManagerD3D11->SetOwner(m_ParticleMediumCollection);
				if (!updateManagerD3D11->BindContext(context))
				{
					return false;
				}
			}
		}
	}
#endif
#endif
	return true;
}

//----------------------------------------------------------------------------

void	CAAEScene::_FastForwardSimulation(SAAEIOData &AAEData, float timeTarget)
{
	(void)AAEData;

	if (m_Effect == null)
	{
		m_FrameAbortedDuringSeeking = A_Err_GENERIC;
		return;
	}
	if (!PK_VERIFY(m_ParticleMediumCollection != null))
		return;

	CTimer		updateTimer;

	updateTimer.Start();

	const float	maxUpdateTime = 30.0f;

	if (!PK_VERIFY(m_Effect->FeatureFlagsAll() & SParticleDeclaration::FeatureFlag_Determinism))	// if at least one layer doesn't have the flag, fail.
		CAELog::TryLogErrorWindows("Effect is baked with Determinism disabled.");

	ParticleToolbox::SSeekingContextNew		seekingCtx(timeTarget, m_DT, m_DT, maxUpdateTime, 0.0f, m_Effect.Get(), m_EmitterTransformsCurrent);

	ParticleToolbox::SSeekingContextNew::SInstanceInfo	instInfo;
	PK_ASSERT(m_Effect != null);
	instInfo.m_Effect = m_Effect.Get();
	instInfo.m_CurTransforms = m_EmitterTransformsCurrent;
	instInfo.m_SpawnTime = 0.0f; 
	PK_VERIFY(seekingCtx.m_InstancesToSpawn.PushBack(instInfo).Valid());

	seekingCtx.m_LoadAndRunEffectDelegate		= FastDelegate<void(CParticleMediumCollection*,
																	const CParticleEffect*,
																	const CFloat4x4&,
																	float,
																	float,
																	float)>(this, &CAAEScene::_SeekingLoadAndRunEffect);
	seekingCtx.m_UpdateEffectDelegate			= FastDelegate<bool(float, float, float, u32, u32)>(this, &CAAEScene::_SeekingUpdateEffect);
	seekingCtx.m_WaitForUpdateFenceDelegate		= FastDelegate<bool(CTimer*, float)>(this, &CAAEScene::_SeekingWaitForUpdateFence);

	seekingCtx.m_PrecisionEpsilon = m_DT / 10.0f;		// Arbitrary value for precision issues
	seekingCtx.m_ForceRestartWhenNoUpdateNeeded = true;	// Restarts when the elapsed time is equal to the target time
	seekingCtx.m_ForceRestartFromZero = m_ForceRestartSeeking;

	m_AAEDataForSeeking = &AAEData;
	PF_PROGRESS(m_AAEDataForSeeking->m_InData, 0, 1000);
	ParticleToolbox::SSeekingContextNew::SeekToTargetTime(seekingCtx, m_ParticleMediumCollection);
	m_AAEDataForSeeking = null;
	m_Stats_SimulationTime = (float)updateTimer.Stop();
}

//----------------------------------------------------------------------------

void		CAAEScene::_SeekingLoadAndRunEffect(	CParticleMediumCollection *mediumCollection,
													const CParticleEffect *effect,
													const CFloat4x4 &transform,
													float timeFromStartOfFrame,
													float timeToEndOfFrame,
													float elapsedTime)
{
	(void)timeFromStartOfFrame;
	(void)timeToEndOfFrame;
	(void)transform;
	(void)effect;
	(void)mediumCollection;
	m_FrameAbortedDuringSeeking |= CAEUpdater::UpdateLayerAtTime(m_LayerHolder, elapsedTime, true);
	SetupScene(true, false);
	m_FrameAbortedDuringSeeking |= CAEUpdater::UpdateLayerAtTime(m_LayerHolder, elapsedTime, true);
}

//----------------------------------------------------------------------------

bool	CAAEScene::_SeekingUpdateEffect(float dt, float elapsedTime, float timeTarget, u32 curUpdateIdx, u32 totalUpdatesCount)
{
	(void)dt;
	(void)elapsedTime;
	(void)timeTarget;

	if (totalUpdatesCount != 0)
	{
		PF_Err	res = PF_Err_NONE;
		res = PF_PROGRESS(m_AAEDataForSeeking->m_InData, curUpdateIdx, totalUpdatesCount);
		if (res != PF_Err_NONE)
		{
			m_AAEDataForSeeking->m_ReturnCode = res;
			return false;
		}
	}
	m_FrameAbortedDuringSeeking |= CAEUpdater::UpdateLayerAtTime(m_LayerHolder, elapsedTime, curUpdateIdx != totalUpdatesCount);

	if (m_SkinnedMeshInstance != null)
	{
		if (elapsedTime == 0.0f)
			m_SkinnedMeshInstance->ResetAnimationCursor();
		const CFloat4x4		&backdropTransforms = (m_ParticleScene != null) ? m_ParticleScene->BackdropMeshTransforms() : CFloat4x4::IDENTITY;
		m_SkinnedMeshInstance->SetBackdropXForms(backdropTransforms);
		m_SkinnedMeshInstance->Update(dt);
		m_SkinnedMeshInstance->StartAsyncUpdateSkin(dt);
		m_SkinnedMeshInstance->WaitAsyncUpdateSkin();
	}
	_UpdateCamera();
	_UpdateEmitter(dt);
	_UpdateMediumCollectionView();
	return m_FrameAbortedDuringSeeking == A_Err_NONE;
}

//----------------------------------------------------------------------------

bool	CAAEScene::_SeekingWaitForUpdateFence(CTimer *waitTimer, float customMaxUpdateTime)
{
	(void)waitTimer;

	const u32	kMaxWaitTimeMs = 100;//ms max wait time
	const float	kMaxUpdateTimeSecsSetting = 1000;//ms
	bool		updateInProgress = true;
	float		maxUpdateTimeSecs = customMaxUpdateTime >= 0.0f ? customMaxUpdateTime : kMaxUpdateTimeSecsSetting;
	CTimer		ignoreTimer;

	ignoreTimer.Start();
	ignoreTimer.Pause();
	while (updateInProgress)
	{
		updateInProgress = m_ParticleMediumCollection->UpdateFence(kMaxWaitTimeMs);
		if (updateInProgress || maxUpdateTimeSecs <= 0.0f)
		{
		}
	}
	return true;
}

//----------------------------------------------------------------------------

u32 CAAEScene::_PickNewEffectSeed()
{
	return (u32)m_AEEmitterSeed;
}

//----------------------------------------------------------------------------

bool	CAAEScene::_UpdateShapeSampler(SPendingAttribute &samplerData, SAttributeSamplerDesc *descriptor)
{
	if (!PK_VERIFY(descriptor != null) ||
		!PK_VERIFY(samplerData.m_PKDesc != null))
		return false;

	SSamplerShape				*pkDesc = static_cast<SSamplerShape*>(samplerData.m_PKDesc);
	SShapeSamplerDescriptor		*aeDesc = static_cast<SShapeSamplerDescriptor*>(descriptor->m_Descriptor);

	if (aeDesc == null)
		return false;

	pkDesc->UpdateShape(aeDesc);

	CParticleSamplerDescriptor_Shape_Default	*shapeSmpDesc = null;
	if (aeDesc->m_BindToBackdrop && m_SkinnedMeshInstance != null && m_SkinnedMeshInstance->m_ShapeDescOverride != null)
		shapeSmpDesc = static_cast<CParticleSamplerDescriptor_Shape_Default*>(m_SkinnedMeshInstance->m_ShapeDescOverride.Get());
	else
		shapeSmpDesc = static_cast<CParticleSamplerDescriptor_Shape_Default*>(pkDesc->m_SamplerDescriptor.Get());
	m_EffectLastInstance->SetAttributeSampler(descriptor->m_Name.data(), shapeSmpDesc);
	return true;
}

//----------------------------------------------------------------------------

bool	CAAEScene::_UpdateTextSampler(SPendingAttribute &samplerData, SAttributeSamplerDesc *descriptor)
{
	if (!PK_VERIFY(descriptor != null) ||
		!PK_VERIFY(samplerData.m_PKDesc != null))
		return false;
	SSamplerText				*pkDesc = static_cast<SSamplerText*>(samplerData.m_PKDesc);
	STextSamplerDescriptor		*aeDesc = static_cast<STextSamplerDescriptor*>(descriptor->m_Descriptor);

	if (aeDesc == null)
		return false;

	pkDesc->UpdateText(aeDesc);

	CParticleSamplerDescriptor_Text_Default		*textSmpDesc = static_cast<CParticleSamplerDescriptor_Text_Default*>(pkDesc->m_SamplerDescriptor.Get());

	if (!PK_VERIFY(m_EffectLastInstance->SetAttributeSampler(descriptor->m_Name.data(), textSmpDesc)))
		return false;
	return true;
}

//----------------------------------------------------------------------------

bool	CAAEScene::_UpdateImageSampler(SPendingAttribute &samplerData, SAttributeSamplerDesc *descriptor)
{
	if (!PK_VERIFY(descriptor != null) ||
		!PK_VERIFY(samplerData.m_PKDesc != null))
		return false;
	SSamplerImage				*pkDesc = static_cast<SSamplerImage*>(samplerData.m_PKDesc);
	SImageSamplerDescriptor		*aeDesc = static_cast<SImageSamplerDescriptor*>(descriptor->m_Descriptor);

	if (aeDesc == null)
		return false;
	if (pkDesc->m_Dirty)
	{
		pkDesc->UpdateImage(aeDesc);
	}
	
	CParticleSamplerDescriptor_Image_Default		*imageSmpDesc = static_cast<CParticleSamplerDescriptor_Image_Default*>(pkDesc->m_SamplerDescriptor.Get());

	if (!PK_VERIFY(m_EffectLastInstance->SetAttributeSampler(descriptor->m_Name.data(), imageSmpDesc)))
		return false;
	return true;
}

//----------------------------------------------------------------------------

bool	CAAEScene::_UpdateAudioSampler(SPendingAttribute &samplerData, SAttributeSamplerDesc *descriptor)
{
	if (!PK_VERIFY(descriptor != null) ||
		!PK_VERIFY(samplerData.m_PKDesc != null))
		return false;

	SSamplerAudio		*pkDesc = static_cast<SSamplerAudio*>(samplerData.m_PKDesc);

	pkDesc->m_BuiltThisFrame = false;
	m_ActiveAudioSamplers.PushBack(pkDesc);
	return true;
}

//----------------------------------------------------------------------------

bool	CAAEScene::_UpdateVectorFieldSampler(SPendingAttribute &samplerData, SAttributeSamplerDesc *descriptor)
{
	if (!PK_VERIFY(descriptor != null) ||
		!PK_VERIFY(samplerData.m_PKDesc != null))
		return false;

	SSamplerVectorField				*pkDesc = static_cast<SSamplerVectorField*>(samplerData.m_PKDesc);
	SVectorFieldSamplerDescriptor	*aeDesc = static_cast<SVectorFieldSamplerDescriptor*>(descriptor->m_Descriptor);

	if (aeDesc == null)
		return false;
	pkDesc->UpdateVectorField(aeDesc);

	CParticleSamplerDescriptor_VectorField_Grid		*turbulenceSmpDesc = static_cast<CParticleSamplerDescriptor_VectorField_Grid*>(pkDesc->m_SamplerDescriptor.Get());

	if (!PK_VERIFY(m_EffectLastInstance->SetAttributeSampler(descriptor->m_Name.data(), turbulenceSmpDesc)))
		return false;

	return true;
}

//----------------------------------------------------------------------------

bool	CAAEScene::_LoadScene(const SSimpleSceneDef &sceneDef)
{
	m_EffectLastInstance = null;

	if (!_LoadEffectIFN(sceneDef))
		return false;

	m_EffectLastInstance = _InstantiateEffect();
	PK_ASSERT(m_EffectLastInstance != null);
	if (m_EffectLastInstance == null)	// failed instantiating
		return false;
	m_EffectDesc->m_Name = sceneDef.m_EffectPath.Data();
	if (sceneDef.m_BackdropMeshPath.Data() != null)
		m_EffectDesc->m_BackdropMesh.m_Path = sceneDef.m_BackdropMeshPath.Data();

	if (m_SkinnedMeshInstance != null)
	{
		m_SkinnedMeshInstance->Update(0.0f);
		m_SkinnedMeshInstance->StartAsyncUpdateSkin(0.0f);
		m_SkinnedMeshInstance->WaitAsyncUpdateSkin();

		m_SkinnedMeshInstance->SetupAttributeSampler(m_ResourceMesh->BatchList()[0]->RawMesh(),
													 m_IsWeightedSampling,
													 m_ColorStreamID,
													 m_WeightStreamID);
	}
	return true;
}

//----------------------------------------------------------------------------

void	CAAEScene::_CollectCurrentFrame()
{
	PK_SCOPEDPROFILE();
	m_FrameCollector.CollectFrame();
}

//----------------------------------------------------------------------------

void	CAAEScene::_RenderLastCollectedFrame()
{
	// Setup your view(s):
	// You need to specify each view's inverse matrix, that we use to billboard/sort particles
	PopcornFX::SSceneView		view;
	PKSample::SRHIRenderContext	ctx;
	CFloat4x4					matW2P = m_Camera.m_ViewInv;//AE Axis System

	matW2P.Axis(1) *= -1; //AE Y-down to PopcornFX Y-up 
	matW2P.Axis(2) *= -1; //AE Z-foward to PopcornFX Z-backward
	view.m_InvViewMatrix = matW2P; 
	view.m_NeedsSortedIndices = true;

	ctx.m_Views = TMemoryView<PopcornFX::SSceneView>(view);

	m_FrameCollector.BuildNewFrame(m_FrameCollector.GetLastCollectedFrame(), true);
	m_DrawOutputs.Clear();

	// Build necessary renderer caches
	CPopcornFXWorld					&pkfxWorld = CPopcornFXWorld::Instance();
	PAAERenderContext				currentRenderContext = pkfxWorld.GetCurrentRenderContext();

	m_ParticleRenderDataFactory.RenderThread_BuildPendingCaches(currentRenderContext->GetAEGraphicContext()->GetApiManager());

	if (m_FrameCollector.BeginRenderBuiltFrame(ctx)) // the policy fills "ctx.m_DrawOutputs".
	{
		m_FrameCollector.RenderFence(); // Blocking wait
		m_FrameCollector.EndRenderBuiltFrame(ctx, false);
		m_DrawOutputs = ctx.m_DrawOutputs;
	}
}

//----------------------------------------------------------------------------

void	CAAEScene::_UpdateEmitter(float dt)
{
	CQuaternion quat = Transforms::Quaternion::FromEuler(m_AEEmitterRotation);

	if (m_EmitterTransformType == ETransformType_2D) //If project coordinate to match 2D Widget
	{
		//normalize to -1;1
		const CFloat2	viewportDimensions = CFloat2(m_SAAEWorldData.m_WorldWidth, m_SAAEWorldData.m_WorldHeight);
		const CFloat2	normalizedViewportCoords = (m_AEEmitterPosition.xy() / CFloat2(viewportDimensions)) * CFloat2(2, 2) - 1;

		const CFloat4x4	viewProj = m_Camera.m_View * m_Camera.m_Proj;
		const CFloat4x4	viewProjInv = viewProj.Inverse();

		const CFloat4	clipPos = CFloat4(normalizedViewportCoords, 0, 1);
		const CFloat4	worldPosOnNearPlaneH = viewProjInv.TransformVector(clipPos);
		const CFloat3	worldPosOnNearPlane = worldPosOnNearPlaneH.xyz() / worldPosOnNearPlaneH.w();
		const CFloat3	worldOrigin = m_Camera.m_View.Inverse().StrippedTranslations();
		const CFloat3	worldDirection = (worldPosOnNearPlane - worldOrigin).Normalized();

		m_EmitterTransformsPrevious = m_EmitterTransformsCurrent;
		m_EmitterTransformsCurrent = Transforms::Matrix::FromQuaternion(quat, CFloat4(worldOrigin + worldDirection * m_AEEmitterPosition.z(), 1.0f));
	}
	else
	{
		CFloat3 pos(m_AEEmitterPosition.x(), m_AEEmitterPosition.y(), m_AEEmitterPosition.z());

		m_EmitterTransformsPrevious = m_EmitterTransformsCurrent;
		m_EmitterTransformsCurrent = Transforms::Matrix::FromQuaternion(quat, CFloat4(pos, 1.0f));
	}
	if (m_TeleportEmitter == true)
	{
		m_EmitterTransformsPrevious = m_EmitterTransformsCurrent;
		m_TeleportEmitter = false;
	}

	m_EmitterVelPrevious = m_EmitterVelCurrent;
	if (dt > 1.0e-6f)
	{
		const float		invDt = PKRcp(dt);
		const CFloat3	posDelta = m_EmitterTransformsCurrent.StrippedTranslations() - m_EmitterTransformsPrevious.StrippedTranslations();
		m_EmitterVelCurrent = posDelta * invDt;
	}

	if (m_LayerHolder->m_SpawnedEmitter.m_Desc->m_SimStatePrev != m_LayerHolder->m_SpawnedEmitter.m_Desc->m_SimState)
	{
		if (m_LayerHolder->m_SpawnedEmitter.m_Desc->m_SimState)
			m_EffectLastInstance = _InstantiateEffect(0, 0);
		else if (m_EffectLastInstance != null)
			m_EffectLastInstance->Stop();
	}
}

//----------------------------------------------------------------------------

void CAAEScene::_UpdateCamera()
{
	CFloat4 cameraPos = m_AECameraPos;

	m_Camera.m_View = m_AEViewMatrix ;
	if (m_EmitterTransformType == ETransformType_2D)//If project coordinate to match 2D Widget
	{
		{
			//normalize to -1;1
			m_EmitterPositionNormalized_Debug = CFloat4(cameraPos.x() - (m_SAAEWorldData.m_WorldWidth / 2.0f),
				cameraPos.y() - (m_SAAEWorldData.m_WorldHeight / 2.0f),
				cameraPos.z(),
				1.0f);
			m_Camera.m_Position = m_EmitterPositionNormalized_Debug.xyz();
		}

		m_Camera.m_View.WAxis() = CFloat4(m_Camera.m_Position.x(), m_Camera.m_Position.y(), m_Camera.m_Position.z(), 1.0f);
	}
	else
		m_Camera.m_Position = cameraPos.xyz();

	if (m_SAAEWorldData.m_WorldWidth > 0 && m_SAAEWorldData.m_WorldHeight > 0)
	{
		if (m_EmitterTransformType == ETransformType_2D)
		{
			float ratio = Units::DegreesToRadians((((m_AECameraZoom) * 2.0f) / (m_SAAEWorldData.m_WorldWidth)));
			float HFov = 2.0f * (1.0f / tan(ratio));

			_SetProj(HFov, CFloat2(m_SAAEWorldData.m_WorldWidth, m_SAAEWorldData.m_WorldHeight), m_EffectDesc->m_Camera.m_Near, m_EffectDesc->m_Camera.m_Far);
		}
		else
		{
			double	tanTheta = (((double)m_SAAEWorldData.m_WorldHeight) / (double)(2.0 * m_AECameraZoom));
			double	halfFOV = Units::RadiansToDegrees(atan(tanTheta));
			float	FOVDeg = (float)(halfFOV * 2.0);

			_SetProj(FOVDeg, CFloat2(m_SAAEWorldData.m_WorldWidth, m_SAAEWorldData.m_WorldHeight), m_EffectDesc->m_Camera.m_Near, m_EffectDesc->m_Camera.m_Far);
		}
	}
	else
	{
		PK_ASSERT_NOT_REACHED();
	}
	m_Camera.Update();
}

//----------------------------------------------------------------------------

void CAAEScene::_UpdateMediumCollectionView()
{
	const CFloat4x4			matW2V = m_Camera.m_View;			//AE Axis system
	const CFloat4x4			matW2P = matW2V * m_Camera.m_Proj;	//RHYUP

	PK_ASSERT(m_ViewSlotInMediumCollection.Valid());
	m_ParticleMediumCollection->UpdateView(m_ViewSlotInMediumCollection, matW2V, matW2P, m_Camera.m_WinSize);
}

//----------------------------------------------------------------------------

void	CAAEScene::_OnUpdateComplete(CParticleMediumCollection *collection)
{
	(void)collection;
	for (u32 i = 0; i < m_ActiveAudioSamplers.Count(); ++i)
	{
		m_ActiveAudioSamplers[i]->ReleaseAEResources();
	}
	m_ActiveAudioSamplers.Clear();
}

//----------------------------------------------------------------------------

bool CAAEScene::_LoadEffectIFN(const SSimpleSceneDef &sceneDef)
{
	CPopcornFXWorld		&world = CPopcornFXWorld::Instance();

	if (m_LayerHolder->m_LayerProperty == null)
		world.CreateLayerPropertyIFP(m_LayerHolder);

	if (m_Effect != null && sceneDef.m_Refresh == false)
		return true;

	for (u32 i = 0; i < m_OverridableProperties.Count(); ++i)
		PK_SAFE_DELETE(m_OverridableProperties[i]);
	m_OverridableProperties.Clean();
	if (sceneDef.m_EffectPath == null)
	{
		CLog::Log(PK_ERROR, "empty effect path.");
		return false;
	}

	CLog::Log(PK_INFO, "Loading Effect : %s", sceneDef.m_EffectPath.Data());

	IFileSystem	*fileSystem = File::DefaultFileSystem();

	// Method #1 for loading: Pass down your own memory buffer to the load function
	u32			rawFileSize = 0;	// will be filled by 'Bufferize' call below
	u8			*rawFileBuffer = fileSystem->Bufferize(m_LoadedPack->Path() / sceneDef.m_EffectPath, &rawFileSize, true);
	if (rawFileBuffer != null)
	{
		CConstMemoryStream	memoryFileView(rawFileBuffer, rawFileSize);

		PBaseObjectFile objFile = m_HBOContext->LoadFileFromStream(memoryFileView, sceneDef.m_EffectPath);

		if (!PK_VERIFY(objFile != null))
		{
			CLog::Log(PK_ERROR, "Loading file from stream failed: %s", sceneDef.m_EffectPath.Data());
			return false;
		}

		if (m_LayerHolder->m_LayerProperty != null)
		{
			for (auto obj : objFile->ObjectList())
			{
				PLayerCompileCache	srcLayer = HBO::Cast<CLayerCompileCache>(obj.Get());

				if (srcLayer != null)
				{
					for (auto &srcRenderer : srcLayer->Renderers())
					{
						for (u32 pidx = 0; pidx < srcRenderer->Properties().Count(); ++pidx)
						{
							CLayerCompileCacheRendererProperty	*srcProp = srcRenderer->Properties()[pidx];

							if (srcProp->PropertyType() == PropertyType_TexturePath)
							{
								SRendererProperties *prop = PK_NEW(SRendererProperties(srcProp->PropertyName(), srcProp->PropertyValueStr(), srcLayer->LayerName(), srcLayer->UID(), CStringId(m_EffectDesc->m_UUID.c_str()), srcRenderer->UID(), srcProp->UID()));
								if (!PK_VERIFY(m_OverridableProperties.PushBack(prop).Valid()))
									return false;

								for (u32 i = 0; i < m_LayerHolder->m_LayerProperty->RendererProperties().Count(); ++i)
								{
									if (srcRenderer->UID() == m_LayerHolder->m_LayerProperty->RendererProperties()[i]->RendererID() &&
										srcProp->UID() == m_LayerHolder->m_LayerProperty->RendererProperties()[i]->PropertyID())
									{
										srcProp->SetPropertyValueStr(m_LayerHolder->m_LayerProperty->RendererProperties()[i]->Value());
										prop->m_Value = srcProp->PropertyValueStr();
									}
								}
							}
						}
					}
				}
			}

			if (m_EffectFile != null)
			{
				m_Effect = null;
				m_EffectFile->Unload();
				m_EffectFile = null;
			}
		}
		if (m_Effect == null)
			m_Effect = CParticleEffect::Load(objFile);

		PK_FREE(rawFileBuffer);

		if (!PK_VERIFY(m_Effect != null))
		{
			CLog::Log(PK_ERROR, "Failed loading effect \"%s\"", sceneDef.m_EffectPath.Data());
			return false;
		}
		if (!m_Effect->Install(m_ParticleMediumCollection))
		{
			CLog::Log(PK_ERROR, "Failed Install effect \"%s\"", sceneDef.m_EffectPath.Data());
			return false;
		}
		m_AttributesList = m_Effect->AttributeFlatList().Get();
		if (!PK_VERIFY(m_AttributesList != null))
		{
			CLog::Log(PK_WARN, "no attributes descriptor");
			return false;
		}
		if (!_RebuildAttributes(sceneDef))
		{
			CLog::Log(PK_ERROR, "_RebuildAttributes failed");
			return false;
		}
		m_AttributesList = null;
	}

	m_EffectPath = sceneDef.m_EffectPath;
	return true;
}

//----------------------------------------------------------------------------

PParticleEffectInstance CAAEScene::_InstantiateEffect()
{
	_UpdateCamera();
	_UpdateEmitter(m_DT);
	return _InstantiateEffect(0, 0);
}

//----------------------------------------------------------------------------
// This version if 'InstantiateEffect' takes two time arguments to pinpoint with accuracy
// where during the frame the instantiation happened. If you don't care, just pass zero in both parameters.

PParticleEffectInstance	CAAEScene::_InstantiateEffect(float timeFromStartOfFrame, float timeToEndOfFrame)
{
	PK_ASSERT(m_Effect != null);
	PK_ASSERT(timeFromStartOfFrame >= 0.0f);
	PK_ASSERT(timeToEndOfFrame >= 0.0f);

	// CParticleEffect::Instantiate() returns an effect instance, registered into the specified medium collection
	PParticleEffectInstance	instance = m_Effect->Instantiate_AlreadyInstalled(m_ParticleMediumCollection);

	if (instance != null)
	{
		// The death notifier will be called as soon as the instance dies. See the comment above the 'm_DeathNotifier' member in <ps_effect.h>
		instance->m_DeathNotifier += FastDelegate<void(const PParticleEffectInstance &)>(this, &CAAEScene::_OnInstanceDeath);

		instance->EnableSeed(true);
		instance->Seed(m_LayerHolder->m_SpawnedEmitter.m_Desc->m_Seed);

		SEffectStartCtl	effectStartCtl;
		effectStartCtl.m_TimeFromStartOfFrame = timeFromStartOfFrame;
		effectStartCtl.m_TimeToEndOfFrame = timeToEndOfFrame;
		effectStartCtl.m_SpawnTransformsPack.m_WorldTr_Current = &m_EmitterTransformsCurrent;
		effectStartCtl.m_SpawnTransformsPack.m_WorldTr_Previous = &m_EmitterTransformsPrevious;
		effectStartCtl.m_SpawnTransformsPack.m_WorldVel_Current = &m_EmitterVelCurrent;
		effectStartCtl.m_SpawnTransformsPack.m_WorldVel_Previous = &m_EmitterVelPrevious;

		// Start the instance. (Frame start/end times are optional, you can call 'instance->Start(xforms)' if you don't care)
		instance->Start(effectStartCtl);
	}

	return instance;
}

//----------------------------------------------------------------------------

void	CAAEScene::_OnInstanceDeath(const PParticleEffectInstance &instance)
{
	PK_ASSERT(instance != null);
	// This callback will be called from worker threads

	// Not strictly necessary to -= the callback, the instance is getting destroyed anyway.
	instance->m_DeathNotifier -= FastDelegate<void(const PParticleEffectInstance &)>(this, &CAAEScene::_OnInstanceDeath);
	if (instance == m_EffectLastInstance)
	{
		instance->KillImmediate();
		m_EffectLastInstance = null;
	}
}

//----------------------------------------------------------------------------

bool	CAAEScene::_RebuildAttributes(const SSimpleSceneDef &sceneDef)
{
	(void)sceneDef;

	auto		&attrList = m_AttributesList->AttributeAndSamplerList();
	const u32	attrCount = attrList.Count();

	TArray<SAttributeBaseDesc*>	attrOrder;

	for (u32 i = 0; i < attrCount; ++i)
	{
		CParticleAttributeDeclarationAbstract		*attrAbstractDecl = attrList[i];

		PK_ASSERT(attrAbstractDecl != null);

		CString				category = attrAbstractDecl->CategoryName().MapENG().ToAscii();

		if (!attrAbstractDecl->IsSampler())
		{
			CParticleAttributeDeclaration		*attrDecl = attrAbstractDecl->AsAttribute();
			PK_ASSERT(attrDecl != null);

			SAttributeDesc						*attrDesc = new SAttributeDesc(attrDecl->ExportedName().Data(), (const char*)category.Data(), AttributePKToAAE((EBaseTypeID)attrDecl->ExportedType()), AttributePKToAAE(attrDecl->GetEffectiveDataSemantic()));

			SAttributesContainer_SAttrib		value = attrDecl->GetDefaultValue();
			SAttributesContainer_SAttrib		valueMin = attrDecl->GetMinValue();
			SAttributesContainer_SAttrib		valueMax = attrDecl->GetMaxValue();

			attrDesc->m_HasMax = attrDecl->HasMax();
			attrDesc->m_HasMin = attrDecl->HasMin();
			EAttributeType						type = attrDesc->m_Type;
			if (type >= AttributeType_Bool1 && type <= AttributeType_Bool4)
			{
				attrDesc->SetValue(value.Get<bool>());
				attrDesc->SetDefaultValue(value.Get<bool>());
				attrDesc->SetMinValue(valueMin.Get<bool>());
				attrDesc->SetMaxValue(valueMax.Get<bool>());
			}
			if (type >= AttributeType_Int1 && type <= AttributeType_Int4)
			{
				attrDesc->SetValue(value.Get<s32>());
				attrDesc->SetDefaultValue(value.Get<s32>());
				attrDesc->SetMinValue(valueMin.Get<s32>());
				attrDesc->SetMaxValue(valueMax.Get<s32>());
			}
			if (type >= AttributeType_Float1 && type <= AttributeType_Float4)
			{
				attrDesc->SetValue(value.Get<float>());
				attrDesc->SetDefaultValue(value.Get<float>());
				attrDesc->SetMinValue(valueMin.Get<float>());
				attrDesc->SetMaxValue(valueMax.Get<float>());
			}
			attrDesc->m_IsDefaultValue = true;

			if (!PK_VERIFY(attrOrder.PushBack(attrDesc).Valid()))
				return false;
		}
		else
		{
			CParticleAttributeSamplerDeclaration	*samplerDecl = attrAbstractDecl->AsSampler();
			PK_ASSERT(samplerDecl != null);

			SAttributeSamplerDesc	*smplrDesc = new SAttributeSamplerDesc(samplerDecl->ExportedName().Data(), (const char*)category.Data(), AttributeSamplerPKToAAE((SParticleDeclaration::SSampler::ESamplerType)samplerDecl->ExportedType()));

			switch (smplrDesc->m_Type)
			{
			case AttributeSamplerType_Geometry:
				smplrDesc->m_Descriptor = new SShapeSamplerDescriptor();
				break;
			case AttributeSamplerType_Text:
				smplrDesc->m_Descriptor = new STextSamplerDescriptor();
				break;
			case AttributeSamplerType_Image:
				smplrDesc->m_Descriptor = new SImageSamplerDescriptor();
				break;
			case AttributeSamplerType_Audio:
			{
				smplrDesc->m_Descriptor = new SAudioSamplerDescriptor();

				CResourceDescriptor_Audio	*nodeSamplerData = HBO::Cast<CResourceDescriptor_Audio>(samplerDecl->AttribSamplerDefaultValue().Get());
				if (PK_VERIFY(nodeSamplerData != null))
					((SAudioSamplerDescriptor*)smplrDesc->m_Descriptor)->m_ChannelGroup = nodeSamplerData->ChannelGroup().Data();
				break;
			}
			case AttributeSamplerType_VectorField:
				smplrDesc->m_Descriptor = new SVectorFieldSamplerDescriptor();
				break;
			default:
				smplrDesc->m_Descriptor = null;
				break;
			}
			if (smplrDesc->m_Descriptor)
				smplrDesc->m_Descriptor->m_UsageFlags = samplerDecl->UsageFlags();

			if (!PK_VERIFY(attrOrder.PushBack(smplrDesc).Valid()))
				return false;
		}
	}

	typedef TArray<SAttributeBaseDesc*>::Iterator	attrDescIt;

	struct	SSortAttributesPolicy
	{
		PK_FORCEINLINE static bool	Less(const attrDescIt &it0, const attrDescIt &it1)
		{
			int		cmpCategoryRes = (*it0)->m_CategoryName.compare((*it1)->m_CategoryName);
	
			if (cmpCategoryRes < 0)
				return true;
			if (cmpCategoryRes > 0)
				return false;
			if (cmpCategoryRes == 0)
			{
				if ((*it0)->m_Name.compare((*it1)->m_Name) < 0)
					return true;
				else
					return false;
			}
			return false;
		}
	
		PK_FORCEINLINE static bool	Equal(const attrDescIt &it0, const attrDescIt &it1)
		{
			int		cmpCategoryRes = (*it0)->m_CategoryName.compare((*it1)->m_CategoryName);
			int		cmpNameRes = (*it0)->m_Name.compare((*it1)->m_Name);
			return cmpCategoryRes == 0 && cmpNameRes == 0;
		}
	};

	QuickSort<SSortAttributesPolicy, attrDescIt>(attrOrder.Begin(), attrOrder.End());

	CPopcornFXWorld::Instance().HandleNewAttributes(attrOrder, m_EffectRef, m_LayerHolder, false);
	return true;
}

//----------------------------------------------------------------------------

void	CAAEScene::_ExtractAEFrameInfo(SAAEIOData &AAEData)
{
	u32 targetFrame = AAEData.m_InData->current_time / AAEData.m_InData->local_time_step;
	if (m_FrameNumber + 1 != targetFrame)
		m_ForceRestartSeeking = true;
	else
		m_ForceRestartSeeking = false;
	m_FrameNumber = targetFrame;

	m_DT = (float)((double)AAEData.m_InData->local_time_step / (double)AAEData.m_InData->time_scale);
	m_PreviousTimeSec = m_CurrentTimeSec;
	m_CurrentTimeSec = (float)((double)AAEData.m_InData->current_time / (double)AAEData.m_InData->time_scale);
}

//----------------------------------------------------------------------------

bool	CAAEScene::_CheckRenderAbort(SAAEIOData *AAEData)
{
	if (AAEData == null)
	{
		CLog::Log(PK_INFO, "RenderAbort");
		return true;
	}
	PF_Err	res = PF_Err_NONE;
	res = PF_ABORT(AAEData->m_InData);
	if (res != PF_Err_NONE)
	{
		AAEData->m_ReturnCode = res;
		CLog::Log(PK_INFO, "RenderAbort");
		return true;
	}
	return false;
}

//----------------------------------------------------------------------------

void	CAAEScene::_SetProj(float fovydegree, const CFloat2 &winDimPixel, float zNear, float zFar)
{
	float	fX, fY;

	m_Camera.m_ProjNear = zNear;
	m_Camera.m_ProjFar = zFar;
	m_Camera.m_WinSize = winDimPixel;
	if (m_EmitterTransformType == ETransformType_2D)
	{
		const float	aspect = winDimPixel.y() / winDimPixel.x();
		fX = 1.f / tanf(Units::DegreesToRadians(fovydegree) * 0.5f);
		fY = fX / aspect;
	}
	else
	{
		m_Camera.m_ProjFovy = fovydegree;
		const float	aspect = winDimPixel.x() / winDimPixel.y();
		fY = 1.f / tanf(Units::DegreesToRadians(fovydegree * 0.5f));
		fX = fY / aspect;
	}

	const float	kRcpRange = 1.0f / (zNear - zFar);
	const float	kA = (zFar + zNear) * kRcpRange;
	const float	kB = (zFar * zNear) * kRcpRange;

	RHI::EGraphicalApi api = CPopcornFXWorld::Instance().GetRenderApi();
	if (api == RHI::GApi_D3D11 || api == RHI::GApi_D3D12 || api == RHI::GApi_Metal)
	{
		// GApi_D3D11: GApi_D3D12
		m_Camera.m_Proj = CFloat4x4(fX, 0,   0,                0,
									0,  -fY, 0,                0,
									0,  0,   0.5f * kA - 0.5f, -1,
									0,  0,   kB,               0);
	}
	else
	{
		PK_ASSERT_NOT_REACHED_MESSAGE("No projection matrix for this API");
	}

	CFloat4x4		current2RHYUp = CFloat4x4::IDENTITY;
	CCoordinateFrame::BuildTransitionFrame(CCoordinateFrame::GlobalFrame(), Frame_RightHand_Y_Up, current2RHYUp);

	m_Camera.m_Proj = current2RHYUp * m_Camera.m_Proj;
}

//----------------------------------------------------------------------------

void	CAAEScene::_FillAdditionnalDataForRender()
{
	TArray<SSkinnedDataSimple>	&outSkinnedDatas = m_FXInstancesSkinnedData;

	outSkinnedDatas.Clear();

	const PSkinnedMeshInstance	instance = m_SkinnedMeshInstance;
	if (instance == null)
		return;
	if (!PK_VERIFY(outSkinnedDatas.PushBack().Valid()))
		return;

	SSkinnedDataSimple	&skinnedDataSimple = outSkinnedDatas.Last();

	skinnedDataSimple.m_Valid = (instance->m_SkinnedMesh != null && instance->m_SkinnedMesh->Valid() && instance->m_SkinnedMesh->HasGeometry());
	skinnedDataSimple.m_SubMeshes.Clear();

	if (skinnedDataSimple.m_Valid)
	{
		if (!PK_VERIFY(skinnedDataSimple.m_SubMeshes.Reserve(instance->m_SkinnedMesh->SubMeshCount())))
			return;

		for (u32 smidx = 0, smCount = instance->m_SkinnedMesh->SubMeshCount(); smidx < smCount; smidx++)
		{
			SSkinnedDataSimple::SSkinnedDataSubMesh	submesh;
			submesh.m_SubMeshID = smidx;
			submesh.m_Positions = instance->m_SkinnedMesh->Positions(smidx);
			submesh.m_Normals = instance->m_SkinnedMesh->Normals(smidx);
			if (!submesh.m_Positions.Empty() && !submesh.m_Normals.Empty())
				skinnedDataSimple.m_SubMeshes.PushBackUnsafe(submesh);
		}
	}

	for (u32 iInstance = 0; iInstance < 1; ++iInstance)
	{
		SSkinnedDataSimple	&skinnedData = m_FXInstancesSkinnedData[iInstance];
		if (!skinnedData.m_Valid)
			continue;

		// Generate data for render.
#if 1 // This data is not used ??? If so, just if-0
		for (u32 smidx = 0; smidx < skinnedData.m_SubMeshes.Count(); smidx++)
		{
			auto	&submesh = skinnedData.m_SubMeshes[smidx];

			const u32	posCount = submesh.m_Positions.Count();
			PK_ASSERT(posCount != 0 && posCount == submesh.m_Normals.Count());
			const u32	totalSizeInFloats = posCount * 3 + submesh.m_Normals.Count() * 3;
			PK_ASSERT(totalSizeInFloats > 0);
			if (!PK_VERIFY(submesh.m_RawDataForRendering.Resize(totalSizeInFloats)))
			{
				skinnedData.m_Valid = false; // discard the entire instance
				continue;
			}

			CFloat3						*dstData = reinterpret_cast<CFloat3*>(submesh.m_RawDataForRendering.RawDataPointer());
			TStridedMemoryView<CFloat3>	dstPositions(dstData, posCount, sizeof(CFloat3));
			TStridedMemoryView<CFloat3>	dstNormals(dstData + posCount, posCount, sizeof(CFloat3));
			Mem::CopyStreamToStream(dstPositions, submesh.m_Positions);
			Mem::CopyStreamToStream(dstNormals, submesh.m_Normals);
		}
#else
		skinnedData.m_Valid = false; // No render-data generated
#endif
	}
}

//----------------------------------------------------------------------------

#if (PK_PARTICLES_UPDATER_USE_D3D12 != 0 || PK_PARTICLES_UPDATER_USE_D3D11 != 0)
bool	CAAEScene::SimDispatchMask(const PopcornFX::CParticleDescriptor *descriptor, PopcornFX::SSimDispatchHint &outHint)
{
	const PopcornFX::SParticleDeclaration	&decl = descriptor->ParticleDeclaration();
	// Tell PopcornFX Runtime that our code does not handle any of the following renderers on GPU:
	// if a particle layer contain any of those, even if the simulation graph has compute shader bytecodes, it will fallback on CPU.
	if (!decl.m_LightRenderers.Empty() ||
		!decl.m_SoundRenderers.Empty() ||
		!decl.m_RibbonRenderers.Empty() ||
		!decl.m_TriangleRenderers.Empty() ||
		!decl.m_DecalRenderers.Empty())
	{
		outHint.m_AllowedDispatchMask &= ~(1 << PopcornFX::SSimDispatchHint::Location_GPU);	// can't draw any of these on the GPU
	}
	// if we are allowed on GPU, prefer GPU, otherwise prefer CPU:
	if (outHint.m_AllowedDispatchMask & (1 << PopcornFX::SSimDispatchHint::Location_GPU))
		outHint.m_PreferredDispatchMask = (1 << PopcornFX::SSimDispatchHint::Location_GPU);
	else
		outHint.m_PreferredDispatchMask = (1 << PopcornFX::SSimDispatchHint::Location_CPU);
	return true;
}

//----------------------------------------------------------------------------

void	CAAEScene::D3D12_EnqueueTask(const PopcornFX::PParticleUpdaterTaskD3D12 &task)
{
	CPopcornFXWorld		&pkfxWorld = CPopcornFXWorld::Instance();
	PAAERenderContext	renderContext = pkfxWorld.GetCurrentRenderContext();
	CAAED3D12Context	*D3D12Context = static_cast<CAAED3D12Context*>(renderContext->GetAEGraphicContext());

	if (D3D12Context != null && D3D12Context->m_D3D12Context->m_CommandQueue != null)	// Your ID3D12CommandQueue (can be D3D12_COMMAND_LIST_TYPE_DIRECT or D3D12_COMMAND_LIST_TYPE_COMPUTE)
		task->Execute(D3D12Context->m_D3D12Context->m_CommandQueue);					// Internally calls commandQueue->ExecuteCommandLists and fences
}
#endif

//----------------------------------------------------------------------------

__AEGP_PK_END
