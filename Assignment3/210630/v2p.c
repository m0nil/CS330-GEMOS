#include <types.h>
#include <mmap.h>
#include <fork.h>
#include <v2p.h>
#include <page.h>

/* 
 * You may define macros and other helper functions here
 * You must not declare and use any static/global variables 
 * */
#define PAGE_SIZE 4096
u64* walk_to_pte_child(struct exec_context *current,u64 addr){
    //break the addr into 4*9 + 12 bits
    u64 pgd_offset = (addr >> 39) & 0x1FF;
    u64 pud_offset = (addr >> 30) & 0x1FF;
    u64 pmd_offset = (addr >> 21) & 0x1FF;
    u64 pte_offset = (addr >> 12) & 0x1FF;
    u64 physical_frame_offset = addr & 0xFFF;

    u64* pgd = (u64*)osmap(current->pgd);
        u64 pgd_t = *(pgd+pgd_offset);
        if ((pgd_t&1) == 0){ //present bit is 0
            u64 pfn = (u64)os_pfn_alloc(OS_PT_REG);
            if (pfn == 0){
                return NULL;
            }
            // printk("the pfn in pgd allocate is %x\n", pfn);
            pgd_t = (pfn << 12)|(pgd_t & 0xFFF);
            pgd_t = pgd_t | 16 | 8 | 1;
        }
        *(pgd+pgd_offset) = pgd_t;
        u64* pud = (u64*)osmap(pgd_t>>12);
        u64 pud_t = *(pud+pud_offset);
        if ((pud_t&1) == 0){ //present bit is 0
            // printk("entering pud\n");
            u64 pfn = (u64)os_pfn_alloc(OS_PT_REG);
            if (pfn == 0){
                return NULL;
            }
            // printk("the pfn in pud allocate is %x\n", pfn);
            pud_t = (pfn << 12)|(pud_t & 0xFFF);
            pud_t = pud_t | 16 | 8 | 1;
        }
        *(pud+pud_offset) = pud_t;
        u64* pmd = (u64*)osmap(pud_t>>12);
        u64 pmd_t = *(pmd+pmd_offset);
        if ((pmd_t&1 )== 0){ //present bit is 0
            // printk("entering pmd\n");
            u64 pfn =(u64) os_pfn_alloc(OS_PT_REG);
            if (pfn == 0){
                return NULL;
            }
            // printk("the pfn in pmd allocate is %x\n", pfn);
            pmd_t = (pfn << 12)|(pmd_t & 0xFFF);
            pmd_t = pmd_t | 16 | 8 | 1;
        }
        *(pmd+pmd_offset) = pmd_t;
        u64* pte =(u64*)osmap(pmd_t>>12);
        u64* pte_t = (pte+pte_offset);
        return pte_t;
}
void* get_pte_base(struct exec_context *current,u64 addr){
    //break the addr into 4*9 + 12 bits
    u64 pgd_offset = (addr >> 39) & 0x1FF;
    u64 pud_offset = (addr >> 30) & 0x1FF;
    u64 pmd_offset = (addr >> 21) & 0x1FF;
    u64 pte_offset = (addr >> 12) & 0x1FF;
    u64 physical_frame_offset = addr & 0xFFF;
    // printk("pgd_offset = %x, pud_offset = %x, pmd_offset = %x, pte_offset = %x, physical_frame_offset = %x\n", pgd_offset, pud_offset, pmd_offset, pte_offset, physical_frame_offset);
    //check the access flags of the node with the access asked by the process
    u64* pgd = (u64*)osmap(current->pgd);
    u64 pgd_t = *(pgd+pgd_offset);
    if ((pgd_t&1) == 0){ //present bit is 0
        return NULL;
    }
    u64* pud = (u64*)osmap(pgd_t>>12);
    u64 pud_t = *(pud+pud_offset);
    if ((pud_t&1) == 0){ //present bit is 0
        return NULL;
    }
    u64* pmd = (u64*)osmap(pud_t>>12);
    u64 pmd_t = *(pmd+pmd_offset);
    if ((pmd_t&1 )== 0){ //present bit is 0
        return NULL;
    }
    u64* pte =(u64*)osmap(pmd_t>>12);
    u64 pte_t = *(pte+pte_offset);
    if ((pte_t&1) == 0){ //present bit is 0
        return NULL;
    }
    return pte;
}
void free_pfn(struct exec_context *current,u64 addr){
    u64* pte = get_pte_base(current, addr);
    if (pte == 0){
        return;
    }
    //  u64 pgd_offset = (addr >> 39) & 0x1FF;
    // u64 pud_offset = (addr >> 30) & 0x1FF;
    // u64 pmd_offset = (addr >> 21) & 0x1FF;
    u64 pte_offset = (addr >> 12) & 0x1FF;
    // u64 physical_frame_offset = addr & 0xFFF;
    u64 pte_t = *(pte+ pte_offset);
    if (pte_t == 0 || (pte_t&1) == 0){
        return;
    }
    u64 pfn = pte_t>>12;
    if (get_pfn_refcount(pfn) == 1){
        put_pfn(pfn);
        os_pfn_free(USER_REG, pfn);
    }
    else{
        put_pfn(pfn);
    }
    // os_pfn_free(USER_REG, pfn); //free the pfn
    *(pte+pte_offset) = 0;
    asm volatile ("invlpg (%0);" :: "r"(addr)  : "memory"); //invalidating THE tlb entry 
    return;
}
void change_pte_protections(struct exec_context* current,u64 addr, int prot){
    u64* pte = get_pte_base(current, addr);
    if (pte == 0){
        return;
    }
    u64 pte_offset = (addr >> 12) & 0x1FF;
    u64 pte_t = *(pte+ pte_offset);
    if (pte_t == 0 || (pte_t&1) == 0){
        return;
    }
if (get_pfn_refcount(pte_t>>12) > 1){
    put_pfn(pte_t>>12);
}
else {
    if (prot == PROT_READ){
        //set 4th bit of pte_t to 0;
        pte_t = pte_t - (pte_t&8);
    }
    else if (prot == PROT_WRITE || prot == PROT_READ|PROT_WRITE){
        pte_t = pte_t | 8;
    }
    *(pte+pte_offset) = pte_t;
    asm volatile ("invlpg (%0);" :: "r"(addr)  : "memory"); //invalidating THE tlb entry 
    return;
}
}
/**
 * mprotect System call Implementation.
 */
