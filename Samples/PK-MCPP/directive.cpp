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
 *                          D I R E C T I V E . C
 *              P r o c e s s   D i r e c t i v e  L i n e s
 *
 * The routines to handle directives other than #include and #pragma
 * are placed here.
 */

#include "precompiled.h"
#include "pk_mcpp_bridge.h"

#if	defined(PK_COMPILER_MSVC)
#elif defined(PK_COMPILER_CLANG)
#	pragma clang diagnostic push
#	pragma clang diagnostic ignored "-Wwrite-strings"
#	pragma clang diagnostic ignored "-Wimplicit-fallthrough"
#elif defined(PK_COMPILER_GCC)
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wwrite-strings"
#	pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#endif

static int      do_if( int hash, const char * directive_name);
                /* #if, #elif, #ifdef, #ifndef      */
static void     sync_linenum( void);
                /* Synchronize number of newlines   */
static long     do_line( void);
                /* Process #line directive          */
static int      get_parm( void);
                /* Get parameters of macro, its nargs, names, lengths       */
static int      get_repl( const char * macroname);
                /* Get replacement text embedding parameter number  */
static char *   is_formal( const char * name, int conv);
                /* If formal parameter, save the number     */
static char *   def_stringization( char * repl_cur);
                /* Define stringization     */
static char *   mgtoken_save( const char * macroname);
                /* Prefix DEF_MAGIC to macro name in repl-text      */
static char *   str_parm_scan( char * string_end);
                /* Scan the parameter in quote      */
static void     do_undef( void);
                /* Process #undef directive         */
static void     dump_repl( const DEFBUF * dp, SIOHandle * fp, int gcc2_va);
                /* Dump replacement text            */

/*
 * Generate (by hand-inspection) a set of unique values for each directive.
 * MCPP won't compile if there are hash conflicts.
 */
#define L_if            ('i' ^ (EOS << 1))
#define L_ifdef         ('i' ^ ('d' << 1))
#define L_ifndef        ('i' ^ ('n' << 1))
#define L_elif          ('e' ^ ('i' << 1))
#define L_else          ('e' ^ ('s' << 1))
#define L_endif         ('e' ^ ('d' << 1))
#define L_define        ('d' ^ ('f' << 1))
#define L_undef         ('u' ^ ('d' << 1))
#define L_line          ('l' ^ ('n' << 1))
#define L_include       ('i' ^ ('c' << 1))
#if COMPILER == GNUC
#define L_include_next  ('i' ^ ('c' << 1) ^ ('_' << 1))
#endif
#if SYSTEM == SYS_MAC
#define L_import        ('i' ^ ('p' << 1))
#endif
#define L_error         ('e' ^ ('r' << 1))
#define L_pragma        ('p' ^ ('a' << 1))

static const char * const   not_ident
            = "Not an identifier \"%s\"";               /* _E_      */
static const char * const   no_arg = "No argument";     /* _E_      */
static const char * const   excess
            = "Excessive token sequence \"%s\"";        /* _E_ _W1_ */

