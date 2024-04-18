/*-
 * Copyright (c) 1998, 2002-2008 Kiyoshi Matsui <kmatsui@t3.rim.or.jp>
 * All rights reserved.
 *
 * Some parts of this code are derived from the public domain software
 * DECUS cpp (1984,1985) written by Martin Minow.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 *                              M A I N . C
 *                  M C P P   M a i n   P r o g r a m
 *
 * The main routine and it's supplementary routines are placed here.
 * The post-preprocessing routines are also placed here.
 */

#include "precompiled.h"
#include "pk_mcpp_bridge.h"

#if	defined(PK_COMPILER_MSVC)
#	pragma warning( push )
#	pragma warning( disable : 4611)
#	pragma warning( disable : 4101)
#elif defined(PK_COMPILER_CLANG)
#	pragma clang diagnostic push
#	pragma clang diagnostic ignored "-Wwrite-strings"
#	pragma clang diagnostic ignored "-Wunused-function"
#	pragma clang diagnostic ignored "-Wchar-subscripts"
#elif defined(PK_COMPILER_GCC)
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wwrite-strings"
#	pragma GCC diagnostic ignored "-Wunused-function"
#	pragma GCC diagnostic ignored "-Wchar-subscripts"
#endif

#define MBCHAR_IS_ESCAPE_FREE   (SJIS_IS_ESCAPE_FREE && \
            BIGFIVE_IS_ESCAPE_FREE && ISO2022_JP_IS_ESCAPE_FREE)

#if MCPP_LIB
static void     init_main( void);
                /* Initialize static variables      */
#endif
static void     init_defines( void);
                /* Predefine macros                 */
static void     mcpp_main( void);
                /* Main loop to process input lines */
static void     do_pragma_op( void);
                /* Execute the _Pragma() operator   */
static void     put_seq( char * begin, char * seq);
                /* Put out the failed sequence      */
static char *   de_stringize( char * in, char * out);
                /* "De-stringize" for _Pragma() op. */
static void     putout( char * out);
                /* May concatenate adjacent string  */
#if COMPILER != GNUC && COMPILER != MSC
static void     devide_line( char * out);
                /* Devide long line for compiler    */
#endif
static void     put_a_line( char * out);
                /* Put out the processed line       */
#if ! HAVE_DIGRAPHS || ! MBCHAR_IS_ESCAPE_FREE
static int      post_preproc( char * out);
                /* Post-preprocess for older comps  */
#if ! HAVE_DIGRAPHS
static char *   conv_a_digraph( char * cp);
                /* Convert a digraph in place       */
#endif
#if ! MBCHAR_IS_ESCAPE_FREE
static char *   esc_mbchar( char * str, char * str_end);
                /* Double \ as 2nd byte of SJIS     */
#endif
#endif


