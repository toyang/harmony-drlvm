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
#include "interpreter.h"
#include "interpreter_exports.h"
#include "interpreter_imports.h"

#include <math.h>

#include "exceptions.h"
#include "vm_arrays.h"
#include "vm_strings.h"
#include "jit_runtime_support_common.h"

#include "interp_native.h"
#include "interp_defs.h"
#include "interp_vm_helpers.h"
#include "ini.h"
#include "compile.h"
#include "open/thread.h"
#include "thread_manager.h"

// ppervov: HACK: allows using STL modifiers (dec/hex) and special constants (endl)
using namespace std;

bool interpreter_enable_debug = true;
int interpreter_enable_debug_trigger = -1;

const char *opcodeNames[256] = {
    "NOP", "ACONST_NULL", "ICONST_M1", "ICONST_0", "ICONST_1", "ICONST_2",
    "ICONST_3", "ICONST_4", "ICONST_5", "LCONST_0", "LCONST_1", "FCONST_0",
    "FCONST_1", "FCONST_2", "DCONST_0", "DCONST_1", "BIPUSH", "SIPUSH",
    "LDC", "LDC_W", "LDC2_W", "ILOAD", "LLOAD", "FLOAD", "DLOAD", "ALOAD",
    "ILOAD_0", "ILOAD_1", "ILOAD_2", "ILOAD_3", "LLOAD_0", "LLOAD_1", "LLOAD_2",
    "LLOAD_3", "FLOAD_0", "FLOAD_1", "FLOAD_2", "FLOAD_3", "DLOAD_0", "DLOAD_1",
    "DLOAD_2", "DLOAD_3", "ALOAD_0", "ALOAD_1", "ALOAD_2", "ALOAD_3", "IALOAD",
    "LALOAD", "FALOAD", "DALOAD", "AALOAD", "BALOAD", "CALOAD", "SALOAD",
    "ISTORE", "LSTORE", "FSTORE", "DSTORE", "ASTORE", "ISTORE_0", "ISTORE_1",
    "ISTORE_2", "ISTORE_3", "LSTORE_0", "LSTORE_1", "LSTORE_2", "LSTORE_3",
    "FSTORE_0", "FSTORE_1", "FSTORE_2", "FSTORE_3", "DSTORE_0", "DSTORE_1",
    "DSTORE_2", "DSTORE_3", "ASTORE_0", "ASTORE_1", "ASTORE_2", "ASTORE_3",
    "IASTORE", "LASTORE", "FASTORE", "DASTORE", "AASTORE", "BASTORE", "CASTORE",
    "SASTORE", "POP", "POP2", "DUP", "DUP_X1", "DUP_X2", "DUP2", "DUP2_X1",
    "DUP2_X2", "SWAP", "IADD", "LADD", "FADD", "DADD", "ISUB", "LSUB", "FSUB",
    "DSUB", "IMUL", "LMUL", "FMUL", "DMUL", "IDIV", "LDIV", "FDIV", "DDIV",
    "IREM", "LREM", "FREM", "DREM", "INEG", "LNEG", "FNEG", "DNEG", "ISHL",
    "LSHL", "ISHR", "LSHR", "IUSHR", "LUSHR", "IAND", "LAND", "IOR", "LOR",
    "IXOR", "LXOR", "IINC", "I2L", "I2F", "I2D", "L2I", "L2F", "L2D", "F2I",
    "F2L", "F2D", "D2I", "D2L", "D2F", "I2B", "I2C", "I2S", "LCMP", "FCMPL",
    "FCMPG", "DCMPL", "DCMPG", "IFEQ", "IFNE", "IFLT", "IFGE", "IFGT", "IFLE",
    "IF_ICMPEQ", "IF_ICMPNE", "IF_ICMPLT", "IF_ICMPGE", "IF_ICMPGT",
    "IF_ICMPLE", "IF_ACMPEQ", "IF_ACMPNE", "GOTO", "JSR", "RET", "TABLESWITCH",
    "LOOKUPSWITCH", "IRETURN", "LRETURN", "FRETURN", "DRETURN", "ARETURN",
    "RETURN", "GETSTATIC", "PUTSTATIC", "GETFIELD", "PUTFIELD", "INVOKEVIRTUAL",
    "INVOKESPECIAL", "INVOKESTATIC", "INVOKEINTERFACE", "_OPCODE_UNDEFINED",
    "NEW", "NEWARRAY", "ANEWARRAY", "ARRAYLENGTH", "ATHROW", "CHECKCAST",
    "INSTANCEOF", "MONITORENTER", "MONITOREXIT", "WIDE", "MULTIANEWARRAY",
    "IFNULL", "IFNONNULL", "GOTO_W", "JSR_W",

    "BREAKPOINT",
    "UNKNOWN_0xCB", "UNKNOWN_0xCC", "UNKNOWN_0xCD", "UNKNOWN_0xCE",
    "UNKNOWN_0xCF", "UNKNOWN_0xD0", "UNKNOWN_0xD1", "UNKNOWN_0xD2",
    "UNKNOWN_0xD3", "UNKNOWN_0xD4", "UNKNOWN_0xD5", "UNKNOWN_0xD6",
    "UNKNOWN_0xD7", "UNKNOWN_0xD8", "UNKNOWN_0xD9", "UNKNOWN_0xDA",
    "UNKNOWN_0xDB", "UNKNOWN_0xDC", "UNKNOWN_0xDD", "UNKNOWN_0xDE",
    "UNKNOWN_0xDF", "UNKNOWN_0xE0", "UNKNOWN_0xE1", "UNKNOWN_0xE2",
    "UNKNOWN_0xE3", "UNKNOWN_0xE4", "UNKNOWN_0xE5", "UNKNOWN_0xE6",
    "UNKNOWN_0xE7", "UNKNOWN_0xE8", "UNKNOWN_0xE9", "UNKNOWN_0xEA",
    "UNKNOWN_0xEB", "UNKNOWN_0xEC", "UNKNOWN_0xED", "UNKNOWN_0xEE",
    "UNKNOWN_0xEF", "UNKNOWN_0xF0", "UNKNOWN_0xF1", "UNKNOWN_0xF2",
    "UNKNOWN_0xF3", "UNKNOWN_0xF4", "UNKNOWN_0xF5", "UNKNOWN_0xF6",
    "UNKNOWN_0xF7", "UNKNOWN_0xF8", "UNKNOWN_0xF9", "UNKNOWN_0xFA",
    "UNKNOWN_0xFB", "UNKNOWN_0xFC", "UNKNOWN_0xFD", "UNKNOWN_0xFE",
    "UNKNOWN_0xFF"
};


static void interpreterInvokeStatic(StackFrame& prevFrame, Method *method);
static void interpreterInvokeVirtual(StackFrame& prevFrame, Method *method);
static void interpreterInvokeInterface(StackFrame& prevFrame, Method *method);
static void interpreterInvokeSpecial(StackFrame& prevFrame, Method *method);

/****************************************************/
/*** INLINE FUNCTIONS *******************************/
/****************************************************/

static inline int16
read_uint8(uint8 *addr) {
    return *addr;
}

static inline int16
read_int8(uint8 *addr) {
    return ((int8*)addr)[0];
}

static inline int16
read_int16(uint8 *addr) {
    int res = (((int8*)addr)[0] << 8);
    return (int16)(res + (int)addr[1]);
}

static inline uint16
read_uint16(uint8 *addr) {
    return (int16)((addr[0] << 8) + addr[1]);
}

static inline int32
read_int32(uint8 *addr) {
    uint32 res = (addr[0] << 24) + (addr[1] << 16) + (addr[2] << 8) + addr[3];
    return (int32) res;
}


static void throwAIOOBE(int32 index) {
    char buf[64];
    sprintf(buf, "%i", index);
    DEBUG("****** ArrayIndexOutOfBoundsException ******\n");
    interp_throw_exception("java/lang/ArrayIndexOutOfBoundsException", buf);
}

static void throwNPE() {
    DEBUG("****** NullPointerException ******\n");
    interp_throw_exception("java/lang/NullPointerException");
}

static void throwAME() {
    DEBUG("****** AbstractMethodError ******\n");
    interp_throw_exception("java/lang/AbstractMethodError");
}

static void throwIAE() {
    DEBUG("****** IllegalAccessError ******\n");
    interp_throw_exception("java/lang/IllegalAccessError");
}

static inline void
Opcode_NOP(StackFrame& frame) {
    frame.ip++;
}

static inline void
Opcode_ACONST_NULL(StackFrame& frame) {
    frame.stack.push();
    frame.stack.pick().u = 0;
    frame.stack.ref() = FLAG_OBJECT;
    frame.ip++;
}

#define DEF_OPCODE_CONST32_N(T,V,t,VAL)                 \
static inline void                                      \
Opcode_ ## T ## CONST_ ## V(StackFrame& frame) {        \
    frame.stack.push();                                 \
    frame.stack.pick().t = VAL;                         \
    frame.ip++;                                         \
}

DEF_OPCODE_CONST32_N(I,M1,i,-1) // Opcode_ICONST_M1
DEF_OPCODE_CONST32_N(I,0,i,0)   // Opcode_ICONST_0
DEF_OPCODE_CONST32_N(I,1,i,1)   // Opcode_ICONST_1
DEF_OPCODE_CONST32_N(I,2,i,2)   // Opcode_ICONST_2
DEF_OPCODE_CONST32_N(I,3,i,3)   // Opcode_ICONST_3
DEF_OPCODE_CONST32_N(I,4,i,4)   // Opcode_ICONST_4
DEF_OPCODE_CONST32_N(I,5,i,5)   // Opcode_ICONST_5
DEF_OPCODE_CONST32_N(F,0,f,0)   // Opcode_FCONST_0
DEF_OPCODE_CONST32_N(F,1,f,1)   // Opcode_FCONST_1
DEF_OPCODE_CONST32_N(F,2,f,2)   // Opcode_FCONST_2

#define DEF_OPCODE_CONST64_N(T,V,t)                         \
static inline void                                          \
Opcode_ ## T ## V(StackFrame& frame) {                      \
    frame.stack.push(2);                                    \
    Value2 val;                                             \
    val.t = V;                                              \
    frame.stack.setLong(0, val);                            \
    frame.ip++;                                             \
}

DEF_OPCODE_CONST64_N(LCONST_,0,i64) // Opcode_LCONST_0
DEF_OPCODE_CONST64_N(LCONST_,1,i64) // Opcode_LCONST_1
DEF_OPCODE_CONST64_N(DCONST_,0,d)   // Opcode_DCONST_0
DEF_OPCODE_CONST64_N(DCONST_,1,d)   // Opcode_DCONST_0



static inline void
Opcode_BIPUSH(StackFrame& frame) {
    frame.stack.push();
    frame.stack.pick().i = read_int8(frame.ip + 1);
    DEBUG_BYTECODE("push " << frame.stack.pick().i);
    frame.ip += 2;
}

static inline void
Opcode_SIPUSH(StackFrame& frame) {
    frame.stack.push();
    frame.stack.pick().i = read_int16(frame.ip + 1);
    DEBUG_BYTECODE("push " << frame.stack.pick().i);
    frame.ip += 3;
}

static inline void
Opcode_ALOAD(StackFrame& frame) {
    // get value from local variable
    uint32 varId = read_uint8(frame.ip + 1);
    Value& val = frame.locals(varId);
    ASSERT_TAGS(frame.locals.ref(varId));
   
    // store value in operand stack
    frame.stack.push();
    frame.stack.pick() = val;
    frame.stack.ref() = frame.locals.ref(varId);
    if (frame.locals.ref(varId) == FLAG_OBJECT) { ASSERT_OBJECT(UNCOMPRESS_REF(val.cr)); }
    DEBUG_BYTECODE("var" << (int)varId << " -> stack (val = " << (int)frame.stack.pick().i << ")");
    frame.ip += 2;
}

static inline void
Opcode_WIDE_ALOAD(StackFrame& frame) {
    // get value from local variable
    uint32 varId = read_uint16(frame.ip + 2);
    Value& val = frame.locals(varId);
    ASSERT_TAGS(frame.locals.ref(varId));
   
    // store value in operand stack
    frame.stack.push();
    frame.stack.pick() = val;
    frame.stack.ref() = frame.locals.ref(varId);
    DEBUG_BYTECODE("var" << (int)varId << " -> stack (val = " << (int)frame.stack.pick().i << ")");
    frame.ip += 4;
}

static inline void
Opcode_ILOAD(StackFrame& frame) {
    // get value from local variable
    uint32 varId = read_uint8(frame.ip + 1);
    Value& val = frame.locals(varId);
    ASSERT_TAGS(!frame.locals.ref(varId));
   
    // store value in operand stack
    frame.stack.push();
    frame.stack.pick() = val;
    DEBUG_BYTECODE("var" << (int)varId << " -> stack (val = " << (int)frame.stack.pick().i << ")");
    frame.ip += 2;
}

static inline void
Opcode_WIDE_ILOAD(StackFrame& frame) {
    // get value from local variable
    uint32 varId = read_uint16(frame.ip + 2);
    Value& val = frame.locals(varId);
    ASSERT_TAGS(!frame.locals.ref(varId));
   
    // store value in operand stack
    frame.stack.push();
    frame.stack.pick() = val;
    DEBUG_BYTECODE("var" << (int)varId << " -> stack (val = " << (int)frame.stack.pick().i << ")");
    frame.ip += 4;
}

#define DEF_OPCODE_ALOAD_N(N)           \
static inline void                      \
Opcode_ALOAD_ ## N(StackFrame& frame) { \
    Value& val = frame.locals(N);       \
    ASSERT_TAGS(frame.locals.ref(N));   \
    frame.stack.push();                 \
    frame.stack.pick() = val;           \
    frame.stack.ref() = FLAG_OBJECT;               \
    frame.stack.ref() = frame.locals.ref(N);\
    DEBUG_BYTECODE("var" #N " -> stack (val = " << (int)frame.stack.pick().i << ")"); \
    frame.ip++;                         \
}

DEF_OPCODE_ALOAD_N(0) // Opcode_ALOAD_0
DEF_OPCODE_ALOAD_N(1) // Opcode_ALOAD_1
DEF_OPCODE_ALOAD_N(2) // Opcode_ALOAD_2
DEF_OPCODE_ALOAD_N(3) // Opcode_ALOAD_3

#define DEF_OPCODE_ILOAD_N(N)           \
static inline void                      \
Opcode_ILOAD_ ## N(StackFrame& frame) { \
    Value& val = frame.locals(N);       \
    ASSERT_TAGS(!frame.locals.ref(N));  \
    frame.stack.push();                 \
    frame.stack.pick() = val;           \
    DEBUG_BYTECODE("var" #N " -> stack (val = " << (int)frame.stack.pick().i << ")"); \
    frame.ip++;                         \
}

DEF_OPCODE_ILOAD_N(0) // Opcode_ILOAD_0
DEF_OPCODE_ILOAD_N(1) // Opcode_ILOAD_1
DEF_OPCODE_ILOAD_N(2) // Opcode_ILOAD_2
DEF_OPCODE_ILOAD_N(3) // Opcode_ILOAD_3

static inline void
Opcode_LLOAD(StackFrame& frame) {
    // store value to local variable
    uint32 varId = read_uint8(frame.ip + 1);

    frame.stack.push(2);
    frame.stack.pick(s1) = frame.locals(varId + l1);
    frame.stack.pick(s0) = frame.locals(varId + l0);
    ASSERT_TAGS(!frame.locals.ref(varId + 0));
    ASSERT_TAGS(!frame.locals.ref(varId + 1));

    DEBUG_BYTECODE("stack -> var" << (int)varId);
    frame.ip += 2;
}

static inline void
Opcode_WIDE_LLOAD(StackFrame& frame) {
    // store value to local variable
    uint32 varId = read_uint16(frame.ip + 2);

    frame.stack.push(2);
    frame.stack.pick(s1) = frame.locals(varId + l1);
    frame.stack.pick(s0) = frame.locals(varId + l0);
    ASSERT_TAGS(!frame.locals.ref(varId + 0));
    ASSERT_TAGS(!frame.locals.ref(varId + 1));

    DEBUG_BYTECODE("stack -> var" << (int)varId);
    frame.ip += 4;
}

#define DEF_OPCODE_LLOAD_N(N)                           \
static inline void                                      \
Opcode_LLOAD_ ## N(StackFrame& frame) {                 \
    frame.stack.push(2);                                \
    frame.stack.pick(s1) = frame.locals(N + l1);        \
    frame.stack.pick(s0) = frame.locals(N + l0);        \
    ASSERT_TAGS(!frame.locals.ref(N + 0));              \
    ASSERT_TAGS(!frame.locals.ref(N + 1));              \
                                                        \
    DEBUG_BYTECODE("var" #N " -> stack");                        \
    frame.ip++;                                         \
}