void    directive( void)
/*
 * Process #directive lines.  Each directive have their own subroutines.
 */
{
    const char * const  many_nesting =
"More than %.0s%ld nesting of #if (#ifdef) sections%s"; /* _F_ _W4_ _W8_    */
    const char * const  not_in_section
    = "Not in a #if (#ifdef) section in a source file"; /* _E_ _W1_ */
    const char * const  illeg_dir
            = "Illegal #directive \"%s%.0ld%s\"";       /* _E_ _W1_ _W8_    */
    const char * const  in_skipped = " (in skipped block)"; /* _W8_ */
    FILEINFO *  file;
    int     token_type;
    int     hash;
    int     c;
    const char *  tp;

    g_internal_data->in_directive = TRUE;
    if (g_internal_data->keep_comments) {
        mcpp_fputc( '\n', DEST_OUT);     /* Possibly flush out comments  */
        g_internal_data->newlines--;
    }
    c = skip_ws();
    if (c == '\n')                              /* 'null' directive */
        goto  ret;
    token_type = scan_token( c, (g_internal_data->workp = g_internal_data->work_buf, &g_internal_data->workp), g_internal_data->work_end);
    if (g_internal_data->in_asm && (token_type != NAM
            || (! str_eq( g_internal_data->identifier, "asm")
                && ! str_eq( g_internal_data->identifier, "endasm"))))
        /* In #asm block, ignore #anything other than #asm or #endasm       */
        goto  skip_line;
    if (token_type != NAM) {
        if (g_internal_data->mcpp_mode == OLD_PREP && token_type == NUM) {   /* # 123 [fname]*/
            strcpy( g_internal_data->identifier, "line");
        } else {
            if (compiling) {
                if (g_internal_data->option_flags.lang_asm) {
                    if (g_internal_data->warn_level & 1)
                        cwarn( illeg_dir, g_internal_data->work_buf, 0L, NULL);
                } else {
                    cerror( illeg_dir, g_internal_data->work_buf, 0L, NULL);
                }
            } else if (g_internal_data->warn_level & 8) {
                cwarn( illeg_dir, g_internal_data->work_buf, 0L, in_skipped);
            }
            goto  skip_line;
        }
    }
    hash = (g_internal_data->identifier[ 1] == EOS) ? g_internal_data->identifier[ 0]
            : (g_internal_data->identifier[ 0] ^ (g_internal_data->identifier[ 2] << 1));
    if (strlen( g_internal_data->identifier) > 7)
        hash ^= (g_internal_data->identifier[ 7] << 1);

    /* hash is set to a unique value corresponding to the directive.*/
    switch (hash) {
    case L_if:      tp = "if";      break;
    case L_ifdef:   tp = "ifdef";   break;
    case L_ifndef:  tp = "ifndef";  break;
    case L_elif:    tp = "elif";    break;
    case L_else:    tp = "else";    break;
    case L_endif:   tp = "endif";   break;
    case L_define:  tp = "define";  break;
    case L_undef:   tp = "undef";   break;
    case L_line:    tp = "line";    break;
    case L_include: tp = "include"; break;
#if COMPILER == GNUC
    case L_include_next:    tp = "include_next";    break;
#endif
#if SYSTEM == SYS_MAC
    case L_import:  tp = "import";  break;
#endif
    case L_error:   tp = "error";   break;
    case L_pragma:  tp = "pragma";  break;
    default:        tp = NULL;      break;
    }

    if (tp != NULL && ! str_eq( g_internal_data->identifier, tp)) {  /* Hash conflict*/
        hash = 0;                       /* Unknown directive, will  */
        tp = NULL;                      /*   be handled by do_old() */
    }

    if (! compiling) {                      /* Not compiling now    */
        switch (hash) {
        case L_elif :
            if (! g_internal_data->standard) {
                if (g_internal_data->warn_level & 8)
                    do_old();               /* Unknown directive    */
                goto  skip_line;            /* Skip the line        */
            }   /* Else fall through    */
        case L_else :       /* Test the #if's nest, if 0, compile   */
        case L_endif:                       /* Un-nest #if          */
            break;
        case L_if   :                       /* These can't turn     */
        case L_ifdef:                       /*  compilation on, but */
        case L_ifndef   :                   /*   we must nest #if's.*/
            if (&g_internal_data->ifstack[ BLK_NEST] < ++g_internal_data->ifptr)
                goto  if_nest_err;
            if (g_internal_data->standard && (g_internal_data->warn_level & 8)
                    && &g_internal_data->ifstack[ g_internal_data->std_limits.blk_nest + 1] == g_internal_data->ifptr)
                cwarn( many_nesting, NULL, (long) g_internal_data->std_limits.blk_nest
                        , in_skipped);
            g_internal_data->ifptr->stat = 0;                /* !WAS_COMPILING       */
            g_internal_data->ifptr->ifline = g_internal_data->src_line;       /* Line at section start*/
            goto  skip_line;
        default :                           /* Other directives     */
            if (tp == NULL && (g_internal_data->warn_level & 8))
                do_old();                   /* Unknown directive ?  */
            goto  skip_line;                /* Skip the line        */
        }
    }

    g_internal_data->macro_line = 0;                         /* Reset error flag     */
    file = g_internal_data->infile;                  /* Remember the current file    */

    switch (hash) {

    case L_if:
    case L_ifdef:
    case L_ifndef:
        if (&g_internal_data->ifstack[ BLK_NEST] < ++g_internal_data->ifptr)
            goto  if_nest_err;
        if (g_internal_data->standard && (g_internal_data->warn_level & 4) &&
                &g_internal_data->ifstack[ g_internal_data->std_limits.blk_nest + 1] == g_internal_data->ifptr)
            cwarn( many_nesting, NULL , (long) g_internal_data->std_limits.blk_nest, NULL);
        g_internal_data->ifptr->stat = WAS_COMPILING;
        g_internal_data->ifptr->ifline = g_internal_data->src_line;
        goto  ifdo;

    case L_elif:
        if (! g_internal_data->standard) {
            do_old();               /* Unrecognized directive       */
            break;
        }
        if (g_internal_data->ifptr == &g_internal_data->ifstack[0])
            goto  nest_err;
        if (g_internal_data->ifptr == g_internal_data->infile->initif) {
            goto  in_file_nest_err;
        }
        if (g_internal_data->ifptr->stat & ELSE_SEEN)
            goto  else_seen_err;
        if ((g_internal_data->ifptr->stat & (WAS_COMPILING | TRUE_SEEN)) != WAS_COMPILING) {
            compiling = FALSE;              /* Done compiling stuff */
            goto  skip_line;                /* Skip this group      */
        }
        hash = L_if;
ifdo:
        c = do_if( hash, tp);
        if (g_internal_data->mcpp_debug & IF) {
            mcpp_fprintf( DEST_DBG
                    , "#if (#elif, #ifdef, #ifndef) evaluate to %s.\n"
                    , compiling ? "TRUE" : "FALSE");
            mcpp_fprintf( DEST_DBG, "line %ld: %s", g_internal_data->src_line, g_internal_data->infile->buffer);
        }
        if (c == FALSE) {                   /* Error                */
            compiling = FALSE;              /* Skip this group      */
            goto  skip_line;    /* Prevent an extra error message   */
        }
        break;

    case L_else:
        if (g_internal_data->ifptr == &g_internal_data->ifstack[0])
            goto  nest_err;
        if (g_internal_data->ifptr == g_internal_data->infile->initif) {
            if (g_internal_data->standard)
                goto  in_file_nest_err;
            else if (g_internal_data->warn_level & 1)
                cwarn( not_in_section, NULL, 0L, NULL);
        }
        if (g_internal_data->ifptr->stat & ELSE_SEEN)
            goto  else_seen_err;
        g_internal_data->ifptr->stat |= ELSE_SEEN;
        g_internal_data->ifptr->elseline = g_internal_data->src_line;
        if (g_internal_data->ifptr->stat & WAS_COMPILING) {
            if (compiling || (g_internal_data->ifptr->stat & TRUE_SEEN) != 0)
                compiling = FALSE;
            else
                compiling = TRUE;
        }
        if ((g_internal_data->mcpp_debug & MACRO_CALL) && (g_internal_data->ifptr->stat & WAS_COMPILING)) {
            sync_linenum();
            mcpp_fprintf( DEST_OUT, "/*else %ld:%c*/\n", g_internal_data->src_line
                    , compiling ? 'T' : 'F');   /* Show that #else is seen  */
        }
        break;

    case L_endif:
        if (g_internal_data->ifptr == &g_internal_data->ifstack[0])
            goto  nest_err;
        if (g_internal_data->ifptr <= g_internal_data->infile->initif) {
            if (g_internal_data->standard)
                goto  in_file_nest_err;
            else if (g_internal_data->warn_level & 1)
                cwarn( not_in_section, NULL, 0L, NULL);
        }
        if (! compiling && (g_internal_data->ifptr->stat & WAS_COMPILING))
            g_internal_data->wrong_line = TRUE;
        compiling = (g_internal_data->ifptr->stat & WAS_COMPILING);
        if ((g_internal_data->mcpp_debug & MACRO_CALL) && compiling) {
            sync_linenum();
            mcpp_fprintf( DEST_OUT, "/*endif %ld*/\n", g_internal_data->src_line);
            /* Show that #if block has ended    */
        }
        --g_internal_data->ifptr;
        break;

    case L_define:
        do_define( FALSE, 0);
        break;

    case L_undef:
        do_undef();
        break;

    case L_line:
        if ((c = do_line()) > 0) {
            g_internal_data->src_line = c;
            sharp( NULL, 0);    /* Putout the new line number and file name */
            g_internal_data->infile->line = --g_internal_data->src_line;  /* Next line number is 'src_line'   */
            g_internal_data->newlines = -1;
        } else {            /* Error already diagnosed by do_line() */
            skip_nl();
        }
        break;

    case L_include:
        g_internal_data->in_include = TRUE;
        if (do_include( FALSE) == TRUE && file != g_internal_data->infile)
            g_internal_data->newlines = -1;  /* File has been included. Clear blank lines    */
        g_internal_data->in_include = FALSE;
        break;

    case L_error:
        if (! g_internal_data->standard) {
            do_old();               /* Unrecognized directive       */
            break;
        }
        cerror( g_internal_data->infile->buffer, NULL, 0L, NULL);            /* _E_  */
        break;

    case L_pragma:
        if (! g_internal_data->standard) {
            do_old();               /* Unrecognized directive       */
            break;
        }
        do_pragma();
        g_internal_data->newlines = -1;              /* Do not putout excessive '\n' */
        break;

    default:                /* Non-Standard or unknown directives   */
        do_old();
        break;
    }

    switch (hash) {
    case L_if       :
    case L_elif     :
    case L_define   :
    case L_line     :
        goto  skip_line;    /* To prevent duplicate error message   */
#if COMPILER == GNUC
    case L_include_next  :
        if (file != g_internal_data->infile)             /* File has been included   */
            g_internal_data->newlines = -1;
#endif
#if SYSTEM == SYS_MAC
    case L_import   :
        if (file != g_internal_data->infile)             /* File has been included   */
            g_internal_data->newlines = -1;
#endif
    case L_error    :
        if (g_internal_data->standard)
            goto  skip_line;
        /* Else fall through    */
    case L_include  :
    case L_pragma   :
        if (g_internal_data->standard)
            break;          /* Already read over the line           */
        /* Else fall through    */
    default :               /* L_else, L_endif, L_undef, etc.       */
        if (g_internal_data->mcpp_mode == OLD_PREP) {
        /*
         * Ignore the rest of the #directive line so you can write
         *          #if     foo
         *          #endif  foo
         */
            ;
        } else if (skip_ws() != '\n') {
#if COMPILER == GNUC
            if (g_internal_data->standard && hash != L_endif)
#else
            if (g_internal_data->standard)
#endif
                cerror( excess, g_internal_data->infile->bptr-1, 0L, NULL);
            else if (g_internal_data->warn_level & 1)
                cwarn( excess, g_internal_data->infile->bptr-1, 0L, NULL);
        }
        skip_nl();
    }
    goto  ret;

in_file_nest_err:
    cerror( not_in_section, NULL, 0L, NULL);
    goto  skip_line;
nest_err:
    cerror( "Not in a #if (#ifdef) section", NULL, 0L, NULL);       /* _E_  */
    goto  skip_line;
else_seen_err:
    cerror( "Already seen #else at line %.0s%ld"            /* _E_  */
            , NULL, g_internal_data->ifptr->elseline, NULL);
skip_line:
    skip_nl();                              /* Ignore rest of line  */
    goto  ret;

if_nest_err:
    cfatal( many_nesting, NULL, (long) BLK_NEST, NULL);

ret:
    g_internal_data->in_directive = FALSE;
    g_internal_data->keep_comments = g_internal_data->option_flags.c && compiling && !g_internal_data->no_output;
    g_internal_data->keep_spaces = g_internal_data->option_flags.k && compiling;
       /* keep_spaces is on for #define line even if no_output is TRUE  */
    if (! g_internal_data->wrong_line)
        g_internal_data->newlines++;
}

