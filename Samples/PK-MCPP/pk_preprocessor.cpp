
#include "precompiled.h"
#include "pk_preprocessor.h"
#include "pk_mcpp_bridge.h"

#include "mcpp_lib.h"

#include <pk_kernel/include/kr_buffer_parsing_utils.h>

__PK_API_BEGIN
//----------------------------------------------------------------------------

Threads::CCriticalSection	CPreprocessor::m_Lock;

//----------------------------------------------------------------------------

bool	CPreprocessor::PreprocessString(TArray<CString> &defines,
										const CString &input,
										const CString &execDirectory,
										SPreprocessOutput &output)
{
	u32						argOffset = 4;
	TArray<char*>			argv;
	// mcpp_main uses char* as input, need to copy the const static char*
	CString					inputFilePath = INPUT_FILE_PATH;
	CString					outputFilePath = OUTPUT_FILE_PATH;
	CString					defineOption = "-D";
	CString					noLineOutputOption = "-P";
	// MCPP never reads argv[0], we set it to "mcpp"
	CString					mcppOption = "mcpp";

	if (!PK_VERIFY(argv.Resize(defines.Count() * 2 + argOffset)))
		return false;

	argv[0] = mcppOption.RawDataForWriting();
	argv[1] = inputFilePath.RawDataForWriting();
	argv[2] = outputFilePath.RawDataForWriting();
	argv[3] = noLineOutputOption.RawDataForWriting();
	for (u32 i = 0; i < defines.Count(); ++i)
	{
		argv[2 * i + argOffset] = defineOption.RawDataForWriting();
		argv[2 * i + argOffset + 1] = defines[i].RawDataForWriting();
	}

	SMCPPGlobalData		globalData;
	SMCPPInternalData	internalData;
	SMCPPSystemData		systemData;

	globalData.m_cwd = null;
	if (!execDirectory.Empty())
		globalData.m_cwd = execDirectory.Data();

	// We create input and output files here:
	SIOHandle	&inputFile = globalData.m_input_file;
	inputFile.m_Path = INPUT_FILE_PATH;
	inputFile.m_Handler = NULL;
	inputFile.m_Content = input;

	SIOHandle	&outputFile = globalData.m_output_file;
	outputFile.m_Path = OUTPUT_FILE_PATH;
	outputFile.m_Handler = NULL;

	int	exitValue = 0;

	{
		PK_SCOPEDLOCK(m_Lock);

		g_global_data = &globalData;
		g_internal_data = &internalData;
		g_system_data = &systemData;

		exitValue = mcpp_lib_main(argv.Count(), argv.RawDataPointer());

		g_global_data = null;
		g_internal_data = null;
		g_system_data = null;
	}

	output.m_Output = outputFile.m_Content;
	output.m_Error = globalData.m_std_err.m_Content;
	output.m_Info = globalData.m_std_out.m_Content;
	output.m_Dependencies = globalData.m_dependencies;
	return exitValue == EXIT_SUCCESS;
}

//----------------------------------------------------------------------------

#define IS_OPEN_QUOTE(c) (c == '<' || c == '"')
#define IS_CLOSE_QUOTE(c) (c == '>' || c == '"')

#define ADVANCE_UNTIL_CONDITION(_cond)		while (src < srcEnd && _cond(*src)) { if (*src == '\\') src += 2 else src += 1;

//----------------------------------------------------------------------------

static bool		IsSpace(const char *curPtr, const char *endPtr)
{
	(void)endPtr;
	return KR_BUFFER_IS_SPACE(*curPtr) != 0;
}

//----------------------------------------------------------------------------

static bool		IsNotNewLine(const char *curPtr, const char *endPtr)
{
	// Handle \r\n:
	if (curPtr + 1 < endPtr)
	{
		const u16	eol = KR_BUFFER_2CHAR_UNALIGNED_LOAD(curPtr);
		if (eol == SBufferParsingUtils::Newline1 || eol == SBufferParsingUtils::Newline2)
			return false;
	}
	return !KR_BUFFER_IS_VSPACE(*curPtr);
}

//----------------------------------------------------------------------------