DEF_OPCODE_LLOAD_N(0) // Opcode_LLOAD_0
DEF_OPCODE_LLOAD_N(1) // Opcode_LLOAD_1
DEF_OPCODE_LLOAD_N(2) // Opcode_LLOAD_2
DEF_OPCODE_LLOAD_N(3) // Opcode_LLOAD_3


    
static inline void
Opcode_ASTORE(StackFrame& frame) {
    Value &val = frame.stack.pick();
    ASSERT_TAGS(frame.stack.ref());

    // store value to local variable
    uint32 varId = read_uint8(frame.ip + 1);
    frame.locals(varId) = val;
    frame.locals.ref(varId) = frame.stack.ref();

    frame.stack.ref() = FLAG_NONE;
    frame.stack.pop();
    DEBUG_BYTECODE("stack -> var" << (int)varId);
    frame.ip += 2;
}

static inline void
Opcode_ISTORE(StackFrame& frame) {
    Value &val = frame.stack.pick();
    ASSERT_TAGS(!frame.stack.ref());

    // store value to local variable
    uint32 varId = read_uint8(frame.ip + 1);
    frame.locals(varId) = val;
    frame.locals.ref(varId) = FLAG_NONE;

    frame.stack.pop();
    DEBUG_BYTECODE("stack -> var" << (int)varId);
    frame.ip += 2;
}

static inline void
Opcode_WIDE_ASTORE(StackFrame& frame) {
    Value &val = frame.stack.pick();
    ASSERT_TAGS(frame.stack.ref());

    // store value to local variable
    uint32 varId = read_uint16(frame.ip + 2);

    frame.locals(varId) = val;
    frame.locals.ref(varId) = frame.stack.ref();

    frame.stack.ref() = FLAG_NONE;
    frame.stack.pop();
    DEBUG_BYTECODE("stack -> var" << (int)varId);
    frame.ip += 4;
}

static inline void
Opcode_WIDE_ISTORE(StackFrame& frame) {
    Value &val = frame.stack.pick();
    ASSERT_TAGS(!frame.stack.ref());

    // store value to local variable
    uint32 varId = read_uint16(frame.ip + 2);
    frame.locals(varId) = val;
    frame.locals.ref(varId) = FLAG_NONE;

    frame.stack.pop();
    DEBUG_BYTECODE("stack -> var" << (int)varId);
    frame.ip += 4;
}

#define DEF_OPCODE_ASTORE_N(N)                      \
static inline void                                  \
Opcode_ASTORE_##N(StackFrame& frame) {              \
    Value& val = frame.stack.pick();                \
    ASSERT_TAGS(frame.stack.ref());                 \
    frame.locals(N) = val;                          \
    frame.locals.ref(N) = frame.stack.ref();        \
    frame.stack.ref() = FLAG_NONE;                         \
    frame.stack.pop();                              \
    DEBUG_BYTECODE("stack -> var" #N );                      \
    frame.ip++;                                     \
}

DEF_OPCODE_ASTORE_N(0) // Opcode_ASTORE_0
DEF_OPCODE_ASTORE_N(1) // Opcode_ASTORE_1
DEF_OPCODE_ASTORE_N(2) // Opcode_ASTORE_2
DEF_OPCODE_ASTORE_N(3) // Opcode_ASTORE_3

#define DEF_OPCODE_ISTORE_N(N)                      \
static inline void                                  \
Opcode_ISTORE_##N(StackFrame& frame) {              \
    Value& val = frame.stack.pick();                \
    ASSERT_TAGS(!frame.stack.ref());                \
    frame.locals(N) = val;                          \
    frame.locals.ref(N) = FLAG_NONE;                \
    frame.stack.pop();                              \
    DEBUG_BYTECODE("stack -> var" #N );                      \
    frame.ip++;                                     \
}

DEF_OPCODE_ISTORE_N(0) // Opcode_ASTORE_0
DEF_OPCODE_ISTORE_N(1) // Opcode_ASTORE_1
DEF_OPCODE_ISTORE_N(2) // Opcode_ASTORE_2
DEF_OPCODE_ISTORE_N(3) // Opcode_ASTORE_3

static inline void
Opcode_LSTORE(StackFrame& frame) {
    uint32 varId = read_uint8(frame.ip + 1);
    ASSERT_TAGS(!frame.stack.ref(0));
    ASSERT_TAGS(!frame.stack.ref(1));
    frame.locals(varId + l1) = frame.stack.pick(s1);
    frame.locals(varId + l0) = frame.stack.pick(s0);
    frame.locals.ref(varId + 0) = FLAG_NONE;
    frame.locals.ref(varId + 1) = FLAG_NONE;
    frame.stack.pop(2);
    DEBUG_BYTECODE("stack -> var" << (int)varId);
    frame.ip += 2;
}

static inline void
Opcode_WIDE_LSTORE(StackFrame& frame) {
    uint32 varId = read_uint16(frame.ip + 2);
    ASSERT_TAGS(!frame.stack.ref(0));
    ASSERT_TAGS(!frame.stack.ref(1));
    frame.locals(varId + l1) = frame.stack.pick(s1);
    frame.locals(varId + l0) = frame.stack.pick(s0);
    frame.locals.ref(varId + 0) = FLAG_NONE;
    frame.locals.ref(varId + 1) = FLAG_NONE;
    frame.stack.pop(2);
    DEBUG_BYTECODE("stack -> var" << (int)varId);
    frame.ip += 4;
}

#define DEF_OPCODE_LSTORE_N(N)                          \
static inline void                                      \
Opcode_LSTORE_ ## N(StackFrame& frame) {                \
    ASSERT_TAGS(!frame.stack.ref(0));                   \
    ASSERT_TAGS(!frame.stack.ref(1));                   \
    frame.locals(N + l1) = frame.stack.pick(s1);        \
    frame.locals(N + l0) = frame.stack.pick(s0);        \
    frame.locals.ref(N + 0) = FLAG_NONE;                \
    frame.locals.ref(N + 1) = FLAG_NONE;                \
    frame.stack.pop(2);                                 \
    DEBUG_BYTECODE("stack -> var" #N);                  \
    frame.ip++;                                         \
}

DEF_OPCODE_LSTORE_N(0) // Opcode_LSTORE_0
DEF_OPCODE_LSTORE_N(1) // Opcode_LSTORE_1
DEF_OPCODE_LSTORE_N(2) // Opcode_LSTORE_2
DEF_OPCODE_LSTORE_N(3) // Opcode_LSTORE_3

#if defined (__INTEL_COMPILER)
#pragma warning(push) 
#pragma warning (disable:1572)    // conversion from pointer to same-sized integral type (potential portability problem)
#endif

static inline int
fcmpl(float a, float b) {
    if (a > b) return 1;
    if (a == b) return 0;
    return -1;
}

static inline int
fcmpg(float a, float b) {
    if (a < b) return -1;
    if (a == b) return 0;
    return 1;
}

static inline int
dcmpl(double a, double b) {
    if (a > b) return 1;
    if (a == b) return 0;
    return -1;
}

static inline int
dcmpg(double a, double b) {
    if (a < b) return -1;
    if (a == b) return 0;
    return 1;
}
#if defined (__INTEL_COMPILER)
#pragma warning(pop) 
#endif


#define DEF_OPCODE_MATH_32_32_TO_32(CODE, expr)                 \
static inline void                                              \
Opcode_##CODE(StackFrame& frame) {                              \
    Value& arg0 = frame.stack.pick(0);                          \
    Value& arg1 = frame.stack.pick(1);                          \
    Value& res = arg1;                                          \
    DEBUG_BYTECODE("(" << arg1.i << ", ");                               \
    expr;                                                       \
    DEBUG_BYTECODE(arg0.i << " ) = " << res.i);                          \
    frame.stack.pop();                                          \
    frame.ip++;                                                 \
}

#define DEF_OPCODE_MATH_32_TO_32(CODE, expr)                    \
static inline void                                              \
Opcode_## CODE(StackFrame& frame) {                             \
    Value& arg = frame.stack.pick(0);                           \
    Value& res = arg;                                           \
    expr;                                                       \
    frame.ip++;                                                 \
}

#define DEF_OPCODE_MATH_32_TO_64(OPCODE, expr)                  \
static inline void                                              \
Opcode_ ## OPCODE(StackFrame& frame) {                          \
    Value& arg = frame.stack.pick();                            \
    Value2 res;                                                 \
    expr;                                                       \
    frame.stack.push();                                         \
    frame.stack.setLong(0, res);                                \
    frame.ip++;                                                 \
}

#define DEF_OPCODE_MATH_64_TO_64(OPCODE, expr)                  \
static inline void                                              \
Opcode_ ## OPCODE(StackFrame& frame) {                          \
    Value2 arg, res;                                            \
    arg = frame.stack.getLong(0);                               \
    expr;                                                       \
    frame.stack.setLong(0, res);                                \
    frame.ip++;                                                 \
}

#define DEF_OPCODE_MATH_64_64_TO_64(CODE, expr)                 \
static inline void                                              \
        Opcode_ ## CODE(StackFrame& frame) {                    \
    Value2 arg0, arg1, res;                                     \
    arg0 = frame.stack.getLong(0);                              \
    arg1 = frame.stack.getLong(2);                              \
    frame.stack.pop(2);                                         \
    expr;                                                       \
    frame.stack.setLong(0, res);                                \
    DEBUG_BYTECODE("(" << arg0.d << ", " << arg1.d << ") = " << res.d);  \
    frame.ip++;                                                 \
}

#define DEF_OPCODE_MATH_64_64_TO_32(CODE, expr)                 \
static inline void                                              \
        Opcode_ ## CODE(StackFrame& frame) {                    \
    Value2 arg0, arg1;                                          \
    Value res;                                                  \
    arg0 = frame.stack.getLong(0);                              \
    arg1 = frame.stack.getLong(2);                              \
    frame.stack.pop(3);                                         \
    expr;                                                       \
    frame.stack.pick() = res;                                   \
    DEBUG_BYTECODE("(" << arg0.d << ", " << arg1.d << ") = " << res.i);  \
    frame.ip++;                                                 \
}

#define DEF_OPCODE_MATH_64_TO_32(CODE,expr)                     \
static inline void                                              \
Opcode_ ## CODE(StackFrame& frame) {                            \
    Value2 arg;                                                 \
    Value res;                                                  \
    arg = frame.stack.getLong(0);                               \
    expr;                                                       \
    frame.stack.pop();                                          \
    frame.stack.pick() = res;                                   \
    frame.ip++;                                                 \
}

#define DEF_OPCODE_MATH_64_32_TO_64(CODE, expr)                 \
static inline void                                              \
Opcode_ ## CODE(StackFrame& frame) {                            \
    Value arg0;                                                 \
    Value2 arg1, res;                                           \
    arg0 = frame.stack.pick(0);                                 \
    arg1 = frame.stack.getLong(1);                              \
    frame.stack.pop();                                          \
    expr;                                                       \
    frame.stack.setLong(0, res);                                \
    frame.ip++;                                                 \
}


DEF_OPCODE_MATH_32_32_TO_32(IADD,  res.i = arg1.i + arg0.i)   // Opcode_IADD
DEF_OPCODE_MATH_32_32_TO_32(ISUB,  res.i = arg1.i - arg0.i)   // Opcode_ISUB
DEF_OPCODE_MATH_32_32_TO_32(IMUL,  res.i = arg1.i * arg0.i)   // Opcode_IMUL
DEF_OPCODE_MATH_32_32_TO_32(IOR,   res.i = arg1.i | arg0.i)    // Opcode_IOR
DEF_OPCODE_MATH_32_32_TO_32(IAND,  res.i = arg1.i & arg0.i)  // Opcode_IAND
DEF_OPCODE_MATH_32_32_TO_32(IXOR,  res.i = arg1.i ^ arg0.i)  // Opcode_IXOR
DEF_OPCODE_MATH_32_32_TO_32(ISHL,  res.i = arg1.i << (arg0.i & 0x1f)) // Opcode_ISHL
DEF_OPCODE_MATH_32_32_TO_32(ISHR,  res.i = arg1.i >> (arg0.i & 0x1f)) // Opcode_ISHR
DEF_OPCODE_MATH_32_32_TO_32(IUSHR, res.i = ((uint32)arg1.i) >> (arg0.i & 0x1f))   // Opcode_IUSHR

DEF_OPCODE_MATH_32_32_TO_32(FADD,  res.f = arg1.f + arg0.f)   // Opcode_FADD
DEF_OPCODE_MATH_32_32_TO_32(FSUB,  res.f = arg1.f - arg0.f)   // Opcode_FSUB
DEF_OPCODE_MATH_32_32_TO_32(FMUL,  res.f = arg1.f * arg0.f)   // Opcode_FMUL
DEF_OPCODE_MATH_32_32_TO_32(FDIV,  res.f = arg1.f / arg0.f)   // Opcode_FDIV
// FIXME: is it correct? bitness is ok?
DEF_OPCODE_MATH_32_32_TO_32(FREM,  res.f = fmodf(arg1.f, arg0.f)) // Opcode_FREM

DEF_OPCODE_MATH_64_64_TO_64(LADD,  res.i64 = arg1.i64 + arg0.i64)
DEF_OPCODE_MATH_64_64_TO_64(LSUB,  res.i64 = arg1.i64 - arg0.i64)
DEF_OPCODE_MATH_64_64_TO_64(LMUL,  res.i64 = arg1.i64 * arg0.i64)
DEF_OPCODE_MATH_64_64_TO_64(LOR,   res.i64 = arg1.i64 | arg0.i64)
DEF_OPCODE_MATH_64_64_TO_64(LAND,  res.i64 = arg1.i64 & arg0.i64)
DEF_OPCODE_MATH_64_64_TO_64(LXOR,  res.i64 = arg1.i64 ^ arg0.i64)

DEF_OPCODE_MATH_64_64_TO_64(DADD, res.d = arg1.d + arg0.d)
DEF_OPCODE_MATH_64_64_TO_64(DSUB, res.d = arg1.d - arg0.d)
DEF_OPCODE_MATH_64_64_TO_64(DMUL, res.d = arg1.d * arg0.d)
DEF_OPCODE_MATH_64_64_TO_64(DDIV, res.d = arg1.d / arg0.d)
DEF_OPCODE_MATH_64_64_TO_64(DREM, res.d = fmod(arg1.d, arg0.d))

DEF_OPCODE_MATH_64_32_TO_64(LSHL, res.i64 = arg1.i64 << (arg0.i & 0x3f))
DEF_OPCODE_MATH_64_32_TO_64(LSHR, res.i64 = arg1.i64 >> (arg0.i & 0x3f))
DEF_OPCODE_MATH_64_32_TO_64(LUSHR, res.i64 = ((uint64)arg1.i64) >> (arg0.i & 0x3f))   // Opcode_LUSHR

DEF_OPCODE_MATH_32_32_TO_32(FCMPL, res.i = fcmpl(arg1.f, arg0.f)) // Opcode_FCMPL
DEF_OPCODE_MATH_32_32_TO_32(FCMPG, res.i = fcmpg(arg1.f, arg0.f)) // Opcode_FCMPG
DEF_OPCODE_MATH_64_64_TO_32(DCMPL, res.i = dcmpl(arg1.d, arg0.d)) // Opcode_FCMPL
DEF_OPCODE_MATH_64_64_TO_32(DCMPG, res.i = dcmpg(arg1.d, arg0.d)) // Opcode_FCMPG


DEF_OPCODE_MATH_32_TO_32(INEG, res.i = -arg.i)     // Opcode_INEG
DEF_OPCODE_MATH_32_TO_32(FNEG, res.f = -arg.f)     // Opcode_FNEG
DEF_OPCODE_MATH_64_TO_64(LNEG, res.i64 = -arg.i64) // Opcode_LNEG
DEF_OPCODE_MATH_64_TO_64(DNEG, res.d = -arg.d)     // Opcode_DNEG


DEF_OPCODE_MATH_32_TO_32(I2F, res.f = (float) arg.i)     // Opcode_I2F
//DEF_OPCODE_MATH_32_TO_32(F2I, res.i = (int32) arg.f)     // Opcode_F2I
DEF_OPCODE_MATH_32_TO_32(I2B, res.i = (int8) arg.i)      // Opcode_I2B
DEF_OPCODE_MATH_32_TO_32(I2S, res.i = (int16) arg.i)     // Opcode_I2S
DEF_OPCODE_MATH_32_TO_32(I2C, res.i = (uint16) arg.i)    // Opcode_I2C

DEF_OPCODE_MATH_32_TO_64(I2L, res.i64 = (int64) arg.i)   // Opcode_I2L
DEF_OPCODE_MATH_32_TO_64(I2D, res.d = (double) arg.i)    // Opcode_I2D
//DEF_OPCODE_MATH_32_TO_64(F2L, res.i64 = (int64) arg.f)   // Opcode_F2L
DEF_OPCODE_MATH_32_TO_64(F2D, res.d = (double) arg.f)    // Opcode_F2D

