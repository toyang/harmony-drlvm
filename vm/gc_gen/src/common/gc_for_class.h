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
 * @author Xiao-Feng Li, 2006/10/05
 */

#ifndef _GC_TYPES_H_
#define _GC_TYPES_H_

#include "open/types.h"
#include "gc_platform.h"

/* CONST_MARK_BIT is used in mark_scan in vt, no matter MARK_BIT_FLIPPING used or not. 
   MARK_BIT_FLIPPING is used in oi for marking and forwarding in non-gen nursery forwarding
   (the marking is for those objects not in nos.)
   For gen mode, we can use or not use MARK_BIT_FLIPPING, because we never mark any object not
   in nos. And for live objects in nos, its bits are reset when forwared. So there is no need 
   to use a lower-performance bit flipping in gen mode.
   When MARK_BIT_FLIPPING is defined, all configurations are working.
   If it is not defined, we can't run one configuration: non-gen-mode nos-trace-forwarding. We have 
   to run nos-mark-forwarding/copying which has an extra pass to reset the mark bit.
   
   Important invariants:
   1. We never put forwarding pointer in vt. 
   2. Forwarding pointer only exists during collection. No obj has fw (or fw_bit) in oi during execution.
   3. During app execution, no obj has mark_bit set without MARK_BIT_FLIPPING defined.
   
*/
#define CONST_MARK_BIT 0x1

#define DUAL_MARKBITS  0x3
#define DUAL_MARKBITS_MASK  (~DUAL_MARKBITS)

#define MARK_BIT_FLIPPING

#ifdef MARK_BIT_FLIPPING

  extern unsigned int Cur_Mark_Bit;
  extern unsigned int Cur_Forward_Bit;
  #define FLIP_MARK_BIT Cur_Mark_Bit
  #define FLIP_FORWARD_BIT Cur_Forward_Bit

  #define FORWARD_BIT FLIP_FORWARD_BIT

#else  /* #ifdef MARK_BIT_FLIPPING*/

  #define CONST_FORWARD_BIT 0x2
  #define FORWARD_BIT CONST_FORWARD_BIT

#endif /* else MARK_BIT_FLIPPING */

typedef void *Thread_Handle; 

#define GC_CLASS_FLAG_FINALIZER 1
#define GC_CLASS_FLAG_ARRAY 2
#define GC_CLASS_FLAG_REFS 4
#define GC_CLASS_IS_REF_ARRAY (GC_CLASS_FLAG_ARRAY|GC_CLASS_FLAG_REFS)
#define GC_CLASS_FLAGS_MASK (~(GC_CLASS_IS_REF_ARRAY|GC_CLASS_FLAG_FINALIZER))

#define GC_OBJECT_ALIGN_MASK (GC_OBJECT_ALIGNMENT-1)
#define GCVT_ALIGNMENT 8
#define GCVT_ALIGN_MASK (GCVT_ALIGNMENT-1)

typedef POINTER_SIZE_INT Obj_Info_Type;

typedef struct GC_VTable_Info {

  unsigned int gc_number_of_ref_fields;

  uint32 gc_class_properties;    // This is the same as class_properties in VM's VTable.

  unsigned int gc_allocated_size;

  unsigned int array_elem_size;

  // This is the offset from the start of the object to the first element in the
  // array. It isn't a constant since we pad double words.
  int array_first_elem_offset;

  // The GC needs access to the class name for debugging and for collecting information
  // about the allocation behavior of certain classes. Store the name of the class here.
  const char *gc_class_name;
  Class_Handle gc_clss;

  // This array holds an array of offsets to the pointer fields in
  // an instance of this class, including or not the weak referent field depending on compilation option
  int gc_ref_offset_array[1];
  
} GC_VTable_Info;

typedef struct Partial_Reveal_VTable {
  GC_VTable_Info *gcvt;
} Partial_Reveal_VTable;

typedef struct Partial_Reveal_Object {
  Partial_Reveal_VTable *vt_raw;
  Obj_Info_Type obj_info;
} Partial_Reveal_Object;

typedef struct Partial_Reveal_Array {
  Partial_Reveal_VTable *vt_raw;
  Obj_Info_Type obj_info;
  unsigned int array_len;
} Partial_Reveal_Array;

inline Obj_Info_Type get_obj_info_raw(Partial_Reveal_Object *obj) 
{  assert(obj); return obj->obj_info; }

#ifndef MARK_BIT_FLIPPING

inline Obj_Info_Type get_obj_info(Partial_Reveal_Object *obj) 
{  assert(obj); return obj->obj_info & ~CONST_MARK_BIT; }

#else

inline Obj_Info_Type get_obj_info(Partial_Reveal_Object *obj) 
{  assert(obj); return obj->obj_info & DUAL_MARKBITS_MASK; }

#endif /* MARK_BIT_FLIPPING */

inline void set_obj_info(Partial_Reveal_Object *obj, Obj_Info_Type new_obj_info) 
{  assert(obj); obj->obj_info = new_obj_info; }

inline Obj_Info_Type *get_obj_info_addr(Partial_Reveal_Object *obj) 
{  assert(obj); return &obj->obj_info; }

inline Partial_Reveal_VTable *obj_get_vt_raw(Partial_Reveal_Object *obj) 
{  assert(obj && obj->vt_raw); return obj->vt_raw; }

