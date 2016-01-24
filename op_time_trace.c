/* ******************************************************************************
 * Copyright (c) 2014 Google, Inc.  All rights reserved.
 * Copyright (c) 2011 Massachusetts Institute of Technology  All rights reserved.
 * Copyright (c) 2008 VMware, Inc.  All rights reserved.
 * ******************************************************************************/

/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of VMware, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* Code Manipulation API Sample:
 * inscount.c
 *
 * Reports the dynamic count of the total number of instructions executed.
 * Illustrates how to perform performant clean calls.
 * Demonstrates effect of clean call optimization and auto-inlining with
 * different -opt_cleancall values.
 */

#include "dr_api.h"
#include "drmgr.h"
#include "drx.h"
#include "utils.h" //file usage


#ifdef WINDOWS
# define DISPLAY_STRING(msg) dr_messagebox(msg)
#else
# define DISPLAY_STRING(msg) dr_printf("%s\n", msg);
#endif

#define NULL_TERMINATE(buf) buf[(sizeof(buf)/sizeof(buf[0])) - 1] = '\0'


/* we only have a global count */
static uint64 global_count;
/* A simple clean call that will be automatically inlined because it has only
 * one argument and contains no calls to other functions.
 */
file_t  bbstructlog;
file_t  bbtrace;

static void inscount(void *tag) 
{ 
    //dr_printf("in dynamorio_basic_block(tag=0x%016lx, bb=0x%lx)\n", tag);
    dr_fprintf(bbtrace, "tag=0x%016lx\n", tag);
    //instr_t *instr = instrlist_first_app(bb);
    //uint seq = 0;
}

static void event_exit(void);
static dr_emit_flags_t event_bb_analysis(void *drcontext, void *tag,
                                         instrlist_t *bb,
                                         bool for_trace, bool translating,
                                         void **user_data);
static dr_emit_flags_t event_app_instruction(void *drcontext, void *tag,
                                             instrlist_t *bb, instr_t *inst,
                                             bool for_trace, bool translating,
                                             void *user_data);

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    dr_set_client_name("DynamoRIO Sample Client 'inscount'",
                       "http://dynamorio.org/issues");
    bbstructlog = log_file_open(id, NULL, NULL,
            "bb-struct-trace", 0);
    bbtrace = log_file_open(id, NULL, NULL,
            "bb-time-trace", 0); 
    DR_ASSERT(bbstructlog != INVALID_FILE);
    DR_ASSERT(bbtrace != INVALID_FILE); 
    drmgr_init();
    /* register events */
    dr_register_exit_event(event_exit);
    drmgr_register_bb_instrumentation_event(event_bb_analysis,
                                            event_app_instruction,
                                            NULL);

    /* make it easy to tell, by looking at log file, which client executed */
    dr_log(NULL, LOG_ALL, 1, "Client 'inscount' initializing\n");
#ifdef SHOW_RESULTS
    /* also give notification to stderr */
    if (dr_is_notify_on()) {
# ifdef WINDOWS
        /* ask for best-effort printing to cmd window.  must be called at init. */
        dr_enable_console_printing();
# endif
        dr_fprintf(STDERR, "Client inscount is running\n");
    }
#endif
}

static void
event_exit(void)
{
    dr_close_file(bbstructlog);
    dr_close_file(bbtrace);
    drmgr_exit();
}

static dr_emit_flags_t
event_bb_analysis(void *drcontext, void *tag, instrlist_t *bb,
                  bool for_trace, bool translating, void **user_data)
{
    instr_t *instr;
    uint num_instrs;

#ifdef VERBOSE
    dr_printf("in dynamorio_basic_block(tag="PFX")\n", tag);
# ifdef VERBOSE_VERBOSE
    instrlist_disassemble(drcontext, tag, bb, STDOUT);
# endif
#endif

    for (instr = instrlist_first_app(bb), num_instrs = 0;
         instr != NULL;
         instr = instr_get_next_app(instr)) {
        num_instrs++;
    }
    *user_data = (void *)(ptr_uint_t)num_instrs;

#if defined(VERBOSE) && defined(VERBOSE_VERBOSE)
    dr_printf("Finished counting for dynamorio_basic_block(tag="PFX")\n", tag);
    instrlist_disassemble(drcontext, tag, bb, STDOUT);
#endif
    return DR_EMIT_DEFAULT;
}

static dr_emit_flags_t
event_app_instruction(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr,
                      bool for_trace, bool translating, void *user_data)
{
    uint num_instrs;
    if (!drmgr_is_first_instr(drcontext, instr))
        return DR_EMIT_DEFAULT;
    num_instrs = (uint)(ptr_uint_t)user_data;
    dr_fprintf(bbstructlog, "instrument(tag="PFX", bb="PFX")\n", tag, bb);
    dr_insert_clean_call(drcontext, bb, instrlist_first_app(bb),
                         (void *)inscount, false /* save fpstate */, 1,
                         OPND_CREATE_INT64(tag));
    for(instr = instrlist_first_app(bb);
        instr != NULL;
        instr = instr_get_next_app(instr)) {
            //dr_printf("opcode %s\n", decode_opcode_name(instr_get_opcode(instr)));
            dr_fprintf(bbstructlog, "opcode %d\n", instr_get_opcode(instr));
    }
    return DR_EMIT_DEFAULT;
}