DEF_OPCODE_MATH_64_TO_64(L2D, res.d = (double) arg.i64)  // Opcode_L2D
//DEF_OPCODE_MATH_64_TO_64(D2L, res.i64 = (int64) arg.d)   // Opcode_D2L

#if defined (__INTEL_COMPILER)
#pragma warning( push )
#pragma warning (disable:1683) // to get rid of remark #1683: explicit conversion of a 64-bit integral type to a smaller integral type
#endif

DEF_OPCODE_MATH_64_TO_32(D2F, res.f = (float) arg.d)     // Opcode_D2F
//DEF_OPCODE_MATH_64_TO_32(D2I, res.i = (int32) arg.d)     // Opcode_D2I
DEF_OPCODE_MATH_64_TO_32(L2F, res.f = (float) arg.i64)   // Opcode_L2F
DEF_OPCODE_MATH_64_TO_32(L2I, res.i = (int32) arg.i64)   // Opcode_L2I

#if defined (__INTEL_COMPILER)
#pragma warning( pop )
#endif

static inline void
Opcode_D2I(StackFrame& frame) {
    Value2 arg;
    Value res;
    arg = frame.stack.getLong(0);
    if (arg.d < 2147483647.) {
        if (arg.d > -2147483648.) {
            res.i = (int32) arg.d;
        } else {
            res.i = (int32)2147483648u;
        }
    } else {
        if (arg.d >= 2147483647.) {
            // +inf
            res.i = (int32)2147483647;
        } else {
            // nan
            res.i = 0;
        }
    }
    frame.stack.pop();
    frame.stack.pick() = res;
    frame.ip++;
}

static inline void
Opcode_F2I(StackFrame& frame) {
    Value& arg = frame.stack.pick(0);

    if (arg.f < 2147483647.f) {
        if (arg.f > -2147483648.f) {
            arg.i = (int32) arg.f;
        } else {
            arg.i = (int32)2147483648u;
        }
    } else {
        if (arg.f >= 2147483647.f) {
            // +inf
            arg.i = (int32)2147483647;
        } else {
            // nan
            arg.i = 0;
        }
    }
    frame.ip++;
}

static inline void
Opcode_D2L(StackFrame& frame) {
    Value2 arg;
    Value2 res;
    arg = frame.stack.getLong(0);
    if (arg.d <= 4611686018427387903.) {
        if (arg.d >= -4611686018427387904.) {
            res.i64 = (int64) arg.d;
        } else {
            res.i64 = ((int64)-1) << 63; // 80000......
        }
    } else {
        if (arg.d > 4611686018427387903.) {
            // +inf
            res.i64 = -((((int64)-1) << 63) + 1); // 7FFFF.....
        } else {
            // nan
            res.i64 = 0;
        }
    }
    frame.stack.setLong(0, res);
    frame.ip++;
}

static inline void
Opcode_F2L(StackFrame& frame) {
    Value arg;
    Value2 res;
    arg = frame.stack.pick(0);
    if (arg.f <= 4611686018427387903.) {
        if (arg.f >= -4611686018427387904.) {
            res.i64 = (int64) arg.f;
        } else {
            res.i64 = ((int64)-1) << 63; // 80000......
        }
    } else {
        if (arg.f > 4611686018427387903.) {
            // +inf
            res.i64 = -((((int64)-1) << 63) + 1); // 7FFFF.....
        } else {
            // nan
            res.i64 = 0;
        }
    }
    frame.stack.push();
    frame.stack.setLong(0, res);
    frame.ip++;
}

#define DEF_OPCODE_DIV_32_32_TO_32(CODE, expr)                  \
static inline void                                              \
Opcode_##CODE(StackFrame& frame) {                              \
    Value& arg0 = frame.stack.pick(0);                          \
    if (arg0.i == 0) {                                          \
        interp_throw_exception("java/lang/ArithmeticException");  \
       return;                                                 \
    }                                                           \
    Value& arg1 = frame.stack.pick(1);                          \
    Value& res = arg1;                                          \
    expr;                                                       \
    frame.stack.pop();                                          \
    frame.ip++;                                                 \
}

#if defined (__INTEL_COMPILER) 
#pragma warning( push )
#pragma warning (disable:1683) // to get rid of remark #1683: explicit conversion of a 64-bit integral type to a smaller integral type
#endif

DEF_OPCODE_DIV_32_32_TO_32(IDIV,  res.i = (int32)((int64) arg1.i / arg0.i))   // Opcode_IDIV
DEF_OPCODE_DIV_32_32_TO_32(IREM,  res.i = (int32)((int64) arg1.i % arg0.i))   // Opcode_IREM

#if defined (__INTEL_COMPILER)
#pragma warning( pop )
#endif

static inline void
Opcode_LDIV(StackFrame& frame) {
    Value2 arg0, arg1, res;
    arg0 = frame.stack.getLong(0);
    if (arg0.i64 == 0) {
       interp_throw_exception("java/lang/ArithmeticException");
       return;
    }
    arg1 = frame.stack.getLong(2);
    frame.stack.pop(2);

#ifdef _EM64T_
    if (arg1.i64 == -arg1.i64) {
        if (arg0.i64 == -1) {
            res.i64 = arg1.i64;
            frame.stack.setLong(0, res);
            frame.ip++;
            return;
        }
    }
#endif
    res.i64 = arg1.i64 / arg0.i64;
    frame.stack.setLong(0, res);
    frame.ip++;
}
static inline void
Opcode_LREM(StackFrame& frame) {
    Value2 arg0, arg1, res;
    arg0 = frame.stack.getLong(0);
    if (arg0.i64 == 0) {
         interp_throw_exception("java/lang/ArithmeticException");       
        return;
    }
    arg1 = frame.stack.getLong(2);
    frame.stack.pop(2);
#ifdef _EM64T_
    if (arg1.i64 == -arg1.i64) {
        if (arg0.i64 == -1) {
            res.i64 = 0l;
            frame.stack.setLong(0, res);
            frame.ip++;
            return;
        }
    }
#endif
    res.i64 = arg1.i64 % arg0.i64;
    frame.stack.setLong(0, res);
    frame.ip++;
}

#define DEF_OPCODE_CMP(CMP,check)                   \
static inline void                                  \
Opcode_##CMP(StackFrame& frame) {                   \
    tmn_safe_point();                                \
    int32 val = frame.stack.pick().i;               \
    frame.stack.ref() = FLAG_NONE; /* for OPCODE_IFNULL */ \
    DEBUG_BYTECODE("val = " << (int)val);                    \
    if (val check) {                                \
    frame.ip += read_int16(frame.ip + 1);           \
    DEBUG_BYTECODE(", going to instruction");                \
    } else {                                        \
    DEBUG_BYTECODE(", false condition");                     \
    frame.ip += 3;                                  \
    }                                               \
    frame.stack.pop();                              \
}

DEF_OPCODE_CMP(IFEQ,==0) // Opcode_IFEQ
DEF_OPCODE_CMP(IFNE,!=0) // Opcode_IFNE
DEF_OPCODE_CMP(IFGE,>=0) // Opcode_IFGE
DEF_OPCODE_CMP(IFGT,>0)  // Opcode_IFGT
DEF_OPCODE_CMP(IFLE,<=0) // Opcode_IFLE
DEF_OPCODE_CMP(IFLT,<0)  // Opcode_IFLT

#define DEF_OPCODE_IF_ICMPXX(NAME,cmp)                          \
static inline void                                              \
Opcode_IF_ICMP ## NAME(StackFrame& frame) {                     \
    tmn_safe_point();                                           \
    int32 val0 = frame.stack.pick(1).i;                         \
    int32 val1 = frame.stack.pick(0).i;                         \
    frame.stack.ref(1) = FLAG_NONE;                             \
    frame.stack.ref(0) = FLAG_NONE;                             \
    if (val0 cmp val1) {                                        \
        frame.ip += read_int16(frame.ip + 1);                   \
        DEBUG_BYTECODE(val1 << " " << val0 << " branch taken"); \
    } else {                                                    \
    frame.ip += 3;                                              \
        DEBUG_BYTECODE(val1 << " " << val0 << " branch not taken");\
    }                                                           \
    frame.stack.pop(2);                                         \
}

DEF_OPCODE_IF_ICMPXX(EQ,==) // Opcode_IF_ICMPEQ OPCODE_IF_ACMPEQ
DEF_OPCODE_IF_ICMPXX(NE,!=) // Opcode_IF_ICMPNE OPCODE_IF_ACMPNE
DEF_OPCODE_IF_ICMPXX(GE,>=) // Opcode_IF_ICMPGE
DEF_OPCODE_IF_ICMPXX(GT,>)  // Opcode_IF_ICMPGT
DEF_OPCODE_IF_ICMPXX(LE,<=) // Opcode_IF_ICMPLE
DEF_OPCODE_IF_ICMPXX(LT,<)  // Opcode_IF_ICMPLT


static inline void
Opcode_LCMP(StackFrame& frame) {
    Value2 v0, v1;
    int res;
    v1 = frame.stack.getLong(0);
    v0 = frame.stack.getLong(2);
    if (v0.i64 < v1.i64) res = -1;
    else if (v0.i64 == v1.i64) res = 0;
    else res = 1;
    frame.stack.pop(3);
    frame.stack.pick().i = res;
    DEBUG_BYTECODE("res = " << res);
    frame.ip++;
}

static bool
ldc(StackFrame& frame, uint32 index) {
    Class *clazz = frame.method->get_class();
    Const_Pool *cp = clazz->const_pool;

#ifndef NDEBUG
    switch(cp_tag(cp, index)) {
        case CONSTANT_String:
            DEBUG_BYTECODE("#" << dec << (int)index << " String: \"" << cp[index].CONSTANT_String.string->bytes << "\"");
            break;
        case CONSTANT_Integer:
            DEBUG_BYTECODE("#" << dec << (int)index << " Integer: " << (int)cp[index].int_value);
            break;
        case CONSTANT_Float:
            DEBUG_BYTECODE("#" << dec << (int)index << " Float: " << cp[index].float_value);
            break;
        case CONSTANT_Class:
            DEBUG_BYTECODE("#" << dec << (int)index << " Class: \"" << const_pool_get_class_name(clazz, index) << "\"");
            break;
        default:
            DEBUG_BYTECODE("#" << dec << (int)index << " Unknown type = " << cp_tag(cp, index));
            DIE("ldc instruction: unexpected type (" << cp_tag(cp, index) 
                << ") of constant pool entry [" << index << "]");
            break;
    }
#endif

    frame.stack.push();
    if (cp_is_string(cp, index)) {
        // FIXME: is string reference is packed??
        // possibly not
        String* str = (String*) cp[index].CONSTANT_String.string;
        // FIXME: only compressed references
        frame.stack.pick().cr = COMPRESS_REF(vm_instantiate_cp_string_resolved(str));
        frame.stack.ref() = FLAG_OBJECT;
        return !exn_raised();
    } 
    else if (cp_is_class(cp, index)) 
    {
        Class *other_class = interp_resolve_class(clazz, index);
        if (!other_class) {
             return false;
        }
        assert(!tmn_is_suspend_enabled());
        
        frame.stack.pick().cr = COMPRESS_REF(*(other_class->class_handle));
        frame.stack.ref() = FLAG_OBJECT;
        
        return !exn_raised();
    }
    
    frame.stack.pick().u = cp[index].int_value;
    return true;
}


static inline void
Opcode_LDC(StackFrame& frame) {
    uint32 index = read_uint8(frame.ip + 1);
    if (!ldc(frame, index)) return;
    frame.ip += 2;
}

static inline void
Opcode_LDC_W(StackFrame& frame) {
    uint32 index = read_uint16(frame.ip + 1);
    if(!ldc(frame, index)) return;
    frame.ip += 3;
}

static inline void
Opcode_LDC2_W(StackFrame& frame) {
    uint32 index = read_uint16(frame.ip + 1);

    Class *clazz = frame.method->get_class();
    Const_Pool *cp = clazz->const_pool;
    frame.stack.push(2);
    Value2 val;
    val.u64 = ((uint64)cp[index].CONSTANT_8byte.high_bytes << 32) | cp[index].CONSTANT_8byte.low_bytes;
    frame.stack.setLong(0, val);
    DEBUG_BYTECODE("#" << dec << (int)index << " (val = " << hex << val.d << ")");
    frame.ip += 3;
}

// TODO ivan 20041005: check if the types defined somewhere else
enum ArrayElementType {
    AR_BOOLEAN = 4, // values makes sense for opcode NEWARRAY
    AR_CHAR,
    AR_FLOAT,
    AR_DOUBLE,
    AR_BYTE,
    AR_SHORT,
    AR_INT,
    AR_LONG
};

static inline void
Opcode_NEWARRAY(StackFrame& frame) {
    int type = read_int8(frame.ip + 1);
    Class *clazz = NULL;

    // TODO ivan 20041005: can be optimized, rely on order
    Global_Env *env = VM_Global_State::loader_env;
    switch (type) {
        case AR_BOOLEAN: clazz = env->ArrayOfBoolean_Class; break;
        case AR_CHAR:    clazz = env->ArrayOfChar_Class; break;
        case AR_FLOAT:   clazz = env->ArrayOfFloat_Class; break;
        case AR_DOUBLE:  clazz = env->ArrayOfDouble_Class; break;
        case AR_BYTE:    clazz = env->ArrayOfByte_Class; break;
        case AR_SHORT:   clazz = env->ArrayOfShort_Class; break;
        case AR_INT:     clazz = env->ArrayOfInt_Class; break;
        case AR_LONG:    clazz = env->ArrayOfLong_Class; break;
        default: ABORT("Invalid array type");
    }
    assert(clazz);

    // TODO: is it possible to optimize?
    // array data size = length << (type & 3);
    // how it can be usable?
    int32 length = frame.stack.pick().i;

    if (length < 0) {
        interp_throw_exception("java/lang/NegativeArraySizeException");
        return;
    }

    Vector_Handle array  = vm_new_vector_primitive(clazz,length);
    if (exn_raised()) { 
        // OutOfMemoryError occured
        return;
    }
   
    // COMPRESS_REF will fail assertion if array == 0 on ipf 
    frame.stack.pick().cr = COMPRESS_REF((ManagedObject*)array);
    DEBUG_BYTECODE(" (val = " << hex << (int)frame.stack.pick().i << ")");
    frame.stack.ref() = FLAG_OBJECT;
    frame.ip += 2;
}

static inline void
Opcode_ANEWARRAY(StackFrame& frame) {
    int classId = read_uint16(frame.ip + 1);
    Class *clazz = frame.method->get_class();

    Class *objClass = interp_resolve_class(clazz, classId);
    if (!objClass) return; // exception

    Class *arrayClass = interp_class_get_array_of_class(objClass);

    int32 length = frame.stack.pick().i;

    if (length < 0) {
        interp_throw_exception("java/lang/NegativeArraySizeException");
        return;
    }


    Vector_Handle array = vm_new_vector(arrayClass, length);
    if (exn_raised()) { 
        // OutOfMemoryError occured
        return;
    }

    set_vector_length(array, length);
    DEBUG_BYTECODE("length = " << dec << length);

    // COMPRESS_REF will fail assertion if array == 0 on ipf 
    frame.stack.pick().cr = COMPRESS_REF((ManagedObject*)array);
    frame.stack.ref() = FLAG_OBJECT;
    frame.ip += 3;
}

static inline bool
allocDimensions(StackFrame& frame, Class *arrayClass, int depth) {

    int *length = (int*)ALLOC_FRAME(sizeof(int) * (depth));
    int *pos = (int*)ALLOC_FRAME(sizeof(int) * (depth));
    Class **clss = (Class**)ALLOC_FRAME(sizeof(Class*) * (depth));
    int d;
    int max_depth = depth - 1;

   // check dimensions phase
    for(d = 0; d < depth; d++) {
        pos[d] = 0;
        int len = length[d] = frame.stack.pick(depth - 1 - d).i; 

        if (len < 0) {
            interp_throw_exception("java/lang/NegativeArraySizeException");
            return false;
        }
        
        if (len == 0) {
            if (d < max_depth) max_depth = d;
        }

        frame.stack.pick(depth - 1 - d).cr = 0;
        frame.stack.ref(depth - 1 - d) = FLAG_OBJECT;
    }

    // init Class* array
    Class *c = clss[0] = arrayClass;

    for(d = 1; d < depth; d++) {
        c = c->array_element_class;

        clss[d] = c;
    }

    // init root element
    ManagedObject* array = (ManagedObject*) vm_new_vector(clss[0], length[0]);
    if (exn_raised()) { 
        // OutOfMemoryError occured
        return false;
    }
    set_vector_length(array, length[0]);
    frame.stack.pick(depth - 1).cr = COMPRESS_REF(array);
    if (max_depth == 0) return true;

    d = 1;
    // allocation dimensions
    while(true) {
        ManagedObject *element = (ManagedObject*) vm_new_vector(clss[d], length[d]);
        if (exn_raised()) { 
            // OutOfMemoryError occured
            return false;
        }

        set_vector_length(element, length[d]);

        if (d != max_depth) {
            frame.stack.pick(depth - 1 - d).cr = COMPRESS_REF(element);
            d++;
            continue;
        }

        while(true) {
            array = UNCOMPRESS_REF(frame.stack.pick((depth - 1) - (d - 1)).cr);
            CREF* addr = (CREF*) get_vector_element_address_ref(array, pos[d-1]);
            *addr = COMPRESS_REF(element);
            pos[d-1]++;

            if (pos[d-1] < length[d-1]) {
                break;
            }

            pos[d-1] = 0;
            element = array;
            d--;

            if (d == 0) return true;
        }
    }
}

