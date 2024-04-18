//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#include "ae_precompiled.h"

#include "AEGP_Main.h"
#include "AEGP_World.h"
#include "AEGP_FileDialog.h"
#include "AEGP_PopcornFXPlugins.h"
#include "AEGP_Scene.h"
#include "AEGP_AEPKConversion.h"
#include "AEGP_Log.h"
#include "AEGP_VaultHandler.h"
#include <PopcornFX_Suite.h>

//AE
#include <AE_GeneralPlug.h>
#include <SuiteHelper.h>

#include <cstdlib>

#include <pk_kernel/include/kr_string.h>

using namespace PopcornFX;
//----------------------------------------------------------------------------

static SPAPI A_Err		InitializePopcornFXIFN(AAePk::SAAEIOData& AAEData)
{
	AEGPPk::CAELog::SetIOData(&AAEData);
	if (AEGPPk::CPopcornFXWorld::Instance().InitializeIFN(AAEData) == false)
	{
		AEGPPk::CAELog::ClearIOData();
		return A_Err_GENERIC;
	}
	return A_Err_NONE;
}

//----------------------------------------------------------------------------

static SPAPI A_Err		HandleNewEmitterEvent(AAePk::SAAEIOData& AAEData, AAePk::SEmitterDesc *descriptor)
{
	AEGPPk::CAELog::SetIOData(&AAEData);
	if (AEGPPk::CPopcornFXWorld::Instance().HandleNewEmitterEvent(AAEData, descriptor) == false)
	{
		AEGPPk::CAELog::ClearIOData();
		return A_Err_GENERIC;
	}
	return A_Err_NONE;
}

//----------------------------------------------------------------------------

static SPAPI A_Err		HandleDeleteEmitterEvent(AAePk::SAAEIOData& AAEData, AAePk::SEmitterDesc *descriptor)
{
	AEGPPk::CAELog::SetIOData(&AAEData);
	if (AEGPPk::CPopcornFXWorld::Instance().HandleDeleteEmitterEvent(AAEData, descriptor) == false)
	{
		AEGPPk::CAELog::ClearIOData();
		return A_Err_GENERIC;
	}
	return A_Err_NONE;
}

//----------------------------------------------------------------------------

static SPAPI bool		CheckEmitterValidity(AAePk::SAAEIOData &AAEData, AAePk::SEmitterDesc *descriptor)
{
	AEGPPk::CAELog::SetIOData(&AAEData);
	bool ret = AEGPPk::CPopcornFXWorld::Instance().CheckEmitterValidity(AAEData, descriptor);
	AEGPPk::CAELog::ClearIOData();
	return ret;
}

//----------------------------------------------------------------------------

static SPAPI A_Err		UpdateScene(AAePk::SAAEIOData &AAEData, AAePk::SEmitterDesc *descriptor)
{
	AEGPPk::CAELog::SetIOData(&AAEData);
	if (AEGPPk::CPopcornFXWorld::Instance().UpdateScene(AAEData, descriptor) == false)
	{
		AEGPPk::CAELog::ClearIOData();
		return A_Err_GENERIC;
	}
	return A_Err_NONE;
}

//----------------------------------------------------------------------------

static SPAPI A_Err		UpdateEmitter(AAePk::SAAEIOData &AAEData, AAePk::SEmitterDesc *descriptor)
{
	AEGPPk::CAELog::SetIOData(&AAEData);
	if (AEGPPk::CPopcornFXWorld::Instance().UpdateEmitter(AAEData, descriptor) == false)
	{
		AEGPPk::CAELog::ClearIOData();
		return A_Err_GENERIC;
	}
	return A_Err_NONE;
}

//----------------------------------------------------------------------------

static SPAPI A_Err		DisplayMarketplacePanel(AAePk::SAAEIOData &AAEData, AAePk::SEmitterDesc *descriptor)
{
	AEGPPk::CAELog::SetIOData(&AAEData);

	AEGPPk::CPopcornFXWorld::Instance().LaunchEditorAsPopup(AAEData, descriptor);

	AAEData.m_OutData->out_flags |= PF_OutFlag_FORCE_RERENDER | PF_OutFlag_REFRESH_UI;
	return A_Err_NONE;
}

//----------------------------------------------------------------------------

