#pragma once

#include <pk_kernel/include/kr_string.h>

struct	SIOHandle;

#include "system.h"
#include "internal.h"

using namespace		PopcornFX;

#define INPUT_FILE_PATH		"##InputPath##"
#define OUTPUT_FILE_PATH	"##OutputPath##"

struct	SIOHandle
{
	CString			m_Path;

	// Either a real file handler or just a string:
	FILE			*m_Handler;

	CString			m_Content;
	u32				m_CurChar;

	SIOHandle(const CString &path = CString::EmptyString, FILE *handler = null)
	:	m_Path(path)
	,	m_Handler(handler)
	,	m_CurChar(0)
	{
	}

	SIOHandle(const CString &path, const CString &content)
	:	m_Path(path)
	,	m_Handler(NULL)
	,	m_Content(content)
	,	m_CurChar(0)
	{
	}
};

struct	SMCPPInternalBase
{
	/* The minimum translation limits specified by the Standards.       */
	struct std_limits_ {
	        long    str_len;            /* Least maximum of string len. */
	        size_t  id_len;             /* Least maximum of ident len.  */
	        int     n_mac_pars;         /* Least maximum of num of pars.*/
	        int     exp_nest;           /* Least maximum of expr nest   */
	        int     blk_nest;           /* Least maximum of block nest  */
	        int     inc_nest;           /* Least maximum of include nest*/
	        long    n_macro;            /* Least maximum of num of macro*/
	        long    line_num;           /* Maximum source line number   */

			std_limits_();
	}		std_limits;
	/* The boolean flags specified by the execution options.    */
	struct option_flags_ {
	        int     c;                  /* -C option (keep comments)    */
	        int     k;                  /* -k option (keep white spaces)*/
	        int     z;					/* -z option (no-output of included file)   */
	        int     p;                  /* -P option (no #line output)  */
	        int     q;                  /* -Q option (diag to mcpp.err) */
	        int     v;                  /* -v option (verbose)          */
	        int     trig;               /* -3 option (toggle trigraphs) */
	        int     dig;                /* -2 option (toggle digraphs)  */
	        int     lang_asm;           /* -a option (assembler source) */
	        int     no_source_line;     /* Do not output line in diag.  */
	        int     dollar_in_name;     /* Allow $ in identifiers       */

			option_flags_();
	}		option_flags;

	int      mcpp_mode;          /* Mode of preprocessing        */
	int      stdc_val;           /* Value of __STDC__            */
	long     stdc_ver;           /* Value of __STDC_VERSION__    */
	long     cplus_val;          /* Value of __cplusplus for C++ */
	int      stdc2;				 /* cplus_val or (stdc_ver >= 199901L)   */
	int      stdc3;				 /* (stdc_ver or cplus_val) >= 199901L   */
	int      standard;           /* mcpp_mode is STD or POST_STD */
	int      std_line_prefix;    /* #line in C source style      */
	int      warn_level;         /* Level of warning             */
	int      errors;             /* Error counter                */
	long     src_line;           /* Current source line number   */
	int      wrong_line;         /* Force #line to compiler      */
	int      newlines;           /* Count of blank lines         */
	int      keep_comments;      /* Don't remove comments        */
	int      keep_spaces;        /* Don't remove white spaces    */
	int      include_nest;       /* Nesting level of #include    */
	const char *     empty_str;  /* "" string for convenience    */
	const char **    inc_dirp;   /* Directory of #includer       */
	const char *     cur_fname;  /* Current source file name     */
	int      no_output;          /* Don't output included file   */
	int      in_directive;       /* In process of #directive     */
	int      in_define;          /* In #define line              */
	int      in_getarg;          /* Collecting arguments of macro*/
	int      in_include;         /* In #include line             */
	int      in_if;              /* In #if and non-skipped expr. */
	long     macro_line;         /* Line number of macro call    */
	char *   macro_name;         /* Currently expanding macro    */
	int      openum;             /* Number of operator or punct. */
	IFINFO *     ifptr;          /* -> current ifstack item      */
	FILEINFO *   infile;         /* Current input file or macro  */
	SIOHandle *   fp_in;         /* Input stream to preprocess   */
	SIOHandle *   fp_out;        /* Output stream preprocessed   */
	SIOHandle *   fp_err;        /* Diagnostics stream           */
	SIOHandle *   fp_debug;      /* Debugging information stream */
	int      insert_sep;         /* Inserted token separator flag*/
	int      mkdep;              /* Output source file dependency*/
	int      mbchar;             /* Encoding of multi-byte char  */
	int      mbchk;              /* Possible multi-byte char     */
	int      bsl_in_mbchar;      /* 2nd byte of mbchar has '\\'  */
	int      bsl_need_escape;    /* '\\' in mbchar should be escaped */
	int      mcpp_debug;         /* Class of debug information   */
	long     in_asm;             /* In #asm - #endasm block      */
	jmp_buf  error_exit;         /* Exit on fatal error          */
	const char *   cur_fullname;       /* Full name of current source  */
	short *  char_type;          /* Character classifier         */
	char *   workp;              /* Free space in work[]         */
	char     identifier[IDMAX + IDMAX/8];       /* Lastly scanned name          */
	IFINFO   ifstack[BLK_NEST + 1];          /* Information of #if nesting   */
	FILEINFO * sh_file;
	int      sh_line;
	int      src_col;        /* Column number of source line */

