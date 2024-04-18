/* mcpp_out.h: declarations of OUTDEST data types for MCPP  */
#ifndef _MCPP_OUT_H
#define _MCPP_OUT_H

/* Choices for output destination */
typedef enum {
    DEST_OUT,                        /* ~= fp_out    */
    DEST_ERR,                        /* ~= fp_err    */
    DEST_DBG,                        /* ~= fp_debug  */
    NUM_OUTDEST
} OUTDEST;

#endif  /* _MCPP_OUT_H  */
