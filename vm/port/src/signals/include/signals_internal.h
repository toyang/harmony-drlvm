/*
 *  Licensed to the Apache Software Foundation (ASF) under one or more
 *  contributor license agreements.  See the NOTICE file distributed with
 *  this work for additional information regarding copyright ownership.
 *  The ASF licenses this file to You under the Apache License, Version 2.0
 *  (the "License"); you may not use this file except in compliance with
 *  the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#ifndef _SIGNALS_INTERNAL_H_
#define _SIGNALS_INTERNAL_H_

#include "port_general.h"
#include "port_thread.h"
#include "open/platform_types.h"
#include "open/hythread_ext.h" /* For windows.h */


/* Thread-specific structure */
typedef struct
{
    /* Previous crash handling stage to restart crash processing */
    int     crash_stage;

#ifndef WIN32 /* UNIX */
    /* Flag and restart address for memory access violation detection */
    int     violation_flag;
    void*   restart_address;
#endif /* UNIX */

#ifdef WIN32
    Boolean assert_dialog;
#endif

    /* Flag to produce minidump/core on the second exception catch */
    Boolean   produce_core;

} port_tls_data;


#ifdef WIN32
typedef DWORD port_tls_key_t;
#else
typedef pthread_key_t port_tls_key_t;
#endif


#ifdef __cplusplus
extern "C" {
#endif


extern port_tls_key_t port_tls_key;

int init_private_tls_data();
int free_private_tls_data();

PORT_INLINE port_tls_data* get_private_tls_data()
{
#ifdef WIN32
        return (port_tls_data*)TlsGetValue(port_tls_key);
#else
        return (port_tls_data*)pthread_getspecific(port_tls_key);
#endif
}

PORT_INLINE int set_private_tls_data(port_tls_data* data)
{
#ifdef WIN32
        return TlsSetValue(port_tls_key, data) ? 0 : -1;
#else
        return pthread_setspecific(port_tls_key, data);
#endif
}


/* Transfer control to specified register context */
void port_transfer_to_regs(Registers* regs);

/**
* Prepares 'Registers' structure and stack area pointed in for calling
* 'fn' function with a set of arguments provided in variable args list.
* THe 'fn' function is called through a special stub function with
* preserving 'red zone' on Linux and clearing direction flag on Windows.
* After returning from 'fn' and stub, processor registers are restored
* with a values provided in 'regs' argument.
* The function can be used to prepare register context for transfering
* a control to a signal/exception handling function out of the OS handler.
*
* When the first argument passed to 'fn' is the same 'regs' pointer, its
* value is substituted with the pointer stored 'Registers' structure used
* to restore register context. If 'fn' function modifies the context
* pointed by the first argument, these changes will take effect after
* returning from 'fn'.
*
* The stub for calling 'fn' is written in assembler language; 'Registers'
* fields and size are hardcoded. It would be better to rewrite it using
* encoder in future, to keep control on 'Registers' structure and size.
*
* @param [in] fn    - the address of the function to be called
* @param [in] regs  - the register context
* @param [in] num   - the number of parameters passed to the 'fn' function
*                     in the variable args list (6 args at maximum)
* @param [in] ...   - the parameters for 'fn'; should all be void* or of
*                     the same size (pointer-sized)
*/
void port_set_longjump_regs(void* fn, Registers* regs, int num, ...);

/**
* The same as 'port_set_longjump_regs', but transfers a control to the
* prepared registers context by itself.
* Actually it's a combination of 'port_set_longjump_regs' and
* 'port_transfer_to_regs' functions, but 'regs' fields are kept unchanged.
*
* @param [in] fn    - the address of the function to be called
* @param [in] regs  - the register context
* @param [in] num   - the number of parameters passed to the 'fn' function
*                     in the variable args list (6 args at maximum)
* @param [in] ...   - the parameters for 'fn'; should all be void* or of
*                     the same size (pointer-sized)
*/
void port_transfer_to_function(void* fn, Registers* regs, int num, ...);



#define INSTRUMENTATION_BYTE_HLT 0xf4 // HLT instruction
#define INSTRUMENTATION_BYTE_CLI 0xfa // CLI instruction
#define INSTRUMENTATION_BYTE_INT3 0xcc // INT 3 instruction

#ifdef WINNT
#define INSTRUMENTATION_BYTE INSTRUMENTATION_BYTE_CLI
#else
#define INSTRUMENTATION_BYTE INSTRUMENTATION_BYTE_INT3
#endif


#ifdef WIN32

/**
 * Assembler wrapper for clearing CLD flag - bug in VEHs
 * appeared in debug prolog.
 */
LONG NTAPI vectored_exception_handler(LPEXCEPTION_POINTERS nt_exception);

/* Internal exception handler */
LONG NTAPI vectored_exception_handler_internal(LPEXCEPTION_POINTERS nt_exception);

#else /* UNIX */

//

#endif /* WIN32 */


#ifdef __cplusplus
}
#endif

#endif /* _SIGNALS_INTERNAL_H_ */
