/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *        The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <spl.h>
#include <spinlock.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>
#include <proc.h>

/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 *
 * UNSW: If you use ASST3 config as required, then this file forms
 * part of the VM subsystem.
 *
 */

struct addrspace *
as_create(void)
{
        struct addrspace *as;

        as = kmalloc(sizeof(struct addrspace));
        if (as == NULL) {
                return NULL;
        }

        as->regions = NULL;

        return as;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
        struct addrspace *newas;
        struct region *old_region, *prev_region;

        newas = as_create();
        if (newas==NULL) {
                return ENOMEM;
        }
        newas->regions=NULL;
        prev_region = newas->regions;
        old_region = old->regions;
        while (old_region != NULL){
                struct region* new_region = kmalloc(sizeof(struct region));
                new_region->as_vbase = old_region->as_vbase;
                new_region->as_npages = old_region->as_npages;
                new_region->writeable = old_region->writeable;
                new_region->prev_writeable = old_region->prev_writeable;
                new_region->next_region=NULL;
                if(prev_region == NULL){
                        // kprintf("hehe\n");
                        newas->regions=new_region;
                } else{
                        // kprintf("haha\n");
                        prev_region->next_region = new_region;
                }
                prev_region = new_region;
                old_region = old_region->next_region;

        }


        hpt_copy((uint32_t)old, (uint32_t)newas);
        *ret = newas;
        return 0;
}



void
as_destroy(struct addrspace *as)
{

        KASSERT(as != NULL);
        //free regions
        struct region *next, *curr;
        curr = as->regions;
        next = as->regions;
        while(curr!= NULL){
                // kprintf("i am gonna destroy\n");
                next = curr -> next_region;
                kfree(curr);
                curr = next;
        }
        kfree(curr);
        kfree(next);

        hpt_remove((uint32_t)as);

        kfree(as);
}

void
as_activate(void)
{
	int i, spl;
	struct addrspace *as;

	as = proc_getas();
	if (as == NULL) {
		return;
	}

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}

	splx(spl);
}

void
as_deactivate(void)
{
        int i, spl;
        struct addrspace *as;

        as = proc_getas();
        if (as == NULL) {
                return;
        }

        /* Disable interrupts on this CPU while frobbing the TLB. */
        spl = splhigh();

        for (i=0; i<NUM_TLB; i++) {
                tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
        }

        splx(spl);

}

/*
 * Set up a segment at virtual address VADDR of size MEMSIZE. The
 * segment in memory extends from VADDR up to (but not including)
 * VADDR+MEMSIZE.
 *
 * The READABLE, WRITEABLE, and EXECUTABLE flags are set if read,
 * write, or execute permission should be set on the segment. At the
 * moment, these are ignored. When you write the VM system, you may
 * want to implement them.
 */
int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t memsize,
                 int readable, int writeable, int executable)
{
        (void)readable;
        (void)executable;
        // kprintf("defining region, addr is %d\n",vaddr);
        size_t npages;
        /* Align the region. First, the base... */
        memsize += vaddr & ~(vaddr_t)PAGE_FRAME;
        vaddr &= PAGE_FRAME;

        /* ...and now the length. */
        memsize = (memsize + PAGE_SIZE - 1) & PAGE_FRAME;

        npages = memsize / PAGE_SIZE;

        //save region
        struct region *newRegion = kmalloc(sizeof(struct region));
        if (newRegion == NULL){
                return ENOMEM;
        }

        newRegion->as_vbase = vaddr;
        newRegion->as_npages = npages;
        newRegion->writeable = writeable;
        newRegion->prev_writeable = writeable;
        newRegion->next_region = NULL;

        struct region *curr = as->regions;
        // struct region *prev = NULL;
        int counter = 0;
        if (curr == NULL){
                counter ++;
                as->regions = newRegion;
        } else {
                while (curr->next_region != 0){
                        // prev = curr
                        counter ++;
                        curr = curr -> next_region;
                }
                curr->next_region = newRegion;
        }
        return 0;
}

int
as_prepare_load(struct addrspace *as)
{

        struct region *curr = as->regions;
        while(curr != NULL){
                curr->writeable = 1;
                curr = curr->next_region;
        }

        return 0;
}

int
as_complete_load(struct addrspace *as)
{
        struct region *curr = as->regions;
        while(curr != NULL){
                curr->writeable = curr->prev_writeable;
                curr = curr->next_region;
        }

        return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
        int err;
        /*
        * make a stack vm_object
        *
        * The stack is USERSTACKSIZE bytes, which is defined in machine/vm.h.
        * This is generally quite large, so it is zerofilled to make swap use
        * efficient and fork reasonably fast.
        */
        err = as_define_region(as, USERSTACK-USERSTACKSIZE, USERSTACKSIZE, 1, 1, 0);
        if (err) {
                return err;
        }

        /* Initial user-level stack pointer */
        *stackptr = USERSTACK;

        return 0;
}
