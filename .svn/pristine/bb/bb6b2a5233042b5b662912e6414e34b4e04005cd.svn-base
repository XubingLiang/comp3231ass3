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



/*variable assignment*/
int hpt_size;
struct PTE* hpt;
static struct spinlock hpt_lock = SPINLOCK_INITIALIZER;
static struct spinlock hpt_lock1 = SPINLOCK_INITIALIZER;


/* Place your page table functions here */

/*page table Initialization*/
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

/*look up if there is a valid entry on the table*/
uint32_t hpt_lookup(uint32_t pid,vaddr_t faultaddr){
        //get index on the table
        uint32_t index = hpt_hash(pid,faultaddr);
        //lock critical section
        spinlock_acquire(&hpt_lock);
        //for iterate the collision list
        int curr =index;
        uint32_t entry_lo= 0;
        faultaddr &= PAGE_FRAME;
        //if the entry on index is nothing, set entry_lo to 0
        if((hpt[index].entry_lo & TLBLO_VALID) == 0){
                entry_lo = 0;
        } else {
                //if the entry of the index is target, set entry_lo
                if(hpt[index].pid==pid && hpt[index].vaddr== faultaddr){
                        entry_lo=hpt[index].entry_lo;
                } else {
                        //traverse the collision list, to see if the target in the list.
                        while(curr != -1){
                                //there is a valid entry
                                if(hpt[curr].pid==pid && hpt[curr].vaddr==faultaddr ){
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

//create new entry
uint32_t hpt_insert(uint32_t pid,vaddr_t faultaddr,uint32_t dirty){
        //variable assignment
        vaddr_t vaddr = alloc_kpages(1);

        if (vaddr == 0) {
                return ENOMEM;
        }
        spinlock_acquire(&hpt_lock);
        int i=0;
        uint32_t entry_lo=0;
        paddr_t paddr = KVADDR_TO_PADDR(vaddr);
        uint32_t index = hpt_hash(pid,faultaddr);
        int j =index;
        //if the entry of the index is avaiable to insert
        if((hpt[index].entry_lo & TLBLO_VALID) == 0){
                hpt[index].pid=pid;
                hpt[index].vaddr=faultaddr& PAGE_FRAME;
                hpt[index].entry_lo=(paddr & PAGE_FRAME) | dirty | TLBLO_VALID;
                hpt[index].next=-1;
                entry_lo = hpt[j].entry_lo;
        } else {
                //start from the index, get the first free entry
                for(i=0;i<hpt_size;i++){
                        j=(index+i)%hpt_size;
                        if((hpt[j].entry_lo & TLBLO_VALID) == 0){
                                hpt[j].pid=pid;
                                hpt[j].vaddr=faultaddr & PAGE_FRAME;
                                hpt[j].entry_lo=(paddr & PAGE_FRAME) | dirty | TLBLO_VALID;
                                hpt[j].next=-1;
                                entry_lo = hpt[j].entry_lo;
                                break;
                        }
                }
                i=index;
                //set up a collision list
                while(hpt[i].next!=-1){
                        i=hpt[i].next;
                }
                hpt[i].next=j;
        }
        spinlock_release(&hpt_lock);
        return entry_lo;

}

//copy entry of a old pid to a new pid
int hpt_copy(uint32_t old, uint32_t new){
        /*variable */
        uint32_t new_pid=new;
        vaddr_t old_vaddr=0;
        uint32_t old_entry_lo=0;
        uint32_t new_entry_lo=0;
        spinlock_acquire(&hpt_lock1);
        //traverse whole table
        for (int i = 0; i < hpt_size; i++){
                if (hpt[i].pid == old){
                        //assign value
                        old_vaddr = hpt[i].vaddr;
                        old_entry_lo= hpt[i].entry_lo;
                        uint32_t dirty = old_entry_lo & TLBLO_DIRTY;
                        //create new entry
                        new_entry_lo=hpt_insert(new_pid,old_vaddr,dirty);
                        paddr_t new_paddr= new_entry_lo & PAGE_FRAME;
                        paddr_t old_paddr= old_entry_lo & PAGE_FRAME;
                        //move staff from old frame to new frame
                        memcpy((void *)PADDR_TO_KVADDR(new_paddr), (const void *)PADDR_TO_KVADDR(old_paddr), PAGE_SIZE);
                }
        }
        spinlock_release(&hpt_lock1);
        return 0;
}



//remove all entry of that pid from hash page table
void hpt_remove(uint32_t pid){
        int next =0;
        spinlock_acquire(&hpt_lock);
        for (int i=0; i < hpt_size; i ++){
                if (hpt[i].pid == pid){
                        // kprintf("removing %d, addr is 0x%x\n", i, hpt[i].entry_lo&PAGE_FRAME);
                        free_kpages(PADDR_TO_KVADDR(hpt[i].entry_lo&PAGE_FRAME));
                        /*if the entry is not the end of a collision list,
                         move the next staff to this location ,
                         and set all the content to initialised value*/
                        if (hpt[i].next != -1){
                                next = hpt[i].next;
                                // move content of next index to curr index
                                hpt[i].pid = hpt[next].pid;
                                hpt[i].vaddr = hpt[next].vaddr;
                                hpt[i].entry_lo = hpt[next].entry_lo;
                                hpt[i].next = hpt[next].next;
                                //f
                                hpt[next].pid=0;
                                hpt[next].vaddr=0;
                                hpt[next].entry_lo =  0;
                                hpt[next].next = -1;
                                i--;
                        //otherwise
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

//a helper function to look up through the region list
struct region* lookup_region(vaddr_t faultaddress, uint32_t* dirty){
        struct addrspace *as = proc_getas();
        struct region* region=as->regions;
        while(region!= NULL){
                if(faultaddress >= region->as_vbase && faultaddress  <= region->as_vbase+(region->as_npages*PAGE_SIZE)){
                        if(region-> writeable == 0){
                                *dirty = 0;
                        } else {
                                *dirty=TLBLO_DIRTY;
                        }
                        break;
                }
                region = region->next_region;
        }
        return region;
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
        //get the address space
        struct addrspace *as = proc_getas();
        uint32_t pid = (uint32_t)as;
        if (as == NULL) {
                return EFAULT;
        }
        //do the mask shiit!
        faultaddress &= PAGE_FRAME;
        //look up on the table see if there is a avaiable entry
        entry_lo=hpt_lookup(pid,faultaddress);
        /*if no valid entry, look up region
        * see if the vaddr is valid*/
        struct region* region;
        if(entry_lo == 0){
                region=lookup_region(faultaddress,&dirty);
                //check if region valid
                if(region==NULL){
                        return EFAULT;
                }
                //allocate frame, zero-fill, insert pte
                entry_lo=hpt_insert(pid,faultaddress,dirty);
        } else {
                region=lookup_region(faultaddress,&dirty);
                //change permission, if dirty =0
                if(dirty==0){
                        entry_lo = entry_lo & ~(TLBLO_DIRTY);
                }
        }

        entry_hi=faultaddress & PAGE_FRAME;
        //interrupt,and load tlb
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
