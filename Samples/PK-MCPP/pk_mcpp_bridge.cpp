
#include "precompiled.h"
#include "pk_mcpp_bridge.h"
#include <string.h>
#include <stdio.h>

#if  defined(PK_DESKTOP)
#	if defined(PK_WINDOWS)
#		include	<direct.h>
#define	PK_GET_CWD		_getcwd
#define	PK_STAT			_stat
#define PK_VSPRINTF		_vsnprintf
#	else
#		include <unistd.h>
#		include <sys/stat.h>
#define	PK_GET_CWD		getcwd
#define	PK_STAT			stat
#define PK_VSPRINTF		vsnprintf
#	endif
#endif

SMCPPGlobalData		*g_global_data = null;
SMCPPInternalData	*g_internal_data = null;
SMCPPSystemData		*g_system_data = null;

SMCPPSystemData::SMCPPSystemData()
{
    sharp_filename = NULL;
    incend = incdir = NULL;
    fnamelist = once_list = NULL;
    search_rule = SEARCH_INIT;
    mb_changed = nflag = ansi = compat_mode = FALSE;
    mkdep_fp = NULL;
    mkdep_target = mkdep_mf = mkdep_md = mkdep_mq = mkdep_mt = NULL;
    std_val = -1L;
    def_cnt = undef_cnt = 0;
    mcpp_optind = mcpp_opterr = 1;
#if COMPILER == GNUC
    sys_dirp = NULL;
    sysroot = NULL;
    gcc_work_dir = i_split = FALSE;
    quote_dir_end = quote_dir;
    dDflag = dMflag = FALSE;
#endif
#if COMPILER == MSC
    wchar_t_modified = FALSE;
#endif
#if COMPILER == GNUC || COMPILER == MSC
    preinc_end = preinclude;
#endif
#if SYSTEM == SYS_CYGWIN
    no_cygwin = FALSE;
#elif   SYSTEM == SYS_MAC
    num_framework = sys_framework = 0;
    to_search_framework = NULL;
#endif
#if NO_DIR
    no_dir = FALSE;
#endif
}

SMCPPSystemData::~SMCPPSystemData()
{
	if (sharp_filename)
		free(sharp_filename);
}

SMCPPInternalBase::std_limits_::std_limits_()
{
	Mem::Clear(*this);
	str_len = NBUFF;
	id_len = IDMAX;
	n_mac_pars = NMACPARS;
}

SMCPPInternalBase::option_flags_::option_flags_()
{
	Mem::Clear(*this);
	c = FALSE;
	k = FALSE;
	z = FALSE;
	p = FALSE;
	q = FALSE;
	v = FALSE;
	trig = TRIGRAPHS_INIT;
	dig = DIGRAPHS_INIT;
	lang_asm = FALSE;
	no_source_line = FALSE;
	dollar_in_name = FALSE;
}

SMCPPInternalBase::SMCPPInternalBase()
{
	Mem::Clear(*this);
	empty_str = "";
    mcpp_mode = STD;
    cplus_val = stdc_ver = 0L;
    stdc_val = 0;
    standard = TRUE;
    std_line_prefix = STD_LINE_PREFIX;
    errors = src_col = 0;
    warn_level = -1;
    infile = NULL;
    in_directive = in_define = in_getarg = in_include = in_if = FALSE;
    src_line = macro_line = in_asm = 0L;
	macro_name = NULL;
    mcpp_debug = mkdep = no_output = keep_comments = keep_spaces = 0;
    include_nest = 0;
    insert_sep = NO_SEP;
    mbchar = MBCHAR;
    ifptr = ifstack;
    ifstack[0].stat = WAS_COMPILING;
    ifstack[0].ifline = ifstack[0].elseline = 0L;
    std_limits.str_len = NBUFF;
    std_limits.id_len = IDMAX;
    std_limits.n_mac_pars =  NMACPARS;
    option_flags.c = option_flags.k = option_flags.z = option_flags.p
            = option_flags.q = option_flags.v = option_flags.lang_asm
            = option_flags.no_source_line = option_flags.dollar_in_name
            = FALSE;
    option_flags.trig = TRIGRAPHS_INIT;
    option_flags.dig = DIGRAPHS_INIT;
    sh_file = NULL;
    sh_line = 0;
}

SMCPPInternalData::SMCPPInternalData()
:	work_end(&work_buf[NWORK])
{
	Mem::Clear(work_buf);
}

SMCPPGlobalData::SMCPPGlobalData()
:	m_std_in("<stdin>")
,	m_std_out("<stdout>")
,	m_std_err("<stderr>")
{
}


SIOHandle		*pk_fopen(const char *filename, const char *accessMode)
{
	if (strcmp(filename, INPUT_FILE_PATH) == 0)
	{
		g_global_data->m_input_file.m_CurChar = 0;
		return &g_global_data->m_input_file;
	}
	else if (strcmp(filename, OUTPUT_FILE_PATH) == 0)
	{
		g_global_data->m_output_file.m_CurChar = 0;
		return &g_global_data->m_output_file;
	}
	else
	{
		FILE		*handler = NULL;
		if (!CFilePath::IsAbsolute(filename))
		{
			CString		absoluteFilePath = CString(g_global_data->m_cwd) / filename;
			handler = fopen(absoluteFilePath.Data(), accessMode);
			if (handler != NULL &&
				!g_global_data->m_dependencies.Contains(absoluteFilePath) &&
				strcmp(accessMode, "r") == 0)
			{
				g_global_data->m_dependencies.PushBack(absoluteFilePath);
			}
		}
		else
		{
			handler = fopen(filename, accessMode);
			if (handler != NULL &&
				!g_global_data->m_dependencies.Contains(filename) &&
				strcmp(accessMode, "r") == 0)
			{
				g_global_data->m_dependencies.PushBack(filename);
			}
		}

		if (handler == NULL)
			return NULL;
		SIOHandle	*pkHandle = PK_NEW(SIOHandle(filename, handler));
		return pkHandle;
	}
}