template<bool _Cond(const char *curPtr, const char *endPtr)>
static const char		*SkipWhileCondition(const char *curPtr, const char *endPtr)
{
	// Skip characters while condition:
	do
	{
		// Process all comments and backslashes
		bool	moreToProcess = true;
		while (moreToProcess)
		{
			if (curPtr + 1 < endPtr && (*curPtr == '/' && *(curPtr + 1) == '*')) // Comment starting (/*)
			{
				curPtr += 2;
				// Jump to end of comment (*/):
				while (curPtr + 1 < endPtr)
				{
					if (*curPtr == '*' && *(curPtr + 1) == '/')
					{
						curPtr += 2;
						break;
					}
					else
						++curPtr;
				}
			}
			else if (curPtr + 1 < endPtr && (*curPtr == '/' && *(curPtr + 1) == '/')) // Comment starting (//)
			{
				while (curPtr < endPtr && *curPtr != '\n')
				{
					// Ignore \n with backslash
					if (curPtr + 1 < endPtr && (*curPtr == '\\' && KR_BUFFER_IS_VSPACE(*(curPtr + 1))))
						curPtr += 2;
					else if (curPtr + 2 < endPtr && (*curPtr == '\\' && !IsNotNewLine(curPtr + 1, endPtr)))
						curPtr += 3;
					else
						++curPtr;
				}
			}
			else if (curPtr + 1 < endPtr && (*curPtr == '\\' && KR_BUFFER_IS_VSPACE(*(curPtr + 1)))) // Ignore \n with backslash
			{
				curPtr += 2;
			}
			else if (curPtr + 2 < endPtr && (*curPtr == '\\' && !IsNotNewLine(curPtr + 1, endPtr))) // Ignore \r\n with backslash
			{
				curPtr += 3;
			}
			else
			{
				moreToProcess = false;
			}
		}
	} while (curPtr < endPtr && _Cond(curPtr, endPtr) && ++curPtr);
	return curPtr;
}

//----------------------------------------------------------------------------
// Ultra lightweight function to gather shader dependencies:
// This just handles:
// '/* */' and '//' comments
// '\' to skip end of lines
// #include at beginning of lines

bool	CPreprocessor::FindShaderDependencies(	const CString &input,
												const CString &execDirectory,
												TArray<CString> &outDep,
												IFileSystem *controller)
{
	const u32			startDepIdx = outDep.Count();
	const CStringView	includeStr = "include";
	const char			*src = input.Data();
	const char			*srcEnd = src + input.Length();

	// For each line:
	while (src < srcEnd)
	{
		// Skip spaces
		src = SkipWhileCondition<IsSpace>(src, srcEnd);
		// If we find a sharp: preprocessor directive
		if (src < srcEnd && *src == '#')
		{
			u32		i = 0;
			// Skip sharp
			++src;
			// Skip spaces
			src = SkipWhileCondition<IsSpace>(src, srcEnd);
			// strcmp with "include"
			while (src < srcEnd && i < includeStr.Length() && *src == includeStr[i])
			{
				++src;
				++i;
			}
			// If it is followed by the "include" directive
			if (src < srcEnd && i == includeStr.Length())
			{
				src = SkipWhileCondition<IsSpace>(src, srcEnd);
				if (src < srcEnd && IS_OPEN_QUOTE(*src))
				{
					// Skip open quote
					++src;
					const char	*pathStart = src;
					while (src < srcEnd && !IS_CLOSE_QUOTE(*src))
						++src;
					if (src < srcEnd && IS_CLOSE_QUOTE(*src))
					{
						const CStringView	includePath = CStringView(pathStart, (u32)(src - pathStart));
						const CString		actualPath = CFilePath::IsAbsolute(includePath) ? includePath.ToString() : execDirectory / includePath;
						const CString		purePath = CFilePath::Purified(actualPath);
						if (!outDep.Contains(purePath) && controller->Exists(purePath))
						{
							if (!outDep.PushBack(purePath).Valid())
								return false;
						}
						// Skip close quote
						++src;
					}
				}
			}
		}
		src = SkipWhileCondition<IsNotNewLine>(src, srcEnd);
		++src; // Skip the new line
	}

	const u32		stopDepIdx = outDep.Count();

	// Parse all included files to find their own dependencies:
	for (u32 i = startDepIdx; i < stopDepIdx; ++i)
	{
		// Load the file:
		const CString	filePath = outDep[i];
		const CString	fileContent = controller->BufferizeToString(filePath, CFilePath::IsAbsolute(filePath));
		if (!fileContent.Empty())
		{
			// Search for other dependencies in this file:
			if (!FindShaderDependencies(fileContent, CFilePath::StripFilename(filePath), outDep, controller))
				return false;
		}
	}
	return true;
}

//----------------------------------------------------------------------------
__PK_API_END