static inline void
Opcode_MULTIANEWARRAY(StackFrame& frame) {
    int classId = read_uint16(frame.ip + 1);
    int depth = read_uint8(frame.ip + 3);
    Class *clazz = frame.method->get_class();

    Class *arrayClass = interp_resolve_class(clazz, classId);
    if (!arrayClass) return; // exception

    DEBUG_BYTECODE(arrayClass->name->bytes << " " << depth);

    bool success = allocDimensions(frame, arrayClass, depth);
    if (!success) {
        return;
    }

    frame.stack.popClearRef(depth - 1);
    DEBUG_BYTECODE(" (val = " << hex << (int)frame.stack.pick().i << ")");
    frame.ip += 4;
}

static inline void
Opcode_NEW(StackFrame& frame) {
    uint32 classId = read_uint16(frame.ip + 1);
    Class *clazz = frame.method->get_class();

    Class *objClass = interp_resolve_class_new(clazz, classId);
    if (!objClass) return; // exception

    DEBUG_BYTECODE("cless = " << objClass->name->bytes);

    class_initialize(objClass);

    if (exn_raised()) {
        return;
    }

    ManagedObject *obj = class_alloc_new_object(objClass);

    if (exn_raised()) {
        // OutOfMemoryError occured
        return;
    }

    assert(obj);
   
    frame.stack.push();

    // COMPRESS_REF will fail assertion if obj == 0 on ipf 
    frame.stack.pick().cr = COMPRESS_REF((ManagedObject*)obj);
    DEBUG_BYTECODE(" (val = " << hex << (int)frame.stack.pick().i << ")");
    frame.stack.ref() = FLAG_OBJECT;
    frame.ip += 3;
}

static inline void
Opcode_POP(StackFrame& frame) {
    frame.stack.ref() = FLAG_NONE;
    frame.stack.pop();
    frame.ip++;
}

static inline void
Opcode_POP2(StackFrame& frame) {
    frame.stack.ref(0) = FLAG_NONE;
    frame.stack.ref(1) = FLAG_NONE;
    frame.stack.pop(2);
    frame.ip++;
}

static inline void
Opcode_SWAP(StackFrame& frame) {
    Value tmp = frame.stack.pick(0);
    frame.stack.pick(0) = frame.stack.pick(1);
    frame.stack.pick(1) = tmp;

    uint8 ref = frame.stack.ref(0);
    frame.stack.ref(0) = frame.stack.ref(1);
    frame.stack.ref(1) = ref;
    frame.ip++;
}

static inline void
Opcode_DUP(StackFrame& frame) {
    frame.stack.push();
    frame.stack.pick(0) = frame.stack.pick(1);
    frame.stack.ref(0) = frame.stack.ref(1);
    frame.ip++;
}

static inline void
Opcode_DUP2(StackFrame& frame) {
    frame.stack.push(2);
    frame.stack.pick(0) = frame.stack.pick(2);
    frame.stack.pick(1) = frame.stack.pick(3);

    frame.stack.ref(0) = frame.stack.ref(2);
    frame.stack.ref(1) = frame.stack.ref(3);
    frame.ip++;
}

static inline void
Opcode_DUP_X1(StackFrame& frame) {
    frame.stack.push();
    frame.stack.pick(0) = frame.stack.pick(1);
    frame.stack.pick(1) = frame.stack.pick(2);
    frame.stack.pick(2) = frame.stack.pick(0);

    frame.stack.ref(0) = frame.stack.ref(1);
    frame.stack.ref(1) = frame.stack.ref(2);
    frame.stack.ref(2) = frame.stack.ref(0);
    frame.ip++;
}

static inline void
Opcode_DUP_X2(StackFrame& frame) {
    frame.stack.push();
    frame.stack.pick(0) = frame.stack.pick(1);
    frame.stack.pick(1) = frame.stack.pick(2);
    frame.stack.pick(2) = frame.stack.pick(3);
    frame.stack.pick(3) = frame.stack.pick(0);
    
    frame.stack.ref(0) = frame.stack.ref(1);
    frame.stack.ref(1) = frame.stack.ref(2);
    frame.stack.ref(2) = frame.stack.ref(3);
    frame.stack.ref(3) = frame.stack.ref(0);
    frame.ip++;
}

static inline void
Opcode_DUP2_X1(StackFrame& frame) {
    frame.stack.push(2);
    frame.stack.pick(0) = frame.stack.pick(2);
    frame.stack.pick(1) = frame.stack.pick(3);
    frame.stack.pick(2) = frame.stack.pick(4);
    frame.stack.pick(3) = frame.stack.pick(0);
    frame.stack.pick(4) = frame.stack.pick(1);

    frame.stack.ref(0) = frame.stack.ref(2);
    frame.stack.ref(1) = frame.stack.ref(3);
    frame.stack.ref(2) = frame.stack.ref(4);
    frame.stack.ref(3) = frame.stack.ref(0);
    frame.stack.ref(4) = frame.stack.ref(1);
    frame.ip++;
}

static inline void
Opcode_DUP2_X2(StackFrame& frame) {
    frame.stack.push(2);
    frame.stack.pick(0) = frame.stack.pick(2);
    frame.stack.pick(1) = frame.stack.pick(3);
    frame.stack.pick(2) = frame.stack.pick(4);
    frame.stack.pick(3) = frame.stack.pick(5);
    frame.stack.pick(4) = frame.stack.pick(0);
    frame.stack.pick(5) = frame.stack.pick(1);

    frame.stack.ref(0) = frame.stack.ref(2);
    frame.stack.ref(1) = frame.stack.ref(3);
    frame.stack.ref(2) = frame.stack.ref(4);
    frame.stack.ref(3) = frame.stack.ref(5);
    frame.stack.ref(4) = frame.stack.ref(0);
    frame.stack.ref(5) = frame.stack.ref(1);
    frame.ip++;
}

static inline void
Opcode_IINC(StackFrame& frame) {
    uint32 varNum = read_uint8(frame.ip + 1);
    int incr = read_int8(frame.ip + 2);
    frame.locals(varNum).i += incr;
    DEBUG_BYTECODE("var" << (int)varNum << " += " << incr);
    frame.ip += 3;
}

static inline void
Opcode_WIDE_IINC(StackFrame& frame) {
    uint32 varNum = read_uint16(frame.ip + 2);
    int incr = read_int16(frame.ip + 4);
    frame.locals(varNum).i += incr;
    DEBUG_BYTECODE(" += " << incr);
    frame.ip += 6;
}

static inline void 
Opcode_ARRAYLENGTH(StackFrame& frame) {
    CREF cref = frame.stack.pick().cr;
    if (cref == 0) {
        throwNPE();
        return;
    }
    ManagedObject *ref = UNCOMPRESS_REF(cref);

    frame.stack.ref() = FLAG_NONE;
    frame.stack.pick().i = get_vector_length((Vector_Handle)ref);
    DEBUG_BYTECODE("length = " << dec << frame.stack.pick().i);
    frame.ip++;
}

static inline void
Opcode_AALOAD(StackFrame& frame) {
    CREF cref = frame.stack.pick(1).cr;
    if (cref == 0) {
        throwNPE();
        return;
    }
    Vector_Handle array = (Vector_Handle) UNCOMPRESS_REF(cref);
    uint32 length = get_vector_length(array);
    uint32 pos = frame.stack.pick(0).u;

    DEBUG_BYTECODE("length = " << dec << (int)length << " pos = " << (int)pos);

    if (pos >= length) {
        throwAIOOBE(pos);
        return;
    }

    frame.stack.pop();

    CREF* addr = (CREF*) get_vector_element_address_ref(array, pos);
    /* FIXME: assume compressed reference */
    frame.stack.pick().cr = *addr;
    frame.ip++;
}

#define DEF_OPCODE_XALOAD(CODE,arraytype,type,store)                            \
static inline void                                                              \
Opcode_ ## CODE(StackFrame& frame) {                                            \
    CREF cref = frame.stack.pick(1).cr;                         \
    if (cref == 0) {                                                            \
        throwNPE();                                                             \
        return;                                                                 \
    }                                                                           \
    Vector_Handle array = (Vector_Handle) UNCOMPRESS_REF(cref);                 \
    uint32 length = get_vector_length(array);                                   \
    uint32 pos = frame.stack.pick(0).u;                                         \
                                                                                \
    DEBUG_BYTECODE("length = " << dec << (int)length << " pos = " << (int)pos); \
                                                                                \
    if (pos >= length) {                                                        \
        throwAIOOBE(pos);                                                       \
        return;                                                                 \
    }                                                                           \
                                                                                \
    frame.stack.pop();                                                          \
                                                                                \
    type* addr = (type*) get_vector_element_address_ ## arraytype(array, pos);  \
    /* FIXME: assume compressed reference */                                    \
    frame.stack.pick().store = *addr;                                           \
    frame.stack.ref() = FLAG_NONE;                                              \
    frame.ip++;                                                                 \
}

DEF_OPCODE_XALOAD(BALOAD, int8, int8, i)
DEF_OPCODE_XALOAD(CALOAD, uint16, uint16, u)
DEF_OPCODE_XALOAD(SALOAD, int16, int16, i)
DEF_OPCODE_XALOAD(IALOAD, int32, int32, i)
DEF_OPCODE_XALOAD(FALOAD, f32, float, f)

static inline void
Opcode_LALOAD(StackFrame& frame) {
    CREF cref = frame.stack.pick(1).cr;
    if (cref == 0) {
        throwNPE();
        return;
    }
    Vector_Handle array = (Vector_Handle) UNCOMPRESS_REF(cref);
    uint32 length = get_vector_length(array);
    uint32 pos = frame.stack.pick(0).u;

    DEBUG_BYTECODE("length = " << dec << (int)length << " pos = " << (int)pos);

    if (pos >= length) {
        throwAIOOBE(pos);
        return;
    }

    frame.stack.ref(1) = FLAG_NONE;

    Value2* addr = (Value2*) get_vector_element_address_int64(array, pos);
    frame.stack.setLong(0, *addr);
    frame.ip++;
}

static inline void
Opcode_AASTORE(StackFrame& frame) {
    CREF cref = frame.stack.pick(2).cr;
    if (cref == 0) {
        throwNPE();
        return;
    }
    Vector_Handle array = (Vector_Handle) UNCOMPRESS_REF(cref);
    uint32 length = get_vector_length(array);
    uint32 pos = frame.stack.pick(1).u;

    DEBUG_BYTECODE("length = " << dec << (int)length << " pos = " << (int)pos);

    if (pos >= length) {
        throwAIOOBE(pos);
        return;
    }

    // TODO: check ArrayStoreException
    ManagedObject *arrayObj = (ManagedObject*) array;
    Class *arrayClass = arrayObj->vt()->clss;
    Class *elementClass = arrayClass->array_element_class;
    ManagedObject *obj = UNCOMPRESS_REF(frame.stack.pick().cr);
    if (!(obj == 0 || vm_instanceof(obj, elementClass))) {
        interp_throw_exception("java/lang/ArrayStoreException");
        return;
    }

    // FIXME: compressed refs only
    CREF* addr = (CREF*) get_vector_element_address_ref(array, pos);
    *addr = frame.stack.pick().cr;
    frame.stack.ref(2) = FLAG_NONE;
    frame.stack.ref(0) = FLAG_NONE;
    frame.stack.pop(3);
    frame.ip++;
}

#define DEF_OPCODE_IASTORE(CODE,arraytype,type,ldtype)                          \
static inline void                                                              \
Opcode_ ## CODE(StackFrame& frame) {                                            \
    CREF cref = frame.stack.pick(2).cr;                         \
    if (cref == 0) {                                                            \
        throwNPE();                                                             \
        return;                                                                 \
    }                                                                           \
    Vector_Handle array = (Vector_Handle) UNCOMPRESS_REF(cref);                 \
    uint32 length = get_vector_length(array);                                   \
    uint32 pos = frame.stack.pick(1).u;                                         \
                                                                                \
    DEBUG_BYTECODE("length = " << dec << (int)length << " pos = " << (int)pos); \
                                                                                \
    if (pos >= length) {                                                        \
        throwAIOOBE(pos);                                                       \
        return;                                                                 \
    }                                                                           \
                                                                                \
    type* addr = (type*) get_vector_element_address_ ## arraytype(array, pos);  \
    *addr = frame.stack.pick().ldtype;                                          \
    frame.stack.ref(2) = FLAG_NONE;                                             \
    frame.stack.pop(3);                                                         \
    frame.ip++;                                                                 \
}

#if defined (__INTEL_COMPILER) 
#pragma warning( push )
#pragma warning (disable:810) // to get rid of remark #810: conversion from "int" to "unsigned char" may lose significant bits
#endif

DEF_OPCODE_IASTORE(CASTORE, uint16, uint16, u)
DEF_OPCODE_IASTORE(BASTORE, int8, int8, i)
DEF_OPCODE_IASTORE(SASTORE, int16, int16, i)
DEF_OPCODE_IASTORE(IASTORE, int32, int32, i)
DEF_OPCODE_IASTORE(FASTORE, f32, float, f)

#if defined (__INTEL_COMPILER)
#pragma warning( pop )
#endif

static inline void
Opcode_LASTORE(StackFrame& frame) {
    CREF cref = frame.stack.pick(3).cr;
    if (cref == 0) {
        throwNPE();
        return;
    }
    Vector_Handle array = (Vector_Handle) UNCOMPRESS_REF(cref);
    uint32 length = get_vector_length(array);
    uint32 pos = frame.stack.pick(2).u;

    DEBUG_BYTECODE("length = " << dec << (int)length << " pos = " << (int)pos);

    if (pos >= length) {
        /* FIXME ivan 20041005: array index out of bounds exception */
        throwAIOOBE(pos);
        return;
    }

    Value2* addr = (Value2*) get_vector_element_address_int64(array, pos);
    *addr = frame.stack.getLong(0);
    frame.stack.ref(3) = FLAG_NONE;
    frame.stack.pop(4);
    frame.ip++;
}

static inline void
Opcode_PUTSTATIC(StackFrame& frame) {
    uint32 fieldId = read_uint16(frame.ip + 1);
    Class *clazz = frame.method->get_class();
    
    Field *field = interp_resolve_static_field(clazz, fieldId, true);
    if (!field) return; // exception
    
    class_initialize(field->get_class());

    if (exn_raised()) {
        return;
    }

    if (field->is_final()) {
        if (frame.method->get_class()->state != ST_Initializing) {
            throwIAE();
            return;
        }
    }

    void *addr = field->get_address();

    DEBUG_BYTECODE(field->get_name()->bytes << " " << field->get_descriptor()->bytes
            << " (val = " << hex << (int)frame.stack.pick().i << ")");

    switch (field->get_java_type()) {

#ifdef COMPACT_FIELDS // use compact fields on ipf
        case VM_DATA_TYPE_BOOLEAN:
        case VM_DATA_TYPE_INT8:
            *(uint8*)addr = (uint8) frame.stack.pick().u;
            frame.stack.pop();
            break;

        case VM_DATA_TYPE_CHAR:
        case VM_DATA_TYPE_INT16:
            *(uint16*)addr = (uint16) frame.stack.pick().u;
            frame.stack.pop();
            break;

#else // ia32 not using compact fields
        case VM_DATA_TYPE_BOOLEAN:
            *(uint32*)addr = (uint8) frame.stack.pick().u;
            frame.stack.pop();
            break;
        case VM_DATA_TYPE_CHAR:
            *(uint32*)addr = (uint16) frame.stack.pick().u;
            frame.stack.pop();
            break;

        case VM_DATA_TYPE_INT8:
            *(int32*)addr = (int8) frame.stack.pick().i;
            frame.stack.pop();
            break;
        case VM_DATA_TYPE_INT16:
            *(int32*)addr = (int16) frame.stack.pick().i;
            frame.stack.pop();
            break;
#endif
        case VM_DATA_TYPE_INT32:
        case VM_DATA_TYPE_F4:
            *(int32*)addr = frame.stack.pick().i;
            frame.stack.pop();
            break;

        case VM_DATA_TYPE_ARRAY:
        case VM_DATA_TYPE_CLASS:
            frame.stack.ref() = FLAG_NONE;
            *(CREF*)addr = frame.stack.pick().cr;
            frame.stack.pop();
            break;

        case VM_DATA_TYPE_INT64:
        case VM_DATA_TYPE_F8:
            {
                double *vaddr = (double*) addr;
                *vaddr = frame.stack.getLong(0).d;
                frame.stack.pop(2);
                break;
            }
        default:
            ABORT("Unexpected data type");
    }
    frame.ip += 3;
}