static int  do_if( int hash, const char * directive_name)
/*
 * Process an #if (#elif), #ifdef or #ifndef.  The latter two are straight-
 * forward, while #if needs a subroutine to evaluate the expression.
 * do_if() is called only if compiling is TRUE.  If false, compilation is
 * always supressed, so we don't need to evaluate anything.  This supresses
 * unnecessary warnings.
 */
{
    int     c;
    int     found;
    DEFBUF *    defp;

    if ((c = skip_ws()) == '\n') {
        unget_ch();
        cerror( no_arg, NULL, 0L, NULL);
        return  FALSE;
    }
    if (g_internal_data->mcpp_debug & MACRO_CALL) {
        sync_linenum();
        mcpp_fprintf( DEST_OUT, "/*%s %ld*/", directive_name, g_internal_data->src_line);
    }
    if (hash == L_if) {                 /* #if or #elif             */
        unget_ch();
        found = (eval_if() != 0L);      /* Evaluate expression      */
        if (g_internal_data->mcpp_debug & MACRO_CALL)
            g_internal_data->in_if = FALSE;      /* 'in_if' is dynamically set in eval_lex() */
        hash = L_ifdef;                 /* #if is now like #ifdef   */
    } else {                            /* #ifdef or #ifndef        */
        if (scan_token( c, (g_internal_data->workp = g_internal_data->work_buf, &g_internal_data->workp), g_internal_data->work_end) != NAM) {
            cerror( not_ident, g_internal_data->work_buf, 0L, NULL);
            return  FALSE;      /* Next token is not an identifier  */
        }
        found = ((defp = look_id( g_internal_data->identifier)) != NULL);    /* Look in table*/
        if (g_internal_data->mcpp_debug & MACRO_CALL) {
            if (found)
                mcpp_fprintf( DEST_OUT, "/*%s*/", defp->name);
        }
    }
    if (found == (hash == L_ifdef)) {
        compiling = TRUE;
        g_internal_data->ifptr->stat |= TRUE_SEEN;
    } else {
        compiling = FALSE;
    }
    if (g_internal_data->mcpp_debug & MACRO_CALL) {
        mcpp_fprintf( DEST_OUT, "/*i %c*/\n", compiling ? 'T' : 'F');
        /* Report wheather the directive is evaluated TRUE or FALSE */
    }
    return  TRUE;
}

static void sync_linenum( void)
/*
 * Put out newlines or #line line to synchronize line number with the
 * annotations about #if, #elif, #ifdef, #ifndef, #else or #endif on -K option.
 */
{
    if (g_internal_data->wrong_line || g_internal_data->newlines > 10) {
        sharp( NULL, 0);
    } else {
        while (g_internal_data->newlines-- > 0)
            mcpp_fputc('\n', DEST_OUT);
    }
    g_internal_data->newlines = -1;
}

static long do_line( void)
/*
 * Parse the line to update the line number and "filename" field for the next
 * input line.
 * Values returned are as follows:
 *  -1:     syntax error or out-of-range error (diagnosed by do_line(),
 *          eval_num()).
 *  [1,32767]:  legal line number for C90, [1,2147483647] for C99.
 * Line number [32768,2147483647] in C90 mode is only warned (not an error).
 * do_line() always absorbs the line (except the <newline>).
 */
{
    const char * const  not_digits
        = "Line number \"%s\" isn't a decimal digits sequence"; /* _E_ _W1_ */
    const char * const  out_of_range
        = "Line number \"%s\" is out of range of [1,%ld]";      /* _E_ _W1_ */
    int     token_type;
    VAL_SIGN *      valp;
    char *  save;
    int     c;

    if ((c = skip_ws()) == '\n') {
        cerror( no_arg, NULL, 0L, NULL);
        unget_ch();                         /* Push back <newline>  */
        return  -1L;                /* Line number is not changed   */
    }

    if (g_internal_data->standard) {
        token_type = get_unexpandable( c, FALSE);
        if (g_internal_data->macro_line == MACRO_ERROR)      /* Unterminated macro   */
            return  -1L;                    /*   already diagnosed. */
        if (token_type == NO_TOKEN) /* Macro expanded to 0 token    */
            goto  no_num;
        if (token_type != NUM)
            goto  illeg_num;
    } else if (scan_token( c, (g_internal_data->workp = g_internal_data->work_buf, &g_internal_data->workp), g_internal_data->work_end) != NUM) {
        goto  illeg_num;
    }
    for (g_internal_data->workp = g_internal_data->work_buf; *g_internal_data->workp != EOS; g_internal_data->workp++) {
        if (! isdigit( *g_internal_data->workp & UCHARMAX)) {
            if (g_internal_data->standard) {
                cerror( not_digits, g_internal_data->work_buf, 0L, NULL);
                return  -1L;
            } else if (g_internal_data->warn_level & 1) {
                cwarn( not_digits, g_internal_data->work_buf, 0L, NULL);
            }
        }
    }
    valp = eval_num( g_internal_data->work_buf);             /* Evaluate number      */
    if (valp->sign == VAL_ERROR) {  /* Error diagnosed by eval_num()*/
        return  -1;
    } else if (g_internal_data->standard
            && (g_internal_data->std_limits.line_num < valp->val || valp->val <= 0L)) {
        if (valp->val < LINE99LIMIT && valp->val > 0L) {
            if (g_internal_data->warn_level & 1)
                cwarn( out_of_range, g_internal_data->work_buf, g_internal_data->std_limits.line_num, NULL);
        } else {
            cerror( out_of_range, g_internal_data->work_buf, g_internal_data->std_limits.line_num, NULL);
            return  -1L;
        }
    }

    if (g_internal_data->standard) {
        token_type = get_unexpandable( skip_ws(), FALSE);
        if (g_internal_data->macro_line == MACRO_ERROR)
            return  -1L;
        if (token_type != STR) {
            if (token_type == NO_TOKEN) {   /* Filename is absent   */
                return  (long) valp->val;
            } else {    /* Expanded macro should be a quoted string */
                goto  not_fname;
            }
        }
    } else {
        if ((c = skip_ws()) == '\n') {
            unget_ch();
            return  (long) valp->val;
        }
        if (scan_token( c, (g_internal_data->workp = g_internal_data->work_buf, &g_internal_data->workp), g_internal_data->work_end) != STR)
            goto  not_fname;
    }
#if COMPILER == GNUC
    if (memcmp( g_internal_data->workp - 3, "//", 2) == 0) { /* "/cur-dir//"         */
        save = g_internal_data->infile->filename;    /* Do not change the file name  */
    } else
#endif
    {
        *(g_internal_data->workp - 1) = EOS;                 /* Ignore right '"'     */
        save = save_string( &g_internal_data->work_buf[ 1]); /* Ignore left '"'      */
    }

    if (g_internal_data->standard) {
        if (get_unexpandable( skip_ws(), FALSE) != NO_TOKEN) {
            cerror( excess, g_internal_data->work_buf, 0L, NULL);
            free( save);
            return  -1L;
        }
    } else if (g_internal_data->mcpp_mode == OLD_PREP) {
        skip_nl();
        unget_ch();
    } else if ((c = skip_ws()) == '\n') {
        unget_ch();
    } else {
        if (g_internal_data->warn_level & 1) {
            scan_token( c, (g_internal_data->workp = g_internal_data->work_buf, &g_internal_data->workp), g_internal_data->work_end);
            cwarn( excess, g_internal_data->work_buf, 0, NULL);
        }
        skip_nl();
        unget_ch();
    }

    if (g_internal_data->infile->filename)
        free( g_internal_data->infile->filename);
    g_internal_data->infile->filename = save;                /* New file name        */
            /* Note that this does not change g_internal_data->infile->real_fname    */
    return  (long) valp->val;               /* New line number      */

no_num:
    cerror( "No line number", NULL, 0L, NULL);              /* _E_  */
    return  -1L;
illeg_num:
    cerror( "Not a line number \"%s\"", g_internal_data->work_buf, 0L, NULL);        /* _E_  */
    return  -1L;
not_fname:
    cerror( "Not a file name \"%s\"", g_internal_data->work_buf, 0L, NULL);  /* _E_  */
    return  -1L;
}

/*
 *                  M a c r o  D e f i n i t i o n s
 */

/*
 * look_id()    Looks for the name in the defined symbol table.  Returns a
 *              pointer to the definition if found, or NULL if not present.
 * install_macro()  Installs the definition.  Updates the symbol table.
 * undefine()   Deletes the definition from the symbol table.
 */

/*
 * Global work_buf[] are used to store #define parameter lists and
 * parms[].name point to them.
 * 'nargs' contains the actual number of parameters stored.
 */