long vm_area_mprotect(struct exec_context *current, u64 addr, int length, int prot)
{   
    length = (length + 4095) & ~4095; //making length multiples of 4KB
    struct vm_area *vma = current->vm_area;
    struct vm_area *prev1 = vma;
    struct vm_area *trav1 = vma->vm_next;
    //we have to change the access flags of all the nodes which are inside the range and create new VMAs if required.
    while(trav1!=NULL){
        if (trav1->vm_start >= addr && trav1->vm_end <= addr+length){
            //if this node is completely inside the range and access flags are same then do nothing.
            for (u64 i = trav1->vm_start; i < trav1->vm_end; i+=PAGE_SIZE){
                change_pte_protections(current, i, prot);
            }
            trav1->access_flags = prot;
            trav1 = trav1->vm_next;
            prev1 = prev1->vm_next;
            continue;
        }
        else if (trav1->vm_start >= addr && trav1->vm_start<=addr+length && trav1->vm_end >= addr+length){
            //if this node has start inside the range but not the end then change the start address of this node and access flags
            for (u64 i = trav1->vm_start; i < addr+length; i+=PAGE_SIZE){
                change_pte_protections(current, i, prot);
            }
            struct vm_area *new_vma = os_alloc(sizeof(struct vm_area));
            stats->num_vm_area++;
            new_vma->vm_start = addr+length;
            new_vma->vm_end = trav1->vm_end;
            new_vma->access_flags = trav1->access_flags;
            new_vma->vm_next = trav1->vm_next;
            trav1->vm_end = addr+length;
            trav1->access_flags = prot;
            trav1->vm_next = new_vma;
            //after this we can break because no other node will be inside the range
            break;
        }
        else if (trav1->vm_start < addr && trav1->vm_end < addr+length && trav1->vm_end > addr){
            //if this node has end inside the range but not the start then change the end address of this node
            for(u64 i = addr; i < trav1->vm_end; i+=PAGE_SIZE){
                change_pte_protections(current, i, prot);
            }
            trav1->vm_end = addr;
            struct vm_area *new_vma = os_alloc(sizeof(struct vm_area));
            if (new_vma == NULL){
                return -EINVAL;
            }
            stats->num_vm_area++;
            new_vma->vm_start = addr;
            new_vma->vm_end = trav1->vm_end;
            new_vma->access_flags = prot;
            new_vma->vm_next = trav1->vm_next;
            trav1->vm_next = new_vma;
            trav1 = new_vma->vm_next;
            prev1 = new_vma;
            continue;
        }
        else if (trav1->vm_start < addr && trav1->vm_end > addr+length){
            //if this node is partially inside the range then split the node into 3 nodes
            for(u64 i = addr; i < addr+length; i+=PAGE_SIZE){
                change_pte_protections(current, i, prot);
            }
            struct vm_area *new_vma = os_alloc(sizeof(struct vm_area));
            if (new_vma == NULL){
                return -EINVAL;
            }
            stats->num_vm_area++;
            new_vma->vm_start = addr;
            new_vma->vm_end = addr+length;
            new_vma->access_flags = prot;
            struct vm_area *new_vma1 = os_alloc(sizeof(struct vm_area));
            if (new_vma1 == NULL){
                return -EINVAL;
            }
            stats->num_vm_area++;
            new_vma1->vm_start = addr+length;
            new_vma1->vm_end = trav1->vm_end;
            new_vma1->access_flags = trav1->access_flags;
            new_vma1->vm_next = trav1->vm_next;
            new_vma->vm_next = new_vma1;
            trav1->vm_next = new_vma;
            trav1->vm_end = addr;
            //after this we can break because no other node will be inside the range
            break;
        }
        trav1 = trav1->vm_next;
        prev1 = prev1->vm_next;
    }  
    //now we have to merge the nodes if possible
    struct vm_area *trav2 = vma->vm_next;
    while(trav2->vm_next!=NULL){
        if (trav2->vm_end == trav2->vm_next->vm_start && trav2->access_flags == trav2->vm_next->access_flags){
            struct vm_area *temp = trav2->vm_next;
            trav2->vm_end = temp->vm_end;
            trav2->vm_next = temp->vm_next;
            os_free(temp, sizeof(struct vm_area));
            stats->num_vm_area--;
            temp = NULL;
            continue;
        }
        trav2 = trav2->vm_next;
    }
    //at the end of this loop all the merges will be done
    return 0;
  
}