static SPAPI A_Err		DisplayBrowseEffectPanel(AAePk::SAAEIOData &AAEData, AAePk::SEmitterDesc *descriptor)
{
	AEGPPk::CAELog::SetIOData(&AAEData);

	AEGPPk::SFileDialog	cbData;

	cbData.AddFilter("PopcornFX effect file (*.pkfx)", "*.pkfx");

	struct	SFunctor
	{
		void Function(const PopcornFX::CString path)
		{
			AEGPPk::CPopcornFXWorld::Instance().SetSelectedEffectFromPath(m_Descriptor, path);
		}

		AAePk::SEmitterDesc		*m_Descriptor = null;
	};

	static SFunctor	functor;

	functor.m_Descriptor = descriptor;
	cbData.SetCallback(PopcornFX::FastDelegate<void(const PopcornFX::CString)>(&functor, &SFunctor::Function));

	if (!cbData.BasicFileOpen())
	{
		AEGPPk::CAELog::ClearIOData();
		if (cbData.IsCancelled())
			return A_Err_NONE;
		return A_Err_GENERIC;
	}
	AEGPPk::CAELog::ClearIOData();
	AAEData.m_OutData->out_flags |= PF_OutFlag_FORCE_RERENDER | PF_OutFlag_REFRESH_UI;
	return A_Err_NONE;
}

//----------------------------------------------------------------------------

static SPAPI A_Err		DisplayBrowseMeshDialog(AAePk::SAAEIOData &AAEData, AAePk::SEmitterDesc *descriptor)
{
	AEGPPk::CAELog::SetIOData(&AAEData);

	AEGPPk::SFileDialog		cbData;

	cbData.AddFilter("Baked Mesh file (*.fbx)", "*.fbx");

	struct	SFunctor
	{
		void Function(const CString path)
		{
			if (m_Descriptor != null)
			{
				AEGPPk::CPopcornFXWorld		&world = AEGPPk::CPopcornFXWorld::Instance();
				AEGPPk::SResourceBakeConfig	bakeConfig;
				
				m_Descriptor->m_BackdropMesh.m_Path = world.GetVaultHandler().BakeResource(path, bakeConfig).Data();
				bakeConfig.m_IsSkeletalAnim = true;
				world.GetVaultHandler().BakeResource(path, bakeConfig).Data();
				m_Descriptor->m_LoadBackdrop = true;
				m_Descriptor->m_UpdateBackdrop = true;
			}
		}

		AAePk::SEmitterDesc*	m_Descriptor = null;
	};

	static SFunctor	functor;

	functor.m_Descriptor = descriptor;
	cbData.SetCallback(PopcornFX::FastDelegate<void(const CString)>(&functor, &SFunctor::Function));


	AEGPPk::CPopcornFXWorld		&world = AEGPPk::CPopcornFXWorld::Instance();
	AEGPPk::SLayerHolder		*layer = world.GetLayerForSEmitterDesc(descriptor);

	if (layer)
	{
		AEGP_LayerH			layerH = null;
		AEGP_SuiteHandler	suites(world.GetAESuites());
		PF_Err				result = A_Err_NONE;

		result |= suites.PFInterfaceSuite1()->AEGP_GetEffectLayer(AAEData.m_InData->effect_ref, &layerH);
		if (!PK_VERIFY(result == A_Err_NONE))
			return A_Err_GENERIC;
		if (layerH != null)
			layer->m_EffectLayer = layerH;
		if (!cbData.BasicFileOpen())
		{
			AEGPPk::CAELog::ClearIOData();
			if (cbData.IsCancelled())
				return A_Err_NONE;
			return A_Err_GENERIC;
		}
	}
	return A_Err_NONE;
}

//----------------------------------------------------------------------------

static SPAPI A_Err		DisplayBrowseEnvironmentMapDialog(AAePk::SAAEIOData &AAEData, AAePk::SEmitterDesc *descriptor)
{
	AEGPPk::CAELog::SetIOData(&AAEData);
	AEGPPk::SFileDialog	cbData;
	cbData.AddFilter("Image file", "*.*");

	struct	SFunctor
	{
		void Function(const CString path)
		{
			if (m_Descriptor != null)
			{
				AEGPPk::CPopcornFXWorld			&world = AEGPPk::CPopcornFXWorld::Instance();
				AEGPPk::SResourceBakeConfig		bakeConfig;

				bakeConfig.m_StraightCopy = true;
				m_Descriptor->m_BackdropEnvironmentMap.m_Path = world.GetVaultHandler().BakeResource(path, bakeConfig).Data();
				m_Descriptor->m_Update = true;
			}
		}
		AAePk::SEmitterDesc*	m_Descriptor = null;
	};

	static SFunctor	functor;

	functor.m_Descriptor = descriptor;
	cbData.SetCallback(PopcornFX::FastDelegate<void(const CString)>(&functor, &SFunctor::Function));

	if (!cbData.BasicFileOpen())
	{
		AEGPPk::CAELog::ClearIOData();
		if (cbData.IsCancelled())
			return A_Err_NONE;
		return A_Err_GENERIC;
	}
	return A_Err_NONE;
}