#if MCPP_LIB
int     mcpp_lib_main
#else
int     main
#endif
(
    int argc,
    char ** argv
)
{
    char *  in_file = NULL;
    char *  out_file = NULL;
    const char *  stdin_name = "<stdin>";

    if (setjmp( g_internal_data->error_exit) == -1) {
        g_internal_data->errors++;
        goto  fatal_error_exit;
    }

#if MCPP_LIB
    /* Initialize global and static variables.  */
    init_directive();
    init_eval();
    init_support();
#endif

    g_internal_data->fp_in = &g_global_data->m_std_in;
    g_internal_data->fp_out = &g_global_data->m_std_out;
    g_internal_data->fp_err = &g_global_data->m_std_err;
    g_internal_data->fp_debug = &g_global_data->m_std_out;
        /*
         * Debugging information is output to stdout in order to
         *      synchronize with preprocessed output.
         */

    g_internal_data->inc_dirp = &g_internal_data->empty_str;   /* Initialize to current (null) directory   */
    g_internal_data->cur_fname = g_internal_data->cur_fullname = const_cast<char*>("(predefined)");  /* For predefined macros    */
    init_defines();                         /* Predefine macros     */
    mb_init();      /* Should be initialized prior to get options   */
    do_options( argc, argv, &in_file, &out_file);   /* Command line options */

    /* Open input file, "-" means stdin.    */
    if (in_file != NULL && ! str_eq( in_file, "-")) {
        if ((g_internal_data->fp_in = pk_fopen( in_file, "r")) == NULL) {
            mcpp_fprintf( DEST_ERR, "Can't open input file \"%s\".\n", in_file);
            g_internal_data->errors++;
#if MCPP_LIB
            goto  fatal_error_exit;
#else
            return( IO_ERROR);
#endif
        }
    } else {
        in_file = const_cast<char*>(stdin_name);
    }
    /* Open output file, "-" means stdout.  */
    if (out_file != NULL && ! str_eq( out_file, "-")) {
        if ((g_internal_data->fp_out = pk_fopen( out_file, "w")) == NULL) {
            mcpp_fprintf( DEST_ERR, "Can't open output file \"%s\".\n", out_file);
            g_internal_data->errors++;
#if MCPP_LIB
            goto  fatal_error_exit;
#else
            return( IO_ERROR);
#endif
        }
        g_internal_data->fp_debug = g_internal_data->fp_out;
    }
    if (g_internal_data->option_flags.q) {                   /* Redirect diagnostics */
        if ((g_internal_data->fp_err = pk_fopen( "mcpp.err", "a")) == NULL) {
            g_internal_data->errors++;
            mcpp_fprintf( DEST_OUT, "Can't open \"mcpp.err\"\n");
#if MCPP_LIB
            goto  fatal_error_exit;
#else
            return( IO_ERROR);
#endif
        }
    }
    init_sys_macro();       /* Initialize system-specific macros    */
    add_file( g_internal_data->fp_in, NULL, in_file, in_file, FALSE);
                                        /* "open" main input file   */
    g_internal_data->infile->dirp = g_internal_data->inc_dirp;
    g_internal_data->infile->sys_header = FALSE;
    g_internal_data->cur_fullname = in_file;
    if (g_internal_data->mkdep && str_eq( g_internal_data->infile->real_fname, stdin_name) == FALSE)
        put_depend( in_file);       /* Putout target file name      */
    at_start();                     /* Do the pre-main commands     */

    mcpp_main();                    /* Process main file            */

    if (g_internal_data->mkdep)
        put_depend( NULL);      /* Append '\n' to dependency line   */
    at_end();                       /* Do the final commands        */

fatal_error_exit:
#if MCPP_LIB
    /* Free malloced memory */
    if (g_internal_data->mcpp_debug & MACRO_CALL) {
        if (in_file != stdin_name)
            free( in_file);
    }
    clear_filelist();
    clear_symtable();
#endif

    if (g_internal_data->fp_in != &g_global_data->m_std_in)
        pk_fclose( g_internal_data->fp_in);
    if (g_internal_data->fp_out != &g_global_data->m_std_out)
        pk_fclose( g_internal_data->fp_out);
    if (g_internal_data->fp_err != &g_global_data->m_std_err)
        pk_fclose( g_internal_data->fp_err);

    if (g_internal_data->mcpp_debug & MEMORY)
        print_heap();
    if (g_internal_data->errors > 0 && g_internal_data->option_flags.no_source_line == FALSE) {
        mcpp_fprintf( DEST_ERR, "%d error%s in preprocessor.\n",
                g_internal_data->errors, (g_internal_data->errors == 1) ? "" : "s");
        return  IO_ERROR;
    }
    return  IO_SUCCESS;                             /* No errors    */
}

/*
 * This is the table used to predefine target machine, operating system and
 * compiler designators.  It may need hacking for specific circumstances.
 * The -N option supresses these definitions.
 */
typedef struct pre_set {
    const char *    name;
    const char *    val;
} PRESET;