/**
 * mmap system call implementation.
 */
long vm_area_map(struct exec_context *current, u64 addr, int length, int prot, int flags)
{   
    //  printk("mmap called \n");
    // scan the linked list of VMAs, to find the
    // appropriate position in the linked list where a new vm area node can be added or an existing vm area
    // node can be expanded.
    //making length multiples of 4KB
    length = (length + 4095) & ~4095;
    if (flags!= MAP_FIXED && flags!=0){
        return -EINVAL;
    }
    if (prot!=0x1 && prot!=0x3){
        return -EINVAL;
    }
    if (addr !=0 && (addr < MMAP_AREA_START || addr > MMAP_AREA_END)){
        return -EINVAL;
    }
    if (length <= 0 || length > 2*1024*1024){
        return -EINVAL;
    }
    if(addr == 0 && flags == MAP_FIXED){
        return -EINVAL;
    }
    struct vm_area *vma = current->vm_area;
    if (vma == NULL) {
        // create a new vm area node
        vma = os_alloc(sizeof(struct vm_area));
        if(vma == NULL)
            return -EINVAL;
        //creating dummy node
        vma->vm_start = MMAP_AREA_START;
        vma->vm_end = MMAP_AREA_START + 4096;
        vma->access_flags = 0x0;
        vma->vm_next = NULL;

        current->vm_area = vma;
        stats->num_vm_area = 1;
        //creating new node
        struct vm_area *new_vma1 = os_alloc(sizeof(struct vm_area));
        if (new_vma1 == NULL){
            return -EINVAL;
        }
        stats->num_vm_area++;
        if (addr == 0){ //if hint address is null
            addr = vma->vm_end;
        }
        new_vma1->vm_start = addr;
        new_vma1->vm_end = addr + length;
        new_vma1->access_flags = prot;
        new_vma1->vm_next = NULL;
        vma->vm_next = new_vma1;
        // printk("mmap ended\n");

        return new_vma1->vm_start;
    }

    //if vma is not initially null then 
    // i) if hint address specified,
    if (addr!= 0){
        // printk("hint address is not null\n");
        struct vm_area *traverse = vma;
        int flag = 0; //if flag = 1 then hint address is invalid
        while(traverse!=NULL){
            if ((traverse->vm_start <= addr && traverse->vm_end > addr) || (traverse->vm_start < addr + length && traverse->vm_end >= addr + length) || (traverse->vm_start >= addr && traverse->vm_end <= addr + length)){
                flag = 1;
                break;
            }
            traverse = traverse->vm_next;
        }
        if (flag && flags == MAP_FIXED){
            return -EINVAL;
        }
        else if (flag){
            //then hint address is invalid but MAP isnt fixed so we go with normal allocation.
        }
        else {
            struct vm_area *trav2 = vma->vm_next;
            struct vm_area *prev = vma;
            while(trav2!=NULL){
                if (trav2->vm_start > addr){
                    break;
                }
                prev = trav2;
                trav2 = trav2->vm_next;
            }
            if (trav2 == NULL){
                //if we reach the end of linked list then we have to check if there is enough space between last node and end of mmap area
                if (MMAP_AREA_END - prev->vm_end < length){
                    return -EINVAL;
                }
                //if there is enough space then we can insert new node at the end
                struct vm_area *new_vma = os_alloc(sizeof(struct vm_area));
                if (new_vma == NULL){
                    return -EINVAL;
                }
                stats->num_vm_area++;
                new_vma->vm_start = addr;
                new_vma->vm_end = addr + length;
                new_vma->access_flags = prot;
                new_vma->vm_next = NULL;
                prev->vm_next = new_vma;
                u64 ret_addr = new_vma->vm_start;
                //if trav2 and new_vma are consecutive then merge them
                if (prev->vm_end == new_vma->vm_start && prev->access_flags == new_vma->access_flags){
                    prev->vm_end = new_vma->vm_end;
                    prev->vm_next = new_vma->vm_next;
                    os_free(new_vma, sizeof(struct vm_area));
                    stats->num_vm_area--;
                    new_vma = NULL;
                }
                // printk("mmap ended\n");
                return ret_addr;
            }
            //so we have to insert new node between prev and trav2
            struct vm_area *new_vma = os_alloc(sizeof(struct vm_area));
            if (new_vma == NULL){
                return -EINVAL;
            }
            stats->num_vm_area++;
            new_vma->vm_start = addr;
            new_vma->vm_end = addr + length;
            new_vma->access_flags = prot;
            new_vma->vm_next = trav2;
            prev->vm_next = new_vma;
            //if all 3 are consecutive then merge them
            if (prev->vm_end == new_vma->vm_start && prev->access_flags == new_vma->access_flags && new_vma->vm_end == trav2->vm_start && new_vma->access_flags == trav2->access_flags){
                prev->vm_end = trav2->vm_end;
                prev->vm_next = trav2->vm_next;
                os_free(new_vma, sizeof(struct vm_area));
                os_free(trav2, sizeof(struct vm_area));
                stats->num_vm_area-=2;
                new_vma = NULL;
                trav2 = NULL;
            }
            else if (prev->vm_end == new_vma->vm_start && prev->access_flags == new_vma->access_flags){
                prev->vm_end = new_vma->vm_end;
                prev->vm_next = new_vma->vm_next;
                os_free(new_vma, sizeof(struct vm_area));
                stats->num_vm_area--;
                new_vma = NULL;
            }
            else if (new_vma->vm_end == trav2->vm_start && new_vma->access_flags == trav2->access_flags){
                new_vma->vm_end = trav2->vm_end;
                new_vma->vm_next = trav2->vm_next;
                os_free(trav2, sizeof(struct vm_area));
                stats->num_vm_area--;
                trav2 = NULL;
            }
            // printk("mmap ended\n");
            return addr;
        }
    }
    // ii) if hint address is not specified
    // printk("hint address is null\n");
    struct vm_area *traverse = vma->vm_next;
    struct vm_area *prev = vma;
    //we can insert new node between prev and traverse so traverse->vm_start - prev->vm_end should be greater than length
    while(traverse!=NULL){
        if (traverse->vm_start - prev->vm_end >= length){
            // printk("found space between %x and %x, %x, length = %d\n", prev->vm_end, traverse->vm_start, 
            // traverse->vm_start - prev->vm_end, length);
            break;
        }
        prev = traverse;
        traverse = traverse->vm_next;
    }
    if (traverse == NULL){
        //if we reach the end of linked list then we have to check if there is enough space between last node and end of mmap area
        if (MMAP_AREA_END - prev->vm_end < length){
            return -EINVAL;
        }
        //if there is enough space then we can insert new node at the end
        struct vm_area *new_vma = os_alloc(sizeof(struct vm_area));
        if (new_vma == NULL){
            return -EINVAL;
        }
        stats->num_vm_area++;
        new_vma->vm_start = prev->vm_end;
        new_vma->vm_end = prev->vm_end + length;
        new_vma->access_flags = prot;
        new_vma->vm_next = NULL;
        prev->vm_next = new_vma;
        u64 ret_addr = new_vma->vm_start;
        //if prev and new_vma are consecutive then merge them
        if (prev->vm_end == new_vma->vm_start && prev->access_flags == new_vma->access_flags){
                    prev->vm_end = new_vma->vm_end;
                    prev->vm_next = new_vma->vm_next;
                    os_free(new_vma, sizeof(struct vm_area));
                    stats->num_vm_area--;
                    new_vma = NULL;
        }
        // printk("mmap ended\n");
        return ret_addr;
    }
    //so we have to insert new node between prev and traverse
    struct vm_area *new_vma = os_alloc(sizeof(struct vm_area));
    if (new_vma == NULL){
        return -EINVAL;
    }
    stats->num_vm_area++;
    new_vma->vm_start = prev->vm_end;
    new_vma->vm_end = prev->vm_end + length;
    new_vma->access_flags = prot;
    new_vma->vm_next = traverse;
    prev->vm_next = new_vma;
    u64 ret_addr = new_vma->vm_start;
    //if all 3 are consecutive then merge them
    if (prev->vm_end == new_vma->vm_start && prev->access_flags == new_vma->access_flags && new_vma->vm_end == traverse->vm_start && new_vma->access_flags == traverse->access_flags){
        prev->vm_end = traverse->vm_end;
        prev->vm_next = traverse->vm_next;
        os_free(new_vma, sizeof(struct vm_area));
        os_free(traverse, sizeof(struct vm_area));
        stats->num_vm_area-=2;
        new_vma = NULL;
        traverse = NULL;
    }
    else if (prev->vm_end == new_vma->vm_start && prev->access_flags == new_vma->access_flags){
        prev->vm_end = new_vma->vm_end;
        prev->vm_next = new_vma->vm_next;
        os_free(new_vma, sizeof(struct vm_area));
        stats->num_vm_area--;
        new_vma = NULL;
    }
    else if (new_vma->vm_end == traverse->vm_start && new_vma->access_flags == traverse->access_flags){
        new_vma->vm_end = traverse->vm_end;
        new_vma->vm_next = traverse->vm_next;
        os_free(traverse, sizeof(struct vm_area));
        stats->num_vm_area--;
        traverse = NULL;
    }
    // printk("mmap ended\n");
    return ret_addr;
   
    // return -EINVAL;
}