typedef struct {
    char *  name;                   /* -> Start of each parameter   */
    size_t  len;                    /* Length of parameter name     */
} PARM;
static PARM     parms[ NMACPARS];
static int      nargs;              /* Number of parameters         */
static char *   token_p;            /* Pointer to the token scanned */
static char *   repl_base;          /* Base of buffer for repl-text */
static char *   repl_end;           /* End of buffer for repl-text  */
static const char * const   no_ident = "No identifier";     /* _E_  */
#if COMPILER == GNUC
static int      gcc2_va_arg;        /* GCC2-spec variadic macro     */
#endif

DEFBUF *    do_define(
    int     ignore_redef,       /* Do not redefine   */
    int     predefine           /* Predefine compiler-specific name */
    /*
     * Note: The value of 'predefine' should be one of 0, DEF_NOARGS_PREDEF
     *      or DEF_NOARGS_PREDEF_OLD, the other values cause errors.
     */
)
/*
 * Called from directive() when a #define is scanned or called from
 *      do_options() when a -D option is scanned.  This module parses formal
 *      parameters by get_parm() and the replacement text by get_repl().
 *
 * There is some special case code to distinguish
 *      #define foo     bar     --  object-like macro
 * from #define foo()   bar     --  function-like macro with no parameter
 *
 * Also, we make sure that
 *      #define foo     foo
 * expands to "foo" but doesn't put MCPP into an infinite loop.
 *
 * A warning is printed if you redefine a symbol with a non-identical
 * text.  I.e,
 *      #define foo     123
 *      #define foo     123
 * is ok, but
 *      #define foo     123
 *      #define foo     +123
 * is not.
 *
 * The following subroutines are called from do_define():
 * get_parm()   parsing and remembering parameter names.
 * get_repl()   parsing and remembering replacement text.
 *
 * The following subroutines are called from get_repl():
 * is_formal()  is called when an identifier is scanned.  It checks through
 *              the array of formal parameters.  If a match is found, the
 *              identifier is replaced by a control byte which will be used
 *              to locate the parameter when the macro is expanded.
 * def_stringization()  is called when '#' operator is scanned.  It surrounds
 *              the token to stringize with magic-codes.
 *
 * modes other than STD ignore difference of parameter names in macro
 * redefinition.
 */
{
    const char * const  predef = "\"%s\" shouldn't be redefined";   /* _E_  */
    char    repl_list[ NMACWORK + IDMAX];   /* Replacement text     */
    char    macroname[ IDMAX + 1];  /* Name of the macro defining   */
    DEFBUF *    defp;               /* -> Old definition            */
    DEFBUF **   prevp;      /* -> Pointer to previous def in list   */
    int     c;
    int     redefined;                      /* TRUE if redefined    */
    int     dnargs = 0;                     /* defp->nargs          */
    int     cmp;                    /* Result of name comparison    */
    size_t  def_start = 0;     /* Column of macro definition   */
    size_t  def_end = 0;     /* Column of macro definition   */

    repl_base = repl_list;
    repl_end = & repl_list[ NMACWORK];
    c = skip_ws();
    if ((g_internal_data->mcpp_debug & MACRO_CALL) && g_internal_data->src_line)      /* Start of definition  */
        def_start = g_internal_data->infile->bptr - g_internal_data->infile->buffer - 1;
    if (c == '\n') {
        cerror( no_ident, NULL, 0L, NULL);
        unget_ch();
        return  NULL;
    } else if (scan_token( c, (g_internal_data->workp = g_internal_data->work_buf, &g_internal_data->workp), g_internal_data->work_end) != NAM) {
        cerror( not_ident, g_internal_data->work_buf, 0L, NULL);
        return  NULL;
    } else {
        prevp = look_prev( g_internal_data->identifier, &cmp);
                /* Find place in the macro list to insert the definition    */
        defp = *prevp;
        if (g_internal_data->standard) {
            if (cmp || defp->push) {    /* Not known or 'pushed' macro      */
                if (str_eq( g_internal_data->identifier, "defined")
                        || ((g_internal_data->stdc_val || g_internal_data->cplus_val)
                            &&  str_eq( g_internal_data->identifier, "__VA_ARGS__"))) {
                    cerror(
            "\"%s\" shouldn't be defined", g_internal_data->identifier, 0L, NULL);   /* _E_  */
                    return  NULL;
                }
                redefined = FALSE;          /* Quite new definition */
            } else {                        /* It's known:          */
                if (ignore_redef)
                    return  defp;
                dnargs = (defp->nargs == DEF_NOARGS_STANDARD
                            || defp->nargs == DEF_NOARGS_PREDEF
                            || defp->nargs == DEF_NOARGS_PREDEF_OLD)
                        ? DEF_NOARGS : defp->nargs;
                if (dnargs <= DEF_NOARGS_DYNAMIC    /* __FILE__ and such    */
                        || dnargs == DEF_PRAGMA /* _Pragma() pseudo-macro   */
                        ) {
                    cerror( predef, g_internal_data->identifier, 0L, NULL);
                    return  NULL;
                } else {
                    redefined = TRUE;       /* Remember this fact   */
                }
            }
        } else {
            if (cmp) {
                redefined = FALSE;          /* Quite new definition */
            } else {                        /* It's known:          */
                if (ignore_redef)
                    return  defp;
                dnargs = (defp->nargs == DEF_NOARGS_STANDARD
                            || defp->nargs == DEF_NOARGS_PREDEF
                            || defp->nargs == DEF_NOARGS_PREDEF_OLD)
                        ? DEF_NOARGS : defp->nargs;
                redefined = TRUE;
            }
        }
    }
    strcpy( macroname, g_internal_data->identifier);         /* Remember the name    */

    g_internal_data->in_define = TRUE;                       /* Recognize '#', '##'  */
    if (get_parm() == FALSE) {              /* Get parameter list   */
        g_internal_data->in_define = FALSE;
        return  NULL;                       /* Syntax error         */
    }
    if (get_repl( macroname) == FALSE) {    /* Get replacement text */
        g_internal_data->in_define = FALSE;
        return  NULL;                       /* Syntax error         */
    }
    if ((g_internal_data->mcpp_debug & MACRO_CALL) && g_internal_data->src_line) {
                                    /* Remember location on source  */
        char *  cp;
        cp = g_internal_data->infile->bptr - 1;              /* Before '\n'          */
        while (g_internal_data->char_type[ *cp & UCHARMAX] & HSP)
            cp--;                           /* Trailing space       */
        cp++;                       /* Just after the last token    */
        def_end = cp - g_internal_data->infile->buffer;      /* End of definition    */
    }

    g_internal_data->in_define = FALSE;
    if (redefined) {
        if (dnargs != nargs || ! str_eq( defp->repl, repl_list)
                || (g_internal_data->mcpp_mode == STD && ! str_eq( defp->parmnames, g_internal_data->work_buf))
                ) {             /* Warn if differently redefined    */
            if (g_internal_data->warn_level & 1) {
                cwarn(
            "The macro is redefined", NULL, 0L, NULL);      /* _W1_ */
                if (! g_internal_data->option_flags.no_source_line)
                    dump_a_def( "    previously macro", defp, FALSE, TRUE
                            , g_internal_data->fp_err);
            }
        } else {                        /* Identical redefinition   */
            return  defp;
        }
    }                                   /* Else new or re-definition*/
    defp = install_macro( macroname, nargs, g_internal_data->work_buf, repl_list, prevp, cmp
            , predefine);
    if ((g_internal_data->mcpp_debug & MACRO_CALL) && g_internal_data->src_line) {
                                    /* Get location on source file  */
        LINE_COL    s_line_col, e_line_col;
        s_line_col.line = g_internal_data->src_line;
        s_line_col.col = def_start;
        get_src_location( & s_line_col);
                            /* Convert to pre-line-splicing data    */
        e_line_col.line = g_internal_data->src_line;
        e_line_col.col = def_end;
        get_src_location( & e_line_col);
        /* Putout the macro definition information embedded in comment  */
        mcpp_fprintf( DEST_OUT, "/*m%s %ld:%d-%ld:%d*/\n", defp->name
                , s_line_col.line, s_line_col.col
                , e_line_col.line, e_line_col.col);
        g_internal_data->wrong_line = TRUE;                      /* Need #line later */
    }
    if (g_internal_data->mcpp_mode == STD && g_internal_data->cplus_val && id_operator( macroname)
            && (g_internal_data->warn_level & 1))
        /* These are operators, not identifiers, in C++98   */
        cwarn( "\"%s\" is defined as macro", macroname      /* _W1_ */
                , 0L, NULL);
    return  defp;
}