inline Partial_Reveal_VTable **obj_get_vt_addr(Partial_Reveal_Object *obj) 
{  assert(obj && obj->vt_raw); return &obj->vt_raw; }

inline Partial_Reveal_VTable *obj_get_vt(Partial_Reveal_Object *obj) 
{  assert(obj && obj->vt_raw); return (Partial_Reveal_VTable *)((POINTER_SIZE_INT)obj->vt_raw & ~CONST_MARK_BIT); }

inline void obj_set_vt(Partial_Reveal_Object *obj, Allocation_Handle ah) 
{  assert(obj && ah); obj->vt_raw = (Partial_Reveal_VTable *)ah; }

inline GC_VTable_Info *vtable_get_gcvt_raw(Partial_Reveal_VTable *vt) 
{  assert(vt && vt->gcvt); return vt->gcvt; }

inline GC_VTable_Info *vtable_get_gcvt(Partial_Reveal_VTable *vt) 
{  assert(vt && vt->gcvt); return (GC_VTable_Info*)((POINTER_SIZE_INT)vt->gcvt & GC_CLASS_FLAGS_MASK); }

inline void vtable_set_gcvt(Partial_Reveal_VTable *vt, GC_VTable_Info *new_gcvt) 
{  assert(vt && new_gcvt); vt->gcvt = new_gcvt; }

inline GC_VTable_Info *obj_get_gcvt_raw(Partial_Reveal_Object *obj) 
{
  Partial_Reveal_VTable *vt = obj_get_vt(obj);
  return vtable_get_gcvt_raw(vt);
}

inline GC_VTable_Info *obj_get_gcvt(Partial_Reveal_Object *obj) 
{
  Partial_Reveal_VTable *vt = obj_get_vt(obj);
  return vtable_get_gcvt(vt);
}

inline Boolean object_has_ref_field(Partial_Reveal_Object *obj) 
{
  GC_VTable_Info *gcvt = obj_get_gcvt_raw(obj);
  return (POINTER_SIZE_INT)gcvt & GC_CLASS_FLAG_REFS;   
}

inline Boolean object_has_ref_field_before_scan(Partial_Reveal_Object *obj) 
{
  Partial_Reveal_VTable *vt = obj_get_vt_raw(obj);  
  GC_VTable_Info *gcvt = vtable_get_gcvt_raw(vt);
  return (POINTER_SIZE_INT)gcvt & GC_CLASS_FLAG_REFS;   
}

inline unsigned int object_ref_field_num(Partial_Reveal_Object *obj) 
{
  GC_VTable_Info *gcvt = obj_get_gcvt(obj);
  return gcvt->gc_number_of_ref_fields;   
}

inline Boolean object_is_array(Partial_Reveal_Object *obj) 
{
  GC_VTable_Info *gcvt = obj_get_gcvt_raw(obj);
  return ((POINTER_SIZE_INT)gcvt & GC_CLASS_FLAG_ARRAY);
} 

inline Boolean obj_is_primitive_array(Partial_Reveal_Object *obj) 
{
  return object_is_array(obj) && !object_has_ref_field(obj);
}

inline Class_Handle obj_get_class_handle(Partial_Reveal_Object *obj) 
{
  GC_VTable_Info *gcvt = obj_get_gcvt(obj);  
  return gcvt->gc_clss;
}

inline unsigned int nonarray_object_size(Partial_Reveal_Object *obj) 
{
  GC_VTable_Info *gcvt = obj_get_gcvt(obj);  
  return gcvt->gc_allocated_size;
}

inline unsigned int array_first_element_offset(Partial_Reveal_Array *obj)
{ 
  GC_VTable_Info *gcvt = obj_get_gcvt((Partial_Reveal_Object*)obj);
  return gcvt->array_first_elem_offset;
}

inline unsigned int array_object_size(Partial_Reveal_Object *obj) 
{
  GC_VTable_Info *gcvt = obj_get_gcvt(obj);  
  int array_len = ((Partial_Reveal_Array*)obj)->array_len;
  return (gcvt->array_first_elem_offset + gcvt->array_elem_size * array_len + GC_OBJECT_ALIGN_MASK) & (~GC_OBJECT_ALIGN_MASK);
}

inline unsigned int vm_object_size(Partial_Reveal_Object *obj)
{
  Boolean is_array = object_is_array(obj);
  return is_array? array_object_size(obj) : nonarray_object_size(obj);
}

#define CL_PROP_REFERENCE_TYPE_SHIFT 16
#define CL_PROP_REFERENCE_TYPE_MASK 0x00030000

inline WeakReferenceType special_reference_type(Partial_Reveal_Object *p_reference)
{
  GC_VTable_Info *gcvt = obj_get_gcvt(p_reference);
  return (WeakReferenceType)((gcvt->gc_class_properties & CL_PROP_REFERENCE_TYPE_MASK) >> CL_PROP_REFERENCE_TYPE_SHIFT);
}

inline Boolean type_has_finalizer(Partial_Reveal_VTable *vt)
{
  GC_VTable_Info *gcvt = vtable_get_gcvt_raw(vt);
  return (POINTER_SIZE_INT)gcvt & GC_CLASS_FLAG_FINALIZER;
}

#endif //#ifndef _GC_TYPES_H_

