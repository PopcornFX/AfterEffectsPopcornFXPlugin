//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#pragma once

#ifndef	__FX_WORLD_H__
#define	__FX_WORLD_H__

#include <AEConfig.h>

#include <entry.h>
#if defined(PK_MACOSX)
#	include <Foundation/Foundation.h>
#endif
#include <AE_GeneralPlug.h>
#include <AE_GeneralPlugPanels.h>
#include <A.h>
#include <SPSuites.h>
#include <AE_Macros.h>

#include <pk_base_object/include/hbo_object.h>

#include <pk_kernel/include/kr_refptr.h>

#include <pk_kernel/include/kr_threads_basics.h>
#include <pk_kernel/include/kr_string_id.h>


#include "AEGP_Define.h"
#include "AEGP_Attribute.h"
#include "AEGP_Scene.h"
#include "AEGP_LayerHolder.h"
#include "AEGP_VaultHandler.h"

#include <pk_rhi/include/Enums.h>

#include <PK-SampleLib/RHIRenderParticleSceneHelpers.h>

//----------------------------------------------------------------------------

namespace AAePk
{
	struct	SAAEIOData;
	struct	SAttributeDesc;
	struct	SAttributeSamplerDesc;
	struct	SEmitterDesc;
}

namespace PopcornFX
{
	PK_FORWARD_DECLARE(LogListenerFile);
}


__AEGP_PK_BEGIN
//----------------------------------------------------------------------------

PK_FORWARD_DECLARE(EffectBaker);
PK_FORWARD_DECLARE(AAERenderContext);

class CPanelBaseGUI;

//----------------------------------------------------------------------------

struct SUIEvent
{
	virtual ~SUIEvent() {};
	virtual bool	Exec() = 0;
};

//----------------------------------------------------------------------------

struct SUIEventString : public SUIEvent
{
	SLayerHolder												*m_TargetLayer;
	CString														m_Data;
	PopcornFX::FastDelegate<bool(SLayerHolder *, CString &)>	m_Cb;

	virtual bool	Exec() override
	{
		return m_Cb(m_TargetLayer, m_Data);
	}
};

//----------------------------------------------------------------------------

struct SAEPreferenciesKeys
{
	static constexpr const char	*kSection = "AEPocornFX";
	static constexpr const char	*kApi = "Api";

	static constexpr const EApiValue	kSupportedAPIs[] =
	{
#if PK_BUILD_WITH_D3D12_SUPPORT != 0
		EApiValue::D3D11,
#endif
#if PK_BUILD_WITH_D3D11_SUPPORT != 0
		EApiValue::D3D12,
#endif
#if	PK_BUILD_WITH_METAL_SUPPORT != 0
		EApiValue::Metal,
#endif
	};
	static constexpr const char	*kApiNames[EApiValue::Size] = { "Unknown", "DirectX 11", "DirectX 12", "Metal" };

	static const char	*GetGraphicsApiAsCharPtr(EApiValue value);
	static const char	*GetGraphicsApiAsCharPtr(RHI::EGraphicalApi value);
};

//----------------------------------------------------------------------------

class HBO_CLASS(CAEPProjectProperties), public CBaseObject
{
public:
	HBO_FIELD(CString, 					ProjectName);
	HBO_FIELD(TArray<CLayerProperty*>,	LayerProperties);

public:
	CAEPProjectProperties();
	~CAEPProjectProperties();

	HBO_CLASS_DECLARATION();
};
PK_DECLARE_REFPTRCLASS(AEPProjectProperties);

//----------------------------------------------------------------------------

class CPopcornFXWorld
{
public:
							~CPopcornFXWorld();
	static CPopcornFXWorld	&Instance();

	bool					Setup(SPBasicSuite *pica_basicP, AEGP_PluginID id);
	bool					SetCommandHandle(AEGP_Command &command, const char *name);

	AEGP_PluginID			GetPluginID();
	SPBasicSuite			*GetAESuites();

	bool					FetchAEProject();

	RHI::EGraphicalApi		GetRenderApi();
	void					SetRenderApi(EApiValue AEGraphicsApi);