/**
 * munmap system call implemenations
 */


long vm_area_unmap(struct exec_context *current, u64 addr, int length)
{   
    //  printk("munmap called \n");
    length = (length + 4095) & ~4095; //making length multiples of 4KB
    struct vm_area *vma = current->vm_area;
    
    struct vm_area *prev1 = vma;
    struct vm_area *trav1 = vma->vm_next;
    while(trav1!=NULL){
        if (trav1->vm_start >= addr && trav1->vm_end <= addr+length){
            //if this node is completely inside the range remove the node completely.
            //so we have to free all the allocated pages pte in this range. For this we will check the mapping in the page table and free the pages.
            for (u64 i = trav1->vm_start; i < trav1->vm_end; i+=PAGE_SIZE){
                free_pfn(current, i);
            }
            prev1->vm_next = trav1->vm_next;
            os_free(trav1, sizeof(struct vm_area));
            stats->num_vm_area--;
            trav1 = NULL;
            trav1 = prev1->vm_next;
            continue;
        }
        else if (trav1->vm_start >= addr && trav1->vm_end >= addr+length){
            //if this node has start inside the range but not the end then change the start address of this node
            for (u64 i = trav1->vm_start; i < addr+length; i+=PAGE_SIZE){
                free_pfn(current, i);
            }
            trav1->vm_start = addr+length;
            trav1 = trav1->vm_next;
            //after this we can break because no other node will be inside the range
            return 0;
            break;
        }
        else if (trav1->vm_start < addr && trav1->vm_end <= addr+length && trav1->vm_end > addr){
            //if this node has end inside the range but not the start then change the end address of this node
            for(u64 i = addr; i < trav1->vm_end; i+=PAGE_SIZE){
                free_pfn(current, i);
            }
            trav1->vm_end = addr;
            trav1 = trav1->vm_next;
            prev1 = prev1->vm_next;
            continue;
        }
        else if (trav1->vm_start < addr && trav1->vm_end > addr+length){
            //if this node is partially inside the range then split the node into 2 nodes
            for(u64 i = addr; i < addr+length; i+=PAGE_SIZE){
                free_pfn(current, i);
            }
            struct vm_area *new_vma = os_alloc(sizeof(struct vm_area));
            if (new_vma == NULL){
                return -EINVAL;
            }
            stats->num_vm_area++;
            new_vma->vm_start = addr+length;
            new_vma->vm_end = trav1->vm_end;
            new_vma->access_flags = trav1->access_flags;
            new_vma->vm_next = trav1->vm_next;
            trav1->vm_end = addr;
            trav1->vm_next = new_vma;
            trav1 = new_vma->vm_next;
            //after this we can break because no other node will be inside the range
            return 0;
        }
        prev1 = trav1;
        trav1 = trav1->vm_next;
    }   
    return 0;
}

