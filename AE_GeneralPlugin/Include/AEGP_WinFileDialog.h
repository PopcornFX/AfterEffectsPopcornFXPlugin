//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#pragma once

#if 	defined(PK_WINDOWS)

#include <pk_kernel/include/kr_delegates.h>
#include <shtypes.h>

using namespace PopcornFX;

//----------------------------------------------------------------------------

struct SWinFileDialogFilterData
{
	wchar_t				*m_Desc;
	wchar_t				*m_Type;

	COMDLG_FILTERSPEC	m_Spec;

	SWinFileDialogFilterData(const CString desc, const CString type)
		: m_Desc(null)
		, m_Type(null)
	{
		convert(&m_Desc, desc);
		convert(&m_Type, type);

		m_Spec.pszName = m_Desc;
		m_Spec.pszSpec = m_Type;
	}

	// Simple move assignment operator
	SWinFileDialogFilterData(SWinFileDialogFilterData&& other)
	{
		m_Desc = other.m_Desc;
		m_Type = other.m_Type;

		m_Spec.pszName = m_Desc;
		m_Spec.pszSpec = m_Type;

		other.m_Desc = null;
		other.m_Type = null;
	}

	SWinFileDialogFilterData& operator=(SWinFileDialogFilterData&& other)
	{
		m_Desc = other.m_Desc;
		m_Type = other.m_Type;

		m_Spec.pszName = m_Desc;
		m_Spec.pszSpec = m_Type;

		other.m_Desc = null;
		other.m_Type = null;
		return *this;
	}

	~SWinFileDialogFilterData()
	{
		if (m_Desc != null)
			PK_FREE(m_Desc);
		if (m_Type != null)
			PK_FREE(m_Type);
	}

	void convert(wchar_t **dst, const CString& desc)
	{
		const int			wideStringLength = ::MultiByteToWideChar(CP_UTF8, 0, desc.Data(), -1, NULL, 0);
		if (!PK_VERIFY(wideStringLength >= 0))
			return;
		*dst = PK_TALLOC<WCHAR>(wideStringLength);
		if (!PK_VERIFY(*dst != null))
			return;

		::MultiByteToWideChar(CP_UTF8, 0, desc.Data(), -1, *dst, wideStringLength);
	}
};

//----------------------------------------------------------------------------

struct SWinFileOpenData
{
	TArray<SWinFileDialogFilterData>				m_Filters;
	PopcornFX::FastDelegate<void(const CString)>	m_Cb;

	SWinFileOpenData()
	{
	}


	~SWinFileOpenData()
	{
	}

	bool	AddFilter(const CString &desc, const CString &type)
	{
		//SWinFileDialogFilterData	filterData(desc, type);
		if (!PK_VERIFY(m_Filters.PushBack(SWinFileDialogFilterData(desc, type)).Valid()))
			return false;
		return true;
	}
};

//----------------------------------------------------------------------------

HRESULT		WinBasicFileOpen(SWinFileOpenData& data);

//----------------------------------------------------------------------------

#endif
