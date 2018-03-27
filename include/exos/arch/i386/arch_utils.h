
#pragma once

#include <common/basic_defs.h>
#include <common/arch/generic_x86/x86_utils.h>

// Forward-declaring regs
typedef struct regs regs;


/* This defines what the stack looks like after an ISR ran */
struct regs {
   u32 gs, fs, es, ds;      /* pushed the segs last */
   u32 edi, esi, ebp, esp, ebx, edx, ecx, eax;  /* pushed by 'pusha' */
   u32 int_num, err_code;    /* our 'push byte #' and error codes do this */
   u32 eip, cs, eflags, useresp, ss;   /* pushed by the CPU automatically */
};

static ALWAYS_INLINE void set_return_register(regs *r, u32 value)
{
   r->eax = value;
}


NORETURN void asm_context_switch_x86(regs state_copy);
NORETURN void asm_kernel_context_switch_x86(regs state_copy);

NORETURN static ALWAYS_INLINE void context_switch(regs *r)
{
   asm_context_switch_x86(*r);
}

NORETURN static ALWAYS_INLINE void kernel_context_switch(regs *r)
{
   asm_kernel_context_switch_x86(*r);
}