/**
 * Function will invoked whenever there is page fault for an address in the vm area region
 * created using mmap
 */

long vm_area_pagefault(struct exec_context *current, u64 addr, int error_code)
{   
    // printk("entered vm_area_pagefault with addr = %x and error code = %x\n",addr, error_code);
    //finding the vm area node in which the address belongs
    struct vm_area *vma = current->vm_area;
    struct vm_area *trav = vma->vm_next;
    while(trav!=NULL){
        if (trav->vm_start <= addr && trav->vm_end > addr){
            break;
        }
        trav = trav->vm_next;
    }
    if (trav == NULL){
        return -1;
    }
    if (error_code == 0x6 && trav->access_flags == PROT_READ){
        // printk("returned from here 1");
        return -1;
    }
    if (error_code == 0x7 && trav->access_flags == PROT_READ){
        // printk("returned from here 2");
        return -1;
    }
    if (error_code!=0x4 && error_code!=0x6 && error_code!=0x7){
        // printk("returned from here 3");
        return -1;
    }
    //break the addr into 4*9 + 12 bits
    u64 pgd_offset = (addr >> 39) & 0x1FF;
    u64 pud_offset = (addr >> 30) & 0x1FF;
    u64 pmd_offset = (addr >> 21) & 0x1FF;
    u64 pte_offset = (addr >> 12) & 0x1FF;
    u64 physical_frame_offset = addr & 0xFFF;
    // printk("pgd_offset = %x, pud_offset = %x, pmd_offset = %x, pte_offset = %x, physical_frame_offset = %x\n", pgd_offset, pud_offset, pmd_offset, pte_offset, physical_frame_offset);
    //check the access flags of the node with the access asked by the process
    if (error_code == 0x4 || error_code == 0x6){ //read access or write
    //    printk("entered here 1\n");
        u64* pgd = (u64*)osmap(current->pgd);
        u64 pgd_t = *(pgd+pgd_offset);
        if ((pgd_t&1) == 0){ //present bit is 0
            u64 pfn = (u64)os_pfn_alloc(OS_PT_REG);
            if (pfn == 0){
                return -1;
            }
            // printk("the pfn in pgd allocate is %x\n", pfn);
            pgd_t = (pfn << 12)|(pgd_t & 0xFFF);
            pgd_t = pgd_t | 16 | 8 | 1;
        }
        *(pgd+pgd_offset) = pgd_t;
        u64* pud = (u64*)osmap(pgd_t>>12);
        u64 pud_t = *(pud+pud_offset);
        if ((pud_t&1) == 0){ //present bit is 0
            // printk("entering pud\n");
            u64 pfn = (u64)os_pfn_alloc(OS_PT_REG);
            if (pfn == 0){
                return -1;
            }
            // printk("the pfn in pud allocate is %x\n", pfn);
            pud_t = (pfn << 12)|(pud_t & 0xFFF);
            pud_t = pud_t | 16 | 8 | 1;
        }
        *(pud+pud_offset) = pud_t;
        u64* pmd = (u64*)osmap(pud_t>>12);
        u64 pmd_t = *(pmd+pmd_offset);
        if ((pmd_t&1 )== 0){ //present bit is 0
            // printk("entering pmd\n");
            u64 pfn =(u64) os_pfn_alloc(OS_PT_REG);
            if (pfn == 0){
                return -1;
            }
            // printk("the pfn in pmd allocate is %x\n", pfn);
            pmd_t = (pfn << 12)|(pmd_t & 0xFFF);
            pmd_t = pmd_t | 16 | 8 | 1;
        }
        *(pmd+pmd_offset) = pmd_t;
        u64* pte =(u64*)osmap(pmd_t>>12);
        u64 pte_t = *(pte+pte_offset);
        if ((pte_t&1) == 0){ //present bit is 0
            // printk("entering pte\n");
            u64 pfn = (u64) os_pfn_alloc(USER_REG); // this is the physical frame number storage type
            if (pfn == 0){
                return -1;
            }
            // printk("the pfn in pte allocate is %x\n", pfn);
            pte_t = (pfn << 12)|(pte_t & 0xFFF);
            pte_t = pte_t | 16 | 1;
        } 
        if (error_code == 0x6){
            pte_t = pte_t | 8;
        }
        *(pte+pte_offset) = pte_t;
        // printk("pgd_t = %x, pud_t = %x, pmd_t = %x, pte_t = %x\n", pgd_t, pud_t, pmd_t, pte_t);
        // printk("pgd = %x, pud = %x, pmd = %x, pte = %x\n", pgd, pud, pmd, pte);
        return 1;
    }
    else if (error_code == 0x7){
        handle_cow_fault(current, addr, trav->access_flags);
        return 1;
    }
    return 1;
}

