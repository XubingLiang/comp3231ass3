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
        //malloc new address space
        as = kmalloc(sizeof(struct addrspace));
        if (as == NULL) {
            //if fails to malloc
                return NULL;
        }
        //head of the list of regions 
        as->regions = NULL;

        return as;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
        struct addrspace *newas;
        struct region *old_region, *prev_region;
        //create new address space
        newas = as_create();
        if (newas==NULL) {
                return ENOMEM;
        }
        newas->regions=NULL;
        prev_region = newas->regions;
        old_region = old->regions;
        while (old_region != NULL){
                //creaet new region and clone the content from each old region
                struct region* new_region = kmalloc(sizeof(struct region));
                new_region->as_vbase = old_region->as_vbase;
                new_region->as_npages = old_region->as_npages;
                new_region->writeable = old_region->writeable;
                new_region->prev_writeable = old_region->prev_writeable;
                new_region->next_region=NULL;
                //if it's the first region of this address space, set it as head of the list
                if(prev_region == NULL){
                        newas->regions=new_region;
                } else{
                        prev_region->next_region = new_region;
                }
                prev_region = new_region;
                old_region = old_region->next_region;

        }

        //copy PTE from hash page table
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
                next = curr -> next_region;
                kfree(curr);
                curr = next;
        }
        kfree(curr);
        kfree(next);
        //remove PTE from page table
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
        //set content in this region
        newRegion->as_vbase = vaddr;
        newRegion->as_npages = npages;
        newRegion->writeable = writeable;
        newRegion->prev_writeable = writeable;
        newRegion->next_region = NULL;

        //append this region to the end of regions fo this address space
        struct region *curr = as->regions;
        // struct region *prev = NULL;
        if (curr == NULL){
                as->regions = newRegion;
        } else {
                while (curr->next_region != 0){
                        // prev = curr
                        curr = curr -> next_region;
                }
                curr->next_region = newRegion;
        }
        return 0;
}


int
as_prepare_load(struct addrspace *as)
{
        //set writeable bit to 1
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
        //reset the writeable bit to origin
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

        err = as_define_region(as, USERSTACK-USERSTACKSIZE, USERSTACKSIZE, 1, 1, 0);
        if (err) {
                return err;
        }

        /* Initial user-level stack pointer */
        *stackptr = USERSTACK;

        return 0;
}
