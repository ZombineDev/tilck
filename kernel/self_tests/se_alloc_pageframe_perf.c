
#include <common/basic_defs.h>
#include <common/string_util.h>
#include <common/utils.h>

#include <exos/hal.h>
#include <exos/kmalloc.h>
#include <exos/paging.h>
#include <exos/pageframe_allocator.h>

#include "se_data.h"

static int calc_perc_free_pageframes(void)
{
   return 100*get_free_pg_count()/get_usable_pg_count();
}

static void print_free_pageframes(void)
{
   printk("Free pageframes: %u/%u [%u%%]\n",
          get_free_pg_count(),
          get_usable_pg_count(),
          calc_perc_free_pageframes());
}

static uptr block_paddrs[1024];
extern u32 pageframes_bitfield[8 * (MAX_MEM_SIZE_IN_MB - LINEAR_MAPPING_MB)];


static void
alloc_pageframe_perf_perc_free(const int free_perc_threshold,
                               const bool alloc_128k)
{
   const u32 max_pages = MAX_MEM_SIZE_IN_MB * MB / PAGE_SIZE;
   uptr *paddrs = kmalloc(max_pages * sizeof(uptr));

   u64 start, duration;

   // Prepare a random bitfield for tests

   u32 allocated = 0;
   int iters = 0;
   int full_128k_blocks = 0;

   for (int i = 0;
        calc_perc_free_pageframes() > free_perc_threshold;
        i = (i + 2) % RANDOM_VALUES_COUNT) {

      iters++;

      u32 shift = (random_values[i] ^ iters) % 31;
      u32 val = random_values[i] | (random_values[i+1] << shift);
      u32 f = 0;

      for (u32 j = 0; j < 32; j++) {
         if ((val & (1U << j)) == 0)
            f++;
      }

      // This random data used contains mostly small numbers (too many 0s).
      // Therefore, flip all the bits.
      f = 32 - f;
      val = ~val;


      // Force the existence of FULL 128 K blocks.
      // With f <= 8, 71% of the total allocated mem are 128 K blocks.
      // With f <= 7, 57% of the total allocated mem are 128 K blocks.
      // With f <= 6, 43% of the total allocated mem are 128 K blocks.
      // With f <= 5, 28% of the total allocated mem are 128 K blocks.
      // With f <= 4, 15% of the total allocated mem are 128 K blocks.
      if (f <= 7) {
         f = 0;
         val = ~0;
      }

      /*
       * Now, ideally we'd like just to set an integer in the pageframe
       * bitfield to have the value 'val'. But, in order to support any further
       * implementation change in the pageframe allocator, we won't touch its
       * data. We'll hack it using its external interface.
       */

      uptr local_paddrs[32];

      // 1. Alloc 32 pageframes.
      for (int j = 0; j < 32; j++) {
         local_paddrs[j] = alloc_pageframe();
         VERIFY(local_paddrs[j] != INVALID_PADDR);
      }

      // 2. Free 'f' of them, according to the 0s in 'val'.
      for (int j = 0; j < 32; j++) {
         if ((val & (1U << j)) == 0) {
            free_pageframe(local_paddrs[j]);
            local_paddrs[j] = 0;
         }
      }

      // 3. Save the allocated pageframes in 'paddrs' for the clean-up.
      for (int j = 0; j < 32; j++) {
         if (local_paddrs[j])
            paddrs[allocated++] = local_paddrs[j];
      }

      if (f == 0)
         full_128k_blocks++;
   }

   // {
   //    uptr paddr = (157*4+3) << (PAGE_SHIFT + 3); // random addr

   //    int allocated_in_random_128k_block = 0;
   //    for (int i = 0; i < 32; i++) {

   //       if (is_allocated_pageframe(paddr)) {
   //          free_pageframe(paddr);
   //          allocated_in_random_128k_block++;
   //       }

   //       paddr += PAGE_SIZE;
   //    }

   // }

   //print_free_pageframes();
   //printk("Iters before hitting the threshold: %i\n", iters);

   // u32 used = get_usable_pg_count() - get_free_pg_count();
   // printk("Full 128K blocks: %i, %u%% of the total allocated\n",
   //       full_128k_blocks, 100*full_128k_blocks*32 / used);

   const int free_pageframes_count = get_free_pg_count();

   if (!alloc_128k) {

      start = RDTSC();
      for (int i = 0; i < free_pageframes_count; i++) {
         paddrs[allocated++] = alloc_pageframe();
      }
      duration = RDTSC() - start;

      printk("[%i%% free pageframes] AVG cost of 1-alloc: %llu cycles [%u allocs]\n",
             free_perc_threshold, duration/free_pageframes_count, allocated);

   } else {

      u32 blocks = 0;

      start = RDTSC();
      for (blocks = 0; blocks < 1024; blocks++) {

         uptr paddr = alloc_32_pageframes();

         if (paddr == INVALID_PADDR)
            break;

         block_paddrs[blocks] = paddr;
      }
      duration = RDTSC() - start;

      for (u32 j = 0; j < blocks; j++)
        free_32_pageframes(block_paddrs[j]);

      if (blocks) {
         printk("[%i%% free pageframes] AVG cost of 32-alloc: %llu cycles [%u allocs]\n",
                free_perc_threshold, duration/blocks, blocks);

         //printk("Details: total cost: %llu cycles\n", duration);

      } else {
         printk("[%i%% free pageframes] AVG cost of 32-alloc: UNKNOWN [0 allocs]\n",
                free_perc_threshold);
      }
   }


   for (u32 i = 0; i < allocated; i++) {
      if (paddrs[i] && is_allocated_pageframe(paddrs[i]))
         free_pageframe(paddrs[i]);
   }

   kfree2(paddrs, max_pages * sizeof(uptr));
}

