
#pragma once

//#define DEBUG_printk printk
#define DEBUG_printk(...)

#define DEBUG_stop_coaleshe                                                 \
                                                                            \
   DEBUG_printk("STOP: unable to make node %i (size %u) as free\n",         \
                n, curr_size);                                              \
                                                                            \
   DEBUG_printk("node left: free:  %i, split: %i\n",                        \
                !left.full, left.split);                                    \
                                                                            \
   DEBUG_printk("node right: free: %i, split: %i\n",                        \
                !right.full, left.split)                                    \

#define DEBUG_coaleshe                                                      \
   DEBUG_printk("Marking node = %i (size: %u) as free\n", n, curr_size)

#define DEBUG_allocate_node1                                                \
   DEBUG_printk("For node# %i, using alloc block (%i/%i): %p (node #%u)\n", \
                ptr_to_node((void *)vaddr, node_size), i+1,                 \
                alloc_block_count, alloc_block_vaddr, alloc_node)           \

#define DEBUG_allocate_node2                                                \
   DEBUG_printk("Allocating block of pages..\n")

#define DEBUG_allocate_node3                                                \
   DEBUG_printk("Returning addr %p (%u alloc blocks)\n",                    \
                vaddr,                                                      \
                alloc_block_count)                                          \

#define DEBUG_kmalloc_begin                                                 \
   DEBUG_printk("kmalloc(%u)...\n", desired_size)

#define DEBUG_kmalloc_call_begin                                            \
      DEBUG_printk("Node# %i, node_size = %u, vaddr = %p\n",                \
                   node, node_size, node_to_ptr(node, node_size))           \

#define DEBUG_already_full                                                  \
   DEBUG_printk("Already FULL, return NULL\n")

#define DEBUG_already_split                                                 \
   DEBUG_printk("Already split, return NULL\n")

#define DEBUG_kmalloc_split                                                 \
   DEBUG_printk("Splitting node #%u...\n", node)

#define DEBUG_going_left                                                    \
   DEBUG_printk("going to left..\n")

#define DEBUG_left_failed                                                   \
   DEBUG_printk("allocation on left node not possible, trying with right..\n")

#define DEBUG_going_right                                                   \
   DEBUG_printk("going on right..\n")

#define DEBUG_free1                                                         \
   DEBUG_printk("free_node: node# %i (size %u)\n", node, size)

#define DEBUG_free_after_coaleshe                                           \
   DEBUG_printk("After coaleshe, biggest_free_node# %i, "                   \
                "biggest_free_size = %u\n",                                 \
                biggest_free_node, biggest_free_size)                       \

#define DEBUG_free_alloc_block_count                                        \
   DEBUG_printk("The block node used up to %i pages\n", alloc_block_count)


#define DEBUG_check_alloc_block                                             \
   DEBUG_printk("Checking alloc block i = %i, pNode = %i, pAddr = %p, "     \
                 "alloc = %i, free = %i, split = %i\n",                     \
                 i, alloc_node, alloc_block_vaddr,                          \
                 md->nodes[alloc_node].allocated,                           \
                 !md->nodes[alloc_node].full,                               \
                 md->nodes[alloc_node].split)                               \

#define DEBUG_free_freeing_block                                            \
   DEBUG_printk("---> FREEING the ALLOC BLOCK!\n")