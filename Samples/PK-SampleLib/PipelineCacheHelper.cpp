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

#include "PipelineCacheHelper.h"
#include "SampleUtils.h"

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

CPipelineCacheHelper::CPipelineCacheHelper()
{
}

//----------------------------------------------------------------------------

CPipelineCacheHelper::~CPipelineCacheHelper()
{
}

//----------------------------------------------------------------------------

void	CPipelineCacheHelper::LoadPipelineCache(const RHI::PApiManager &apiManager, const CString &filePath)
{
	if (apiManager->ApiDesc().m_SupportPipelineCache)
	{
		CString			apiFilePath = filePath + GetShaderExtensionStringFromApi(apiManager->ApiName());
		PFileStream		fileView = File::DefaultFileSystem()->OpenStream(apiFilePath, IFileSystem::Access_Read);

		if (fileView == null)
		{
			CLog::Log(PK_INFO, "Could not load pipeline cache");
			return;
		}

		u32				cacheSize = 0;
		void			*cacheData = fileView->Bufferize(cacheSize);
		if (!apiManager->SetProgramCache(cacheData, cacheSize))
		{
			CLog::Log(PK_WARN, "The cache data loaded is not valid");
		}
		else
		{
			CLog::Log(PK_INFO, "Pipeline cache successfully loaded");
		}
		PK_FREE(cacheData);
	}
}

//----------------------------------------------------------------------------

bool	CPipelineCacheHelper::SavePipelineCache(const RHI::PApiManager &apiManager, const CString &filePath)
{
	if (apiManager->ApiDesc().m_SupportPipelineCache)
	{
		CString			apiFilePath = filePath + GetShaderExtensionStringFromApi(apiManager->ApiName());
		PFileStream		fileView = File::DefaultFileSystem()->OpenStream(apiFilePath, IFileSystem::Access_WriteCreate);

		void			*cacheData = null;
		u32				cacheSize = 0;
		if (!apiManager->RetrieveProgramCache(cacheData, cacheSize))
		{
			CLog::Log(PK_ERROR, "Could not retrieve the pipeline cache");
			return false;
		}
		if (fileView == null || !fileView->Write(cacheData, cacheSize))
		{
			PK_FREE(cacheData);
			CLog::Log(PK_ERROR, "Could not save the pipeline cache");
			return false;
		}
		PK_FREE(cacheData);
	}
	return true;
}

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