static int  get_parm( void)
/*
 *   Get parameters i.e. numbers into nargs, name into work_buf[], name-length
 * into parms[].len.  parms[].name point into work_buf.
 *   Return TRUE if the parameters are legal, else return FALSE.
 *   In STD mode preprocessor must remember the parameter names, only for
 * checking the validity of macro redefinitions.  This is required by the
 * Standard (what an overhead !).
 */
{
    const char * const  many_parms
            = "More than %.0s%ld parameters";       /* _E_ _W4_     */
    const char * const  illeg_parm
            = "Illegal parameter \"%s\"";           /* _E_          */
    const char * const  misplaced_ellip
            = "\"...\" isn't the last parameter";   /* _E_          */
    int     token_type;
    int     c;

    parms[ 0].name = g_internal_data->workp = g_internal_data->work_buf;
    g_internal_data->work_buf[ 0] = EOS;
#if COMPILER == GNUC
    gcc2_va_arg = FALSE;
#endif

    /* POST_STD mode    */
    g_internal_data->insert_sep = NO_SEP;    /* Clear the inserted token separator   */
    c = get_ch();

    if (c == '(') {                         /* With arguments?      */
        nargs = 0;                          /* Init parms counter   */
        if (skip_ws() == ')')
            return  TRUE;                   /* Macro with 0 parm    */
        else
            unget_ch();

        do {                                /* Collect parameters   */
            if (nargs >= NMACPARS) {
                cerror( many_parms, NULL, (long) NMACPARS, NULL);
                return  FALSE;
            }
            parms[ nargs].name = g_internal_data->workp;     /* Save its start       */
            if ((token_type = scan_token( c = skip_ws(), &g_internal_data->workp, g_internal_data->work_end))
                    != NAM) {
                if (c == '\n') {
                    break;
                } else if (c == ',' || c == ')') {
                    cerror( "Empty parameter", NULL, 0L, NULL);     /* _E_  */
                    return  FALSE;
                } else if (g_internal_data->standard && (g_internal_data->stdc_val || g_internal_data->cplus_val)
                        && token_type == OPE && g_internal_data->openum == OP_ELL) {
                    /*
                     * Enable variable argument macro which is a feature of
                     * C99.  We enable this even on C90 or C++ for GCC
                     * compatibility.
                     */
                    if (skip_ws() != ')') {
                        cerror( misplaced_ellip, NULL, 0L, NULL);
                        return  FALSE;
                    }
                    parms[ nargs++].len = 3;
                    nargs |= VA_ARGS;
                    goto  ret;
                } else {
                    cerror( illeg_parm, parms[ nargs].name, 0L, NULL);
                    return  FALSE;          /* Bad parameter syntax */
                }
            }
            if (g_internal_data->standard && (g_internal_data->stdc_val || g_internal_data->cplus_val)
                    && str_eq( g_internal_data->identifier, "__VA_ARGS__")) {
                cerror( illeg_parm, parms[ nargs].name, 0L, NULL);
                return  FALSE;
                /* __VA_ARGS__ should not be used as a parameter    */
            }
            if (is_formal( parms[ nargs].name, FALSE)) {
                cerror( "Duplicate parameter name \"%s\""   /* _E_  */
                        , parms[ nargs].name, 0L, NULL);
                return  FALSE;
            }
            parms[ nargs].len = (size_t) (g_internal_data->workp - parms[ nargs].name);
                                            /* Save length of param */
            *g_internal_data->workp++ = ',';
            nargs++;
        } while ((c = skip_ws()) == ',');   /* Get another parameter*/

        *--g_internal_data->workp = EOS;                     /* Remove excessive ',' */
        if (c != ')') {                     /* Must end at )        */
#if COMPILER == GNUC
            /* Handle GCC2 variadic params like par...  */
            char *  tp = g_internal_data->workp;
            if (g_internal_data->mcpp_mode == STD
                    &&(token_type = scan_token( c, &g_internal_data->workp, g_internal_data->work_end)) == OPE
                    && g_internal_data->openum == OP_ELL) {
                if ((c = skip_ws()) != ')') {
                    cerror( misplaced_ellip, NULL, 0L, NULL);
                    return  FALSE;
                }
                *tp = EOS;                  /* Remove "..."         */
                nargs |= VA_ARGS;
                gcc2_va_arg = TRUE;
                goto  ret;
            }
#endif
            unget_ch();                     /* Push back '\n'       */
            cerror(
        "Missing \",\" or \")\" in parameter list \"(%s\""  /* _E_  */
                    , g_internal_data->work_buf, 0L, NULL);
            return  FALSE;
        }
    } else {
        /*
         * DEF_NOARGS is needed to distinguish between
         * "#define foo" and "#define foo()".
         */
        nargs = DEF_NOARGS;                 /* Object-like macro    */
        unget_ch();
    }
ret:
#if NMACPARS > NMACPARS90MIN
    if ((g_internal_data->warn_level & 4) && (nargs & ~AVA_ARGS) > g_internal_data->std_limits.n_mac_pars)
        cwarn( many_parms, NULL , (long) g_internal_data->std_limits.n_mac_pars, NULL);
#endif
    return  TRUE;
}