	bool					IdleUpdate();
	bool					HandleNewAttributeEvent(PF_ProgPtr &effectRef, SAttributeDesc *desc, bool asyncMerge = true, SLayerHolder *layer = null);
	bool					HandleNewAttributeSamplerEvent(PF_ProgPtr &effectRef, SAttributeSamplerDesc *desc, bool asyncMerge = true, SLayerHolder *layer = null);
	bool					HandleNewAttributes(TArray<SAttributeBaseDesc*> &attributes, PF_ProgPtr &effectRef, SLayerHolder *layer, bool asyncMerge = true);

	void					CreatePanel(AEGP_PlatformViewRef container, AEGP_PanelH panelH, AEGP_PanelFunctions1 *outFunctionTable, AEGP_PanelRefcon *outRefcon);
	void					Command(AEGP_Command command, AEGP_HookPriority hookPriority, A_Boolean alreadyHandled, A_Boolean *handledPB);
	void					UpdateMenu(AEGP_WindowType activeWindow);

	void					ClearAttributesAndSamplers(SLayerHolder *layer);

	CStringId				GetAEEffectID(AEGP_EffectRefH &effect, s32 paramIdx);

	CStringId				GetAttributeID(AEGP_EffectRefH &effect);
	CStringId				GetAttributeID(SAttributeBaseDesc *desc);

	CStringId				GetAttributeSamplerID(AEGP_EffectRefH &effect);

	s32						_ExecSPendingEmitters(SLayerHolder *layer);

	s32						_ExecSPendingAttributes(SLayerHolder *layer);
	s32						_ExecClearAttributes(SLayerHolder *layer);

	bool					_ExecDeleteAttribute(SPendingAttribute *attribute, AEGP_EffectRefH &effectRef);
	bool					_ExecDeleteAttributeSampler(SPendingAttribute *attribute, AEGP_EffectRefH &effectRef);

	bool					_SetupAutoRender(AEGP_EffectRefH &effect);

	bool					_GetAEPath(AEGP_GetPathTypes type, CString &dst);

	//Pack Management
	bool					SetDestinationPackFromPath(SLayerHolder &layer, const CString &packPath);
	
	CString					GetPluginInstallationPath();
	CString					GetInternalPackPath();
	CString					GetResourcesPath();

	CString						GetPluginVersion() const;
	AEGP_InstalledEffectKey		GetPluginEffectKey(EPKChildPlugins type) const;

	void					SetWorkerCount(u32 count) { m_WorkerCount = count; }
	u32						GetWorkerCount() const { return m_WorkerCount; }

	CVaultHandler			&GetVaultHandler() { return m_VaultHandler; }

	void					RefreshAssetList();

	TArray<SLayerHolder*>	&GetLayers() { return m_Layers; }
	SLayerHolder			*GetLayerForSEmitterDesc(SEmitterDesc *desc);
	SLayerHolder			*GetLayerForSEmitterDescID(CStringId id);

	bool					SetSelectedEffectAsync(SLayerHolder *targetLayer, CString &name);
	bool					SetSelectedEffectFromPath(SEmitterDesc *desc, CString path, bool forceReload = false);
	bool					SetSelectedEffect(SLayerHolder *layer, CString &name);
	bool					SetEffectDefaultTransform(SLayerHolder *layer, const CFloat3 &pos, const CFloat3 &rot);
	bool					SetBackdropMeshDefaultTransform(SLayerHolder *layer);
	
	bool					SetPanelInstance(CPanelBaseGUI *panel);

	//Suite
	bool					InitializeIFN(SAAEIOData &AAEData);
	bool					HandleNewEmitterEvent(AAePk::SAAEIOData &AAEData, SEmitterDesc *desc);
	bool					HandleDeleteEmitterEvent(AAePk::SAAEIOData &AAEData, SEmitterDesc *desc);

	bool					CheckEmitterValidity(AAePk::SAAEIOData &AAEData, AAePk::SEmitterDesc *descriptor);
	bool					UpdateScene(SAAEIOData &AAEData, SEmitterDesc *desc);
	bool					UpdateEmitter(SAAEIOData &AAEData, SEmitterDesc *desc);

