#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <addrspace.h>
#include <vm.h>


#define OFFSET_BITS 12
/* Place your frametable data-structures here
 * You probably also want to write a frametable initialisation
 * function and call it from vm_bootstrap
 */
/*note: frame number shift to left twlve bit can get the physical address
 reversly, physical adress right shift 12 bits can get the frame nuber
 */

/*next_free == 0 indicates that the frame is used. */
struct frame_table_entry {
        int next_free;
};

struct frame_table_entry *frame_table = NULL;
static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;
/*lock for frametable since it's share section*/
static struct spinlock ft_lock = SPINLOCK_INITIALIZER;
/*the first free frame of the frametable*/
static int head;
/*frame table initilization
 */
void ft_init(void){
        /*call ram function*/
        paddr_t size_of_ram=ram_getsize();
        /*set up frame table size and location*/
        unsigned int nframes= size_of_ram /PAGE_SIZE;
        paddr_t location=size_of_ram - (nframes*sizeof(struct frame_table_entry));
        frame_table = (struct frame_table_entry*) PADDR_TO_KVADDR(location);

        paddr_t firstfree = ram_getfirstfree();
        /*for loop to  inilisied the value of frame_table.*/

        unsigned int i;
        //struct frame_table_entry tmp;
        for(i=0;i < nframes;i++){
                if(i == nframes-1){
                        frame_table[i].next_free=-1; /*set the last frame. next_free to -1*/
                } else{
                        frame_table[i].next_free=i+1;
                }
                //memmove(&frame_table[i], &tmp, sizeof(tmp));
        }


        /*initilised the used frame for os161, bottom of the frame table*/
        for (i= 0 ; i<= firstfree >> OFFSET_BITS; i ++){
                frame_table[i].next_free=0;
        }
        frame_table[i].next_free=0;
        head= i+1;
        /*initilised the used frame for the frame table, top of the the frame table*/
        for(i = nframes-1 ; i >= location >> OFFSET_BITS; i --){
                frame_table[i].next_free=0;
        }
        frame_table[i-1].next_free=-1;


}

/* Note that this function returns a VIRTUAL address, not a physical
 * address
 * WARNING: this function gets called very early, before
 * vm_bootstrap().  You may wish to modify main.c to call your
 * frame table initialisation function, or check to see if the
 * frame table has been initialised and call ram_stealmem() otherwise.
 */

vaddr_t alloc_kpages(unsigned int npages)
{
        spinlock_acquire(&ft_lock);
         paddr_t addr=0;

        if(frame_table==NULL){
                spinlock_acquire(&stealmem_lock);
                addr = ram_stealmem(npages);
                spinlock_release(&stealmem_lock);
        }else{

                if(npages != 1){
                        spinlock_release(&ft_lock);
                        return 0;
                }
                if(head == -1){
                        spinlock_release(&ft_lock);
                        return ENOMEM;
                }
                addr = head <<12;
                // if(head < 119) kprintf("alloc ing %d\n", head);

                addr &=PAGE_FRAME;
                int next = frame_table[head].next_free;
                /*check if the current frame is the last free frame*/
                frame_table[head].next_free=0;
                if(frame_table[head].next_free== -1){
                        head = -1;
                } else{
                        head = next;
                }

        }

        spinlock_release(&ft_lock);
        if(addr == 0){
                return 0;
        }
        bzero((void *)PADDR_TO_KVADDR(addr), PAGE_SIZE);
        return PADDR_TO_KVADDR(addr);
}

void free_kpages(vaddr_t addr)
{
        addr &=PAGE_FRAME;
        paddr_t paddr = KVADDR_TO_PADDR(addr);
        int pfn = paddr >>OFFSET_BITS;
        spinlock_acquire(&ft_lock);
        if(frame_table[pfn].next_free != 0){
                spinlock_release(&ft_lock);
                return;
        }

        if(head == -1){
               frame_table[pfn].next_free=-1;
        }else{
                frame_table[pfn].next_free=head;
        }
        head=pfn;
        spinlock_release(&ft_lock);

}
