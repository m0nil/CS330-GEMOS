#include <types.h>
#include <mmap.h>
#include <fork.h>
#include <v2p.h>
#include <page.h>

/* 
 * You may define macros and other helper functions here
 * You must not declare and use any static/global variables 
 * */


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
            trav1->access_flags = prot;
            trav1 = trav1->vm_next;
            prev1 = prev1->vm_next;
            continue;
        }
        else if (trav1->vm_start >= addr && trav1->vm_end >= addr+length){
            //if this node has start inside the range but not the end then change the start address of this node and access flags
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
            trav1->vm_end = addr;
            struct vm_area *new_vma = os_alloc(sizeof(struct vm_area));
            if (new_vma == NULL){
                return -EINVAL;
            }
            stats->num_vm_area++;
            new_vma->vm_start = addr;
            new_vma->vm_end = addr+length;
            new_vma->access_flags = prot;
            new_vma->vm_next = trav1->vm_next;
            trav1->vm_next = new_vma;
            trav1 = new_vma->vm_next;
            prev1 = new_vma;
            continue;
        }
        else if (trav1->vm_start < addr && trav1->vm_end > addr+length){
            //if this node is partially inside the range then split the node into 3 nodes
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
     printk("mmap called \n");
    // scan the linked list of VMAs, to find the
    // appropriate position in the linked list where a new vm area node can be added or an existing vm area
    // node can be expanded.
    //making length multiples of 4KB
    length = (length + 4095) & ~4095;
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
        printk("mmap ended\n");

        return new_vma1->vm_start;
    }

    //if vma is not initially null then 
    // i) if hint address specified,
    if (addr!= 0){
        printk("hint address is not null\n");
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
                printk("mmap ended\n");
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
            printk("mmap ended\n");
            return addr;
        }
    }
    // ii) if hint address is not specified
    printk("hint address is null\n");
    struct vm_area *traverse = vma->vm_next;
    struct vm_area *prev = vma;
    //we can insert new node between prev and traverse so traverse->vm_start - prev->vm_end should be greater than length
    while(traverse!=NULL){
        if (traverse->vm_start - prev->vm_end >= length){
            printk("found space between %x and %x, %x, length = %d\n", prev->vm_end, traverse->vm_start, 
            traverse->vm_start - prev->vm_end, length);
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
        printk("mmap ended\n");
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
    printk("mmap ended\n");
    return ret_addr;
   
    // return -EINVAL;
}

/**
 * munmap system call implemenations
 */

long vm_area_unmap(struct exec_context *current, u64 addr, int length)
{   
     printk("munmap called \n");
    length = (length + 4095) & ~4095; //making length multiples of 4KB
    struct vm_area *vma = current->vm_area;
    
    struct vm_area *prev1 = vma;
    struct vm_area *trav1 = vma->vm_next;
    while(trav1!=NULL){
        if (trav1->vm_start >= addr && trav1->vm_end <= addr+length){
            //if this node is completely inside the range remove the node completely.
            prev1->vm_next = trav1->vm_next;
            os_free(trav1, sizeof(struct vm_area));
            stats->num_vm_area--;
            trav1 = NULL;
            trav1 = prev1->vm_next;
            continue;
        }
        else if (trav1->vm_start >= addr && trav1->vm_end >= addr+length){
            //if this node has start inside the range but not the end then change the start address of this node
            trav1->vm_start = addr+length;
            trav1 = trav1->vm_next;
            //after this we can break because no other node will be inside the range
            return 0;
            break;
        }
        else if (trav1->vm_start < addr && trav1->vm_end <= addr+length && trav1->vm_end > addr){
            //if this node has end inside the range but not the start then change the end address of this node
            trav1->vm_end = addr;
            trav1 = trav1->vm_next;
            prev1 = prev1->vm_next;
            continue;
        }
        else if (trav1->vm_start < addr && trav1->vm_end > addr+length){
            //if this node is partially inside the range then split the node into 2 nodes
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
    
    return -1;
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
  return -1;
}
