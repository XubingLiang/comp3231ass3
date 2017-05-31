struct PTE {
        uint32_t pid;
        vaddr_t vaddr;          //page number
        uint32_t entry_lo;      //entry_lo contain
        struct node *next_node;  //next node in the same index of hash table
};

struct PTE* hash_page_table[TABLE_SIZE];
