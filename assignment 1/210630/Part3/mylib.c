#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>

#define MEM_BLOCK_SIZE (4 * 1024 * 1024)  // 4MB pool size
#define NEXT(ptr) (*(void**)(ptr + 8))
#define PREV(ptr) (*(void**)(ptr + 16))
#define SIZE(ptr) (*(unsigned long*)(ptr))
void * free_list = NULL;
void *memalloc(unsigned long size) 
{	
	if (size == 0) {
		return NULL;
	}
	if (free_list == NULL) {
		//we can use mmap to allocate memory in multiples of 4MB only
		// printf("free_list is NULL\n");
		size_t rounded_size = ((size + 8 + MEM_BLOCK_SIZE - 1) / MEM_BLOCK_SIZE) * MEM_BLOCK_SIZE;
		void* ptr = mmap(NULL, rounded_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		// printf("mmap called ptr = %ld\n", ptr);
		if (ptr == MAP_FAILED) {
			perror("memalloc");
			return NULL;
		}
		SIZE(ptr) = rounded_size;
		NEXT(ptr) = NULL;
		PREV(ptr)= NULL;
		free_list = ptr;
	}
	
	void* temp = free_list;
	while (temp!= NULL){
		// printf("size of temp is %ld\n",*(unsigned long*)temp);
		if (SIZE(temp) >= size+8) {
			// printf("temp is %ld\n", temp);
			size_t padded_size = ((size + 7 + 8) / 8) * 8;
			if (padded_size < 24) {
				padded_size = 24;
			}
			// printf("padded_size is %ld\n", padded_size);
			if (SIZE(temp) - padded_size	 >= 24){ 
				//we will split the free memory block into two blocks one allocated and another free
				//temp first 8 bytes shoul store the size of memory
				//if temp is the head of the free list then we need to update the head
				if (temp == free_list) {
					free_list = NEXT(temp);
					// printf("free_list is %ld\n", free_list);
					if (free_list != NULL) {
						PREV(free_list)= NULL;
					}
				}
				//if temp is not the head of the free list then we need to update the prev of the next of temp
				else {
					// Extract pointers to the previous and next nodes
					void* prev_node = PREV(temp); // temp->prev
					void* next_node = NEXT(temp);  // temp->next
					// Update the next pointer of the previous node
					NEXT(prev_node) = next_node; // temp->prev->next = temp->next

					// Update the previous pointer of the next node
					if (next_node != NULL)
					PREV(next_node) = prev_node; // temp->next->prev = temp->prev
				}

				//temp 1 is the new chunk of free memory to be added to the free list at the head.
				void *temp1;
				temp1 = temp + padded_size;
				SIZE(temp1) = SIZE(temp) - padded_size;
				// printf("temp1 is %ld\n", temp1); 
				NEXT(temp1) = free_list;
				PREV(temp1) = NULL;
				if (free_list != NULL) {
					PREV(free_list) = temp1;
				}
				free_list = temp1;
				SIZE(temp) = padded_size;
				return temp + 8;
			}
			if (*(unsigned long*)temp - padded_size < 24) { //we will allocate the whole free memory block

				//if temp is the head of the free list then we need to update the head

				if (temp == free_list) {
					free_list = NEXT(temp);
					// printf("free_list is %ld\n", free_list);
					if (free_list != NULL) {
						PREV(free_list) = NULL;
					}
				}
				//if temp is not the head of the free list then we need to update the prev of the next of temp
				else {
				
				// Extract pointers to the previous and next nodes
					void* prev_node = PREV(temp); // temp->prev
					void* next_node = NEXT(temp);  // temp->next
					// Update the next pointer of the previous node
					NEXT(prev_node) = next_node; // temp->prev->next = temp->next

					// Update the previous pointer of the next node
					if (next_node != NULL)
					PREV(next_node) = prev_node; // temp->next->prev = temp->prev
				}
				
				return temp + 8;
			}
		}
		temp = NEXT(temp);
	}
	//if we reach here it means we have not found any free memory block of size greater than or equal to the requested size
	//we will allocate a new memory block of given size and add the remaining memory to the free list
	size_t rounded_size = ((size + 8 + MEM_BLOCK_SIZE - 1) / MEM_BLOCK_SIZE) * MEM_BLOCK_SIZE;
	void* ptr = mmap(NULL, rounded_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (ptr == MAP_FAILED) {
		perror("memalloc");
		return NULL;
	}
	SIZE(ptr) = rounded_size;
	NEXT(ptr) = NULL;
	PREV(ptr)= NULL;
	if (free_list == NULL) {
		free_list = ptr;
	}
	else {
		NEXT(ptr) = free_list;
		PREV(free_list) = ptr;
		free_list = ptr;
	}
	return memalloc(size);
	// printf("memalloc() called\n");
	// return NULL;
}

int memfree(void *ptr)
{	
	if (ptr == NULL) {
		return -1;
	}
	void* temp= ptr - 8; //temp is the pointer to the start of the memory block to be freed
	void* traverse_list = free_list;
	void* temp_prev = NULL; //temp_prev is the pointer to the memory block contiguosly just before temp
	void* temp_next = NULL; //temp_next is the pointer to the memory block contiguosly just after temp
	size_t temp_size = SIZE(temp);
	if (free_list == NULL) {
		NEXT(temp)= NULL;
		PREV(temp) = NULL;
		free_list = temp;
		return 0;
	}
	while (traverse_list != NULL) {
		if (traverse_list+SIZE(traverse_list) == temp) {
			temp_prev = traverse_list;
		}
		if (temp+temp_size == traverse_list) {
			temp_next = traverse_list;
		}
		traverse_list = NEXT(traverse_list);
	}
	if (temp_next == NULL && temp_prev == NULL){ //no temp_next or temp_prev found
		//temp has to be made the head of the free list
		PREV(temp) = NULL;
		NEXT(temp) = free_list;
		PREV(free_list) = temp;
		free_list = temp;
		return 0;
	}
	else if (temp_next != NULL && temp_prev == NULL) { //temp_next found but no temp_prev found
		//we need to merge temp and temp_next and then put the merged node at the head of free list
	
		if (temp_next == free_list){
			free_list = NEXT(temp_next);
			SIZE(temp) = SIZE(temp) + SIZE(temp_next);
			NEXT(temp) = free_list;
			if (free_list != NULL) 
				PREV(free_list) = temp;
			PREV(temp) = NULL;
			free_list = temp;
			return 0;
		}
		
		if (NEXT(temp_next) != NULL) 
			PREV(NEXT(temp_next)) = PREV(temp_next);
		NEXT(PREV(temp_next)) = NEXT(temp_next);
		SIZE(temp) = SIZE(temp) + SIZE(temp_next);
		NEXT(temp) = free_list;
		if (free_list != NULL) 
			PREV(free_list) = temp;
		PREV(temp) = NULL;
		free_list = temp;
		return 0;
	}
	else if (temp_next == NULL && temp_prev != NULL){ //temp_prev found but no temp_next found
		//we need to merge temp and temp_prev and then put the merged node at the head of free list
		if (temp_prev == free_list){
			free_list = NEXT(temp_prev);
			SIZE(temp_prev) = SIZE(temp_prev) + SIZE(temp);
			NEXT(temp_prev) = free_list;
			if (free_list != NULL) 
				PREV(free_list) = temp_prev;
			PREV(temp_prev) = NULL;
			free_list = temp_prev;
			return 0;
		}
		if (NEXT(temp_prev) != NULL) 
			PREV(NEXT(temp_prev)) = PREV(temp_prev);
		NEXT(PREV(temp_prev)) = NEXT(temp_prev);
		SIZE(temp_prev) = SIZE(temp_prev) + SIZE(temp);
		NEXT(temp_prev) = free_list;
		if (free_list != NULL) 
			PREV(free_list) = temp_prev;
		PREV(temp_prev) = NULL;
		free_list = temp_prev;
		return 0;
	}
	else if (temp_next != NULL && temp_prev != NULL){ //both temp_next and temp_prev found
		//remove temp_prev from linked list
		if (temp_prev == free_list){
			free_list = NEXT(temp_prev);
		}
		else {
		
		
		if (PREV(temp_prev)!= NULL){
			NEXT(PREV(temp_prev)) = NEXT(temp_prev);
		}
		if (NEXT(temp_prev)!= NULL){
			PREV(NEXT(temp_prev)) = PREV(temp_prev);
		}
		}
		//remove temp_next from linked list
		if (temp_next == free_list){
			free_list = NEXT(temp_next);
		}
		else {
		if (PREV(temp_next)!= NULL){
			NEXT(PREV(temp_next)) = NEXT(temp_next);
		}
		if (NEXT(temp_next)!= NULL){
			PREV(NEXT(temp_next)) = PREV(temp_next);
		}
		}
		SIZE(temp_prev) = SIZE(temp_prev) + SIZE(temp) + SIZE(temp_next);
		NEXT(temp_prev) = free_list;
		if (free_list != NULL) 
			PREV(free_list) = temp_prev;
		PREV(temp_prev) = NULL;
		free_list = temp_prev;
		return 0;
	}
	// printf("temp is %ld\n", temp);
	// printf("memfree() called\n");
	return 0;
}	