static PRESET   preset[] = {

#ifdef  SYSTEM_OLD
        { SYSTEM_OLD, "1"},
#endif
#ifdef  SYSTEM_SP_OLD
        { SYSTEM_SP_OLD, "1"},
#endif
#ifdef  COMPILER_OLD
        { COMPILER_OLD, "1"},
#endif
#ifdef  COMPILER_SP_OLD
        { COMPILER_SP_OLD, "1"},
#endif

        { NULL, NULL},  /* End of macros beginning with alphabet    */

#ifdef  SYSTEM_STD
        { SYSTEM_STD, "1"},
#endif
#ifdef  SYSTEM_STD1
        { SYSTEM_STD1, "1"},
#endif
#ifdef  SYSTEM_STD2
        { SYSTEM_STD2, "1"},
#endif

#ifdef  SYSTEM_EXT
        { SYSTEM_EXT, SYSTEM_EXT_VAL},
#endif
#ifdef  SYSTEM_EXT2
        { SYSTEM_EXT2, SYSTEM_EXT2_VAL},
#endif
#ifdef  SYSTEM_SP_STD
        { SYSTEM_SP_STD, SYSTEM_SP_STD_VAL},
#endif
#ifdef  COMPILER_STD
        { COMPILER_STD, COMPILER_STD_VAL},
#endif
#ifdef  COMPILER_STD1
        { COMPILER_STD1, COMPILER_STD1_VAL},
#endif
#ifdef  COMPILER_STD2
        { COMPILER_STD2, COMPILER_STD2_VAL},
#endif
#ifdef  COMPILER_EXT
        { COMPILER_EXT, COMPILER_EXT_VAL},
#endif
#ifdef  COMPILER_EXT2
        { COMPILER_EXT2, COMPILER_EXT2_VAL},
#endif
#ifdef  COMPILER_SP_STD
        { COMPILER_SP_STD, COMPILER_SP_STD_VAL},
#endif
#ifdef  COMPILER_SP1
        { COMPILER_SP1, COMPILER_SP1_VAL},
#endif
#ifdef  COMPILER_SP2
        { COMPILER_SP2, COMPILER_SP2_VAL},
#endif
#ifdef  COMPILER_SP3
        { COMPILER_SP3, COMPILER_SP3_VAL},
#endif
#ifdef  COMPILER_CPLUS
        { COMPILER_CPLUS, COMPILER_CPLUS_VAL},
#endif
        { NULL, NULL},  /* End of macros with value of any integer  */
};

static void init_defines( void)
/*
 * Initialize the built-in #define's.
 * Called only on cpp startup prior to do_options().
 *
 * Note: the built-in static definitions are removed by the -N option.
 */
{
    int     n = sizeof preset / sizeof (PRESET);
    int     nargs;
    PRESET *    pp;

    /* Predefine the built-in symbols.  */
    nargs = DEF_NOARGS_PREDEF_OLD;
    for (pp = preset; pp < preset + n; pp++) {
        if (pp->name && *(pp->name))
            look_and_install( pp->name, nargs, g_internal_data->empty_str, pp->val);
        else if (! pp->name)
            nargs = DEF_NOARGS_PREDEF;
    }

    look_and_install( "__MCPP", DEF_NOARGS_PREDEF, g_internal_data->empty_str, "2");
    /* MCPP V.2.x   */
    /* This macro is predefined and is not undefined by -N option,  */
    /*      yet can be undefined by -U or #undef.                   */
}

void    un_predefine(
    int clearall                            /* TRUE for -N option   */
)
/*
 * Remove predefined symbols from the symbol table.
 */
{
    PRESET *    pp;
    DEFBUF *    defp;
    int     n = sizeof preset / sizeof (PRESET);

    for (pp = preset; pp < preset + n; pp++) {
        if (pp->name) {
            if (*(pp->name) && (defp = look_id( pp->name)) != NULL
                    && defp->nargs >= DEF_NOARGS_PREDEF)
                undefine( pp->name);
        } else if (clearall == FALSE) {             /* -S<n> option */
            break;
        }
    }
}

/*
 * output[] and out_ptr are used for:
 *      buffer to store preprocessed line (this line is put out or handed to
 *      post_preproc() via putout() in some cases)
 */
static char     output[ NMACWORK];  /* Buffer for preprocessed line */
static char * const out_end = & output[ NWORK - 2];
                /* Limit of output line for other than GCC and VC   */