static int  get_repl(
    const char * macroname
)
/*
 *   Get replacement text i.e. names of formal parameters are converted to
 * the magic numbers, and operators #, ## is converted to magic characters.
 *   Return TRUE if replacement list is legal, else return FALSE.
 *   Any token separator in the text is converted to a single space, no token
 * sepatator is inserted by MCPP.  Those are required by the Standard for
 * stringizing of an argument by # operator.
 *   In POST_STD mode, inserts a space between any tokens in source (except a
 * macro name and the next '(' in macro definition), hence presence or absence
 * of token separator makes no difference.
 */
{
    const char * const  mixed_ops
    = "Macro with mixing of ## and # operators isn't portable";     /* _W4_ */
    const char * const  multiple_cats
    = "Macro with multiple ## operators isn't portable";    /* _W4_ */
    char *  prev_token = NULL;              /* Preceding token      */
    char *  prev_prev_token = NULL;         /* Pre-preceding token  */
    int     multi_cats = FALSE;             /* Multiple ## operators*/
    int     c;
    int     token_type;                     /* Type of token        */
    char *  temp;
    char *  repl_cur = repl_base;   /* Pointer into repl-text buffer*/

    *repl_cur = EOS;
    token_p = NULL;
    if (g_internal_data->mcpp_mode == STD) {
        c = get_ch();
        unget_ch();
        if (((g_internal_data->char_type[ c] & SPA) == 0) && (nargs < 0) && (g_internal_data->warn_level & 1))
            cwarn( "No space between macro name \"%s\" and repl-text"/* _W1_ */
                , macroname, 0L, NULL);
    }
    c = skip_ws();                          /* Get to the body      */

    while (c != '\n') {
        if (g_internal_data->standard) {
            prev_prev_token = prev_token;
            prev_token = token_p;
        }
        token_p = repl_cur;                 /* Remember the pointer */
        token_type = scan_token( c, &repl_cur, repl_end);

        switch (token_type) {
        case OPE:                   /* Operator or punctuator       */
            if (! g_internal_data->standard)
                break;
            switch (g_internal_data->openum) {
            case OP_CAT:                    /* ##                   */
                if (prev_token == NULL) {
                    cerror( "No token before ##"            /* _E_  */
                            , NULL, 0L, NULL);
                    return  FALSE;
                } else if (*prev_token == CAT) {
                    cerror( "## after ##", NULL, 0L, NULL); /* _E_  */
                    return  FALSE;
                } else if (prev_prev_token && *prev_prev_token == CAT) {
                    multi_cats = TRUE;
                } else if (prev_prev_token && *prev_prev_token == ST_QUOTE
                        && (g_internal_data->warn_level & 4)) {      /* # parm ##    */
                    cwarn( mixed_ops, NULL, 0L, NULL);
                }
                repl_cur = token_p;
                *repl_cur++ = CAT;          /* Convert to CAT       */
                break;
            case OP_STR:                    /* #                    */
                if (nargs < 0)              /* In object-like macro */
                    break;                  /* '#' is an usual char */
                if (prev_token && *prev_token == CAT
                        && (g_internal_data->warn_level & 4))        /* ## #         */
                    cwarn( mixed_ops, NULL, 0L, NULL);
                repl_cur = token_p;         /* Overwrite on #       */
                if ((temp = def_stringization( repl_cur)) == NULL) {
                    return  FALSE;          /* Error                */
                } else {
                    repl_cur = temp;
                }
                break;
            default:                    /* Any operator as it is    */
                break;
            }
            break;
        case NAM:
        /*
         * Replace this name if it's a parm.  Note that the macro name is a
         * possible replacement token.  We stuff DEF_MAGIC in front of the
         * token which is treated as a LETTER by the token scanner and eaten
         * by the macro expanding routine.  This prevents the macro expander
         * from looping if someone writes "#define foo foo".
         */
            temp = is_formal( g_internal_data->identifier, TRUE);
            if (temp == NULL) {             /* Not a parameter name */
                if (! g_internal_data->standard)
                    break;
                if ((g_internal_data->stdc_val || g_internal_data->cplus_val)
                            && str_eq( g_internal_data->identifier, "__VA_ARGS__")) {
#if COMPILER == GNUC
                    if (gcc2_va_arg) {
        cerror( "\"%s\" cannot be used in GCC2-spec variadic macro" /* _E_  */
                            , g_internal_data->identifier, 0L, NULL);
                        return  FALSE;
                    }
#endif
                    cerror( "\"%s\" without corresponding \"...\""  /* _E_  */
                            , g_internal_data->identifier, 0L, NULL);
                    return  FALSE;
                }
                if ((temp = mgtoken_save( macroname)) != NULL)
                    repl_cur = temp;        /* Macro name           */
            } else {                        /* Parameter name       */
                repl_cur = temp;
#if COMPILER == GNUC
                if (g_internal_data->mcpp_mode == STD && (nargs & VA_ARGS)
                        && *(repl_cur - 1) == (nargs & ~AVA_ARGS)) {
                    if (! str_eq( g_internal_data->identifier, "__VA_ARGS__")
                            && (g_internal_data->warn_level & 2))
                        cwarn(
                            "GCC2-spec variadic macro is defined"   /* _W2_ */
                                    , NULL, 0L, NULL);
                    if (prev_token && *prev_token == CAT
                            && prev_prev_token && *prev_prev_token == ',')
                        /* ", ## __VA_ARGS__" is sequence peculiar  */
                        /* to GCC3-spec variadic macro.             */
                        /* Or ", ## last_arg" is sequence peculiar  */
                        /* to GCC2-spec variadic macro.             */
                        nargs |= GVA_ARGS;
                        /* Mark as sequence peculiar to GCC         */
                        /* This will be warned at expansion time    */
                }
#endif
            }
            break;

        case STR:                           /* String in mac. body  */
        case CHR:                           /* Character constant   */
            if (g_internal_data->mcpp_mode == OLD_PREP)
                repl_cur = str_parm_scan( repl_cur);
            break;
        case SEP:
            if (g_internal_data->mcpp_mode == OLD_PREP && c == COM_SEP)
                repl_cur--;                 /* Skip comment now     */
            break;
        default:                            /* Any token as it is   */
            break;
        }

        if ((c = get_ch()) == ' ' || c == '\t') {
            *repl_cur++ = ' ';              /* Space                */
            while ((c = get_ch()) == ' ' || c == '\t')
                ;                   /* Skip excessive spaces        */
        }
    }

    while (repl_base < repl_cur
            && (*(repl_cur - 1) == ' ' || *(repl_cur - 1) == '\t'))
        repl_cur--;                     /* Remove trailing spaces   */
    *repl_cur = EOS;                        /* Terminate work       */

    unget_ch();                             /* For syntax check     */
    if (g_internal_data->standard) {
        if (token_p && *token_p == CAT) {
            cerror( "No token after ##", NULL, 0L, NULL);   /* _E_  */
            return  FALSE;
        }
        if (multi_cats && (g_internal_data->warn_level & 4))
            cwarn( multiple_cats, NULL, 0L, NULL);
        if ((nargs & VA_ARGS) && g_internal_data->stdc_ver < 199901L && (g_internal_data->warn_level & 2))
            /* Variable arg macro is the spec of C99, not C90 nor C++98     */
            cwarn( "Variable argument macro is defined",    /* _W2_ */
                    NULL, 0L, NULL);
    }

    return  TRUE;
}

static char *   is_formal(
    const char *    name,
    int         conv                    /* Convert to magic number? */
)
/*
 * If the identifier is a formal parameter, save the MAC_PARM and formal
 * offset, returning the advanced pointer into the replacement text.
 * Else, return NULL.
 */
{
    char *  repl_cur;
    const char *    va_arg = "__VA_ARGS__";
    PARM    parm;
    size_t  len;
    int     i;

    len = strlen( name);
    for (i = 0; i < (nargs & ~AVA_ARGS); i++) {     /* For each parameter   */
        parm = parms[ i];
        if ((len == parm.len
                /* Note: parms[].name are comma separated  */
                    && memcmp( name, parm.name, parm.len) == 0)
                || (g_internal_data->standard && (nargs & VA_ARGS)
                    && i == (nargs & ~AVA_ARGS) - 1 && conv
                    && str_eq( name, va_arg))) {    /* __VA_ARGS__  */
                                            /* If it's known        */
#if COMPILER == GNUC
            if (gcc2_va_arg && str_eq( name, va_arg))
                return  NULL;               /* GCC2 variadic macro  */
#endif
            if (conv) {
                repl_cur = token_p;         /* Overwrite on the name*/
                *repl_cur++ = MAC_PARM;     /* Save the signal      */
                *repl_cur++ = (char)(i + 1);        /* Save the parm number */
                return  repl_cur;           /* Return "gotcha"      */
            } else {
                return  parm.name;          /* Duplicate parm name  */
            }
        }
    }

    return  NULL;                           /* Not a formal param   */
}

static char *   def_stringization( char * repl_cur)
/*
 * Define token stringization.
 * We store a magic cookie (which becomes surrouding " on expansion) preceding
 * the parameter as an operand of # operator.
 * Return the current pointer into replacement text if the token following #
 * is a parameter name, else return NULL.
 */
{
    int     c;
    char *  temp;

    *repl_cur++ = ST_QUOTE;                 /* Prefix               */
    if (g_internal_data->char_type[ c = get_ch()] & HSP) {   /* There is a space     */
        *repl_cur++ = ' ';
        while (g_internal_data->char_type[ c = get_ch()] & HSP)      /* Skip excessive spaces*/
            ;
    }
    token_p = repl_cur;                     /* Remember the pointer */
    if (scan_token( c, &repl_cur, repl_end) == NAM) {
        if ((temp = is_formal( g_internal_data->identifier, TRUE)) != NULL) {
            repl_cur = temp;
            return  repl_cur;
        }
    }
    cerror( "Not a formal parameter \"%s\"", token_p, 0L, NULL);    /* _E_  */
    return  NULL;
}

static char *   mgtoken_save( const char * macroname)
/*
 * A magic cookie is inserted if the token is identical to the macro name,
 * so the expansion doesn't recurse.
 * Return the advanced pointer into the replacement text or NULL.
 */
{
    char *   repl_cur;

    if (str_eq( macroname, g_internal_data->identifier)) {   /* Macro name in body   */
        repl_cur = token_p;                 /* Overwrite on token   */
        *repl_cur++ = DEF_MAGIC;            /* Save magic marker    */
        repl_cur = stpcpy( repl_cur, g_internal_data->identifier);
                                            /* And save the token   */
        return  repl_cur;
    } else {
        return  NULL;
    }
}

static char *   str_parm_scan( char * string_end)
/*
 * String parameter scan.
 * This code -- if enabled -- recognizes a formal parameter in a string
 * literal or in a character constant.
 *      #define foo(bar, v) printf("%bar\n", v)
 *      foo( d, i)
 * expands to:
 *      printf("%d\n", i)
 * str_parm_scan() return the advanced pointer into the replacement text.
 * This has been superceded by # stringizing and string concatenation.
 * This routine is called only in OLD_PREP mode.
 */
{
    int     delim;
    int     c;
    char *  tp;
    char *  wp;             /* Pointer into the quoted literal  */

    delim = *token_p;
    unget_string( ++token_p, NULL);
    /* Pseudo-token-parsing in a string literal */
    wp = token_p;
    while ((c = get_ch()) != delim) {
        token_p = wp;
        if (scan_token( c, &wp, string_end) != NAM)
            continue;
        if ((tp = is_formal( token_p, TRUE)) != NULL)
            wp = tp;
    }
    *wp++ = (char)delim;
    return  wp;
}