//----------------------------------------------------------------------------

static SPAPI A_Err		ReimportEffect(AAePk::SAAEIOData &AAEData, AAePk::SEmitterDesc *descriptor)
{
	if (descriptor == null || descriptor->m_PathSource.empty())
		return A_Err_NONE;

	AEGPPk::CAELog::SetIOData(&AAEData);

	PopcornFX::CString path = PopcornFX::CString(descriptor->m_PathSource.c_str()) / PopcornFX::CString(descriptor->m_Name.c_str());

	path = CFilePath::StripExtension(path) + ".pkfx";

	AEGPPk::CPopcornFXWorld::Instance().SetSelectedEffectFromPath(descriptor, path, true);

	AEGPPk::CPopcornFXWorld::Instance().InvalidateEmitterRender(AAEData, descriptor);

	AAEData.m_OutData->out_flags |= PF_OutFlag_FORCE_RERENDER | PF_OutFlag_REFRESH_UI;
	return A_Err_NONE;
}

//----------------------------------------------------------------------------

static SPAPI A_Err		Display_AttributeSampler_BrowseMeshDialog(AAePk::SAAEIOData &AAEData, AAePk::SAttributeSamplerDesc *descriptor)
{
	AEGPPk::CAELog::SetIOData(&AAEData);
	AEGPPk::SFileDialog	cbData;

	if (descriptor->m_Type == AAePk::AttributeSamplerType_Geometry)
	{
		cbData.AddFilter("Mesh file (*.fbx)", "*.fbx");
	}
	else if (descriptor->m_Type == AAePk::AttributeSamplerType_VectorField)
	{
		cbData.AddFilter("Baked Mesh file (*.fga)", "*.fga");
	}
	else
		return A_Err_NONE;
	
	struct	SFunctor
	{
		AAePk::SAttributeSamplerDesc*	m_Descriptor = null;

		void Function(const CString path)
		{
			if (m_Descriptor != null)
			{
				AEGPPk::SResourceBakeConfig		bakeConfig;

				if (m_Descriptor->m_Type == AEGPPk::AttributeSamplerType_Animtrack)
				{
					bakeConfig.m_IsAnimTrack = true;
				}

				AEGPPk::CPopcornFXWorld &world = AEGPPk::CPopcornFXWorld::Instance();
				m_Descriptor->m_ResourcePath = world.GetVaultHandler().BakeResource(path, bakeConfig).Data();
				 
			}
		}
	};

	static SFunctor	functor;

	functor.m_Descriptor = descriptor;
	cbData.SetCallback(PopcornFX::FastDelegate<void(const CString)>(&functor, &SFunctor::Function));

	if (!cbData.BasicFileOpen())
	{
		AEGPPk::CAELog::ClearIOData();
		if (cbData.IsCancelled())
			return A_Err_NONE;
		return A_Err_GENERIC;
	}
	return A_Err_NONE;
}

//----------------------------------------------------------------------------

static SPAPI A_Err	SetParametersIndexes(const int *indexes, AAePk::EPKChildPlugins plugin)
{
	AEGPPk::CPopcornFXWorld::Instance().SetParametersIndexes(indexes, plugin);
	return A_Err_NONE;
}

//----------------------------------------------------------------------------

static SPAPI A_Err		ShutdownPopcornFXIFN(AAePk::SAAEIOData &AAEData)
{
	AEGPPk::CAELog::SetIOData(&AAEData);
	if (AEGPPk::CPopcornFXWorld::Instance().ShutdownIFN() == false)
	{
		AEGPPk::CAELog::ClearIOData();
		return A_Err_GENERIC;
	}
	return A_Err_NONE;
}

//----------------------------------------------------------------------------

static SPAPI A_Err		SetDefaultLayerPosition(AAePk::SAAEIOData& AAEData, AEGP_LayerH layer)
{
	AEGPPk::CAELog::SetIOData(&AAEData);
	if (AEGPPk::CPopcornFXWorld::Instance().SetDefaultLayerPosition(AAEData, layer) == false)
	{
		AEGPPk::CAELog::ClearIOData();
		return A_Err_GENERIC;
	}
	return A_Err_NONE;
}

//----------------------------------------------------------------------------

static SPAPI A_Err		MoveEffectIntoCurrentView(AAePk::SAAEIOData &AAEData, AAePk::SEmitterDesc *descriptor)
{
	AEGPPk::CAELog::SetIOData(&AAEData);
	if (AEGPPk::CPopcornFXWorld::Instance().MoveEffectIntoCurrentView(AAEData, descriptor) == false)
	{
		AEGPPk::CAELog::ClearIOData();
		return A_Err_GENERIC;
	}
	AEGPPk::CPopcornFXWorld::Instance().InvalidateEmitterRender(AAEData, descriptor);
	AAEData.m_OutData->out_flags |= PF_OutFlag_FORCE_RERENDER | PF_OutFlag_REFRESH_UI;

	return A_Err_NONE;
}