static char * const out_wend = & output[ NMACWORK - 2];
                                    /* Buffer end of output line    */
static char *       out_ptr;        /* Current pointer into output[]*/

static void mcpp_main( void)
/*
 * Main process for mcpp -- copies tokens from the current input stream
 * (main file or included file) to the output file.
 */
{
    int     c;                      /* Current character            */
    char *  wp;                     /* Temporary pointer            */
    DEFBUF *    defp;               /* Macro definition             */
    int     line_top;       /* Is in the line top, possibly spaces  */
    LINE_COL    line_col;   /* Location of macro call in source     */

    g_internal_data->keep_comments = g_internal_data->option_flags.c && !g_internal_data->no_output;
    g_internal_data->keep_spaces = g_internal_data->option_flags.k;       /* Will be turned off if !compiling */
    line_col.col = line_col.line = 0L;

    /*
     * This loop is started "from the top" at the beginning of each line.
     * 'wrong_line' is set TRUE in many places if it is necessary to write
     * a #line record.  (But we don't write them when expanding macros.)
     *
     * 'newlines' variable counts the number of blank lines that have been
     * skipped over.  These are then either output via #line records or
     * by outputting explicit blank lines.
     * 'newlines' will be cleared on end of an included file by get_ch().
     */
    while (1) {                             /* For the whole input  */
        g_internal_data->newlines = 0;                       /* Count empty lines    */

        while (1) {                         /* For each line, ...   */
            out_ptr = output;               /* Top of the line buf  */
            c = get_ch();
            if (g_internal_data->src_col)
                break;  /* There is a residual tokens on the line   */
            while (g_internal_data->char_type[ c] & HSP) {   /* ' ' or '\t'          */
                if (c != COM_SEP)
                    *out_ptr++ = (char)c; /* Retain line top white spaces */
                                    /* Else skip 0-length comment   */
                c = get_ch();
            }
            if (c == '#') {                 /* Is 1st non-space '#' */
                directive();                /* Do a #directive      */
            } else if (g_internal_data->mcpp_mode == STD && g_internal_data->option_flags.dig && c == '%') {
                    /* In POST_STD digraphs are already converted   */
                if (get_ch() == ':') {      /* '%:' i.e. '#'        */
                    directive();            /* Do a #directive      */
                } else {
                    unget_ch();
                    if (! compiling) {
                        skip_nl();
                        g_internal_data->newlines++;
                    } else {
                        break;
                    }
                }
            } else if (c == CHAR_EOF) {     /* End of input         */
                break;
            } else if (! compiling) {       /* #ifdef false?        */
                skip_nl();                  /* Skip to newline      */
                g_internal_data->newlines++;                 /* Count it, too.       */
            } else if (g_internal_data->in_asm && ! g_internal_data->no_output) { /* In #asm block    */
                put_asm();                  /* Put out as it is     */
            } else if (c == '\n') {         /* Blank line           */
                if (g_internal_data->keep_comments)
                    mcpp_fputc( '\n', DEST_OUT); /* May flush comments   */
                else
                    g_internal_data->newlines++;             /* Wait for a token     */
            } else {
                break;                      /* Actual token         */
            }
        }

        if (c == CHAR_EOF)                  /* Exit process at      */
            break;                          /*   end of input       */

        /*
         * If the loop didn't terminate because of end of file, we
         * know there is a token to compile.  First, clean up after
         * absorbing newlines.  newlines has the number we skipped.
         */
        if (g_internal_data->no_output) {
            g_internal_data->wrong_line = FALSE;
        } else {
            if (g_internal_data->wrong_line || g_internal_data->newlines > 10) {
                sharp( NULL, 0);            /* Output # line number */
                if (g_internal_data->keep_spaces && g_internal_data->src_col) {
                    while (g_internal_data->src_col--)       /* Adjust columns       */
                        mcpp_fputc( ' ', DEST_OUT);
                    g_internal_data->src_col = 0;
                }
            } else {                        /* If just a few, stuff */
                while (g_internal_data->newlines-- > 0)      /* them out ourselves   */
                    mcpp_fputc('\n', DEST_OUT);
            }
        }

        /*
         * Process each token on this line.
         */
        line_top = TRUE;
        while (c != '\n' && c != CHAR_EOF) {    /* For the whole line   */
            /*
             * has_pragma is set to TRUE so as to execute _Pragma() operator
             * when the psuedo macro _Pragma() is found.
             */
            int     has_pragma;

            if ((g_internal_data->mcpp_debug & MACRO_CALL) && ! g_internal_data->in_directive) {
                line_col.line = g_internal_data->src_line;       /* Location in source   */
                line_col.col = g_internal_data->infile->bptr - g_internal_data->infile->buffer - 1;
            }
            if (scan_token( c, (wp = out_ptr, &wp), out_wend) == NAM
                    && (defp = is_macro( &wp)) != NULL) {   /* A macro  */
                wp = g_internal_data->expand_macro( defp, out_ptr, out_wend, line_col
                        , & has_pragma);    /* Expand it completely */
                if (line_top) {     /* The first token is a macro   */
                    char *  tp = out_ptr;
                    while (g_internal_data->char_type[ *tp & UCHARMAX] & HSP)
                        tp++;           /* Remove excessive spaces  */
                    memmove( out_ptr, tp, strlen( tp) + 1);
                    wp -= (tp - out_ptr);
                }
                if (has_pragma) {           /* Found _Pramga()      */
                    do_pragma_op();         /* Do _Pragma() operator*/
                    out_ptr = output;       /* Do the rest of line  */
                    g_internal_data->wrong_line = TRUE;      /* Line-num out of sync */
                } else {
                    out_ptr = wp;
                }
                if (g_internal_data->keep_spaces && g_internal_data->wrong_line && g_internal_data->infile
                        && *(g_internal_data->infile->bptr) != '\n' && *(g_internal_data->infile->bptr) != EOS) {
                    g_internal_data->src_col = (int)(g_internal_data->infile->bptr - g_internal_data->infile->buffer);
                    /* Remember the current colums  */
                    break;                  /* Do sharp() now       */
                }
            } else {                        /* Not a macro call     */
                out_ptr = wp;               /* Advance the place    */
                if (g_internal_data->wrong_line)             /* is_macro() swallowed */
                    break;                  /*      the newline     */
            }
            while (g_internal_data->char_type[ c = get_ch()] & HSP) {    /* Horizontal space */
                if (c != COM_SEP)           /* Skip 0-length comment*/
                    *out_ptr++ = (char)c;
            }
            line_top = FALSE;               /* Read over some token */
        }                                   /* Loop for line        */

        putout( output);                    /* Output the line      */
    }                                       /* Continue until EOF   */
}