void selftest_alloc_pageframe_perf(void)
{
   /*
    * HACK: make memsize_in_mb = LINEAR_MAPPING_MB + 128, in order to test
    * the performance of the physical pageframe allocator with 128 MB to
    * operate with.
    */

   memsize_in_mb = LINEAR_MAPPING_MB + 128;

   u32 allocated = 0;
   const u32 max_pages = 128 * MB / PAGE_SIZE;
   uptr *paddrs = kmalloc(max_pages * sizeof(uptr));

   u64 start, duration;

   start = RDTSC();
   while (true) {

      uptr paddr = alloc_pageframe();

      if (paddr == INVALID_PADDR)
         break;

      paddrs[allocated++] = paddr;
   }
   duration = RDTSC() - start;

   u64 avg = duration / allocated;
   printk("Allocated %u pageframes, AVG cost: %llu cycles\n", allocated, avg);

   // Now let's free just one pageframe in somewhere in the middle

   uptr target = paddrs[allocated/2];

   // And re-allocate it.
   const int single_page_iters = 10000;

   start = RDTSC();
   for (int i = 0; i < single_page_iters; i++) {
      free_pageframe(target);
      alloc_pageframe();      // returns always 'target'
   }
   duration = RDTSC() - start;

   u64 one_page_free_avg = duration/single_page_iters;
   printk("[1-page free] alloc + free: %llu cycles\n", one_page_free_avg);

   start = RDTSC();
   for (u32 i = 0; i < allocated; i++) {
      free_pageframe(paddrs[i]);
   }
   duration = RDTSC() - start;

   avg = duration / allocated;
   printk("Freed %u pageframes, AVG cost: %llu cycles\n", allocated, avg);

   kfree2(paddrs, max_pages * sizeof(uptr));

   alloc_pageframe_perf_perc_free(1, false);
   alloc_pageframe_perf_perc_free(2, false);
   alloc_pageframe_perf_perc_free(5, false);
   alloc_pageframe_perf_perc_free(10, false);
   alloc_pageframe_perf_perc_free(20, false);
   alloc_pageframe_perf_perc_free(40, false);

   printk("\nAllocation of blocks of 32-pageframes:\n");

   // Allocation of 128 K blocks.
   alloc_pageframe_perf_perc_free(10, true);
   alloc_pageframe_perf_perc_free(20, true);

   debug_qemu_turn_off_machine();
}