static inline void
Opcode_GETSTATIC(StackFrame& frame) {
    uint32 fieldId = read_uint16(frame.ip + 1);
    Class *clazz = frame.method->get_class();

    Field *field = interp_resolve_static_field(clazz, fieldId, false);
    if (!field) return; // exception

    class_initialize(field->get_class());

    if (exn_raised()) {
        return;
    }

    void *addr = field->get_address();
    frame.stack.push();

    switch (field->get_java_type()) {
#ifdef COMPACT_FIELDS // use compact fields on ipf
        case VM_DATA_TYPE_BOOLEAN:
            frame.stack.pick().u = *(uint8*)addr;
            break;

        case VM_DATA_TYPE_INT8:
            frame.stack.pick().i = *(int8*)addr;
            break;

        case VM_DATA_TYPE_CHAR:
            frame.stack.pick().u = *(uint16*)addr;
            break;

        case VM_DATA_TYPE_INT16:
            frame.stack.pick().i = *(int16*)addr;
            break;

#else // ia32 not using compact fields
        case VM_DATA_TYPE_BOOLEAN:
        case VM_DATA_TYPE_CHAR:
            frame.stack.pick().u = *(uint32*)addr;
            break;

        case VM_DATA_TYPE_INT8:
        case VM_DATA_TYPE_INT16:
#endif
        case VM_DATA_TYPE_INT32:
        case VM_DATA_TYPE_F4:
            frame.stack.pick().i = *(int32*)addr;
            break;
        case VM_DATA_TYPE_ARRAY:
        case VM_DATA_TYPE_CLASS:
            frame.stack.pick().cr = *(CREF*)addr;
            frame.stack.ref() = FLAG_OBJECT;
            break;
            
        case VM_DATA_TYPE_INT64:
        case VM_DATA_TYPE_F8:
            {
                Value2 val;
                val.d = *(double*)addr;
                frame.stack.push();
                frame.stack.setLong(0, val);
            }
            break;

        default:
            ABORT("Unexpected data type");
    }
    DEBUG_BYTECODE(field->get_name()->bytes << " " << field->get_descriptor()->bytes
            << " (val = " << hex << (int)frame.stack.pick().i << ")");
    frame.ip += 3;
}

static inline void
Opcode_PUTFIELD(StackFrame& frame) {
    uint32 fieldId = read_uint16(frame.ip + 1);
    Class *clazz = frame.method->get_class();

    Field *field = interp_resolve_nonstatic_field(clazz, fieldId, true);
    if (!field) return; // exception
    
    if (field->is_final()) {
        if (!frame.method->is_init()) {
            throwIAE();
            return;
        }
    }

    DEBUG_BYTECODE(field->get_name()->bytes << " " << field->get_descriptor()->bytes
            << " (val = " << hex << (int)frame.stack.pick().i << ")");

    switch (field->get_java_type()) {

#ifdef COMPACT_FIELDS // use compact fields on ipf
        case VM_DATA_TYPE_BOOLEAN:
        case VM_DATA_TYPE_INT8:
            {
                uint32 val = frame.stack.pick(0).u;
                CREF cref = frame.stack.pick(1).cr;
                if (cref == 0) {
                    throwNPE();
                    return;
                }
                ManagedObject *obj = UNCOMPRESS_REF(cref);
                uint8 *addr = ((uint8*)obj) + field->get_offset();
                *(uint8*)addr = (uint8) val;
                frame.stack.ref(1) = FLAG_NONE;
                frame.stack.pop(2);
                break;
            }

        case VM_DATA_TYPE_CHAR:
        case VM_DATA_TYPE_INT16:
            {
                uint32 val = frame.stack.pick(0).u;
                CREF cref = frame.stack.pick(1).cr;
                if (cref == 0) {
                    throwNPE();
                    return;
                }
                ManagedObject *obj = UNCOMPRESS_REF(cref);
                uint8 *addr = ((uint8*)obj) + field->get_offset();
                *(uint16*)addr = (uint16) val;
                frame.stack.ref(1) = FLAG_NONE;
                frame.stack.pop(2);
                break;
            }

#else // ia32 not using compact fields
        case VM_DATA_TYPE_BOOLEAN:
            {
                uint32 val = frame.stack.pick(0).u;
                CREF cref = frame.stack.pick(1).cr;
                if (cref == 0) {
                    throwNPE();
                    return;
                }
                ManagedObject *obj = UNCOMPRESS_REF(cref);
                uint8 *addr = ((uint8*)obj) + field->get_offset();
                *(uint32*)addr = (uint8) val;
                frame.stack.ref(1) = FLAG_NONE;
                frame.stack.pop(2);
                break;
            }
        case VM_DATA_TYPE_INT8:
            {
                int32 val = frame.stack.pick(0).i;
                CREF cref = frame.stack.pick(1).cr;
                if (cref == 0) {
                    throwNPE();
                    return;
                }
                ManagedObject *obj = UNCOMPRESS_REF(cref);
                uint8 *addr = ((uint8*)obj) + field->get_offset();
                *(int32*)addr = (int8) val;
                frame.stack.ref(1) = FLAG_NONE;
                frame.stack.pop(2);
                break;
            }

        case VM_DATA_TYPE_CHAR:
            {
                uint32 val = frame.stack.pick(0).u;
                CREF cref = frame.stack.pick(1).cr;
                if (cref == 0) {
                    throwNPE();
                    return;
                }
                ManagedObject *obj = UNCOMPRESS_REF(cref);
                uint8 *addr = ((uint8*)obj) + field->get_offset();
                *(uint32*)addr = (uint16) val;
                frame.stack.ref(1) = FLAG_NONE;
                frame.stack.pop(2);
                break;
            }
        case VM_DATA_TYPE_INT16:
            {
                int32 val = frame.stack.pick(0).i;
                CREF cref = frame.stack.pick(1).cr;
                if (cref == 0) {
                    throwNPE();
                    return;
                }
                ManagedObject *obj = UNCOMPRESS_REF(cref);
                uint8 *addr = ((uint8*)obj) + field->get_offset();
                *(int32*)addr = (int16) val;
                frame.stack.ref(1) = FLAG_NONE;
                frame.stack.pop(2);
                break;
            }

#endif
        case VM_DATA_TYPE_ARRAY:
        case VM_DATA_TYPE_CLASS:
        case VM_DATA_TYPE_INT32:
        case VM_DATA_TYPE_F4:
            {
                CREF val = frame.stack.pick(0).cr;
                CREF cref = frame.stack.pick(1).cr;
                if (cref == 0) {
                    throwNPE();
                    return;
                }
                ManagedObject *obj = UNCOMPRESS_REF(cref);
                uint8 *addr = ((uint8*)obj) + field->get_offset();
                // compressed references is uint32
                *(CREF*)addr = val;
                frame.stack.ref(1) = FLAG_NONE;
                frame.stack.ref(0) = FLAG_NONE;
                frame.stack.pop(2);
                break;
            }

        case VM_DATA_TYPE_INT64:
        case VM_DATA_TYPE_F8:
            {
                CREF cref = frame.stack.pick(2).cr;
                if (cref == 0) {
                    throwNPE();
                    return;
                }
                ManagedObject *obj = UNCOMPRESS_REF(cref);
                uint8 *addr = ((uint8*)obj) + field->get_offset();
                double *vaddr = (double*) addr;
                *vaddr = frame.stack.getLong(0).d;
                frame.stack.ref(2) = FLAG_NONE;
                frame.stack.pop(3);
                break;
            }

        default:
            ABORT("Unexpected data type");
    }
    frame.ip += 3;
}

static inline void
Opcode_GETFIELD(StackFrame& frame) {
    uint32 fieldId = read_uint16(frame.ip + 1);
    Class *clazz = frame.method->get_class();

    Field *field = interp_resolve_nonstatic_field(clazz, fieldId, false);
    if (!field) return; // exception

    CREF cref = frame.stack.pick(0).cr;
    if (cref == 0) {
        throwNPE();
        return;
    }
    ManagedObject *obj = UNCOMPRESS_REF(cref);
    uint8 *addr = ((uint8*)obj) + field->get_offset();
    frame.stack.ref() = FLAG_NONE;

    switch (field->get_java_type()) {

#ifdef COMPACT_FIELDS // use compact fields on ipf
        case VM_DATA_TYPE_BOOLEAN:
            {
                frame.stack.pick(0).u = (uint32) *(uint8*)addr;
                break;
            }
        case VM_DATA_TYPE_INT8:
            {
                frame.stack.pick(0).i = (int32) *(int8*)addr;
                break;
            }

        case VM_DATA_TYPE_CHAR:
            {
                frame.stack.pick(0).u = (uint32) *(uint16*)addr;
                break;
            }
        case VM_DATA_TYPE_INT16:
            {
                frame.stack.pick(0).i = (int32) *(int16*)addr;
                break;
            }
#else // ia32 - not using compact fields
        case VM_DATA_TYPE_BOOLEAN:
        case VM_DATA_TYPE_CHAR:
                frame.stack.pick(0).u = *(uint32*)addr;
                break;
        case VM_DATA_TYPE_INT8:
        case VM_DATA_TYPE_INT16:
#endif

        case VM_DATA_TYPE_INT32:
        case VM_DATA_TYPE_F4:
            {
                frame.stack.pick(0).i = *(int32*)addr;
                break;
            }

        case VM_DATA_TYPE_ARRAY:
        case VM_DATA_TYPE_CLASS:
            {
                frame.stack.ref() = FLAG_OBJECT;
                frame.stack.pick(0).cr = *(CREF*)addr;
                ASSERT_OBJECT(UNCOMPRESS_REF(frame.stack.pick(0).cr));
                break;
            }

        case VM_DATA_TYPE_INT64:
        case VM_DATA_TYPE_F8:
            {
                Value2 val;
                val.d = *(double*)addr;
                frame.stack.push();
                frame.stack.setLong(0, val);
                break;
            }

        default:
            ABORT("Unexpected data type");
    }
    DEBUG_BYTECODE(field->get_name()->bytes << " " << field->get_descriptor()->bytes
            << " (val = " << hex << (int)frame.stack.pick().i << ")");

    frame.ip += 3;
}

static inline void
Opcode_INVOKEVIRTUAL(StackFrame& frame) {
    uint32 methodId = read_uint16(frame.ip + 1);
    Class *clazz = frame.method->get_class();

    Method *method = interp_resolve_virtual_method(clazz, methodId);
    if (!method) return; // exception

    DEBUG_BYTECODE(method->get_class()->name->bytes << "."
            << method->get_name()->bytes << "/"
            << method->get_descriptor()->bytes<< endl);

    tmn_safe_point();
    interpreterInvokeVirtual(frame, method);
}

static inline void
Opcode_INVOKEINTERFACE(StackFrame& frame) {
    uint32 methodId = read_uint16(frame.ip + 1);
    Class *clazz = frame.method->get_class();

    Method *method = interp_resolve_interface_method(clazz, methodId);
    if (!method) return; // exception
    
    DEBUG_BYTECODE(method->get_class()->name->bytes << "."
            << method->get_name()->bytes << "/"
            << method->get_descriptor()->bytes << endl);

    tmn_safe_point();
    interpreterInvokeInterface(frame, method);
}

static inline void
Opcode_INVOKESTATIC(StackFrame& frame) {
    uint32 methodId = read_uint16(frame.ip + 1);
    Class *clazz = frame.method->get_class();

    Method *method = interp_resolve_static_method(clazz, methodId);
    if (!method) return; // exception

    DEBUG_BYTECODE(method->get_class()->name->bytes << "."
            << method->get_name()->bytes << "/"
            << method->get_descriptor()->bytes << endl);

    class_initialize(method->get_class());

    if (exn_raised()) {
        return;
    }

    tmn_safe_point();
    interpreterInvokeStatic(frame, method);
}

static inline void
Opcode_INVOKESPECIAL(StackFrame& frame) {
    uint32 methodId = read_uint16(frame.ip + 1);
    Class *clazz = frame.method->get_class();

    Method *method = interp_resolve_special_method(clazz, methodId);
    if (!method) return; // exception
    
    DEBUG_BYTECODE(method->get_class()->name->bytes << "."
            << method->get_name()->bytes << "/"
             << method->get_descriptor()->bytes << endl);

    tmn_safe_point();
    interpreterInvokeSpecial(frame, method);
}

static inline void
Opcode_TABLESWITCH(StackFrame& frame) {
    uint8* oldip = frame.ip;
    uint8* ip = frame.ip + 1;
    ip = ((ip - (uint8*)frame.method->get_byte_code_addr() + 3) & ~3)
        + (uint8*)frame.method->get_byte_code_addr();
    int32 deflt = read_int32(ip);
    int32 low =   read_int32(ip+4);
    int32 high =    read_int32(ip+8);
    int32 val = frame.stack.pick().i;
    frame.stack.pop();
    DEBUG_BYTECODE("val = " << val << " low = " << low << " high = " << high);
    if (val < low || val > high) {
        DEBUG_BYTECODE("default offset taken!\n");
        frame.ip = oldip + deflt;
    }
    else {
        frame.ip = oldip + read_int32(ip + 12 + ((val - low) << 2));
    }
}

static inline void
Opcode_LOOKUPSWITCH(StackFrame& frame) {
    uint8* oldip = frame.ip;
    uint8* ip = frame.ip + 1;
    ip = ((ip - (uint8*)frame.method->get_byte_code_addr() + 3) & ~3)
        + (uint8*)frame.method->get_byte_code_addr();
    int32 deflt = read_int32(ip);
    int32 num = read_int32(ip+4);
    int32 val = frame.stack.pick().i;
    frame.stack.pop();

    for(int i = 0; i < num; i++) {
        int32 key = read_int32(ip + 8 + i * 8);
        if (val == key) {
            frame.ip = oldip + read_int32(ip + 12 + i * 8);
            return;
        }

    }
    DEBUG_BYTECODE("default offset taken!\n");
    frame.ip = oldip + deflt;
}

static inline void
Opcode_GOTO(StackFrame& frame) {
    tmn_safe_point();
    DEBUG_BYTECODE("going to instruction");
    frame.ip += read_int16(frame.ip + 1);
}

static inline void
Opcode_GOTO_W(StackFrame& frame) {
    tmn_safe_point();
    DEBUG_BYTECODE("going to instruction");
    frame.ip += read_int32(frame.ip + 1);
}

static inline void
Opcode_JSR(StackFrame& frame) {
    uint32 retAddr = frame.ip + 3 - (uint8*)frame.method->get_byte_code_addr();
    frame.stack.push();
    frame.stack.pick().u = retAddr;
    frame.stack.ref() = FLAG_RET_ADDR;
    DEBUG_BYTECODE("going to instruction");
    frame.ip += read_int16(frame.ip + 1);
}

static inline void
Opcode_JSR_W(StackFrame& frame) {
    uint32 retAddr = frame.ip + 5 - (uint8*)frame.method->get_byte_code_addr();
    frame.stack.push();
    frame.stack.pick().u = retAddr;
    frame.stack.ref() = FLAG_OBJECT;
    DEBUG_BYTECODE("going to instruction");
    frame.ip += read_int32(frame.ip + 1);
}

static inline void
Opcode_RET(StackFrame& frame) {
    uint32 varNum = read_uint8(frame.ip + 1);
    uint32 retAddr = frame.locals(varNum).u;
    assert(frame.locals.ref(varNum) == FLAG_RET_ADDR);
    frame.ip = retAddr + (uint8*)frame.method->get_byte_code_addr();
}

static inline void
Opcode_WIDE_RET(StackFrame& frame) {
    uint32 varNum = read_uint16(frame.ip + 2);
    uint32 retAddr = frame.locals(varNum).u;
    frame.ip = retAddr + (uint8*)frame.method->get_byte_code_addr();
}