static void do_pragma_op( void)
/*
 * Execute the _Pragma() operator contained in an expanded macro.
 * Note: _Pragma() operator is also implemented as a special macro.  Therefore
 *      it is always searched as a macro.
 * There might be more than one _Pragma() in a expanded macro and those may be
 *      surrounded by other token sequences.
 * Since all the macros have been expanded completely, any name identical to
 *      macro should not be re-expanded.
 * However, a macro in the string argument of _Pragma() may be expanded by
 *      do_pragma() after de_stringize(), if EXPAND_PRAGMA == TRUE.
 */
{
    FILEINFO *  file;
    DEFBUF *    defp;
    int     prev = output < out_ptr;        /* There is a previous sequence */
    int     token_type;
    char *  cp1, * cp2;
    int     c;

    file = unget_string( out_ptr, NULL);
    while (c = get_ch(), file == g_internal_data->infile) {
        if (g_internal_data->char_type[ c] & HSP) {
            *out_ptr++ = (char)c;
            continue;
        }
        if (scan_token( c, (cp1 = out_ptr, &cp1), out_wend)
                    == NAM && (defp = is_macro( &cp1)) != NULL
                && defp->nargs == DEF_PRAGMA) {     /* _Pragma() operator   */
            if (prev) {
                putout( output);    /* Putout the previous sequence */
                cp1 = stpcpy( output, "pragma ");   /* From top of buffer   */
            }
            /* is_macro() already read over possible spaces after _Pragma   */
            *cp1++ = (char)get_ch();                              /* '('  */
            while (g_internal_data->char_type[ c = get_ch()] & HSP)
                *cp1++ = (char)c;
            if (((token_type = scan_token( c, (cp2 = cp1, &cp1), out_wend))
                    != STR && token_type != WSTR)) {
                /* Not a string literal */
                put_seq( output, cp1);
                return;
            }
            g_internal_data->workp = de_stringize( cp2, g_internal_data->work_buf);
            while (g_internal_data->char_type[ c = get_ch()] & HSP)
                *cp1++ = (char)c;
            if (c != ')') {         /* More than a string literal   */
                unget_ch();
                put_seq( output, cp1);
                return;
            }
            strcpy( g_internal_data->workp, "\n");       /* Terminate with <newline> */
            unget_string( g_internal_data->work_buf, NULL);
            do_pragma();                /* Do the #pragma "line"    */
            g_internal_data->infile->bptr += strlen( g_internal_data->infile->bptr);      /* Clear sequence   */
            cp1 = out_ptr = output;     /* From the top of buffer   */
            prev = FALSE;
        } else {                        /* Not pragma sequence      */
            out_ptr = cp1;
            prev = TRUE;
        }
    }
    unget_ch();
    if (prev)
        putout( output);
}

