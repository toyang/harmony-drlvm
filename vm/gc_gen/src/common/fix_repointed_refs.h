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
 * @author Xiao-Feng Li, 2006/12/12
 */
 
#ifndef _FIX_REPOINTED_REFS_H_
#define _FIX_REPOINTED_REFS_H_

#include "gc_common.h"
extern Boolean IS_MOVE_COMPACT;

inline void slot_fix(Partial_Reveal_Object** p_ref)
{
  Partial_Reveal_Object* p_obj = *p_ref;
  if(!p_obj) return;

  if(IS_MOVE_COMPACT){
    if(obj_is_moved(p_obj))
      *p_ref = obj_get_fw_in_table(p_obj);
  }else{
    if(obj_is_fw_in_oi(p_obj) && obj_is_moved(p_obj)){
      /* Condition obj_is_moved(p_obj) is for preventing mistaking previous mark bit of large obj as fw bit when fallback happens.
       * Because until fallback happens, perhaps the large obj hasn't been marked. So its mark bit remains as the last time.
       * In major collection condition obj_is_fw_in_oi(p_obj) can be omitted,
       * for whose which can be scanned in MOS & NOS must have been set fw bit in oi.
       */
      assert((unsigned int)obj_get_fw_in_oi(p_obj) > DUAL_MARKBITS);
      *p_ref = obj_get_fw_in_oi(p_obj);
    }
  }
    
  return;
}

inline void object_fix_ref_slots(Partial_Reveal_Object* p_obj)
{
  if( !object_has_ref_field(p_obj) ) return;
  
    /* scan array object */
  if (object_is_array(p_obj)) {
    Partial_Reveal_Array* array = (Partial_Reveal_Array*)p_obj;
    assert(!obj_is_primitive_array(p_obj));
    
    int32 array_length = array->array_len;
    Partial_Reveal_Object** p_refs = (Partial_Reveal_Object**)((int)array + (int)array_first_element_offset(array));
    for (int i = 0; i < array_length; i++) {
      slot_fix(p_refs + i);
    }   
    return;
  }

  /* scan non-array object */
  int *offset_scanner = init_object_scanner(p_obj);
  while (true) {
    Partial_Reveal_Object** p_ref = (Partial_Reveal_Object**)offset_get_ref(offset_scanner, p_obj);
    if (p_ref == NULL) break; /* terminating ref slot */
  
    slot_fix(p_ref);
    offset_scanner = offset_next_ref(offset_scanner);
  }

  return;
}

inline void block_fix_ref_after_copying(Block_Header* curr_block)
{
  unsigned int cur_obj = (unsigned int)curr_block->base;
  unsigned int block_end = (unsigned int)curr_block->free;
  while(cur_obj < block_end){
    object_fix_ref_slots((Partial_Reveal_Object*)cur_obj);   
    cur_obj = (unsigned int)cur_obj + vm_object_size((Partial_Reveal_Object*)cur_obj);
  }
  return;
}

inline void block_fix_ref_after_marking(Block_Header* curr_block)
{
  void* start_pos;
  Partial_Reveal_Object* p_obj = block_get_first_marked_object(curr_block, &start_pos);
  
  while( p_obj ){
    assert( obj_is_marked_in_vt(p_obj));
    obj_unmark_in_vt(p_obj);
    object_fix_ref_slots(p_obj);   
    p_obj = block_get_next_marked_object(curr_block, &start_pos);  
  }
  return;
}

inline void block_fix_ref_after_repointing(Block_Header* curr_block)
{
  void* start_pos;
  Partial_Reveal_Object* p_obj = block_get_first_marked_obj_after_prefetch(curr_block, &start_pos);
  
  while( p_obj ){
    assert( obj_is_marked_in_vt(p_obj));
    object_fix_ref_slots(p_obj);   
    p_obj = block_get_next_marked_obj_after_prefetch(curr_block, &start_pos);  
  }
  return;
}


#endif /* #ifndef _FIX_REPOINTED_REFS_H_ */