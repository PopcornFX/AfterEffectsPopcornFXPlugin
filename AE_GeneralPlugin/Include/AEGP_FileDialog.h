//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#pragma once

#ifndef __AEGP_FILEDIALOG_H__
#define __AEGP_FILEDIALOG_H__

#include "AEGP_Define.h"
#if defined (PK_MACOSX)
#include "AEGP_FileDialogMac.h"
#endif
#if defined (PK_WINDOWS)
#include "AEGP_WinFileDialog.h"
#endif
#include <AEGP_SuiteHandler.h>

#include <pk_kernel/include/kr_delegates.h>

__AEGP_PK_BEGIN

//----------------------------------------------------------------------------

#if defined(PK_MACOSX)

//----------------------------------------------------------------------------

struct SMacFileDialogFilterData
{
	CString				m_Desc;
	CString				m_Type;

	SMacFileDialogFilterData(const CString desc, const CString type);
};

//----------------------------------------------------------------------------

struct SMacFileOpenData
{
	TArray<SMacFileDialogFilterData>				m_Filters;
	PopcornFX::FastDelegate<void(const CString)>	m_Cb;

	SMacFileOpenData();

	~SMacFileOpenData();

	bool	AddFilter(const CString &desc, const CString &type);
};

//----------------------------------------------------------------------------

bool	MacBasicFileOpen(SMacFileOpenData &data);

//----------------------------------------------------------------------------
#endif

//----------------------------------------------------------------------------

struct SFileDialog
{
#if defined(PK_WINDOWS)
    SWinFileOpenData    m_Data;
#elif defined (PK_MACOSX)
    SMacFileOpenData    m_Data;
#endif
	bool	m_Cancel;

    SFileDialog();

	bool	AddFilter(const CString &desc, const CString &type);
    void    SetCallback(FastDelegate<void(const CString)> cb) { m_Data.m_Cb = cb; };
    bool    BasicFileOpen();

	bool	IsCancelled();

};

//----------------------------------------------------------------------------

__AEGP_PK_END

#endif
