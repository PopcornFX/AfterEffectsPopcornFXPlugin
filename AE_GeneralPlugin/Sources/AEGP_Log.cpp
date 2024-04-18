//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#include "ae_precompiled.h"

#include "AEGP_Log.h"
#include "AEGP_World.h"

//Suite
#include <PopcornFX_Suite.h>
#include <PopcornFX_Define.h>

//AE
#include <AE_GeneralPlug.h>
#include <SuiteHelper.h>
#include <AEGP_SuiteHandler.h>

__AEGP_PK_BEGIN
//----------------------------------------------------------------------------

bool		CAELog::s_PKState = false;
SAAEIOData	*CAELog::s_IOData = null;

//----------------------------------------------------------------------------

bool	CAELog::LogErrorWindows(SAAEIOData *AAEData, const CString errorStr)
{
	PK_ASSERT(errorStr.Length() < 256);

	if (s_PKState)
		CLog::Log(PK_ERROR, errorStr);
	if (AAEData == null || AAEData->m_OutData == null)
		return false;
	AAEData->m_OutData->out_flags |= PF_OutFlag_DISPLAY_ERROR_MESSAGE;
	sprintf(AAEData->m_OutData->return_msg, "%s", errorStr.Data());

	CPopcornFXWorld		&instance = AEGPPk::CPopcornFXWorld::Instance();
	SPBasicSuite		*basicSuite = instance.GetAESuites();

	if (basicSuite)
	{
		AEGP_SuiteHandler	suites(basicSuite);

		suites.UtilitySuite6()->AEGP_WriteToDebugLog("PopcornFX Plugin", "Error", errorStr.Data());
		suites.AdvAppSuite2()->PF_InfoDrawText("PopcornFX Plugin [ERROR]", errorStr.Data());
	}
	return false;
}

//----------------------------------------------------------------------------

bool	CAELog::SetIOData(SAAEIOData *AAEData)
{
	s_IOData = AAEData;
	return true;
}

//----------------------------------------------------------------------------

void	CAELog::ClearIOData()
{
	s_IOData = null;
}

//----------------------------------------------------------------------------

bool	CAELog::TryLogErrorWindows(const CString errorStr)
{
	if (s_IOData == null)
		return false;
	LogErrorWindows(s_IOData, errorStr);
	return false;
}

//----------------------------------------------------------------------------

void CAELog::SetPKLogState(bool state)
{
	s_PKState = state;
}

//----------------------------------------------------------------------------

bool CAELog::TryLogInfoWindows(const CString infoStr)
{
	CPopcornFXWorld			&instance = AEGPPk::CPopcornFXWorld::Instance();

	if (s_PKState)
		CLog::Log(PK_INFO, infoStr);

	SPBasicSuite	*basicSuite = instance.GetAESuites();

	if (basicSuite)
	{
		AEGP_SuiteHandler	suites(basicSuite);

		suites.UtilitySuite6()->AEGP_ReportInfo(instance.GetPluginID(), infoStr.Data());

		suites.UtilitySuite6()->AEGP_WriteToDebugLog("PopcornFX Plugin", "Info", infoStr.Data());

		suites.AdvAppSuite2()->PF_InfoDrawText("PopcornFX Plugin [INFO]", infoStr.Data());
	}
	return true;
}

//----------------------------------------------------------------------------

__AEGP_PK_END
