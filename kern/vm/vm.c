#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <spl.h>
#include <cpu.h>
#include <addrspace.h>
#include <vm.h>
#include <machine/tlb.h>
#include <synch.h>
#include <current.h>
#include <proc.h>




int hpt_size;
struct PTE* hpt;
static struct spinlock hpt_lock = SPINLOCK_INITIALIZER;
static struct spinlock hpt_lock1 = SPINLOCK_INITIALIZER;


/* Place your page table functions here */
void hpt_init(void){
        int i;
        hpt_size=(int)(2*ram_getsize()/PAGE_SIZE);
        hpt = kmalloc(sizeof(struct PTE) * hpt_size);
        for(i=0; i < hpt_size;i++){
                hpt[i].pid=0;
                hpt[i].vaddr=0;
                hpt[i].entry_lo=0;
                hpt[i].next =-1;
        }

}

uint32_t hpt_lookup(uint32_t as,vaddr_t faultaddr){
        uint32_t index = hpt_hash(as,faultaddr);
        spinlock_acquire(&hpt_lock);
        int curr =index;
        uint32_t entry_lo= 0;
        faultaddr &= PAGE_FRAME;
        if((hpt[index].entry_lo & TLBLO_VALID) == 0){
                entry_lo = 0;
        } else {
                if(hpt[index].pid==as && hpt[index].vaddr== faultaddr){
                        entry_lo=hpt[index].entry_lo;
                } else {
                        while(curr != -1){
                                if(hpt[curr].pid==as && hpt[curr].vaddr==faultaddr ){
                                        entry_lo=hpt[curr].entry_lo;
                                        break;
                                }
                                curr=hpt[curr].next;
                        }

                }
        }
        spinlock_release(&hpt_lock);
        return entry_lo;
}

uint32_t hpt_insert(uint32_t as,vaddr_t faultaddr,uint32_t dirty){

        vaddr_t vaddr = alloc_kpages(1);

        if (vaddr == 0) {
                return ENOMEM;
        }
        spinlock_acquire(&hpt_lock);
        int i=0;
        uint32_t entry_lo=0;
        paddr_t paddr = KVADDR_TO_PADDR(vaddr);
        uint32_t index = hpt_hash(as,faultaddr);
        int j =index;
        if((hpt[index].entry_lo & TLBLO_VALID) == 0){
                hpt[index].pid=as;
                hpt[index].vaddr=faultaddr& PAGE_FRAME;
                hpt[index].entry_lo=(paddr & PAGE_FRAME) | dirty | TLBLO_VALID;
                hpt[index].next=-1;
                entry_lo = hpt[j].entry_lo;
        } else {
                for(i=0;i<hpt_size;i++){
                        j=(index+i)%hpt_size;
                        if((hpt[j].entry_lo & TLBLO_VALID) == 0){
                                hpt[j].pid=as;
                                hpt[j].vaddr=faultaddr & PAGE_FRAME;
                                hpt[j].entry_lo=(paddr & PAGE_FRAME) | dirty | TLBLO_VALID;
                                hpt[j].next=-1;
                                entry_lo = hpt[j].entry_lo;
                                break;
                        }
                }
                i=index;
                while(hpt[i].next!=-1){
                        i=hpt[i].next;
                }
                hpt[i].next=j;
        }
        spinlock_release(&hpt_lock);
        return entry_lo;

}

int hpt_copy(uint32_t old, uint32_t new){
        uint32_t new_pid=new;
        vaddr_t old_vaddr=0;
        uint32_t old_entry_lo=0;
        uint32_t new_entry_lo=0;
        spinlock_acquire(&hpt_lock1);
        for (int i = 0; i < hpt_size; i++){
                if (hpt[i].pid == old){
                        old_vaddr = hpt[i].vaddr;
                        old_entry_lo= hpt[i].entry_lo;
                        uint32_t dirty = old_entry_lo & TLBLO_DIRTY;
                        new_entry_lo=hpt_insert(new_pid,old_vaddr,dirty);
                        paddr_t new_paddr= new_entry_lo & PAGE_FRAME;
                        paddr_t old_paddr= old_entry_lo & PAGE_FRAME;
                        memcpy((void *)PADDR_TO_KVADDR(new_paddr), (const void *)PADDR_TO_KVADDR(old_paddr), PAGE_SIZE);
                }
        }
        spinlock_release(&hpt_lock1);
        return 0;
}