/**
 * cfork system call implemenations
 * The parent returns the pid of child process. The return path of
 * the child process is handled separately through the calls at the 
 * end of this function (e.g., setup_child_context etc.)
 */

long do_cfork(){
    u32 pid;
    struct exec_context *new_ctx = get_new_ctx();
    struct exec_context *ctx = get_current_ctx();
     /* Do not modify above lines
     * 
     * */   
     /*--------------------- Your code [start]---------------*/
     //copying all the members of parent's ctx to the child's ctx
    // printk("entered do_cfork\n");

    new_ctx->type = ctx->type;
    new_ctx->used_mem = ctx->used_mem;
    new_ctx->pgd = ctx->pgd;
    new_ctx->regs = ctx->regs;
    new_ctx->state = ctx->state;
    new_ctx->os_rsp = ctx->os_rsp;
    // new_ctx->os_stack_pfn = ctx->os_stack_pfn;
    new_ctx->pending_signal_bitmap = ctx->pending_signal_bitmap;
    new_ctx->ticks_to_alarm = ctx->ticks_to_alarm;
    new_ctx->alarm_config_time = ctx->alarm_config_time;
    new_ctx->ticks_to_sleep = ctx->ticks_to_sleep;

    //copyiing the name name
	for (int i = 0; i < CNAME_MAX; i++) new_ctx->name[i] = ctx->name[i];
	//copying sighandler array
	for (int i = 0; i < MAX_SIGNALS; i++) new_ctx->sighandlers[i] = ctx->sighandlers[i];
	//copying files
	for (int i = 0; i < MAX_OPEN_FILES; i++) new_ctx->files[i] = ctx->files[i]; 
    // file main ref coun, usko upate kar sakta hai
    //copying the memory segments
    for (int i = 0; i < MAX_MM_SEGS; i++){
        new_ctx->mms[i].start = ctx->mms[i].start;
        new_ctx->mms[i].next_free = ctx->mms[i].next_free;
        new_ctx->mms[i].end = ctx->mms[i].end;
        new_ctx->mms[i].access_flags = ctx->mms[i].access_flags;
    }   
    //setting the pid and ppid of the child
    pid = new_ctx->pid;
    new_ctx->ppid = ctx->pid;
    
    if (ctx->vm_area != NULL){
        //copying the vm_area linked list
        struct vm_area *vma = ctx->vm_area;
        struct vm_area *new_vma = os_alloc(sizeof(struct vm_area));
        if (new_vma == NULL){
            return -1;
        }
        // stats->num_vm_area++;
        new_vma->vm_start = vma->vm_start;
        new_vma->vm_end = vma->vm_end;
        new_vma->access_flags = vma->access_flags;
        new_vma->vm_next = NULL;
        new_ctx->vm_area = new_vma;

        struct vm_area *trav = vma->vm_next;
        struct vm_area *prev = new_vma;
        while(trav!=NULL){
            struct vm_area *new_vma1 = os_alloc(sizeof(struct vm_area));
            if (new_vma1 == NULL){
                return -1;
            }
            // stats->num_vm_area++;
            new_vma1->vm_start = trav->vm_start;
            new_vma1->vm_end = trav->vm_end;
            new_vma1->access_flags = trav->access_flags;
            new_vma1->vm_next = NULL;
            prev->vm_next = new_vma1;
            prev = prev->vm_next;
            trav = trav->vm_next;
        }
    }
    else {
        new_ctx->vm_area = NULL;
    }
    //allocating a new pgd using os_pfn_alloc
    u64 pfn = (u64)os_pfn_alloc(OS_PT_REG);
    if (pfn == 0){
        return -1;
    }
    new_ctx->pgd = pfn; //setting the pgd of the child
    
    //allocating pte's for the memory segments MM_SEG_CODE,MM_SEG_RODATA,MM_SEG_DATA,MM_SEG_STACK and the VMAs of the child
    //for the memory segments
    for (int i = 0; i < MAX_MM_SEGS; i++){
        u64 start = new_ctx->mms[i].start;
        u64 end;
        if (i== MM_SEG_STACK) end = new_ctx->mms[i].end;
        else end = new_ctx->mms[i].next_free;
        u64 access_flags = new_ctx->mms[i].access_flags;
        // printk("mms_i=%x,start = %x, end = %x, access_flags = %x\n",i, start, end, access_flags);
        for (u64 j = start; j < end; j+=PAGE_SIZE){
            u64* parent_pte_base = get_pte_base(ctx, j);
            if (parent_pte_base == NULL){
                continue;
            }
            u64 parent_pte_entry = *(parent_pte_base + ((j>>12)&0x1FF));
            if ((parent_pte_entry&1) == 1){ //present bit check
            // printk("allocating memory segment pte for %x and %x\n", i,j);
            u64* pte_t=walk_to_pte_child(new_ctx, j);
            if (pte_t == NULL){
                return -1;
            }
            // printk("pte_t = %x\n", pte_t);
            //pte_t should point to the same entry as the parent
            
            *pte_t = parent_pte_entry - (parent_pte_entry & 8); //only changing the 4th bit to read only no write acess.
            *(parent_pte_base + ((j>>12)&0x1FF))= *pte_t; //also changing the parent's pte 4th bit to read only no write acess.
            // printk("reference count of pfn %x is %d\n", (*pte_t)>>12, get_pfn_refcount((*pte_t)>>12));
            get_pfn((*pte_t)>>12);
            }
        }
    }
    //for the vm_area linked list
    struct vm_area *trav = new_ctx->vm_area->vm_next;
    while(trav!=NULL){
        for(u64 i = trav->vm_start;i<trav->vm_end;i+=PAGE_SIZE){
            //if physical allocated in parent then only allocate in child
            u64* parent_pte_base = get_pte_base(ctx, i);
            if (parent_pte_base == NULL){
                continue;
            }
            u64 parent_pte_entry = *(parent_pte_base + ((i>>12)&0x1FF));
            if ((parent_pte_entry&1) == 1){ //present bit check
                // printk("allocating vm_area pte for %x\n", i);
                u64* pte_t=walk_to_pte_child(new_ctx, i);
                if (pte_t == NULL){
                    return -1;
                }
                *pte_t = parent_pte_entry - (parent_pte_entry & 8); //only changing the 4th bit to read only no write acess.
                *(parent_pte_base + ((i>>12)&0x1FF))= *pte_t; //also changing the parent's pte 4th bit to read only no write acess.
                // printk("reference count of pfn %x is %d\n", (*pte_t)>>12, get_pfn_refcount((*pte_t)>>12));
                get_pfn((*pte_t)>>12);
            }
        }
        trav = trav->vm_next;
    }
     /*--------------------- Your code [end] ----------------*/
    
     /*
     * The remaining part must not be changed
     */
    copy_os_pts(ctx->pgd, new_ctx->pgd);
    do_file_fork(new_ctx);
    setup_child_context(new_ctx);
    return pid;
}



