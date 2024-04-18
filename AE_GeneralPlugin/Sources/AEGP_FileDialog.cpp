//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#include <ae_precompiled.h>

#include "AEGP_FileDialog.h"

#include "AEGP_World.h"

#include <string>
#include <vector>

__AEGP_PK_BEGIN

#if defined(PK_MACOSX)

//----------------------------------------------------------------------------

SMacFileDialogFilterData::SMacFileDialogFilterData(const CString desc, const CString type)
	: m_Desc(desc)
	, m_Type(type)
{
}

//----------------------------------------------------------------------------

SMacFileOpenData::SMacFileOpenData()
{
}

//----------------------------------------------------------------------------

SMacFileOpenData::~SMacFileOpenData()
{
}

//----------------------------------------------------------------------------

bool	SMacFileOpenData::AddFilter(const CString &desc, const CString &type)
{
	if (!PK_VERIFY(m_Filters.PushBack(SMacFileDialogFilterData(desc, type)).Valid()))
		return false;
	return true;
}

//----------------------------------------------------------------------------

bool	MacBasicFileOpen(SMacFileOpenData &data)
{
	TArray<CString> 	filters;

	for (u32 i = 0; i < data.m_Filters.Count(); ++i)
		filters.PushBack(CFilePath::ExtractExtension(data.m_Filters[i].m_Type));

	CString stringResult = OpenFileDialogMac(filters);

	if (!stringResult.Empty())
	{
		data.m_Cb(stringResult);
		return true;
	}
	return false;
}

//----------------------------------------------------------------------------

#endif

SFileDialog::SFileDialog()
{

}

//----------------------------------------------------------------------------

bool	SFileDialog::AddFilter(const CString &desc, const CString &type)
{
	if (!PK_VERIFY(m_Data.AddFilter(desc, type)))
		return false;
	return true;
}

//----------------------------------------------------------------------------

bool SFileDialog::IsCancelled()
{
	return m_Cancel;
}

//----------------------------------------------------------------------------

bool	SFileDialog::BasicFileOpen()
{
	m_Cancel = false;
#if defined(PK_WINDOWS)
	HRESULT hr = WinBasicFileOpen(m_Data);
	if (HRESULT_CODE(hr) == ERROR_CANCELLED)
		m_Cancel = true;
	return SUCCEEDED(hr);
#elif defined(PK_MACOSX)
	return MacBasicFileOpen(m_Data);
#endif
}

//----------------------------------------------------------------------------

__AEGP_PK_END
