#pragma once

#include <pk_kernel/include/kr_threads.h>

__PK_API_BEGIN

class	CPreprocessor
{
public:
	struct	SPreprocessOutput
	{
		CString					m_Output;
		CString					m_Error;
		CString					m_Info;
		TArray<CString>			m_Dependencies;
	};

	static bool	PreprocessString(	TArray<CString> &mcppCommandLine,
									const CString &input,
									const CString &execDirectory,
									SPreprocessOutput &output);

	static bool	FindShaderDependencies(	const CString &input,
										const CString &execDirectory,
										TArray<CString> &outDep,
										IFileSystem *controller);

private:
	// Ugly global lock still use because mcpp still uses global vars:
	static Threads::CCriticalSection	m_Lock;
};


__PK_API_END