int				pk_stat(const char *path, PK_STAT_TYPE *buf)
{
	if (!CFilePath::IsAbsolute(path))
	{
		CString		absoluteFilePath = CString(g_global_data->m_cwd) / path;
		return PK_STAT(absoluteFilePath.Data(), buf);
	}
	return PK_STAT(path, buf);
}

int				pk_fclose(SIOHandle *stream)
{
	if (stream->m_Handler != null)
	{
		fclose(stream->m_Handler);
		PK_DELETE(stream);
	}
	return 0;
}

int				pk_fseek(SIOHandle *stream, long offset, int whence)
{
	PK_ASSERT(whence == SEEK_SET);

	if (stream->m_Handler != null)
		return fseek(stream->m_Handler, offset, whence);
	else
		stream->m_CurChar = offset;
	return 0;
}

long int		pk_ftell(SIOHandle *stream)
{
	if (stream->m_Handler != null)
		return ftell(stream->m_Handler);
	else
		return stream->m_CurChar;
}

int				pk_fputc(int character, SIOHandle *stream)
{
	if (stream->m_Handler != null)
		return fputc(character, stream->m_Handler);
	else
	{
		if (stream->m_CurChar == stream->m_Content.Length())
		{
			char	str[] = { (char)character, 0 };
			stream->m_Content.Append(str);
			stream->m_CurChar += 1;
		}
		else
		{
			stream->m_Content.RawDataForWriting()[stream->m_CurChar] = (char)character;
			stream->m_CurChar += 1;
		}
	}
	return 0;
}

int				pk_fputs(const char *string, SIOHandle *stream)
{
	if (stream->m_Handler != null)
		return fputs(string, stream->m_Handler);
	else
	{
		if (stream->m_CurChar == stream->m_Content.Length())
		{
			stream->m_Content.Append(string);
			stream->m_CurChar += (u32)strlen(string);
		}
		else
		{
			const u32	len = (u32)strlen(string);
			for (u32 i = 0; i < len; ++i)
			{
				if (stream->m_CurChar == stream->m_Content.Length())
				{
					stream->m_Content.Append(string + i);
					stream->m_CurChar += (u32)strlen(string + i);
					break;
				}
				stream->m_Content.RawDataForWriting()[stream->m_CurChar] = string[i];
				stream->m_CurChar += 1;
			}
		}
	}
	return 0;
}

char			*pk_fgets(char *string, int maxLength, SIOHandle *stream)
{
	if (stream->m_Handler != null)
		return fgets(string, maxLength, stream->m_Handler);
	else
	{
		if (stream->m_CurChar == stream->m_Content.Length() || maxLength == 0)
			return null;

		const ureg	cpyLen = PKMin(stream->m_Content.Length() - stream->m_CurChar, (u32)maxLength - 1);
		u32			charIdx = 0;

		while (charIdx < cpyLen)
		{
			const char	curChar = stream->m_Content[stream->m_CurChar + charIdx];
			string[charIdx] = curChar;
			++charIdx;
			if (curChar == '\n')
				break;
		}
		stream->m_CurChar += charIdx;
		string[charIdx] = 0;
		return string;
	}
}

int				pk_fflush(SIOHandle * stream)
{
	if (stream->m_Handler != null)
		return fflush(stream->m_Handler);
	return 0;
}

int				pk_ferror(SIOHandle * stream)
{
	if (stream->m_Handler != null)
		return ferror(stream->m_Handler);
	return 0;
}

char			*pk_getcwd(char *buf, size_t size)
{
	if (g_global_data->m_cwd.Empty())
		return PK_GET_CWD(buf, (int)size);
	const ureg	buffLen = PKMin((ureg)size - 1, g_global_data->m_cwd.Length());
	Mem::Copy(buf, g_global_data->m_cwd.Data(), buffLen);
	buf[buffLen] = 0;
	return buf;
}

int				pk_getc(SIOHandle *stream)
{
	if (stream->m_Handler != null)
		return getc(stream->m_Handler);
	else
	{
		if (stream->m_Content.Length() == stream->m_CurChar)
			return EOF;
		int		c = (int)stream->m_Content[stream->m_CurChar];
		stream->m_CurChar += 1;
		return c;
	}
}

int				pk_ungetc(int c, SIOHandle *stream)
{
	if (stream->m_Handler != null)
		return ungetc(c, stream->m_Handler);
	else
	{
		if (stream->m_CurChar == 0)
		{
			char	toPrepend[] = { (char)c, 0 };
			stream->m_Content.Prepend(toPrepend);
			return c;
		}
		stream->m_CurChar -= 1;
		stream->m_Content.RawDataForWriting()[stream->m_CurChar] = (char)c;
		return c;
	}
}

int				pk_vfprintf(SIOHandle *stream, const char * format, va_list arg)
{
	if (stream->m_Handler != null)
		return vfprintf(stream->m_Handler, format, arg);
	else
	{
		char	buffer[3200];
		PK_VSPRINTF(buffer, sizeof(buffer) - 1, format, arg);
		buffer[sizeof(buffer) - 1] = 0;
		pk_fputs(buffer, stream);
	}
	return 0;
}