static void do_undef( void)
/*
 * Remove the symbol from the defined list.
 * Called from directive().
 */
{
    DEFBUF *    defp;
    int     c;

    if ((c = skip_ws()) == '\n') {
        cerror( no_ident, NULL, 0L, NULL);
        unget_ch();
        return;
    }
    if (scan_token( c, (g_internal_data->workp = g_internal_data->work_buf, &g_internal_data->workp), g_internal_data->work_end) != NAM) {
        cerror( not_ident, g_internal_data->work_buf, 0L, NULL);
        skip_nl();
        unget_ch();
    } else {
        if ((defp = look_id( g_internal_data->identifier)) == NULL) {
            if (g_internal_data->warn_level & 8)
                cwarn( "\"%s\" wasn't defined"              /* _W8_ */
                        , g_internal_data->identifier, 0L, NULL);
        } else if (g_internal_data->standard && (defp->nargs <= DEF_NOARGS_STANDARD
                                                /* Standard predef  */
                    || defp->nargs == DEF_PRAGMA)) {
                                        /* _Pragma() pseudo-macro   */
            cerror( "\"%s\" shouldn't be undefined"         /* _E_  */
                    , g_internal_data->identifier, 0L, NULL);
        } else if (g_internal_data->standard) {
            c = skip_ws();
            unget_ch();
            if (c != '\n')                      /* Trailing junk    */
                return;
            else
                undefine( g_internal_data->identifier);
        } else {
            undefine( g_internal_data->identifier);
        }
    }
}

/*
 *                  C P P   S y m b o l   T a b l e s
 *
 * SBSIZE defines the number of hash-table slots for the symbol table.
 * It must be a power of 2.
 */

/* Symbol table queue headers.  */
static DEFBUF *     symtab[ SBSIZE];
static long         num_of_macro = 0;

#if MCPP_LIB
void    init_directive( void)
/* Initialize static variables. */
{
    num_of_macro = 0;
}
#endif

DEFBUF *    look_id( const char * name)
/*
 * Look for the identifier in the symbol table.
 * If found, return the table pointer;  Else return NULL.
 */
{
    DEFBUF **   prevp;
    int         cmp;

    prevp = look_prev( name, &cmp);

    if (g_internal_data->standard)
        return ((cmp == 0 && (*prevp)->push == 0) ? *prevp : NULL);
    else
        return ((cmp == 0) ? *prevp : NULL);
}

DEFBUF **   look_prev(
    const char *    name,                   /* Name of the macro    */
    int *   cmp                             /* Result of comparison */
)
/*
 * Look for the place to insert the macro definition.
 * Return a pointer to the previous member in the linked list.
 */
{
    const char *    np;
    DEFBUF **   prevp;
    DEFBUF *    dp;
    size_t      s_name;
    int         hash;

    for (hash = 0, np = name; *np != EOS; )
        hash += *np++;
    hash += (int)(s_name = (size_t)(np - name));
    s_name++;
    prevp = & symtab[ hash & SBMASK];
    *cmp = -1;                              /* Initialize           */

    while ((dp = *prevp) != NULL) {
        if ((*cmp = strcmp( dp->name, name)) >= 0)
            break;
        prevp = &dp->link;
    }

    return  prevp;
}

DEFBUF *    look_and_install(
    const char *    name,                   /* Name of the macro    */
    int     numargs,                        /* The numbers of parms */
    const char *    parmnames,  /* Names of parameters concatenated */
    const char *    repl                    /* Replacement text     */
)
/*
 * Look for the name and (re)define it.
 * Returns a pointer to the definition block.
 * Returns NULL if the symbol was Standard-predefined.
 */
{
    DEFBUF **   prevp;          /* Place to insert definition       */
    DEFBUF *    defp;                       /* New definition block */
    int         cmp;    /* Result of comparison of new name and old */

    prevp = look_prev( name, &cmp);
    defp = install_macro( name, numargs, parmnames, repl, prevp, cmp, 0);
    return  defp;
}

DEFBUF *    install_macro(
    const char *    name,                   /* Name of the macro    */
    int     numargs,                        /* The numbers of parms */
    const char *    parmnames,  /* Names of parameters concatenated */
    const char *    repl,                   /* Replacement text     */
    DEFBUF **  prevp,           /* The place to insert definition   */
    int     cmp,        /* Result of comparison of new name and old */
    int     predefine   /* Predefined macro without leading '_'     */
)
/*
 * Enter this name in the lookup table.
 * Returns a pointer to the definition block.
 * Returns NULL if the symbol was Standard-predefined.
 * Note that predefinedness can be specified by either of 'numargs' or
 * 'predefine'.
 */
{
    DEFBUF *    dp;
    DEFBUF *    defp;
    size_t      s_name, s_parmnames, s_repl;

    defp = *prevp;                  /* Old definition, if cmp == 0  */
    if (cmp == 0 && defp->nargs < DEF_NOARGS - 1)
        return  NULL;                       /* Standard predefined  */
    if (parmnames == NULL || repl == NULL || (predefine && numargs > 0)
            || (predefine && predefine != DEF_NOARGS_PREDEF
                    && predefine != DEF_NOARGS_PREDEF_OLD))
                                                /* Shouldn't happen */
        cfatal( "Bug: Illegal macro installation of \"%s\"" /* _F_  */
                , name, 0L, NULL);      /* Use "" instead of NULL   */
    s_name = strlen( name);
    if (g_internal_data->mcpp_mode == STD)
        s_parmnames = strlen( parmnames) + 1;
    else
        s_parmnames = 0;
    s_repl = strlen( repl) + 1;
    dp = (DEFBUF *)
        xmalloc( sizeof (DEFBUF) + s_name + s_parmnames + s_repl);
    if (cmp || (g_internal_data->standard && (*prevp)->push)) {  /* New definition   */
        dp->link = defp;                /* Insert to linked list    */
        *prevp = dp;
    } else {                            /* Redefinition             */
        dp->link = defp->link;          /* Replace old def with new */
        *prevp = dp;
        free( defp);
    }
    dp->nargs = (short)(predefine ? predefine : numargs);
    if (g_internal_data->standard) {
        dp->push = 0;
        dp->parmnames = (char *)dp + sizeof (DEFBUF) + s_name;
        dp->repl = dp->parmnames + s_parmnames;
        if (g_internal_data->mcpp_mode == STD)
            memcpy( dp->parmnames, parmnames, s_parmnames);
    } else {
        dp->repl = (char *)dp + sizeof (DEFBUF) + s_name;
    }
    memcpy( dp->name, name, s_name + 1);
    memcpy( dp->repl, repl, s_repl);
    /* Remember where the macro is defined  */
    dp->fname = g_internal_data->cur_fullname;   /* Full-path-list of current file   */
    dp->mline = g_internal_data->src_line;
    if (g_internal_data->standard && cmp && ++num_of_macro == g_internal_data->std_limits.n_macro + 1
            && g_internal_data->std_limits.n_macro && (g_internal_data->warn_level & 4))
        /* '&& std_limits.n_macro' to avoid warning before initialization   */
        cwarn( "More than %.0s%ld macros defined"           /* _W4_ */
                , NULL , g_internal_data->std_limits.n_macro, NULL);
    return  dp;
}

int undefine(
    const char *  name                      /* Name of the macro    */
)
/*
 * Delete the macro definition from the symbol table.
 * Returns TRUE, if deleted;
 * Else returns FALSE (when the macro was not defined or was Standard
 * predefined).
 */
{
    DEFBUF **   prevp;          /* Preceding definition in list     */
    DEFBUF *    dp;                     /* Definition to delete     */
    int         cmp;            /* 0 if defined, else not defined   */

    prevp = look_prev( name, &cmp);
    dp = *prevp;                        /* Definition to delete     */
    if (cmp || dp->nargs <= DEF_NOARGS_STANDARD)
        return  FALSE;      /* Not defined or Standard predefined   */
    if (g_internal_data->standard && dp->push)
        return  FALSE;                  /* 'Pushed' macro           */
    *prevp = dp->link;          /* Link the previous and the next   */
    if ((g_internal_data->mcpp_debug & MACRO_CALL) && dp->mline) {
        /* Notice this directive unless the macro is predefined     */
        mcpp_fprintf( DEST_OUT, "/*undef %ld*//*%s*/\n", g_internal_data->src_line, dp->name);
        g_internal_data->wrong_line = TRUE;
    }
    free( dp);                          /* Delete the definition    */
    if (g_internal_data->standard)
        num_of_macro--;
    return  TRUE;
}

