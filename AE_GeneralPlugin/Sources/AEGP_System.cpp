//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------

#include "ae_precompiled.h"
#include "AEGP_System.h"
#include "AEGP_Define.h"

#if defined(PK_WINDOWS)
#include "AEGP_WinSystem.h"
#endif

#include "pk_toolkit/include/pk_toolkit_process.h"

#if defined(PK_WINDOWS)
#	include <Psapi.h>

#elif defined(PK_LINUX) || defined(PK_MACOSX)
#	include <errno.h>
#	include <signal.h>
#	include <sys/utsname.h>
#endif

#if defined(PK_LINUX)
#	include	<sys/types.h>
#elif defined(PK_MACOSX)
#	include <mach-o/arch.h>
#endif

#include "AEGP_Log.h"

__AEGP_PK_BEGIN

//----------------------------------------------------------------------------
//	HardwareID
//----------------------------------------------------------------------------

u16	CSystemHelper::_GetMacHash()
{
	u16	hash = 0;

	//for (const QNetworkInterface &interface : QNetworkInterface::allInterfaces())
	//{
	//	if (interface.isValid() && !interface.flags().testFlag(QNetworkInterface::IsLoopBack))
	//	{
	//		const QStringList	macAddr = interface.hardwareAddress().split(":");
	//
	//		for (int i = 0; i < macAddr.length(); ++i)
	//			hash += (macAddr[i].toInt() << ((i & 1) * 8));
	//	}
	//}
	
	return hash;
}

//----------------------------------------------------------------------------

#if defined(PK_LINUX)
static void _getCpuid(u32 *p, u32 ax)
{
	__asm __volatile
	("movl %%ebx, %%esi\n\t"
		"cpuid\n\t"
		"xchgl %%ebx, %%esi"
		: "=a" (p[0]), "=S" (p[1]),
		"=c" (p[2]), "=d" (p[3])
		: "0" (ax)
	);
}
#endif

//----------------------------------------------------------------------------

u16	CSystemHelper::_GetCPUHash()
{
	u16	hash = 0;

#if defined(PK_WINDOWS)
	int		cpuinfo[4] = { 0, 0, 0, 0 };

	__cpuid(cpuinfo, 0);

	u16	*ptr = (u16*)(&cpuinfo[0]);
	for (u32 i = 0; i < 8; i++)
		hash += ptr[i];

#elif defined(PK_LINUX)

	u32			cpuinfo[4] = { 0, 0, 0, 0 };

	_getCpuid(cpuinfo, 0);

	u32			*ptr = (&cpuinfo[0]);

	for (u32 i = 0; i < 4; i++)
		hash += (ptr[i] & 0xFFFF) + (ptr[i] >> 16);

#elif defined(PK_MACOSX)

	const NXArchInfo	*info = NXGetLocalArchInfo();

	hash += u16(info->cputype);
	hash += u16(info->cpusubtype);

#else
	PK_ASSERT_NOT_REACHED();
#endif

	return hash;
}

//----------------------------------------------------------------------------

const char	*CSystemHelper::_GetMachineName()
{
#if defined(PK_WINDOWS)
	static char		computerName[1024];
	DWORD			size = 1024;

	GetComputerName(computerName, &size);

	return computerName;

#elif defined(PK_LINUX) || defined(PK_MACOSX)
	static struct utsname u;

	if (uname(&u) < 0)
		return "unknown";

	return u.nodename;
#else
	PK_ASSERT_NOT_REACHED();
#endif
}

//----------------------------------------------------------------------------

static const u16	kSmearMask[3] = { 0x4e25, 0xf4a1, 0x5437 };

//----------------------------------------------------------------------------

void	CSystemHelper::_Smear(u16 *id)
{
	for (u32 i = 0; i < 3; i++)
	{
		for (u32 j = i; j < 3; j++)
		{
			if (i != j)
				id[i] ^= id[j];
		}
	}

	for (u32 i = 0; i < 3; i++)
		id[i] ^= kSmearMask[i];
}

//----------------------------------------------------------------------------

void	CSystemHelper::_Unsmear(u16 *id)
{
	for (u32 i = 0; i < 3; i++)
		id[i] ^= kSmearMask[i];

	for (u32 i = 0; i < 3; i++)
	{
		for (u32 j = 0; j < i; j++)
		{
			if (i != j)
				id[2 - i] ^= id[2 - j];
		}
	}
}

//----------------------------------------------------------------------------

u16	*CSystemHelper::_ComputeSystemUniqueId()
{
	static u16	id[3];
	static bool	computed = false;

	if (!computed)
	{
		id[0] = _GetCPUHash();
		id[2] = _GetMacHash();
		id[1] = id[0] + id[2];

		_Smear(id);

		computed = true;
	}

	return id;
}

//----------------------------------------------------------------------------

const CString	CSystemHelper::GetUniqueHardwareID()
{
	CString		hId;

	u16			*id = _ComputeSystemUniqueId();
	for (u32 i = 0; i < 3; i++)
		hId += CString::Format("%04x", id[i]);

	return hId.ToUppercase();
}

//----------------------------------------------------------------------------

const CString	CSystemHelper::GetUniqueHardwareIDForHuman()
{
	CString		hId = _GetMachineName();

	u16			*id = _ComputeSystemUniqueId();
	for (u32 i = 0; i < 3; i++)
		hId += CString::Format("-%04x", id[i]);

	return hId.ToUppercase();
}

bool CSystemHelper::LaunchEditorAsPopup()
{
	SEngineVersion	version{ PK_VERSION_MAJOR, PK_VERSION_MINOR, PK_VERSION_PATCH, PK_VERSION_REVID };
#if defined(PK_WINDOWS)
	 SEditorExecutable application = CWinSystemHelper::GetMatchingEditor(version);
#else
	SEditorExecutable application;
#endif
	if (application.m_BinaryPath.Empty())
	{
		return CAELog::TryLogErrorWindows("No installed versions of the PopcornFX editor were found\nPlease visit: https://auth.popcornfx.com/ws/latest?channel=pkfx-stable&v=2 and download the latest PopcornFX editor</a>");
	}

	CProcess	process;

	TArray<CString>	commandline;

	if (!commandline.PushBack("--aeass").Valid())
		return false;
	if (!commandline.PushBack("toto").Valid())
		return false;
	if (!process.Start(application.m_BinaryPath, commandline, true))
	{
		return CAELog::TryLogErrorWindows("No installed versions of the PopcornFX editor were found\nPlease visit: https://auth.popcornfx.com/ws/latest?channel=pkfx-stable&v=2 and download the latest PopcornFX editor</a>");
	}
	CString output;

#if defined(PK_WINDOWS)
	TArray<DWORD>	errorIgnoreList;
	errorIgnoreList.PushBack(ERROR_NO_MORE_FILES);
#endif

	process.WaitForExit();

	output = process.GetProcessStandardOutput();
	if (output != null && !output.Empty())
	{
		CLog::Log(PK_INFO, "%s", output.Data());
		output.Clear();
	}
	else
	{
#if defined(PK_WINDOWS)
		CString errorstr = CWinSystemHelper::GetLastErrorAsString(errorIgnoreList);
		if (!errorstr.Empty())
		{
			CLog::Log(PK_INFO, "%s", errorstr.Data());
			return false;
		}
#endif
	}
	return true;
}

//----------------------------------------------------------------------------
__AEGP_PK_END