static void put_seq(
    char *  begin,                  /* Sequence already in buffer   */
    char *  seq                     /* Sequence to be read          */
)
/*
 * Put out the failed sequence as it is.
 */
{
    FILEINFO *  file = g_internal_data->infile;
    int     c;

    cerror( "Operand of _Pragma() is not a string literal"  /* _E_  */
            , NULL, 0L, NULL);
    while (c = get_ch(), file == g_internal_data->infile)
        *seq++ = (char)c;
    unget_ch();
    out_ptr = seq;
    putout( begin);
}

static char *   de_stringize(
    char *  in,                 /* Null terminated string literal   */
    char *  out                             /* Output buffer        */
)
/*
 * Make token sequence from a string literal for _Pragma() operator.
 */
{
    char *  in_p;
    int     c1, c;

    in_p = in;
    if (*in_p == 'L')
        in_p++;                             /* Skip 'L' prefix      */
    while ((c = *++in_p) != EOS) {
        if (c == '\\' && ((c1 = *(in_p + 1), c1 == '\\') || c1 == '"'))
            c = *++in_p;            /* "De-escape" escape sequence  */
        *out++ = (char)c;
    }
    *--out = EOS;                   /* Remove the closing '"'       */
    return  out;
}

static void putout(
    char *  out     /* Output line (line-end is always 'out_ptr')   */
)
/*
 * Put out a line with or without "post-preprocessing".
 */
{
    size_t  len;

    *out_ptr++ = '\n';                      /* Put out a newline    */
    *out_ptr = EOS;

#if ! MBCHAR_IS_ESCAPE_FREE
    post_preproc( out);
#elif   ! HAVE_DIGRAPHS
    if (mcpp_mode == STD && option_flag.dig)
        post_preproc( out);
#endif
    /* Else no post-preprocess  */
#if COMPILER != GNUC && COMPILER != MSC
    /* GCC and Visual C can accept very long line   */
    len = strlen( out);
    if (len > NWORK - 1)
        devide_line( out);              /* Devide a too long line   */
    else
#endif
        put_a_line( out);
}

#if COMPILER != GNUC && COMPILER != MSC

