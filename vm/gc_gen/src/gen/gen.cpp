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

#include "port_sysinfo.h"

#include "gen.h"
#include "../thread/mutator.h"
#include "../thread/collector.h"
#include "../verify/verify_live_heap.h"

/* heap size limit is not interesting. only for manual tuning purpose */
unsigned int min_heap_size_bytes = 32 * MB;
unsigned int max_heap_size_bytes = 64 * MB;

/* fspace size limit is not interesting. only for manual tuning purpose */
unsigned int min_nos_size_bytes = 2 * MB;
unsigned int max_nos_size_bytes = 64 * MB;

static void gc_gen_get_system_info(GC_Gen *gc_gen) 
{
  gc_gen->_machine_page_size_bytes = port_vmem_page_sizes()[0];
  gc_gen->_num_processors = port_CPUs_number();
}

void gc_gen_initialize(GC_Gen *gc_gen, unsigned int min_heap_size, unsigned int max_heap_size) 
{
	assert(gc_gen);	
  assert(max_heap_size <= max_heap_size_bytes);
	/* FIXME:: we need let virtual space to include unmapped region.
	   Heuristically for Nursery+MatureFrom+MatureTo(unmapped)+LOS(mapped+unmapped), 
	   we need almost half more than the user specified virtual space size. 
	   That's why we have the below. */
	max_heap_size += max_heap_size>>1;

	min_heap_size = round_up_to_size(min_heap_size, GC_BLOCK_SIZE_BYTES);
	max_heap_size = round_up_to_size(max_heap_size, GC_BLOCK_SIZE_BYTES);

	gc_gen_get_system_info(gc_gen); 

  void *reserved_base = NULL;

  /* allocate memory for gc_gen */
  gc_gen->allocated_memory = NULL;
  pool_create(&gc_gen->aux_pool, 0);
  
  apr_status_t status = port_vmem_reserve(&gc_gen->allocated_memory, 
                  &reserved_base, max_heap_size, 
                  PORT_VMEM_MODE_READ | PORT_VMEM_MODE_WRITE, 
                  gc_gen->_machine_page_size_bytes, gc_gen->aux_pool);
  
  while(APR_SUCCESS != status){
    max_heap_size -= gc_gen->_machine_page_size_bytes;
    status = port_vmem_reserve(&gc_gen->allocated_memory, 
                  &reserved_base, max_heap_size, 
                  PORT_VMEM_MODE_READ | PORT_VMEM_MODE_WRITE, 
                  gc_gen->_machine_page_size_bytes, gc_gen->aux_pool);  
  }
  assert(max_heap_size > min_heap_size_bytes);
  gc_gen->reserved_heap_size = max_heap_size;
  gc_gen->heap_start = reserved_base;
  gc_gen->heap_end = (void*)((unsigned int)reserved_base + max_heap_size);
  gc_gen->num_collections = 0;

  /* heuristic nos + mos + LOS */
  unsigned int nos_size =  max_heap_size >> 4; 
  assert(nos_size > min_nos_size_bytes);
	gc_nos_initialize(gc_gen, reserved_base, nos_size);	

	unsigned int mos_size = max_heap_size >> 1;
	reserved_base = (void*)((unsigned int)reserved_base + nos_size);
	gc_mos_initialize(gc_gen, reserved_base, mos_size);
  
	unsigned int los_size = max_heap_size >> 2;
	reserved_base = (void*)((unsigned int)gc_gen->heap_end - los_size);
	gc_los_initialize(gc_gen, reserved_base, los_size);

  gc_gen->committed_heap_size = space_committed_size((Space*)gc_gen->nos) +
                                space_committed_size((Space*)gc_gen->mos) +
                                space_committed_size((Space*)gc_gen->los);
  
  gc_init_rootset((GC*)gc_gen);	

	gc_gen->mutator_list = NULL;
	gc_gen->mutator_list_lock = FREE_LOCK;

  gc_gen->num_mutators = 0;
  
  collector_initialize((GC*)gc_gen);
  
  if( verify_live_heap ){  /* for live heap verify*/
    gc_init_heap_verification((GC*)gc_gen);
  }

  return;
}