/* Cow fault handling, for the entire user address space
 * For address belonging to memory segments (i.e., stack, data) 
 * it is called when there is a CoW violation in these areas. 
 *
 * For vm areas, your fault handler 'vm_area_pagefault'
 * should invoke this function
 * */

long handle_cow_fault(struct exec_context *current, u64 vaddr, int access_flags)
{
 // cow occurs when a process tries to write to a read only access page
    // printk("entered handle_cow_fault\n");
    //we already have the access flags of the vm_area to which  vaddr belongs
    //we have to check if the access flags of the vm_area is PROT_READ
    if (access_flags == PROT_READ){
        return -1;
    }
    u64* pte_base = get_pte_base(current, vaddr);
    u64* pte = pte_base + ((vaddr>>12)&0x1FF);
    u64 pte_t = *pte;
    if (get_pfn_refcount(pte_t>>12) > 1){
    if ((pte_t&1) == 0){ //present bit is 1 means write access is already there
        return 1;
    }
    //if present bit is 0 then we have to allocate a new page and copy the contents of the old page to the new page
    u64 pfn_new = (u64)os_pfn_alloc(USER_REG);
    if (pfn_new == 0){
        return -1;
    }
    u64 pfn_old = pte_t>>12;
    memcpy((char*)osmap(pfn_new), (char*)osmap(pfn_old), PAGE_SIZE);
    put_pfn(pfn_old);
    //we just have to change the pte entry of the vaddr to the new pfn
    //the pte entry of the new should have write bit 1
    pte_t = (pfn_new << 12)|(pte_t & 0xFFF)|8|1;
    *pte = pte_t;
    }
    else {
        //if refcount is 1 then we can just change the pte entry of the vaddr to have write bit 1
        pte_t = pte_t | 8;
        *pte = pte_t;
    }
    asm volatile ("invlpg (%0);" :: "r"(vaddr)  : "memory"); //invalidating THE tlb entry
    return 1;
}