static void dump_repl(
    const DEFBUF *  dp,
    SIOHandle *  fp,
    int     gcc2_va
)
/*
 * Dump replacement text.
 */
{
    int     numargs = dp->nargs;
    char *  cp1;
    size_t  i;
    int     c;
    const char *    cp;

    for (cp = dp->repl; (c = *cp++ & UCHARMAX) != EOS; ) {

        switch (c) {
        case MAC_PARM:                              /* Parameter    */
            c = (*cp++ & UCHARMAX) - 1;
            if (g_internal_data->standard) {
                PARM    parm = parms[ c];
                if ((numargs & VA_ARGS) && c == (numargs & ~AVA_ARGS) - 1) {
                    mcpp_fputs( gcc2_va ? parm.name : "__VA_ARGS__"
                            , FP2DEST( fp));
                    /* gcc2_va is possible only in STD mode */
                } else {
                    if (g_internal_data->mcpp_mode == STD) {
                        for (i = 0, cp1 = parm.name; i < parm.len; i++)
                            mcpp_fputc( *cp1++, FP2DEST( fp));
                    } else {
                        mcpp_fputc( 'a' + c % 26, FP2DEST( fp));
                        if (c > 26)
                            mcpp_fputc( '0' + c / 26, FP2DEST( fp));
                    }
                }
            } else {
                mcpp_fputc( 'a' + c % 26, FP2DEST( fp));
                if (c > 26)
                    mcpp_fputc( '0' + c / 26, FP2DEST( fp));
            }
            break;
        case DEF_MAGIC:
            if (! g_internal_data->standard)
                mcpp_fputc( c, FP2DEST( fp));
            /* Else skip    */
            break;
        case CAT:
            if (g_internal_data->standard)
                mcpp_fputs( "##", FP2DEST( fp));
            else
                mcpp_fputc( c, FP2DEST( fp));
            break;
        case ST_QUOTE:
            if (g_internal_data->standard)
                mcpp_fputs( "#", FP2DEST( fp));
            else
                mcpp_fputc( c, FP2DEST( fp));
            break;
        case COM_SEP:
            /*
             * Though TOK_SEP coincides to COM_SEP, this cannot appear in
             * Standard mode.
             */
            if (g_internal_data->mcpp_mode == OLD_PREP)
                mcpp_fputs( "/**/", FP2DEST( fp));
            break;
        default:
            mcpp_fputc( c, FP2DEST( fp));
            break;
        }
    }
}

/*
 * If the compiler is so-called "one-pass" compiler, compiler-predefined
 * macros are commented out to avoid redefinition.
 */
#if ONE_PASS
#define CAN_REDEF   DEF_NOARGS
#else
#define CAN_REDEF   DEF_NOARGS_PREDEF
#endif

void    dump_a_def(
    const char *    why,
    const DEFBUF *  dp,
    int     newdef,         /* TRUE if parmnames are currently in parms[] */
    int     comment,        /* Show location of the definition in comment   */
    SIOHandle *  fp
)
/*
 * Dump a macro definition.
 */
{
    char *  cp, * cp1;
    int     numargs = dp->nargs & ~AVA_ARGS;
    int     commented;                      /* To be commented out  */
    int     gcc2_va = FALSE;                /* GCC2-spec variadic   */
    int     i;

    if (g_internal_data->standard && numargs == DEF_PRAGMA)  /* _Pragma pseudo-macro */
        return;
    if ((numargs < CAN_REDEF) || (g_internal_data->standard && dp->push))
        commented = TRUE;
    else
        commented = FALSE;
    if (! comment && commented)             /* For -dM option       */
        return;
    if (why)
        mcpp_fprintf( FP2DEST( fp), "%s \"%s\" defined as: ", why, dp->name);
    mcpp_fprintf( FP2DEST( fp), "%s#define %s", commented ? "/* " : "",
            dp->name);                      /* Macro name           */
    if (numargs >= 0) {                     /* Parameter list       */
        if (g_internal_data->mcpp_mode == STD) {
            const char *    appendix = g_internal_data->empty_str;
            if (! newdef) {
                /* Make parms[] for dump_repl() */
                for (i = 0, cp = dp->parmnames; i < numargs;
                        i++, cp = cp1 + 1) {
                    if ((cp1 = strchr( cp, ',')) == NULL)   /* The last arg */
                        parms[ i].len = strlen( cp);
                    else
                        parms[ i].len = (size_t) (cp1 - cp);
                    parms[ i].name = cp;
                }
            }
#if COMPILER == GNUC
            if ((dp->nargs & VA_ARGS)
                    && memcmp( parms[ numargs - 1].name, "...", 3) != 0) {
                appendix = "...";   /* Append ... so as to become 'args...' */
                gcc2_va = TRUE;
            }
#endif
            mcpp_fprintf( FP2DEST( fp), "(%s%s)", dp->parmnames, appendix);
        } else {
            if (newdef) {
                mcpp_fprintf( FP2DEST( fp), "(%s)", parms[ 0].name);
            } else if (numargs == 0) {
                mcpp_fputs( "()", FP2DEST( fp));
            } else {
                /* Print parameter list automatically made as:      */
                /* a, b, c, ..., a0, b0, c0, ..., a1, b1, c1, ...   */
                mcpp_fputc( '(', FP2DEST( fp));
                for (i = 0; i < numargs; i++) {     /* Make parameter list  */
                    mcpp_fputc( 'a' + i % 26, FP2DEST( fp));
                    if (i >= 26)
                        mcpp_fputc( '0' + i / 26, FP2DEST( fp));
                    if (i + 1 < numargs)
                        mcpp_fputc( ',', FP2DEST( fp));
                }
                mcpp_fputc( ')', FP2DEST( fp));
            }
        }
    }
    if (*dp->repl) {
        mcpp_fputc( ' ', FP2DEST( fp));
        dump_repl( dp, fp, gcc2_va);        /* Replacement text     */
    }
    if (commented)
            /* Standard predefined or one-pass-compiler-predefined  */
        mcpp_fputs( " */", FP2DEST( fp));
    if (comment)                            /* Not -dM option       */
        mcpp_fprintf( FP2DEST( fp), " \t/* %s:%ld\t*/", dp->fname, dp->mline);
    mcpp_fputc( '\n', FP2DEST( fp));
}

void    dump_def(
    int     comment,        /* Location of definition in comment    */
    int     K_opt                       /* -K option is specified   */
)
/*
 * Dump all the current macro definitions to output stream.
 */
{
    DEFBUF *    dp;
    DEFBUF **   symp;

    sharp( NULL, 0);    /* Report the current source file & line    */
    if (comment)
        mcpp_fputs( "/* Currently defined macros. */\n", DEST_OUT);
    for (symp = symtab; symp < &symtab[ SBSIZE]; symp++) {
        if ((dp = *symp) != NULL) {
            do {
                if (K_opt)
                    mcpp_fprintf( DEST_OUT, "/*m%s*/\n", dp->name);
                else
                    dump_a_def( NULL, dp, FALSE, comment, g_internal_data->fp_out);
            } while ((dp = dp->link) != NULL);
        }
    }
    g_internal_data->wrong_line = TRUE;               /* Line number is out of sync  */
}

#if MCPP_LIB
void    clear_symtable( void)
/*
 * Free all the macro definitions.
 */
{
    DEFBUF *    next;
    DEFBUF *    dp;
    DEFBUF **   symp;

    for (symp = symtab; symp < &symtab[ SBSIZE]; symp++) {
        for (next = *symp; next != NULL; ) {
            dp = next;
            next = dp->link;
            free( dp);                      /* Free the symbol      */
        }
        *symp = NULL;
    }
}
#endif

#if	defined(PK_COMPILER_MSVC)
#elif defined(PK_COMPILER_CLANG)
#	pragma clang diagnostic pop
#elif defined(PK_COMPILER_GCC)
#	pragma GCC diagnostic pop
#endif