static void devide_line(
    char * out                      /* 'out' is 'output' in actual  */
)
/*
 * Devide a too long line into output lines shorter than NWORK.
 * This routine is called from putout().
 */
{
    FILEINFO *  file;
    char *  save;
    char *  wp;
    int     c;

    file = unget_string( out, NULL);        /* To re-read the line  */
    wp = out_ptr = out;

    while ((c = get_ch()), file == g_internal_data->infile) {
        if (g_internal_data->char_type[ c] & HSP) {
            if (g_internal_data->keep_spaces || out == out_ptr
                    || (g_internal_data->char_type[ *(out_ptr - 1) & UCHARMAX] & HSP)) {
                *out_ptr++ = (char)c;
                wp++;
            }
            continue;
        }
        scan_token( c, &wp, out_wend);          /* Read a token     */
        if (NWORK-2 < wp - out_ptr) {           /* Too long a token */
            cfatal( "Too long token %s", out_ptr, 0L, NULL);        /* _F_  */
        } else if (out_end <= wp) {             /* Too long line    */
            if (g_internal_data->mcpp_debug & MACRO_CALL) {      /* -K option        */
                /* Other than GCC or Visual C   */
                /* scan_token() scans a comment as sequence of some */
                /* tokens such as '/', '*', ..., '*', '/', since it */
                /* does not expect comment.                         */
                save = out_ptr;
                while ((save = strrchr( save, '/')) != NULL) {
                    if (*(save - 1) == '*') {   /* '*' '/' sequence */
                        out_ptr = save + 1;     /* Devide at the end*/
                        break;                  /*      of a comment*/
                    }
                }
            }
            save = save_string( out_ptr);       /* Save the token   */
            *out_ptr++ = '\n';                  /* Append newline   */
            *out_ptr = EOS;
            put_a_line( out);           /* Putout the former tokens */
            wp = out_ptr = stpcpy( out, save);      /* Restore the token    */
            free( save);
        } else {                            /* Still in size        */
            out_ptr = wp;                   /* Advance the pointer  */
        }
    }

    unget_ch();                 /* Push back the source character   */
    put_a_line( out);                   /* Putout the last tokens   */
    sharp( NULL, 0);                        /* Correct line number  */
}

#endif

static void put_a_line(
    char * out
)
/*
 * Finally put out the preprocessed line.
 */
{
    size_t  len;
    char *  out_p;
    char *  tp;

    if (g_internal_data->no_output)
        return;
    len = strlen( out);
    out_p = out + len;             /* Just before '\n'     */
    if (len >= 2)
        out_p -= 2;
    tp = out_p;
    while (g_internal_data->char_type[ *out_p & UCHARMAX] & SPA)
        out_p--;                    /* Remove trailing white spaces */
    if (out_p < tp) {
        *++out_p = '\n';
        *++out_p = EOS;
    }
    if (mcpp_fputs( out, DEST_OUT) == EOF)
        cfatal( "File write error", NULL, 0L, NULL);        /* _F_  */
}


/*
 *      Routines to  P O S T - P R E P R O C E S S
 *
 * 1998/08      created     kmatsui     (revised 1998/09, 2004/02, 2006/07)
 *    Supplementary phase for the older compiler-propers.
 *      1. Convert digraphs to usual tokens.
 *      2. Double '\\' of the second byte of multi-byte characters.
 *    These conversions are done selectively according to the macros defined
 *  in system.h.
 *      1. Digraphs are converted if ! HAVE_DIGRAPHS and digraph recoginition
 *  is enabled by DIGRAPHS_INIT and/or -2 option on execution.
 *      2. '\\' of the second byte of SJIS (BIGFIVE or ISO2022_JP) is doubled
 *  if bsl_need_escape == TRUE.
 */

#if HAVE_DIGRAPHS && MBCHAR_IS_ESCAPE_FREE
    /* No post_preproc()    */
#else