//----------------------------------------------------------------------------

static PopcornFXSuite1	g_PopcornFXSuite =
{
	InitializePopcornFXIFN,
	HandleNewEmitterEvent,
	HandleDeleteEmitterEvent,
	CheckEmitterValidity,
	UpdateScene,
	UpdateEmitter,
	DisplayMarketplacePanel,
	DisplayBrowseEffectPanel,
	DisplayBrowseMeshDialog,
	DisplayBrowseEnvironmentMapDialog,
	ReimportEffect,
	Display_AttributeSampler_BrowseMeshDialog,
	ShutdownPopcornFXIFN,
	SetParametersIndexes,
	SetDefaultLayerPosition,
	MoveEffectIntoCurrentView,
};

//----------------------------------------------------------------------------

A_Err	EntryPointFunc(	struct SPBasicSuite		*pica_basicP,		/* >> */
						A_long					major_versionL,		/* >> */
						A_long					minor_versionL,		/* >> */
						AEGP_PluginID			aegp_plugin_id,		/* >> */
						AEGP_GlobalRefcon		*global_refconP)	/* << */
{
	(void)global_refconP;
	(void)minor_versionL;
	(void)major_versionL;

	A_Err 							err = A_Err_NONE;
	SPSuiteRef						my_ref = 0;
	AEGP_SuiteHandler				suites(pica_basicP);
	SuiteHelper<AEGP_PanelSuite1>	panelSuite(pica_basicP);
	SPSuitesSuite					*suitesOfSuite = nullptr;
	AEGP_Command					command;
	const A_u_char					*commandName = (const A_u_char*)"PopcornFX";
	AEGPPk::CPopcornFXWorld			&PKWorld = AEGPPk::CPopcornFXWorld::Instance();

	*global_refconP = (AEGP_GlobalRefcon)&PKWorld;

	if (!PKWorld.Setup(pica_basicP, aegp_plugin_id))
		return A_Err_GENERIC;

	AEGP_RegisterSuite5 *registerSuite = suites.RegisterSuite5();
	if (registerSuite != NULL)
	{
		err |= registerSuite->AEGP_RegisterDeathHook(aegp_plugin_id, AEGPPk::CPopcornFXWorld::DeathHook, 0);
		err |= registerSuite->AEGP_RegisterIdleHook(aegp_plugin_id, AEGPPk::CPopcornFXWorld::IdleHook, 0);
	}
	else
		return A_Err_MISSING_SUITE;

	AEGP_CommandSuite1 *commandSuite = suites.CommandSuite1();
	if (commandSuite != NULL)
	{
		err |= commandSuite->AEGP_GetUniqueCommand(&command);
		if (command != 0)
		{
			PKWorld.SetCommandHandle(command, (const char*)commandName);
			err |= commandSuite->AEGP_InsertMenuCommand(command, (const A_char*)commandName, AEGP_Menu_WINDOW, AEGP_MENU_INSERT_SORTED);
		}
	}
	else
		return A_Err_MISSING_SUITE;

	err |= registerSuite->AEGP_RegisterCommandHook(	aegp_plugin_id,
													AEGP_HP_BeforeAE,
													command,
													&AEGPPk::CPopcornFXWorld::CommandHook,
													(AEGP_CommandRefcon)(&PKWorld));
	err |= registerSuite->AEGP_RegisterUpdateMenuHook(	aegp_plugin_id,
														&AEGPPk::CPopcornFXWorld::UpdateMenuHook,
														null);
	err |= panelSuite->AEGP_RegisterCreatePanelHook(	aegp_plugin_id,
														commandName,
														&AEGPPk::CPopcornFXWorld::CreatePanelHook,
														(AEGP_CreatePanelRefcon)&PKWorld,
														true);

	err |= pica_basicP->AcquireSuite(kSPSuitesSuite, kSPSuitesSuiteVersion, (const void **)&suitesOfSuite);
	if (err == A_Err_NONE && suitesOfSuite)
	{
		err |= suitesOfSuite->AddSuite(	kSPRuntimeSuiteList,
										0,
										kPopcornFXSuite1,
										kPopcornFXSuiteVersion1,
										1,
										&g_PopcornFXSuite,
										&my_ref);
		err |= pica_basicP->ReleaseSuite(kSPSuitesSuite, kSPSuitesSuiteVersion);
	}
	return err;
}

//----------------------------------------------------------------------------