void gc_gen_destruct(GC_Gen *gc_gen) 
{
	gc_nos_destruct(gc_gen);
	gc_gen->nos = NULL;
	
	gc_mos_destruct(gc_gen);	
	gc_gen->mos = NULL;

	gc_los_destruct(gc_gen);	
  gc_gen->los = NULL;
  
  collector_destruct((GC*)gc_gen);

  if( verify_live_heap ){
    gc_terminate_heap_verification((GC*)gc_gen);
  }

	STD_FREE(gc_gen);
}


Boolean major_collection_needed(GC_Gen* gc)
{
  return mspace_free_memory_size(gc->mos) < fspace_used_memory_size(gc->nos);  
}

void* mos_alloc(unsigned size, Alloc_Context *alloc_ctx){return mspace_alloc(size, alloc_ctx);}
void* nos_alloc(unsigned size, Alloc_Context *alloc_ctx){return fspace_alloc(size, alloc_ctx);}
void* los_alloc(unsigned size, Alloc_Context *alloc_ctx){return lspace_alloc(size, alloc_ctx);}
Space* gc_get_nos(GC_Gen* gc){ return (Space*)gc->nos;}
Space* gc_get_mos(GC_Gen* gc){ return (Space*)gc->mos;}
Space* gc_get_los(GC_Gen* gc){ return (Space*)gc->los;}
void gc_set_nos(GC_Gen* gc, Space* nos){ gc->nos = (Fspace*)nos;}
void gc_set_mos(GC_Gen* gc, Space* mos){ gc->mos = (Mspace*)mos;}
void gc_set_los(GC_Gen* gc, Space* los){ gc->los = (Lspace*)los;}
unsigned int gc_get_processor_num(GC_Gen* gc){ return gc->_num_processors;}

void gc_preprocess_collector(Collector *collector)
{
  GC_Gen* gc = (GC_Gen*)collector->gc;
  
  /* this_cycle_remset is ready to be used */
  assert(collector->this_cycle_remset->empty());

  return;
}

void gc_postprocess_collector(Collector *collector)
{ 
  GC_Gen* gc = (GC_Gen*)collector->gc;
      
  /* for MINOR_COLLECTION */
  Fspace* fspace = (Fspace*)gc_get_nos(gc);
  
  /* switch its remsets, this_cycle_remset data kept in space->remslot_sets */
  /* last_cycle_remset was in space->remslot_sets and cleared during collection */
  assert(collector->last_cycle_remset->empty());

  RemslotSet* temp_set = collector->this_cycle_remset;
  collector->this_cycle_remset = collector->last_cycle_remset;
  collector->last_cycle_remset = temp_set;
  
  fspace->remslot_sets->push_back(collector->last_cycle_remset);
  
  return;
}

void gc_preprocess_mutator(GC_Gen* gc)
{
  Mutator *mutator = gc->mutator_list;
  Fspace* fspace = (Fspace*)mutator->alloc_space;
  
  while (mutator) {
    fspace->remslot_sets->push_back(mutator->remslot);
    fspace->remobj_sets->push_back(mutator->remobj);  
    mutator = mutator->next;
  }
 
  return;
}

void gc_postprocess_mutator(GC_Gen* gc)
{
  Mutator *mutator = gc->mutator_list;
  while (mutator) {
    assert(mutator->remobj->empty());
    assert(mutator->remslot->empty());
    alloc_context_reset((Alloc_Context*)mutator);    
    mutator = mutator->next;
  }
  
  return;
}

static unsigned int gc_decide_collection_kind(GC_Gen* gc, unsigned int cause)
{
  if(major_collection_needed(gc) || cause== GC_CAUSE_LOS_IS_FULL)
    return  MAJOR_COLLECTION;

  return MINOR_COLLECTION;     
}

void gc_gen_reclaim_heap(GC_Gen* gc, unsigned int cause)
{  
  gc->num_collections++;

  gc->collect_kind = gc_decide_collection_kind(gc, cause);

  /* Stop the threads and collect the roots. */
  gc_reset_rootset((GC*)gc);  
  vm_enumerate_root_set_all_threads();
  
  gc_preprocess_mutator(gc);
  
  if(verify_live_heap) gc_verify_heap((GC*)gc, TRUE);

  if(gc->collect_kind == MINOR_COLLECTION)
    fspace_collection(gc->nos);
  else{
    mspace_collection(gc->mos); /* fspace collection is included */
    lspace_collection(gc->los);
  }
  
  if(verify_live_heap) gc_verify_heap((GC*)gc, FALSE);
      
  gc_postprocess_mutator(gc);

  vm_resume_threads_after();

  return;
}