void hpt_remove(uint32_t as){
        int next =0;
        spinlock_acquire(&hpt_lock);
        for (int i=0; i < hpt_size; i ++){
                if (hpt[i].pid == as){
                        // kprintf("removing %d, addr is 0x%x\n", i, hpt[i].entry_lo&PAGE_FRAME);
                        free_kpages(PADDR_TO_KVADDR(hpt[i].entry_lo&PAGE_FRAME));
                        if (hpt[i].next != -1){
                                next = hpt[i].next;

                                hpt[i].pid = hpt[next].pid;
                                hpt[i].vaddr = hpt[next].vaddr;
                                hpt[i].entry_lo = hpt[next].entry_lo;
                                hpt[i].next = hpt[next].next;
                                //fuck fucks
                                hpt[next].pid=0;
                                hpt[next].vaddr=0;
                                hpt[next].entry_lo =  0;
                                hpt[next].next = -1;
                                i--;
                        } else {

                                //
                                hpt[i].entry_lo = 0;
                                hpt[i].pid =0;
                                hpt[i].vaddr = 0;
                                hpt[i].next = -1;
                        }
                }

        }
        spinlock_release(&hpt_lock);
}


void vm_bootstrap(void)
{
        /* Initialise VM sub-system.  You probably want to initialise your
        frame table here as well.
        */
        hpt_init();
        ft_init();
}

int
vm_fault(int faulttype, vaddr_t faultaddress)
{
        /* TLB entry high*/
        uint32_t entry_hi;
        /* TLB entry low */
        uint32_t entry_lo;
        /*other variable*/
        uint32_t dirty=0;
        //struct PTE* table_entry;
        switch (faulttype) {
                case VM_FAULT_READONLY:
                        return EFAULT;
                case VM_FAULT_READ:
                case VM_FAULT_WRITE:
                        break;
                default:
                        return EINVAL;
        }
        if (curproc == NULL) {
                return EFAULT;
        }
        struct addrspace *as = proc_getas();
        uint32_t pid = (uint32_t)as;
        if (as == NULL) {
                return EFAULT;
        }
        faultaddress &= PAGE_FRAME;
        entry_lo=hpt_lookup(pid,faultaddress);
        /*if no valid entry, look up region
        * see if the vaddr is valid
        */
        if(entry_lo == 0){
                struct region* region=as->regions;
                while(region!= NULL){
                        if(faultaddress >= region->as_vbase && faultaddress  <= region->as_vbase+(region->as_npages*PAGE_SIZE)){
                                if(region-> writeable == 0){
                                        dirty = 0;
                                } else {
                                        dirty=TLBLO_DIRTY;
                                }
                                break;
                        }
                        region = region->next_region;
                }
                //check if region valid
                if(region==NULL){
                        return EFAULT;
                }
                //allocate frame, zero-fill, insert pte
                entry_lo=hpt_insert(pid,faultaddress,dirty);
        } else {
                struct region* region=as->regions;
                while(region!= NULL){
                        if(faultaddress >= region->as_vbase && faultaddress <= region->as_vbase+(region->as_npages*PAGE_SIZE)){
                                if(region-> writeable == 0){
                                        // kprintf("%x ",entry_lo);
                                        entry_lo = entry_lo & ~(TLBLO_DIRTY);
                                        // kprintf("after %x \n",entry_lo);
                                }
                                break;
                        }
                        region = region->next_region;
                }
                if(region==NULL){
                        return EFAULT;
                }

        }

        entry_hi=faultaddress & PAGE_FRAME;
        int spl = splhigh();
        tlb_random(entry_hi, entry_lo);
    	splx(spl);
        return 0;
}


/*
 *
 * SMP-specific functions.  Unused in our configuration.
 */

void
vm_tlbshootdown(const struct tlbshootdown *ts)
{
        (void)ts;
        panic("vm tried to do tlb shootdown?!\n");
}

/*hash function for hash page table*/
uint32_t hpt_hash(uint32_t as, vaddr_t faultaddr){
        uint32_t index;
        index = ((as) ^ (faultaddr >> PAGE_BITS)) % hpt_size;
        return index;
}