	/* Function pointer to expand_macro() functions.    */
	char *   (*expand_macro)( DEFBUF * defp, char * out, char * out_end, LINE_COL line_col, int * pragma_op);
	/* Function pointer to mb_read_*() functions.   */
	size_t  (*mb_read)( int c1, char ** in_pp, char ** out_pp);

	SMCPPInternalBase();
};

struct	SMCPPInternalData : public SMCPPInternalBase
{
	char     work_buf[NWORK + IDMAX];
	char    *work_end;   /* End of work[] buffer         */

	SMCPPInternalData();
};

struct	SMCPPSystemData
{
	/* for mcpp_getopt()    */
	int      mcpp_optind = 1;
	int      mcpp_opterr = 1;
	int      mcpp_optopt;
	char *   mcpp_optarg;

	int      mb_changed = FALSE;     /* Flag of -e option        */
	char     cur_work_dir[ PATHMAX + 1];     /* Current working directory*/

	/*
	 * incdir[] stores the -I directories (and the system-specific #include <...>
	 * directories).  This is set by set_a_dir().  A trailing PATH_DELIM is
	 * appended if absent.
	 */
	const char **    incdir;         /* Include directories      */
	const char **    incend;         /* -> active end of incdir  */
	int          max_inc;            /* Number of incdir[]       */

	/*
	 * fnamelist[] stores the souce file names opened by #include directive for
	 * debugging information.
	 */
	INC_LIST *   fnamelist;          /* Source file names        */
	INC_LIST *   fname_end;          /* -> active end of fnamelist   */
	int          max_fnamelist;      /* Number of fnamelist[]    */

	/* once_list[] stores the #pragma once file names.  */
	INC_LIST *   once_list;          /* Once opened file         */
	INC_LIST *   once_end;           /* -> active end of once_list   */
	int          max_once;           /* Number of once_list[]    */

	/*
	 * 'search_rule' holds searching rule of #include "header.h" to search first
	 * before searching user specified or system-specific include directories.
	 * 'search_rule' is initialized to SEARCH_INIT.  It can be changed by -I1, -I2
	 * or -I3 option.  -I1 specifies CURRENT, -I2 SOURCE and -I3 both.
	 */

	int      search_rule = SEARCH_INIT;  /* Rule to search include file  */
	int      nflag = FALSE;          /* Flag of -N (-undef) option       */
	long     std_val = -1L;  /* Value of __STDC_VERSION__ or __cplusplus */

	char *   def_list[ MAX_DEF];     /* Macros to be defined     */
	char *   undef_list[ MAX_UNDEF]; /* Macros to be undefined   */
	int      def_cnt;                /* Count of def_list        */
	int      undef_cnt;              /* Count of undef_list      */