	bool					InvalidateEmitterRender(SLayerHolder *layer, AEGP_EffectRefH effectRef);
	bool					InvalidateEmitterRender(SAAEIOData &AAEData, SEmitterDesc *desc);

	bool					LaunchEditorAsPopup(AAePk::SAAEIOData &AAEData, SEmitterDesc *desc);

	bool					ShutdownIFN();

	void					SetProfilingState(bool state);

	void					SetParametersIndexes(const int *indexes, EPKChildPlugins plugin);
	bool					SetDefaultLayerPosition(SAAEIOData& AAEData, AEGP_LayerH layer);
	bool					MoveEffectIntoCurrentView(SAAEIOData &AAEData, SEmitterDesc *descriptor);

	PAAERenderContext		GetCurrentRenderContext();

	void					OnEndSetupScene();

	bool					GetMostRecentCompName(CString &compName);
	bool					SetLayerName(SLayerHolder *layer);
	bool					SetLayerCompName(SLayerHolder *layer);

	const PBaseObjectFile	&GetProjectConfFile();
	bool					CreateLayerPropertyIFP(SLayerHolder *layer);

	bool					SetResourceOverride(CStringId layerID, u32 rdrID, u32 propID, const CString &value);
	bool					WriteProjectFileModification();

	//HOOK
	static	A_Err			DeathHook(AEGP_GlobalRefcon pluginRefCon, AEGP_DeathRefcon refCon);
	static	A_Err			IdleHook(AEGP_GlobalRefcon pluginRefCon, AEGP_IdleRefcon refCon, A_long *maxSleep);

	static A_Err			CreatePanelHook(AEGP_GlobalRefcon		pluginRefconP,
											AEGP_CreatePanelRefcon	refconP,
											AEGP_PlatformViewRef	container,
											AEGP_PanelH				panelH,
											AEGP_PanelFunctions1	*outFunctionTable,
											AEGP_PanelRefcon		*outRefcon);

	static A_Err			CommandHook(	AEGP_GlobalRefcon		plugin_refconP,
											AEGP_CommandRefcon		refconP,
											AEGP_Command			command,
											AEGP_HookPriority		hook_priority,
											A_Boolean				already_handledB,
											A_Boolean				*handledPB);

	static A_Err			UpdateMenuHook(	AEGP_GlobalRefcon		plugin_refconP,
											AEGP_UpdateMenuRefcon	refconP,
											AEGP_WindowType			active_window);

	Threads::CCriticalSection	&GetRenderLock() { return m_RenderLock; }
	
private:
	CPopcornFXWorld();


	static CPopcornFXWorld			*m_Instance;

	bool							m_Initialized;

	CString							m_ClassName;

	TArray<CThreadID>				m_AAETreadID;
	u32								m_WorkerCount;

	TArray<SLayerHolder*>			m_Layers;

	Threads::CCriticalSection		m_Lock;

	CPanelBaseGUI					*m_Panel = null;

	SPBasicSuite					*m_Suites;
	AEGP_PluginID					m_AEGPID;
	AEGP_InstalledEffectKey			m_PKInstalledPluginKeys[_PLUGIN_COUNT];

	AEGP_Command					m_Command = 42;
	CString							m_CommandName;

	RHI::EGraphicalApi				m_GraphicsApi;

	CVaultHandler					m_VaultHandler;

	//Developpement
	float							m_CameraZoom = 2000.0;

	PFilePack						m_ProjectPack = null;
	PBaseObjectFile					m_ProjectConfFile = null;
	PAEPProjectProperties			m_ProjectProperty = null;
	CString							m_AEProjectFilename;
	CString							m_AEProjectPath;
	PLogListenerFile				m_AELogFileListener;

	CString							m_PluginPath;
	CString							m_UserPluginPath;
	CString							m_AllUserPluginPath;
	CString							m_AEPath;
	CString							m_MostRecentCompName;


	Threads::CCriticalSection		m_UIEventLock;
	TArray<SUIEvent*>				m_UIEvents;

	Threads::CCriticalSection		m_RenderLock;
};

//----------------------------------------------------------------------------

__AEGP_PK_END

#endif //!__FX_WORLD_H__

