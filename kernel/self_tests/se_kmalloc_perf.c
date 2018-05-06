

#include <common/basic_defs.h>
#include <common/string_util.h>
#include <common/utils.h>
#include <common/arch/generic_x86/x86_utils.h>

#include <exos/kmalloc.h>

#define RANDOM_VALUES_COUNT 1000

extern int random_values[RANDOM_VALUES_COUNT];
extern void *allocations[10000];

static void kmalloc_perf_per_size(int size)
{
   const int iters = size < 4096 ? 10000 : (size <= 16*KB ? 1000 : 100);
   u64 start, duration;

   start = RDTSC();

   for (int i = 0; i < iters; i++) {

      allocations[i] = kmalloc(size);

      if (!allocations[i])
         panic("We were unable to allocate %u bytes\n", size);
   }

   for (int i = 0; i < iters; i++) {
      kfree2(allocations[i], size);
   }

   duration = RDTSC() - start;
   printk("[%i iters] Cycles per kmalloc(%i) + kfree: %llu\n",
          iters, size, duration  / iters);
}

void selftest_kmalloc_perf(void)
{
   const int iters = 1000;

   printk("*** kmalloc perf test ***\n", iters);

   u64 start = RDTSC();

   for (int i = 0; i < iters; i++) {

      for (int j = 0; j < RANDOM_VALUES_COUNT; j++) {

         allocations[j] = kmalloc(random_values[j]);

         if (!allocations[j])
            panic("We were unable to allocate %u bytes\n", random_values[j]);
      }

      for (int j = 0; j < RANDOM_VALUES_COUNT; j++)
         kfree2(allocations[j], random_values[j]);
   }

   u64 duration = (RDTSC() - start) / (iters * RANDOM_VALUES_COUNT);

   printk("[%i iters] Cycles per kmalloc(RANDOM) + kfree: %llu\n",
          iters * RANDOM_VALUES_COUNT, duration);

   for (int s = 32; s <= 256*KB; s *= 2) {
      kmalloc_perf_per_size(s);
   }
}