/*
 *  Copyright 2005-2006 The Apache Software Foundation or its licensors, as applicable.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

/** 
 * @author Nikolay Kuznetsov
 * @version $Revision: 1.1.2.7 $
 */  

/**
 * @file thread_native_attrs.c
 * @brief Hythread priority related finctions
 */

#include <open/hythread_ext.h>
#include "thread_private.h"

/**
 * Set a thread's execution priority.
 * 
 * @param[in] thread a thread
 * @param[in] priority
 * Use the following symbolic constants for priorities:<br>
 *                              HYTHREAD_PRIORITY_MAX<br>
 *                              HYTHREAD_PRIORITY_USER_MAX<br>
 *                              HYTHREAD_PRIORITY_NORMAL<br>
 *                              HYTHREAD_PRIORITY_USER_MIN<br>
 *                              HYTHREAD_PRIORITY_MIN<br>
 * 
 * @returns 0 on success or negative value on failure (priority wasn't changed)
 * 
 * 
 */
IDATA VMCALL hythread_set_priority(hythread_t thread, UDATA priority){
    apr_status_t apr_status = apr_thread_set_priority(thread->os_handle, priority); 
        if (apr_status != APR_SUCCESS) return CONVERT_ERROR(apr_status);
    thread->priority = priority;
    return TM_ERROR_NONE;
}

/**
 * Returns the priority for the <code>thread</code>.
 *
 * @param[in] thread those attribute is read
 */
UDATA VMCALL hythread_get_priority(hythread_t thread) {
    return thread->priority;
}