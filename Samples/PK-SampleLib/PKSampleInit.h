#pragma once

//----------------------------------------------------------------------------
// This program is the property of Persistant Studios SARL.
//
// You may not redistribute it and/or modify it under any conditions
// without written permission from Persistant Studios SARL, unless
// otherwise stated in the latest Persistant Studios Code License.
//
// See the Persistant Studios Code License for further details.
//----------------------------------------------------------------------------

#include <pk_toolkit/include/pk_toolkit_version.h>
#include <pkapi_library.h>

__PK_API_BEGIN
//----------------------------------------------------------------------------

class	PK_EXPORT CPKSampleBase
{
public:
	struct	Config
	{
		bool	m_ShouldBuildShaders;
		Config() : m_ShouldBuildShaders(false) {}
	};

protected:
	static bool			InternalStartup(const Config &config);
	static bool			InternalShutdown();

	static bool			m_Active;

public:
	static const char	*LibraryName() { return "PK-Sample"; }
	static bool			Active() { return m_Active; }
};

//----------------------------------------------------------------------------

typedef	TPKLibrary<CPKSampleBase>	CPKSample;

// FIXME: Uncomment this for dlls, but TPKLibrary<Type> will need to be instanciated inside a cpp
//template class PK_EXPORT		TPKLibrary<CPKSampleBase>;

//----------------------------------------------------------------------------
__PK_API_END