static inline void
Opcode_CHECKCAST(StackFrame& frame) {
    uint32 classId = read_uint16(frame.ip + 1);
    Class *clazz = frame.method->get_class();
    Class *objClass = interp_resolve_class(clazz, classId);
    if (!objClass) return; // exception
    
    DEBUG_BYTECODE("class = " << objClass->name->bytes);

    ManagedObject *obj = UNCOMPRESS_REF(frame.stack.pick().cr);

    if (!(obj == 0 || vm_instanceof(obj, objClass))) {
        interp_throw_exception("java/lang/ClassCastException");
        return;
    }
    frame.ip += 3;
}

static inline void
Opcode_INSTANCEOF(StackFrame& frame) {
    uint32 classId = read_uint16(frame.ip + 1);
    Class *clazz = frame.method->get_class();
    Class *objClass = interp_resolve_class(clazz, classId);
    if (!objClass) return; // exception
    
    DEBUG_BYTECODE("class = " << objClass->name->bytes);

    ManagedObject *obj = UNCOMPRESS_REF(frame.stack.pick().cr);

#ifdef COMPRESS_MODE
    // FIXME ivan 20041027: vm_instanceof checks null pointers, it assumes
    // that null is Class::managed_null, but uncompress_compressed_reference
    // doesn't return managed_null for null compressed references
    frame.stack.pick().u = (!(obj == 0)) && vm_instanceof(obj, objClass);
#else
    frame.stack.pick().u = vm_instanceof(obj, objClass);
#endif
    frame.stack.ref() = FLAG_NONE;
    frame.ip += 3;
}

static inline void
Opcode_MONITORENTER(StackFrame& frame) {
    CREF cr = frame.stack.pick().cr;
    if (cr == 0) {
        throwNPE();
        return;
    }
    frame.locked_monitors->monitor = UNCOMPRESS_REF(cr);

    M2N_ALLOC_MACRO;
    vm_monitor_enter_wrapper(frame.locked_monitors->monitor);
    M2N_FREE_MACRO;

    frame.stack.ref() = FLAG_NONE;
    frame.stack.pop();
    frame.ip++;
}

static inline void
Opcode_MONITOREXIT(StackFrame& frame) {
    CREF cr = frame.stack.pick().cr;
    if (cr == 0) {
        throwNPE();
        return;
    }
    M2N_ALLOC_MACRO;
    vm_monitor_exit_wrapper(UNCOMPRESS_REF(cr));
    M2N_FREE_MACRO;

    if (get_current_thread_exception())
        return;
    
    MonitorList *ml = frame.locked_monitors;

    frame.locked_monitors = ml->next;
    ml->next = frame.free_monitors;
    frame.free_monitors = ml;

    frame.stack.ref() = FLAG_NONE;
    frame.stack.pop();
    frame.ip++;
}

static inline void
Opcode_ATHROW(StackFrame& frame) {
    CREF cr = frame.stack.pick().cr;
    ManagedObject *obj = UNCOMPRESS_REF(cr);
    if (obj == NULL) {
        throwNPE();
        return;
    }

    // TODO: optimize, can add a flag to class which implements Throwable
    // and set it when throwing for the first time.
    if (!vm_instanceof(obj, VM_Global_State::loader_env->java_lang_Throwable_Class)) {
        DEBUG("****** java.lang.VerifyError ******\n");
        interp_throw_exception("java/lang/VerifyError");
        return;
    }
    DEBUG_BYTECODE(" " << obj->vt()->clss->name->bytes << endl);
    assert(!tmn_is_suspend_enabled());
    set_current_thread_exception(obj);
}

bool
findExceptionHandler(StackFrame& frame, ManagedObject **exception, Handler **hh) {
    assert(!exn_raised());
    assert(!tmn_is_suspend_enabled());

    Method *m = frame.method;
    DEBUG_BYTECODE("Searching for exception handler:");
    DEBUG_BYTECODE("   In "
            << m->get_class()->name->bytes
            << "/"
            << m->get_name()->bytes
            << m->get_descriptor()->bytes << endl);

    uint32 ip = frame.ip - (uint8*)m->get_byte_code_addr();
    DEBUG_BYTECODE("ip = " << dec << (int)ip << endl);

    Class *clazz = m->get_class();

    for(uint32 i = 0; i < m->num_bc_exception_handlers(); i++) {
        Handler *h = m->get_bc_exception_handler_info(i);
        DEBUG_BYTECODE("handler" << (int) i
                << ": start_pc=" << (int) h->get_start_pc()
                << " end_pc=" << (int) h->get_end_pc() << endl);

        if (ip < h->get_start_pc()) continue;
        if (ip >= h->get_end_pc()) continue;

        uint32 catch_type_index = h->get_catch_type_index();

        if (catch_type_index) {
            // 0 - is handler for all
            // if !0 - check if the handler catches this exception type

            DEBUG_BYTECODE("catch type index = " << (int)catch_type_index << endl);

            Class *obj = interp_resolve_class(clazz, catch_type_index);

            if (!obj) {
                // possible if verifier is disabled
                frame.exception = get_current_thread_exception();
                clear_current_thread_exception();
                return false;
            }

            if (!vm_instanceof(*exception, obj)) continue;
        }
        *hh = h;
        return true;
    }
    return false;
}

static inline bool
processExceptionHandler(StackFrame& frame, ManagedObject **exception) {
    Method *m = frame.method;
    Handler *h;
    if (findExceptionHandler(frame, exception, &h)){
        DEBUG_BYTECODE("Exception caught: " << (*exception)->vt()->clss->name->bytes << endl);
        DEBUG_BYTECODE("Found handler!\n");
        frame.ip = (uint8*)m->get_byte_code_addr() + h->get_handler_pc();
        return true;
    }
    return false;
}

static inline void
stackDump(StackFrame& frame) {

    StackFrame *f = &frame;

    ECHO("****** STACK DUMP: ************");
    while(f) {
        Method *m = f->method;
        Class *c = m->get_class();
        int line = -2;
        if ( !m->is_native() ) {
            int ip = (uint8*)frame.ip - (uint8*)m->get_byte_code_addr();
            line = m->get_line_number((uint16)ip);
        }
#ifdef INTERPRETER_DEEP_DEBUG
        ECHO(c->name->bytes << "."
                << m->get_name()->bytes
                << m->get_descriptor()->bytes << " ("
                << class_get_source_file_name(c) << ":"
                << line << ")"
                << " last bcs: (8 of " << f->n_last_bytecode << "): "
                << opcodeNames[f->last_bytecodes[
                        (f->n_last_bytecode-1)&7]] << " "
                << opcodeNames[f->last_bytecodes[
                        (f->n_last_bytecode-2)&7]] << " "
                << opcodeNames[f->last_bytecodes[
                        (f->n_last_bytecode-3)&7]] << " "
                << opcodeNames[f->last_bytecodes[
                        (f->n_last_bytecode-4)&7]] << " "
                << opcodeNames[f->last_bytecodes[
                        (f->n_last_bytecode-5)&7]] << " "
                << opcodeNames[f->last_bytecodes[
                        (f->n_last_bytecode-6)&7]] << " "
                << opcodeNames[f->last_bytecodes[
                        (f->n_last_bytecode-7)&7]] << " "
                << opcodeNames[f->last_bytecodes[
                        (f->n_last_bytecode-8)&7]]);
#else
    const char *filename = class_get_source_file_name(c);
        ECHO(c->name->bytes << "."
                << m->get_name()->bytes
                << m->get_descriptor()->bytes << " ("
                << (filename != NULL ? filename : "NULL") << ":"
                << line << ")");
#endif
        f = f->prev;
    }
}

void stack_dump(VM_thread *thread) {
    StackFrame *frame = getLastStackFrame(thread);
    stackDump(*frame);
}

void stack_dump() {
    StackFrame *frame = getLastStackFrame();
    stackDump(*frame);
}


static inline
void UNUSED dump_all_java_stacks() {
    tm_iterator_t * iterator = tm_iterator_create();
    VM_thread *thread = tm_iterator_next(iterator);
    while(thread) {
        stack_dump(thread);
        thread = tm_iterator_next(iterator);
    }
    tm_iterator_release(iterator);
    INFO("****** END OF JAVA STACKS *****\n");
}

void
method_exit_callback_with_frame(Method *method, StackFrame& frame) {
    NativeObjectHandles handles;
    
    bool exc = exn_raised();
    jvalue val;

    val.j = 0;

    if (exc) {
        tmn_suspend_enable();
        method_exit_callback(method, true, val);
        tmn_suspend_disable();
        return;
    }


    switch(method->get_return_java_type()) {
        ObjectHandle h;

        case JAVA_TYPE_VOID:
            val.i = 0;
            break;

        case JAVA_TYPE_CLASS:
        case JAVA_TYPE_ARRAY:
        case JAVA_TYPE_STRING:
            h = oh_allocate_local_handle();
            h->object = (ManagedObject*) UNCOMPRESS_REF(frame.stack.pick().cr);
            val.l = (jobject) h;
            break;

        case JAVA_TYPE_FLOAT:
        case JAVA_TYPE_BOOLEAN:
        case JAVA_TYPE_BYTE:
        case JAVA_TYPE_CHAR:
        case JAVA_TYPE_SHORT:
        case JAVA_TYPE_INT:
            val.i = frame.stack.pick().i;
            break;

        case JAVA_TYPE_LONG:
        case JAVA_TYPE_DOUBLE:
            val.j = frame.stack.getLong(0).i64;
            break;

        default:
            ABORT("Unexpected java type");
    }

    tmn_suspend_enable();
    method_exit_callback(method, false, val);
    tmn_suspend_disable();
}