	SIOHandle *   mkdep_fp;                       /* For -Mx option   */
	char *   mkdep_target;
	    /* For -MT TARGET option and for GCC's queer environment variables.     */
	char *   mkdep_mf;               /* Argument of -MF option   */
	char *   mkdep_md;               /* Argument of -MD option   */
	char *   mkdep_mq;               /* Argument of -MQ option   */
	char *   mkdep_mt;               /* Argument of -MT option   */
	/* sharp_filename is filename for #line line, used only in cur_file()   */
	char *   sharp_filename = NULL;
	char *   argv0;      /* argv[ 0] for usage() and version()   */
	int      ansi;           /* __STRICT_ANSI__ flag for GNUC    */
	int      compat_mode;
	                /* "Compatible" mode of recursive macro expansion   */
	char     arch[ MAX_ARCH_LEN];    /* -arch or -m64, -m32 options      */
#if COMPILER == GNUC
	/* quote_dir[]:     Include directories for "header" specified by -iquote   */
	/* quote_dir_end:   Active end of quote_dir */
	const char *     quote_dir[ N_QUOTE_DIR];
	const char **    quote_dir_end = quote_dir;
	/* sys_dirp indicates the first directory to search for system headers.     */
	const char **    sys_dirp = NULL;        /* System header directory  */
	const char *     sysroot = NULL; /* Logical root directory of header */
	int      i_split = FALSE;                /* For -I- option   */
	int      gcc_work_dir = FALSE;           /* For -fworking-directory  */
	int      gcc_maj_ver;                    /* __GNUC__         */
	int      gcc_min_ver;                    /* __GNUC_MINOR__   */
	int      dDflag = FALSE;         /* Flag of -dD option       */
	int      dMflag = FALSE;         /* Flag of -dM option       */
#endif
#if COMPILER == GNUC || COMPILER == MSC
	char *   preinclude[ NPREINCLUDE];       /* File to pre-include      */
	char **  preinc_end = preinclude;    /* -> active end of preinclude  */
#endif
#if COMPILER == MSC
	int      wchar_t_modified = FALSE;   /* -Zc:wchar_t flag     */
#endif
#if COMPILER == LCC
	const char *     optim_name = "__LCCOPTIMLEVEL";
#endif
#if SYSTEM == SYS_CYGWIN
	int      no_cygwin = FALSE;          /* -mno-cygwin          */
#elif   SYSTEM == SYS_MAC
#define         MAX_FRAMEWORK   8
	char *   framework[ MAX_FRAMEWORK];  /* Framework directories*/
	int      num_framework;          /* Current number of framework[]    */
	int      sys_framework;          /* System framework dir     */
	const char **    to_search_framework;
                        /* Search framework[] next to the directory */
	int      in_import;          /* #import rather than #include */
#endif
#if NO_DIR
/* Unofficial feature to strip directory part of include file   */
	int      no_dir;
#endif

	SMCPPSystemData();
	~SMCPPSystemData();
};

struct	SMCPPGlobalData
{
	SIOHandle	m_input_file;
	SIOHandle	m_output_file;

	SIOHandle	m_std_in;
	SIOHandle	m_std_out;
	SIOHandle	m_std_err;

	CString		m_cwd;

	TArray<CString>	m_dependencies;

	SMCPPGlobalData();
};

extern SMCPPGlobalData		*g_global_data;
extern SMCPPInternalData	*g_internal_data;
extern SMCPPSystemData		*g_system_data;

#include <sys/stat.h>

#if HOST_COMPILER == MSC
#	define		PK_STAT_TYPE	struct _stat
#else
#	define		PK_STAT_TYPE	struct stat
#endif

SIOHandle		*pk_fopen(const char *filename, const char *accessMode);
int				pk_stat(const char *path, PK_STAT_TYPE *buf);

int				pk_fclose(SIOHandle *stream);
int				pk_fseek(SIOHandle *stream, long offset, int whence);
long int		pk_ftell(SIOHandle *stream);
int				pk_fputc(int character, SIOHandle *stream);
int				pk_fputs(const char *string, SIOHandle *stream);
char			*pk_fgets(char *string, int maxLength, SIOHandle *stream);
int				pk_fflush(SIOHandle * stream);
int				pk_ferror(SIOHandle * stream);
char			*pk_getcwd(char *buf, size_t size);
int				pk_getc(SIOHandle *stream);
int				pk_ungetc(int c, SIOHandle *stream);
int				pk_vfprintf(SIOHandle *stream, const char * format, va_list arg);