static int  post_preproc(
    char * out
)
/*
 * Convert digraphs and double '\\' of the second byte of SJIS (BIGFIVE or
 * ISO2022_JP).
 * Note: Output of -K option embeds macro informations into comments.
 * scan_token() does not recognize comment and parses it as '/', '*', etc.
 */
{
#if ! HAVE_DIGRAPHS
    int     di_count = 0;
#endif
    int     token_type;
    int     c;
    char *  str;
    char *  cp = out;

    unget_string( out, NULL);
    while ((c = get_ch()) != '\n') {    /* Not to read over to next line    */
        if (g_internal_data->char_type[ c] & HSP) {
            *cp++ = (char)c;
            continue;
        }
        str = cp;
        token_type = scan_token( c, &cp, out_wend);
        switch (token_type) {
#if ! MBCHAR_IS_ESCAPE_FREE
        case WSTR   :
        case WCHR   :
            str++;                          /* Skip prefix 'L'      */
            /* Fall through */
        case STR    :
        case CHR    :
            if (g_internal_data->bsl_need_escape)
                cp = esc_mbchar( str, cp);
            break;
#endif  /* ! MBCHAR_IS_ESCAPE_FREE  */
#if ! HAVE_DIGRAPHS
        case OPE    :
            if (mcpp_mode == STD && (openum & OP_DIGRAPH)) {
                cp = conv_a_digraph( cp);   /* Convert a digraph    */
                di_count++;
            }
            break;
#endif
        }
    }
    *cp++ = '\n';
    *cp = EOS;
#if ! HAVE_DIGRAPHS
    if (mcpp_mode == STD && di_count && (warn_level & 16))
        cwarn( "%.0s%ld digraph(s) converted"           /* _W16_    */
                , NULL, (long) di_count, NULL);
#endif
    return  0;
}

#endif  /* ! HAVE_DIGRAPHS || ! MBCHAR_IS_ESCAPE_FREE   */

#if ! HAVE_DIGRAPHS
static char *   conv_a_digraph(
    char *  cp                      /* The end of the digraph token */
)
/*
 * Convert a digraph to usual token in place.
 * This routine is never called in POST_STD mode.
 */
{
    cp -= 2;
    switch (openum) {
    case OP_LBRACE_D    :
        *cp++ = '{';
        break;
    case OP_RBRACE_D    :
        *cp++ = '}';
        break;
    case OP_LBRCK_D     :
        *cp++ = '[';
        break;
    case OP_RBRCK_D     :
        *cp++ = ']';
        break;
    case OP_SHARP_D     :                       /* Error of source  */
        *cp++ = '#';
        break;
    case OP_DSHARP_D    :                       /* Error of source  */
        cp -= 2;
        *cp++ = '#';
        *cp++ = '#';
        break;
    }
    return  cp;
}
#endif  /* ! HAVE_DIGRAPHS  */

#if ! MBCHAR_IS_ESCAPE_FREE
static char *   esc_mbchar(
    char *  str,        /* String literal or character constant without 'L' */
    char *  str_end     /* The end of the token */
)
/*
 * Insert \ before the byte of 0x5c('\\') of the SJIS, BIGFIVE or ISO2022_JP
 * multi-byte character code in string literal or character constant.
 * Insert \ also before the byte of 0x22('"') and 0x27('\'') of ISO2022_JP.
 * esc_mbchar() does in-place insertion.
 */
{
    char *  cp;
    int     delim;
    int     c;

    if (! g_internal_data->bsl_need_escape)
        return  str_end;
    if ((delim = *str++) == 'L')
        delim = *str++;                         /* The quote character  */
    while ((c = *str++ & UCHARMAX) != delim) {
        if (g_internal_data->char_type[ c] & g_internal_data->mbchk) {            /* MBCHAR   */
            cp = str;
            g_internal_data->mb_read( c, &str, (g_internal_data->workp = g_internal_data->work_buf, &g_internal_data->workp));
            while (cp++ < str) {
                c = *(cp - 1);
                if (c == '\\' || c == '"' || c == '\'') {
                                    /* Insert \ before 0x5c, 0x22, 0x27 */
                    memmove( cp, cp - 1, (size_t) (str_end - cp) + 2);
                    *(cp++ - 1) = '\\';
                    str++;
                    str_end++;
                }
            }
        } else if (c == '\\' && ! (g_internal_data->char_type[ *str & UCHARMAX] & g_internal_data->mbchk)) {
            str++;                              /* Escape sequence      */
        }
    }
    return  str_end;
}
#endif  /* ! MBCHAR_IS_ESCAPE_FREE  */

#if	defined(PK_COMPILER_MSVC)
#	pragma warning( pop )
#elif defined(PK_COMPILER_CLANG)
#	pragma clang diagnostic pop
#elif defined(PK_COMPILER_GCC)
#	pragma GCC diagnostic pop
#endif