void
interpreter(StackFrame &frame) {
    DEBUG_TRACE_PLAIN("interpreter: "
            << frame.method->get_class()->name->bytes
            << " " << frame.method->get_name()->bytes
            << frame.method->get_descriptor()->bytes << endl);

    M2N_ALLOC_MACRO;
    assert(!exn_raised());
    assert(!tmn_is_suspend_enabled());
    
    uint8 *first = (uint8*) p_TLS_vmthread->firstFrame;
    int stackLength = ((uint8*)first) - ((uint8*)&frame);
    if (stackLength > 500000) { // FIXME: hardcoded stack limit
        if (!(p_TLS_vmthread->interpreter_state & INTERP_STATE_STACK_OVERFLOW)) {
            p_TLS_vmthread->interpreter_state |= INTERP_STATE_STACK_OVERFLOW;
            interp_throw_exception("java/lang/StackOverflowError");
            p_TLS_vmthread->interpreter_state &= ~INTERP_STATE_STACK_OVERFLOW;

            if (frame.framePopListener)
                frame_pop_callback(frame.framePopListener, frame.method, true);
            M2N_FREE_MACRO;
            return;
        }
    }

    if (interpreter_ti_notification_mode
            & INTERPRETER_TI_METHOD_ENTRY_EVENT) {
        M2N_ALLOC_MACRO;
        tmn_suspend_enable();
        method_entry_callback(frame.method);
        tmn_suspend_disable();
        M2N_FREE_MACRO;
    }

    frame.ip = (uint8*) frame.method->get_byte_code_addr();
    if (frame.method->is_synchronized()) {
        MonitorList *ml = (MonitorList*) ALLOC_FRAME(sizeof(MonitorList));
        frame.locked_monitors = ml;
        ml->next = 0;

        if (frame.method->is_static()) {
            ml->monitor = *(frame.method->get_class()->class_handle);
        } else {
            ml->monitor = UNCOMPRESS_REF(frame.locals(0).cr);
        }
        vm_monitor_enter_wrapper(ml->monitor);
    }

    while (true) {
        ManagedObject *exc;
        uint8 ip0 = *frame.ip;

        DEBUG_BYTECODE(endl << "(" << frame.stack.getIndex()
                   << ") " << opcodeNames[ip0] << ": ");

restart:
        if (interpreter_ti_notification_mode) {
            if (frame.jvmti_pop_frame == POP_FRAME_NOW) {
                MonitorList *ml = frame.locked_monitors;
                while(ml) {
                    vm_monitor_exit_wrapper(ml->monitor);
                    ml = ml->next;
                }
                return;
            }
            if (interpreter_ti_notification_mode
                    & INTERPRETER_TI_SINGLE_STEP_EVENT) {
                single_step_callback(frame);
            }
        }

#ifdef INTERPRETER_DEEP_DEBUG
        frame.last_bytecodes[(frame.n_last_bytecode++) & 7] = ip0;
#endif

        assert(!tmn_is_suspend_enabled());
        switch(ip0) {
            case OPCODE_NOP:
                Opcode_NOP(frame); break;

            case OPCODE_ICONST_M1: Opcode_ICONST_M1(frame); break;
            case OPCODE_ACONST_NULL: Opcode_ACONST_NULL(frame); break;
            case OPCODE_ICONST_0: Opcode_ICONST_0(frame); break;
            case OPCODE_ICONST_1: Opcode_ICONST_1(frame); break;
            case OPCODE_ICONST_2: Opcode_ICONST_2(frame); break;
            case OPCODE_ICONST_3: Opcode_ICONST_3(frame); break;
            case OPCODE_ICONST_4: Opcode_ICONST_4(frame); break;
            case OPCODE_ICONST_5: Opcode_ICONST_5(frame); break;
            case OPCODE_FCONST_0: Opcode_FCONST_0(frame); break;
            case OPCODE_FCONST_1: Opcode_FCONST_1(frame); break;
            case OPCODE_FCONST_2: Opcode_FCONST_2(frame); break;

            case OPCODE_LCONST_0: Opcode_LCONST_0(frame); break;
            case OPCODE_LCONST_1: Opcode_LCONST_1(frame); break;
            case OPCODE_DCONST_0: Opcode_DCONST_0(frame); break;
            case OPCODE_DCONST_1: Opcode_DCONST_1(frame); break;

            case OPCODE_ALOAD_0: Opcode_ALOAD_0(frame); break;
            case OPCODE_ALOAD_1: Opcode_ALOAD_1(frame); break;
            case OPCODE_ALOAD_2: Opcode_ALOAD_2(frame); break;
            case OPCODE_ALOAD_3: Opcode_ALOAD_3(frame); break;
            case OPCODE_ILOAD_0: Opcode_ILOAD_0(frame); break;
            case OPCODE_ILOAD_1: Opcode_ILOAD_1(frame); break;
            case OPCODE_ILOAD_2: Opcode_ILOAD_2(frame); break;
            case OPCODE_ILOAD_3: Opcode_ILOAD_3(frame); break;
            case OPCODE_FLOAD_0: Opcode_ILOAD_0(frame); break;
            case OPCODE_FLOAD_1: Opcode_ILOAD_1(frame); break;
            case OPCODE_FLOAD_2: Opcode_ILOAD_2(frame); break;
            case OPCODE_FLOAD_3: Opcode_ILOAD_3(frame); break;

            case OPCODE_ASTORE_0: Opcode_ASTORE_0(frame); break;
            case OPCODE_ASTORE_1: Opcode_ASTORE_1(frame); break;
            case OPCODE_ASTORE_2: Opcode_ASTORE_2(frame); break;
            case OPCODE_ASTORE_3: Opcode_ASTORE_3(frame); break;

            case OPCODE_ISTORE_0: Opcode_ISTORE_0(frame); break;
            case OPCODE_ISTORE_1: Opcode_ISTORE_1(frame); break;
            case OPCODE_ISTORE_2: Opcode_ISTORE_2(frame); break;
            case OPCODE_ISTORE_3: Opcode_ISTORE_3(frame); break;
            case OPCODE_FSTORE_0: Opcode_ISTORE_0(frame); break;
            case OPCODE_FSTORE_1: Opcode_ISTORE_1(frame); break;
            case OPCODE_FSTORE_2: Opcode_ISTORE_2(frame); break;
            case OPCODE_FSTORE_3: Opcode_ISTORE_3(frame); break;

            case OPCODE_ALOAD: Opcode_ALOAD(frame); break;
            case OPCODE_ILOAD: Opcode_ILOAD(frame); break;
            case OPCODE_FLOAD: Opcode_ILOAD(frame); break;

            case OPCODE_ASTORE: Opcode_ASTORE(frame); break;
            case OPCODE_ISTORE: Opcode_ISTORE(frame); break;
            case OPCODE_FSTORE: Opcode_ISTORE(frame); break;

            case OPCODE_LLOAD_0: Opcode_LLOAD_0(frame); break;
            case OPCODE_LLOAD_1: Opcode_LLOAD_1(frame); break;
            case OPCODE_LLOAD_2: Opcode_LLOAD_2(frame); break;
            case OPCODE_LLOAD_3: Opcode_LLOAD_3(frame); break;
            case OPCODE_DLOAD_0: Opcode_LLOAD_0(frame); break;
            case OPCODE_DLOAD_1: Opcode_LLOAD_1(frame); break;
            case OPCODE_DLOAD_2: Opcode_LLOAD_2(frame); break;
            case OPCODE_DLOAD_3: Opcode_LLOAD_3(frame); break;
            case OPCODE_LSTORE_0: Opcode_LSTORE_0(frame); break;
            case OPCODE_LSTORE_1: Opcode_LSTORE_1(frame); break;
            case OPCODE_LSTORE_2: Opcode_LSTORE_2(frame); break;
            case OPCODE_LSTORE_3: Opcode_LSTORE_3(frame); break;
            case OPCODE_DSTORE_0: Opcode_LSTORE_0(frame); break;
            case OPCODE_DSTORE_1: Opcode_LSTORE_1(frame); break;
            case OPCODE_DSTORE_2: Opcode_LSTORE_2(frame); break;
            case OPCODE_DSTORE_3: Opcode_LSTORE_3(frame); break;

            case OPCODE_LLOAD: Opcode_LLOAD(frame); break;
            case OPCODE_DLOAD: Opcode_LLOAD(frame); break;
            case OPCODE_LSTORE: Opcode_LSTORE(frame); break;
            case OPCODE_DSTORE: Opcode_LSTORE(frame); break;

            case OPCODE_IADD: Opcode_IADD(frame); break;
            case OPCODE_ISUB: Opcode_ISUB(frame); break;
            case OPCODE_IMUL: Opcode_IMUL(frame); break;
            case OPCODE_IOR: Opcode_IOR(frame); break;
            case OPCODE_IAND: Opcode_IAND(frame); break;
            case OPCODE_IXOR: Opcode_IXOR(frame); break;
            case OPCODE_ISHL: Opcode_ISHL(frame); break;
            case OPCODE_ISHR: Opcode_ISHR(frame); break;
            case OPCODE_IUSHR: Opcode_IUSHR(frame); break;
            case OPCODE_IDIV: Opcode_IDIV(frame); goto check_exception;
            case OPCODE_IREM: Opcode_IREM(frame); goto check_exception;

            case OPCODE_FADD: Opcode_FADD(frame); break;
            case OPCODE_FSUB: Opcode_FSUB(frame); break;
            case OPCODE_FMUL: Opcode_FMUL(frame); break;
            case OPCODE_FDIV: Opcode_FDIV(frame); break;
            case OPCODE_FREM: Opcode_FREM(frame); break;

            case OPCODE_INEG: Opcode_INEG(frame); break;
            case OPCODE_FNEG: Opcode_FNEG(frame); break;
            case OPCODE_LNEG: Opcode_LNEG(frame); break;
            case OPCODE_DNEG: Opcode_DNEG(frame); break;

            case OPCODE_LADD: Opcode_LADD(frame); break;
            case OPCODE_LSUB: Opcode_LSUB(frame); break;
            case OPCODE_LMUL: Opcode_LMUL(frame); break;
            case OPCODE_LOR: Opcode_LOR(frame); break;
            case OPCODE_LAND: Opcode_LAND(frame); break;
            case OPCODE_LXOR: Opcode_LXOR(frame); break;
            case OPCODE_LSHL: Opcode_LSHL(frame); break;
            case OPCODE_LSHR: Opcode_LSHR(frame); break;
            case OPCODE_LUSHR: Opcode_LUSHR(frame); break;
            case OPCODE_LDIV: Opcode_LDIV(frame); goto check_exception;
            case OPCODE_LREM: Opcode_LREM(frame); goto check_exception;

            case OPCODE_DADD: Opcode_DADD(frame); break;
            case OPCODE_DSUB: Opcode_DSUB(frame); break;
            case OPCODE_DMUL: Opcode_DMUL(frame); break;
            case OPCODE_DDIV: Opcode_DDIV(frame); break;
            case OPCODE_DREM: Opcode_DREM(frame); break;


            case OPCODE_RETURN:
            case OPCODE_IRETURN:
            case OPCODE_FRETURN:
            case OPCODE_ARETURN:
            case OPCODE_LRETURN:
            case OPCODE_DRETURN:
                    if (frame.locked_monitors) {
                        vm_monitor_exit_wrapper(frame.locked_monitors->monitor);
                        assert(!frame.locked_monitors->next);
                    }
                    if (interpreter_ti_notification_mode
                            & INTERPRETER_TI_METHOD_EXIT_EVENT)
                    method_exit_callback_with_frame(frame.method, frame);

                    if (frame.framePopListener)
                        frame_pop_callback(frame.framePopListener, frame.method, false);
            M2N_FREE_MACRO;
                return;

            case OPCODE_BIPUSH: Opcode_BIPUSH(frame); break;
            case OPCODE_SIPUSH: Opcode_SIPUSH(frame); break;

            case OPCODE_IINC: Opcode_IINC(frame); break;
            case OPCODE_POP: Opcode_POP(frame); break;
            case OPCODE_POP2: Opcode_POP2(frame); break;
            case OPCODE_SWAP: Opcode_SWAP(frame); break;
            case OPCODE_DUP: Opcode_DUP(frame); break;
            case OPCODE_DUP2: Opcode_DUP2(frame); break;
            case OPCODE_DUP_X1: Opcode_DUP_X1(frame); break;
            case OPCODE_DUP_X2: Opcode_DUP_X2(frame); break;
            case OPCODE_DUP2_X1: Opcode_DUP2_X1(frame); break;
            case OPCODE_DUP2_X2: Opcode_DUP2_X2(frame); break;

            case OPCODE_MULTIANEWARRAY:
                                Opcode_MULTIANEWARRAY(frame);
                                goto check_exception;
            case OPCODE_ANEWARRAY:
                                Opcode_ANEWARRAY(frame);
                                goto check_exception;
            case OPCODE_NEWARRAY:
                                Opcode_NEWARRAY(frame);
                                goto check_exception;
            case OPCODE_NEW:
                                Opcode_NEW(frame);
                                goto check_exception;

            case OPCODE_ARRAYLENGTH:
                                Opcode_ARRAYLENGTH(frame);
                                goto check_exception;

            case OPCODE_CHECKCAST:
                                Opcode_CHECKCAST(frame);
                                goto check_exception;
            case OPCODE_INSTANCEOF:
                                Opcode_INSTANCEOF(frame);
                                goto check_exception;

            case OPCODE_ATHROW:
                                Opcode_ATHROW(frame);
                                exc = get_current_thread_exception();
                                goto got_exception;

            case OPCODE_MONITORENTER:
                                {
                                    MonitorList *new_ml = frame.free_monitors;
                                    if (new_ml) {
                                        frame.free_monitors = new_ml->next;
                                    } else {
                                        new_ml = (MonitorList*) ALLOC_FRAME(sizeof(MonitorList));
                                    }
                                    new_ml->next = frame.locked_monitors;
                                    new_ml->monitor = NULL;
                                    frame.locked_monitors = new_ml;
                                    Opcode_MONITORENTER(frame);
                                    exc = get_current_thread_exception();
                                    if (exc != 0) {
                                        frame.locked_monitors = new_ml->next;
                                        new_ml->next = frame.free_monitors;
                                        frame.free_monitors = new_ml;
                                        goto got_exception;
                                    }
                                }
                                break;
            case OPCODE_MONITOREXIT:
                                Opcode_MONITOREXIT(frame);
                                goto check_exception;

            case OPCODE_BASTORE: Opcode_BASTORE(frame); goto check_exception;
            case OPCODE_CASTORE: Opcode_CASTORE(frame); goto check_exception;
            case OPCODE_SASTORE: Opcode_SASTORE(frame); goto check_exception;
            case OPCODE_IASTORE: Opcode_IASTORE(frame); goto check_exception;
            case OPCODE_FASTORE: Opcode_FASTORE(frame); goto check_exception;
            case OPCODE_AASTORE: Opcode_AASTORE(frame); goto check_exception;

            case OPCODE_BALOAD: Opcode_BALOAD(frame); goto check_exception;
            case OPCODE_CALOAD: Opcode_CALOAD(frame); goto check_exception;
            case OPCODE_SALOAD: Opcode_SALOAD(frame); goto check_exception;
            case OPCODE_IALOAD: Opcode_IALOAD(frame); goto check_exception;
            case OPCODE_FALOAD: Opcode_FALOAD(frame); goto check_exception;
            case OPCODE_AALOAD: Opcode_AALOAD(frame); goto check_exception;
                                
            case OPCODE_LASTORE: Opcode_LASTORE(frame); goto check_exception;
            case OPCODE_DASTORE: Opcode_LASTORE(frame); goto check_exception;

            case OPCODE_LALOAD: Opcode_LALOAD(frame); goto check_exception;
            case OPCODE_DALOAD: Opcode_LALOAD(frame); goto check_exception;

            case OPCODE_TABLESWITCH: Opcode_TABLESWITCH(frame); break;
            case OPCODE_LOOKUPSWITCH: Opcode_LOOKUPSWITCH(frame); break;
            case OPCODE_GOTO: Opcode_GOTO(frame); break;
            case OPCODE_GOTO_W: Opcode_GOTO_W(frame); break;
            case OPCODE_JSR: Opcode_JSR(frame); break;
            case OPCODE_JSR_W: Opcode_JSR_W(frame); break;
            case OPCODE_RET: Opcode_RET(frame); break;
            case OPCODE_PUTSTATIC:
                                Opcode_PUTSTATIC(frame);
                                goto check_exception;
            case OPCODE_GETSTATIC:
                                Opcode_GETSTATIC(frame);
                                goto check_exception;

            case OPCODE_PUTFIELD:
                                Opcode_PUTFIELD(frame);
                                goto check_exception;
            case OPCODE_GETFIELD:
                                Opcode_GETFIELD(frame);
                                goto check_exception;
            case OPCODE_INVOKESTATIC:
                                Opcode_INVOKESTATIC(frame);
                                exc = get_current_thread_exception();
                                if (exc != 0) goto got_exception;
                                frame.ip += 3;
                                break;

            case OPCODE_INVOKESPECIAL:
                                Opcode_INVOKESPECIAL(frame);
                                exc = get_current_thread_exception();
                                if (exc != 0) goto got_exception;
                                frame.ip += 3;
                                break;

            case OPCODE_INVOKEVIRTUAL:
                                Opcode_INVOKEVIRTUAL(frame);
                                exc = get_current_thread_exception();
                                if (exc != 0) goto got_exception;
                                frame.ip += 3;
                                break;

            case OPCODE_INVOKEINTERFACE:
                                Opcode_INVOKEINTERFACE(frame);
                                exc = get_current_thread_exception();
                                if (exc != 0) goto got_exception;
                                frame.ip += 5;
                                break;

            case OPCODE_LDC: Opcode_LDC(frame); goto check_exception;
            case OPCODE_LDC_W: Opcode_LDC_W(frame); goto check_exception;
            case OPCODE_LDC2_W: Opcode_LDC2_W(frame); break;

            case OPCODE_IFNULL:
            case OPCODE_IFEQ: Opcode_IFEQ(frame); break;

            case OPCODE_IFNONNULL:
            case OPCODE_IFNE: Opcode_IFNE(frame); break;
            case OPCODE_IFGE: Opcode_IFGE(frame); break;
            case OPCODE_IFGT: Opcode_IFGT(frame); break;
            case OPCODE_IFLE: Opcode_IFLE(frame); break;
            case OPCODE_IFLT: Opcode_IFLT(frame); break;

            case OPCODE_IF_ACMPEQ:
            case OPCODE_IF_ICMPEQ: Opcode_IF_ICMPEQ(frame); break;

            case OPCODE_IF_ACMPNE:
            case OPCODE_IF_ICMPNE: Opcode_IF_ICMPNE(frame); break;

            case OPCODE_IF_ICMPGE: Opcode_IF_ICMPGE(frame); break;
            case OPCODE_IF_ICMPGT: Opcode_IF_ICMPGT(frame); break;
            case OPCODE_IF_ICMPLE: Opcode_IF_ICMPLE(frame); break;
            case OPCODE_IF_ICMPLT: Opcode_IF_ICMPLT(frame); break;

            case OPCODE_FCMPL: Opcode_FCMPL(frame); break;
            case OPCODE_FCMPG: Opcode_FCMPG(frame); break;
            case OPCODE_DCMPL: Opcode_DCMPL(frame); break;
            case OPCODE_DCMPG: Opcode_DCMPG(frame); break;

            case OPCODE_LCMP: Opcode_LCMP(frame); break;

            case OPCODE_I2L: Opcode_I2L(frame); break;
            case OPCODE_I2D: Opcode_I2D(frame); break;
            case OPCODE_F2L: Opcode_F2L(frame); break;
            case OPCODE_F2D: Opcode_F2D(frame); break;
            case OPCODE_F2I: Opcode_F2I(frame); break;
            case OPCODE_I2F: Opcode_I2F(frame); break;
            case OPCODE_I2B: Opcode_I2B(frame); break;
            case OPCODE_I2S: Opcode_I2S(frame); break;
            case OPCODE_I2C: Opcode_I2C(frame); break;
            case OPCODE_D2F: Opcode_D2F(frame); break;
            case OPCODE_D2I: Opcode_D2I(frame); break;
            case OPCODE_L2F: Opcode_L2F(frame); break;
            case OPCODE_L2I: Opcode_L2I(frame); break;
            case OPCODE_L2D: Opcode_L2D(frame); break;
            case OPCODE_D2L: Opcode_D2L(frame); break;
            case OPCODE_BREAKPOINT:
                             ip0 = Opcode_BREAKPOINT(frame);
                             goto restart;

            case OPCODE_WIDE:
            {
                uint8* ip1 = frame.ip + 1;
                switch(*ip1) {
                    case OPCODE_ALOAD: Opcode_WIDE_ALOAD(frame); break;
                    case OPCODE_ILOAD: Opcode_WIDE_ILOAD(frame); break;
                    case OPCODE_FLOAD: Opcode_WIDE_ILOAD(frame); break;
                    case OPCODE_DLOAD: Opcode_WIDE_LLOAD(frame); break;
                    case OPCODE_LLOAD: Opcode_WIDE_LLOAD(frame); break;
                    case OPCODE_ASTORE: Opcode_WIDE_ASTORE(frame); break;
                    case OPCODE_ISTORE: Opcode_WIDE_ISTORE(frame); break;
                    case OPCODE_FSTORE: Opcode_WIDE_ISTORE(frame); break;
                    case OPCODE_LSTORE: Opcode_WIDE_LSTORE(frame); break;
                    case OPCODE_DSTORE: Opcode_WIDE_LSTORE(frame); break;
                    case OPCODE_IINC: Opcode_WIDE_IINC(frame); break;
                    case OPCODE_RET: Opcode_WIDE_RET(frame); break;
                    default:
                     DEBUG2("wide bytecode 0x" << hex << (int)*ip1 << " not implemented\n");
                     stackDump(frame);
                     ABORT("Unexpected wide bytecode");
                }
                break;
            }

            default: DEBUG2("bytecode 0x" << hex << (int)ip0 << " not implemented\n");
                     stackDump(frame);
                     ABORT("Unexpected bytecode");
        }
        continue;

check_exception:
        exc = get_current_thread_exception();
        if (exc == 0) continue;
got_exception:
        if (p_TLS_vmthread->ti_exception_callback_pending) {

            assert(exn_raised());
            assert(!tmn_is_suspend_enabled());
            jvmti_interpreter_exception_event_callback_call();
            assert(!tmn_is_suspend_enabled());

            exc = get_current_thread_exception();

            // is exception cleared in JVMTI?
            if (!exc) continue;
        }

        frame.exception = exc;
        clear_current_thread_exception();

        if (processExceptionHandler(frame, &frame.exception)) {
            frame.stack.clear();
            frame.stack.push();
            frame.stack.pick().cr = COMPRESS_REF(frame.exception);
            frame.stack.ref() = FLAG_OBJECT;
            frame.exception = 0;
            continue;
        }
        set_current_thread_exception(frame.exception);
        p_TLS_vmthread->ti_exception_callback_pending = false;
        
        if (frame.locked_monitors) {
            vm_monitor_exit_wrapper(frame.locked_monitors->monitor);
            assert(!frame.locked_monitors->next);
        }
        DEBUG_TRACE("<EXCEPTION> ");
        if (interpreter_ti_notification_mode
                & INTERPRETER_TI_METHOD_EXIT_EVENT)
            method_exit_callback_with_frame(frame.method, frame);

        if (frame.framePopListener)
            frame_pop_callback(frame.framePopListener, frame.method, true);
        M2N_FREE_MACRO;
        assert(!tmn_is_suspend_enabled());
        return;
    }
}

void
interpreter_execute_method(
        Method   *method,
        jvalue   *return_value,
        jvalue   *args) {

    assert(!tmn_is_suspend_enabled());

    StackFrame frame;
    memset(&frame, 0, sizeof(frame));
    frame.method = method;
    frame.prev = getLastStackFrame();

    // TODO: not optimal to call is_static twice.
    if (!method->is_static()) {
            frame.This = args[0].l->object;
    }
    if (frame.prev == 0) {
        p_TLS_vmthread->firstFrame = (void*) &frame;
    }
    setLastStackFrame(&frame);

    if (method->is_native()) {
        interpreter_execute_native_method(method, return_value, args);
        setLastStackFrame(frame.prev);
        return;
    }

    DEBUG_TRACE("\n{{{ interpreter_invoke: "
           << method->get_class()->name->bytes << " "
           << method->get_name()->bytes
           << method->get_descriptor()->bytes << endl);
    
    DEBUG("\tmax stack = " << method->get_max_stack() << endl);
    DEBUG("\tmax locals = " << method->get_max_locals() << endl);

    // Setup locals and stack on C stack.
    SETUP_LOCALS_AND_STACK(frame, method);

    int pos = 0;
    int arg = 0;

    if(!method->is_static()) {
        frame.locals.ref(pos) = FLAG_OBJECT;
        // COMPRESS_REF will fail assertion if obj == 0 on ipf
        CREF cr = 0;
        ObjectHandle h = (ObjectHandle) args[arg++].l;
        if (h) {
            cr = COMPRESS_REF(h->object);
            frame.This = h->object;
        }
        frame.locals(pos++).cr = cr;
    }

    Arg_List_Iterator iter = method->get_argument_list();

    Java_Type typ;
    DEBUG("\targs types = ");
    while((typ = curr_arg(iter)) != JAVA_TYPE_END) {
        DEBUG((char)typ);
        ObjectHandle h;

        switch(typ) {
        case JAVA_TYPE_LONG:
        case JAVA_TYPE_DOUBLE:
            Value2 val;
            val.i64 = args[arg++].j;
            frame.locals.setLong(pos, val);
            pos += 2;
            break;
        case JAVA_TYPE_CLASS:
        case JAVA_TYPE_ARRAY:
            h = (ObjectHandle) args[arg++].l;
#ifdef COMPRESS_MODE
            assert(VM_Global_State::loader_env->compress_references);
#endif
            CREF cref;
            if (h == NULL) {
                cref = 0;
            } else {
                cref = COMPRESS_REF(h->object);
            }
            frame.locals.ref(pos) = FLAG_OBJECT;
            frame.locals(pos++).cr = cref;
            ASSERT_OBJECT(UNCOMPRESS_REF(cref));
            break;
        case JAVA_TYPE_FLOAT:
            frame.locals(pos++).f = args[arg++].f;
            break;
        case JAVA_TYPE_SHORT:
            frame.locals(pos++).i = (int32)args[arg++].s;
            break;
        case JAVA_TYPE_CHAR:
            frame.locals(pos++).i = (uint32)args[arg++].c;
            break;
        case JAVA_TYPE_BYTE:
            frame.locals(pos++).i = (int32)args[arg++].b;
            break;
        case JAVA_TYPE_BOOLEAN:
            frame.locals(pos++).i = (int32)args[arg++].z;
            break;
        default:
            frame.locals(pos++).u = (uint32)args[arg++].i;
            break;
        }
        iter = advance_arg_iterator(iter);
    }
    DEBUG(endl);

    Java_Type ret_type = method->get_return_java_type();
    
    interpreter(frame);

    jvalue *resultPtr = (jvalue*) return_value;

    /**
      gwu2: We should clear the result before checking exception,
      otherwise we could exit this method with some undefined result,
      JNI wrappers like CallObjectMethod will allocate object handle 
      according to the value of result, so an object handle refering wild
      pointer will be inserted into local object list, and will be 
      enumerated,... It'll definitely fail.
    */
    if ((resultPtr != NULL) && (ret_type != JAVA_TYPE_VOID)) { 
        resultPtr->l = 0; //clear it  
    }
                                                     
    if (exn_raised()) {
        setLastStackFrame(frame.prev);
        DEBUG_TRACE("<EXCEPTION> interpreter_invoke }}}\n");
        return;
    }

    switch(ret_type) {
        case JAVA_TYPE_LONG:
        case JAVA_TYPE_DOUBLE:
            {
                Value2 val;
                val = frame.stack.getLong(0);
                resultPtr->j = val.i64;
            }
            break;

        case JAVA_TYPE_FLOAT:
            resultPtr->f = frame.stack.pick().f;
            break;

        case JAVA_TYPE_VOID:
            break;

        case JAVA_TYPE_BOOLEAN:
        case JAVA_TYPE_BYTE:
        case JAVA_TYPE_CHAR:
        case JAVA_TYPE_SHORT:
        case JAVA_TYPE_INT:
        case VM_DATA_TYPE_MP:
        case VM_DATA_TYPE_UP:
            resultPtr->i = frame.stack.pick().i;
            break;

        case JAVA_TYPE_CLASS:
        case JAVA_TYPE_ARRAY:
        case JAVA_TYPE_STRING:
         
            { 
                ASSERT_OBJECT(UNCOMPRESS_REF(frame.stack.pick().cr));
                ObjectHandle h = 0; 
                if (frame.stack.pick().cr) {
                    h = oh_allocate_local_handle();
                    h->object = UNCOMPRESS_REF(frame.stack.pick().cr);
                } 
                resultPtr->l = h;
            }
            break;

        default:
            ABORT("Unexpected java type");
    }
    setLastStackFrame(frame.prev);
    DEBUG_TRACE("interpreter_invoke }}}\n");
}

static void
interpreterInvokeStatic(StackFrame& prevFrame, Method *method) {

    StackFrame frame;
    memset(&frame, 0, sizeof(frame));
    frame.prev = &prevFrame;
    frame.method = method;
    assert(frame.prev == getLastStackFrame());
    setLastStackFrame(&frame);

    if(method->is_native()) {
        interpreterInvokeStaticNative(prevFrame, frame, method);
        setLastStackFrame(frame.prev);
        return;
    }

    DEBUG_TRACE("\n{{{ invoke_static     : "
           << method->get_class()->name->bytes << " "
           << method->get_name()->bytes
           << method->get_descriptor()->bytes << endl);


    DEBUG("\tmax stack = " << method->get_max_stack() << endl);
    DEBUG("\tmax locals = " << method->get_max_locals() << endl);

    // Setup locals and stack on C stack.
    SETUP_LOCALS_AND_STACK(frame, method);

    int args = method->get_num_arg_bytes() >> 2;

    for(int i = args-1; i >= 0; --i) {
        frame.locals(i) = prevFrame.stack.pick(args-1 - i);
        frame.locals.ref(i) = prevFrame.stack.ref(args-1 - i);
    }

    frame.jvmti_pop_frame = POP_FRAME_AVAILABLE;
    
    interpreter(frame);

    if (frame.jvmti_pop_frame == POP_FRAME_NOW) {
        setLastStackFrame(frame.prev);
        clear_current_thread_exception();
        frame.ip -= 3;
        DEBUG_TRACE("<POP_FRAME> invoke_static }}}\n");
        return;
    }

    prevFrame.stack.popClearRef(args);

    if (exn_raised()) {
        setLastStackFrame(frame.prev);
        DEBUG_TRACE("<EXCEPTION> invoke_static }}}\n");
        return;
    }

    switch(method->get_return_java_type()) {
        case JAVA_TYPE_VOID:
            break;

        case JAVA_TYPE_CLASS:
        case JAVA_TYPE_ARRAY:
        case JAVA_TYPE_STRING:
            prevFrame.stack.push();
            prevFrame.stack.pick() = frame.stack.pick();
            prevFrame.stack.ref() = FLAG_OBJECT;
            break;

        case JAVA_TYPE_FLOAT:
        case JAVA_TYPE_BOOLEAN:
        case JAVA_TYPE_BYTE:
        case JAVA_TYPE_CHAR:
        case JAVA_TYPE_SHORT:
        case JAVA_TYPE_INT:
            prevFrame.stack.push();
            prevFrame.stack.pick() = frame.stack.pick();
            break;

        case JAVA_TYPE_LONG:
        case JAVA_TYPE_DOUBLE:
            prevFrame.stack.push(2);
            prevFrame.stack.pick(s0) = frame.stack.pick(s0);
            prevFrame.stack.pick(s1) = frame.stack.pick(s1);
            break;

        default:
            ABORT("Unexpected java type");
    }

    setLastStackFrame(frame.prev);
    DEBUG_TRACE("invoke_static }}}\n");
}

static void
interpreterInvoke(StackFrame& prevFrame, Method *method, int args, ManagedObject *obj, bool intf) {

    StackFrame frame;
    memset(&frame, 0, sizeof(frame));
    frame.prev = &prevFrame;
    assert(frame.prev == getLastStackFrame());
    frame.method = method;
    frame.This = obj;
#ifndef NDEBUG
    frame.dump_bytecodes = prevFrame.dump_bytecodes;
#endif
    setLastStackFrame(&frame);


    if(method->is_native()) {
        interpreterInvokeVirtualNative(prevFrame, frame, method, args);
        setLastStackFrame(frame.prev);
        return;
    }

    DEBUG("\tmax stack = " << method->get_max_stack() << endl);
    DEBUG("\tmax locals = " << method->get_max_locals() << endl);

    // Setup locals and stack on C stack.
    SETUP_LOCALS_AND_STACK(frame, method);

    for(int i = args-1; i >= 0; --i) {
        frame.locals(i) = prevFrame.stack.pick(args-1 - i);
        frame.locals.ref(i) = prevFrame.stack.ref(args-1 - i);
    }


    frame.jvmti_pop_frame = POP_FRAME_AVAILABLE;

    interpreter(frame);

    if (frame.jvmti_pop_frame == POP_FRAME_NOW) {
        setLastStackFrame(frame.prev);
        clear_current_thread_exception();
        if (intf) frame.prev->ip -= 5; else frame.prev->ip -= 3;
        return;
    }

    prevFrame.stack.popClearRef(args);

    if (exn_raised()) {
        setLastStackFrame(frame.prev);
        return;
    }

    switch(method->get_return_java_type()) {
        case JAVA_TYPE_VOID:
            break;

        case JAVA_TYPE_CLASS:
        case JAVA_TYPE_ARRAY:
        case JAVA_TYPE_STRING:
            prevFrame.stack.push();
            prevFrame.stack.pick() = frame.stack.pick();
            prevFrame.stack.ref() = FLAG_OBJECT;
            break;

        case JAVA_TYPE_FLOAT:
        case JAVA_TYPE_BOOLEAN:
        case JAVA_TYPE_BYTE:
        case JAVA_TYPE_CHAR:
        case JAVA_TYPE_SHORT:
        case JAVA_TYPE_INT:
            prevFrame.stack.push();
            prevFrame.stack.pick() = frame.stack.pick();
            break;

        case JAVA_TYPE_LONG:
        case JAVA_TYPE_DOUBLE:
            prevFrame.stack.push(2);
            prevFrame.stack.pick(s0) = frame.stack.pick(s0);
            prevFrame.stack.pick(s1) = frame.stack.pick(s1);
            break;

        default:
            ABORT("Unexpected java type");
    }

    setLastStackFrame(frame.prev);
}

static void
interpreterInvokeVirtual(StackFrame& prevFrame, Method *method) {

    int args = method->get_num_arg_bytes() >> 2;
    CREF cr = prevFrame.stack.pick(args-1).cr;

    if (cr == 0) {
        throwNPE();
        return;
    }

    ManagedObject *obj = UNCOMPRESS_REF(cr);
    ASSERT_OBJECT(obj);

    Class *objClass = obj->vt()->clss;
    method = objClass->vtable_descriptors[method->get_index()];

    if (method->is_abstract()) {
        throwAME();
        return;
    }

    DEBUG_TRACE("\n{{{ invoke_virtual    : "
           << method->get_class()->name->bytes << " "
           << method->get_name()->bytes
           << method->get_descriptor()->bytes << endl);

    interpreterInvoke(prevFrame, method, args, obj, false);
    DEBUG_TRACE("invoke_virtual }}}\n");
}

static void
interpreterInvokeInterface(StackFrame& prevFrame, Method *method) {

    int args = method->get_num_arg_bytes() >> 2;
    CREF cr = prevFrame.stack.pick(args-1).cr;

    if (cr == 0) {
        throwNPE();
        return;
    }

    ManagedObject *obj = UNCOMPRESS_REF(cr);
    ASSERT_OBJECT(obj);

    if (!vm_instanceof(obj, method->get_class())) {
        interp_throw_exception("java/lang/IncompatibleClassChangeError",
            method->get_class()->name->bytes);
        return;
    }

    Class *clss = obj->vt()->clss;
    Method* found_method = class_lookup_method_recursive(clss, method->get_name(), method->get_descriptor());

    if (found_method == NULL) {
        interp_throw_exception("java/lang/AbstractMethodError",
            method->get_name()->bytes);
        return;
    }
    method = found_method;

    if (method->is_abstract()) {
        throwAME();
        return;
    }

    if (!method->is_public()) {
        throwIAE();
        return;
    }

    DEBUG_TRACE("\n{{{ invoke_interface  : "
           << method->get_class()->name->bytes << " "
           << method->get_name()->bytes
           << method->get_descriptor()->bytes << endl);

    interpreterInvoke(prevFrame, method, args, obj, true);

    DEBUG_TRACE("invoke_interface }}}\n");
}

static void
interpreterInvokeSpecial(StackFrame& prevFrame, Method *method) {

    int args = method->get_num_arg_bytes() >> 2;
    CREF cr = prevFrame.stack.pick(args-1).cr;

    if (cr == 0) {
        throwNPE();
        return;
    }

    if (method->is_abstract()) {
        throwAME();
        return;
    }

    ManagedObject *obj = UNCOMPRESS_REF(cr);

    StackFrame frame;
    memset(&frame, 0, sizeof(frame));
    frame.prev = &prevFrame;
    assert(frame.prev == getLastStackFrame());
    frame.method = method;
    frame.This = obj;
    setLastStackFrame(&frame);

    if(method->is_native()) {
        interpreterInvokeVirtualNative(prevFrame, frame, method, args);

        setLastStackFrame(frame.prev);
        return;
    }

    DEBUG_TRACE("\n{{{ invoke_special    : "
           << method->get_class()->name->bytes << " "
           << method->get_name()->bytes
           << method->get_descriptor()->bytes << endl);

    DEBUG("\tmax stack = " << method->get_max_stack() << endl);
    DEBUG("\tmax locals = " << method->get_max_locals() << endl);

    // Setup locals and stack on C stack.
    SETUP_LOCALS_AND_STACK(frame, method);

    for(int i = args-1; i >= 0; --i) {
        frame.locals(i) = prevFrame.stack.pick(args-1 - i);
        frame.locals.ref(i) = prevFrame.stack.ref(args-1 - i);
    }

    frame.jvmti_pop_frame = POP_FRAME_AVAILABLE;

    interpreter(frame);

    if (frame.jvmti_pop_frame == POP_FRAME_NOW) {
        setLastStackFrame(frame.prev);
        clear_current_thread_exception();
        frame.ip -= 3;
        DEBUG_TRACE("<POP_FRAME> invoke_special }}}\n");
        return;
    }

    prevFrame.stack.popClearRef(args);

    if (exn_raised()) {
        setLastStackFrame(frame.prev);
        DEBUG_TRACE("<EXCEPTION> invoke_special }}}\n");
        return;
    }

    switch(method->get_return_java_type()) {
        case JAVA_TYPE_VOID:
            break;

        case JAVA_TYPE_CLASS:
        case JAVA_TYPE_ARRAY:
        case JAVA_TYPE_STRING:
            prevFrame.stack.push();
            prevFrame.stack.pick() = frame.stack.pick();
            prevFrame.stack.ref() = FLAG_OBJECT;
            break;

        case JAVA_TYPE_FLOAT:
        case JAVA_TYPE_BOOLEAN:
        case JAVA_TYPE_BYTE:
        case JAVA_TYPE_CHAR:
        case JAVA_TYPE_SHORT:
        case JAVA_TYPE_INT:
            prevFrame.stack.push();
            prevFrame.stack.pick() = frame.stack.pick();
            break;

        case JAVA_TYPE_LONG:
        case JAVA_TYPE_DOUBLE:
            prevFrame.stack.push(2);
            prevFrame.stack.pick(s0) = frame.stack.pick(s0);
            prevFrame.stack.pick(s1) = frame.stack.pick(s1);
            break;

        default:
            ABORT("Unexpected java type");
    }
    setLastStackFrame(frame.prev);
    DEBUG_TRACE("invoke_special }}}\n");
}

GenericFunctionPointer
interpreterGetNativeMethodAddr(Method* method) {

    assert(method->is_native());

    if (method->get_state() == Method::ST_Linked ) {
        return (GenericFunctionPointer) method->get_code_addr();
    }

    GenericFunctionPointer f = interp_find_native(method);

    // TODO: check if we need synchronization here
    if( f ) {
        method->set_code_addr((NativeCodePtr) f);
        method->set_state( Method::ST_Linked );
    }

    return f;
}
