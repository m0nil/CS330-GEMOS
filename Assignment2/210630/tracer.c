#include<context.h>
#include<memory.h>
#include<lib.h>
#include<entry.h>
#include<file.h>
#include<tracer.h>


///////////////////////////////////////////////////////////////////////////
//// 		Start of Trace buffer functionality 		      /////
///////////////////////////////////////////////////////////////////////////
int is_valid_mem_range(unsigned long buff, u32 count, int access_bit) 
{	
	// printk("inside valid mem\n");
	struct exec_context *current = get_current_ctx();
	//access bit is made of 3 bits for read, write and execute
	int read_bit = access_bit & 0x1;
	int write_bit = (access_bit >> 1) & 0x1;
	int exec_bit = (access_bit >> 2) & 0x1;
	//checking if the memory range is valid
	if (buff >= current->mms[MM_SEG_CODE].start && buff+count <= current->mms[MM_SEG_CODE].next_free-1){
		if (read_bit == 1 && write_bit == 0){
			// printk("here1\n");
			return 0;
		}
		else{
			return -1;
		}
	}
	if (buff >= current->mms[MM_SEG_RODATA].start && buff+count <= current->mms[MM_SEG_RODATA].next_free-1){
		if (read_bit == 1 && write_bit == 0){
			// printk("here2\n");
			return 0;
		}
		else{
			return -1;
		}
	}
	if (buff >= current->mms[MM_SEG_DATA].start && buff+count <= current->mms[MM_SEG_DATA].next_free-1){
		if (read_bit == 1 || write_bit == 1){
			// printk("here3\n");
			return 0;
		}
		else{
			return -1;
		}
	}
	if (buff >= current->mms[MM_SEG_STACK].start && buff+count <= current->mms[MM_SEG_STACK].end-1){
		if (read_bit == 1 || write_bit == 1){
			// printk("here4\n");
			return 0;
		}
		else{
			return -1;
		}
	}
	//checking if memory range is in vm area
	struct vm_area *vm_area = current->vm_area;
	while(vm_area != NULL){
		if (buff >= vm_area->vm_start && buff+count <= vm_area->vm_end-1){
			//access bits for vm area
			int vm_read_bit = vm_area->access_flags & 0x1;
			int vm_write_bit = (vm_area->access_flags >> 1) & 0x1;
			int vm_exec_bit = (vm_area->access_flags >> 2) & 0x1;
			if (read_bit == vm_read_bit && write_bit == vm_write_bit && exec_bit == vm_exec_bit){
				// printk("here5\n");
				return 0;
			}
			else{
				return -1;
			}
		}
		vm_area = vm_area->vm_next;
	}
	return -1;
}



long trace_buffer_close(struct file *filep)
{	
	// return 0;
	// printk("inside close\n");
	//freeing the trace buffer
	// struct exec_context* current = get_current_ctx();
	// for (int i=0;i<MAX_OPEN_FILES;i++){
	// 	if (current->files[i] == filep){
	// 		current->files[i] = NULL;
	// 		printk("files[i] ka i is %d\n",i);
	// 		break;
	// 	}
	// }
	// printk("yes closed");
	// os_page_free(USER_REG,filep->trace_buffer->buff);
	// filep->trace_buffer->buff = NULL;
	// os_free(filep->trace_buffer,sizeof(struct trace_buffer_info));
	// filep->trace_buffer = NULL;
	// os_free(filep->fops,sizeof(struct fileops));
	// filep->fops = NULL;
	// os_free(filep,sizeof(struct file));
	// filep = NULL;
	//freeing the file object
	if(filep != NULL)
	{
		if(filep->trace_buffer != NULL)
		{
			if(filep->trace_buffer->buff != NULL)
			{
				os_page_free(USER_REG, filep->trace_buffer->buff);
				filep->trace_buffer->buff = NULL;	
			}
			os_free(filep->trace_buffer,sizeof(struct trace_buffer_info));
			filep->trace_buffer = NULL;
		}
		if(filep->fops != NULL)
		{
			os_free(filep->fops,sizeof(struct fileops));
		filep->fops = NULL;
		}
		os_free(filep,sizeof(struct file));
		filep = NULL;
	}
	// return 0;
	// if (filep == NULL) printk("yes\n");
	return 0;	
}



int trace_buffer_read(struct file *filep, char *buff, u32 count)
{	
	if (filep->mode == O_WRITE)
		return -EINVAL;
	if (is_valid_mem_range((unsigned long)buff, count, filep->mode) == -1)
		return -EBADMEM;
	// printk("current space in file is %d while read offset is %d and write offset is %d\n",filep->trace_buffer->space,filep->trace_buffer->R,filep->trace_buffer->W);
	// if (is_valid_mem_range((unsigned long)buff, count, 2) == -1)
	// 	return -EBADMEM;
	//checking if the trace buffer is empty
	if (filep->trace_buffer->space == TRACE_BUFFER_MAX_SIZE)
		return 0;
	if (count > TRACE_BUFFER_MAX_SIZE - filep->trace_buffer->space)
		count = TRACE_BUFFER_MAX_SIZE - filep->trace_buffer->space;
	
	//copying the data from the trace buffer
	for(int i=0; i<count; i++)
	{
		buff[i] = filep->trace_buffer->buff[(filep->trace_buffer->R + i)%TRACE_BUFFER_MAX_SIZE];
	}
	filep->trace_buffer->R = (filep->trace_buffer->R + count)%TRACE_BUFFER_MAX_SIZE;
	filep->trace_buffer->space += count;
	return count;
	return 0;
}


int trace_buffer_write(struct file *filep, char *buff, u32 count)
{	
	// printk("inside trace buffer write ");
	if (filep->mode == O_READ)
		return -EINVAL;
	if (is_valid_mem_range((unsigned long)buff, count, filep->mode) == -1)
		return -EBADMEM;
	// printk("current space in file is %d while read offset is %d and write offset is %d\n",filep->trace_buffer->space,filep->trace_buffer->R,filep->trace_buffer->W);

	// if (is_valid_mem_range((unsigned long)buff, count, 1) == -1)
	// 	return -EBADMEM;
	//checking if the trace buffer is full
	if (filep->trace_buffer->space == 0)
		return 0;
	if (count > filep->trace_buffer->space)
		count = filep->trace_buffer->space;
	
	//copying the data to the trace buffer
	for(int i=0; i<count; i++)
	{
		filep->trace_buffer->buff[(filep->trace_buffer->W + i)%TRACE_BUFFER_MAX_SIZE] = buff[i];
	}
	filep->trace_buffer->W = (filep->trace_buffer->W + count)%TRACE_BUFFER_MAX_SIZE;
	filep->trace_buffer->space -= count;
	// printf("%d\n",count);
	return count;
    return 0;
}


int sys_create_trace_buffer(struct exec_context *current, int mode)
{	
	//Finding free file descriptor
	int fd;
	for(fd=0; fd<MAX_OPEN_FILES; fd++)
	{
		if(current->files[fd] == NULL)
			break;
	}
	if(fd == MAX_OPEN_FILES)
		return -EINVAL;
	//allocating a file object
	 struct file *filep = os_alloc(sizeof(struct file));
	if(!filep)
		return -ENOMEM;
	filep->type = TRACE_BUFFER;
	filep->mode = mode;
	filep->offp = 0;
	filep->ref_count = 1;
	filep->inode = NULL;
	struct trace_buffer_info* trace_buffer = os_alloc(sizeof(struct trace_buffer_info));
	if(!trace_buffer)
		return -ENOMEM;
	char* buff = os_page_alloc(USER_REG);
	if (!buff) return -ENOMEM;
	trace_buffer->buff = buff;
	trace_buffer->R = 0;
	trace_buffer->W = 0;
	trace_buffer->space = TRACE_BUFFER_MAX_SIZE;
	filep->trace_buffer = trace_buffer;
	struct fileops* fops = os_alloc(sizeof(struct fileops));
	if (!fops) return -ENOMEM;
	fops->read = trace_buffer_read;
	fops->write = trace_buffer_write;
	fops->close = trace_buffer_close;
	filep->fops = fops;
	current->files[fd] = filep;
	// printk("current space in file is %d while read offset is %d and write offset is %d\n",filep->trace_buffer->space,filep->trace_buffer->R,filep->trace_buffer->W);
	return fd;
	//allocating a buffer
	// return 0;
}

///////////////////////////////////////////////////////////////////////////
//// 		Start of strace functionality 		      	      /////
///////////////////////////////////////////////////////////////////////////


int perform_tracing(u64 syscall_num, u64 param1, u64 param2, u64 param3, u64 param4)
{	
	
	struct exec_context *current = get_current_ctx();
		//checking if strace is enabled
	if (current->st_md_base == NULL)
		return 0;
	if (current->st_md_base->is_traced == 0)
		return 0;
	// printk("perform tracing called\n");
	//if we have reached here means we have to trace the syscall
	struct trace_buffer_info *strace_buffer = current->files[current->st_md_base->strace_fd]->trace_buffer;
	if (current->st_md_base->tracing_mode == FILTERED_TRACING){
		//checking if the syscall is traced
		struct strace_info *st_info = current->st_md_base->next;
		while(st_info != NULL){
			if (st_info->syscall_num == syscall_num)
				break;
			st_info = st_info->next;
		}
		if (st_info == NULL)
			return 0;
	}
	if (syscall_num == SYSCALL_START_STRACE || syscall_num == SYSCALL_END_STRACE ){
		return 0;
	}
	//printing the syscall number
	int no_of_params = 0;
	switch ((int)syscall_num)
	{
	case SYSCALL_EXIT:
		no_of_params = 1;
		break;
	case SYSCALL_GETPID:
		no_of_params = 0;
		break;
	case SYSCALL_GETPPID:
		no_of_params = 0;
		break;
	case SYSCALL_EXPAND:
		no_of_params = 2;
		break;
	case SYSCALL_SHRINK:
		no_of_params = 2;
		break;
	case SYSCALL_ALARM:
		no_of_params = 1;
		break;
	case SYSCALL_SLEEP:
		no_of_params = 1;
		break;
	case SYSCALL_SIGNAL:
		no_of_params = 2;
		break;
	case SYSCALL_CLONE:
		no_of_params = 2;
		break;
	case SYSCALL_FORK:
		no_of_params = 0;
		break;
	case SYSCALL_STATS:
		no_of_params = 0;
		break;
	case SYSCALL_CONFIGURE:
		no_of_params = 1;
		break;
	case SYSCALL_PHYS_INFO:
		no_of_params = 0;
		break;
	case SYSCALL_DUMP_PTT:
		no_of_params = 1;
		break;
	case SYSCALL_CFORK:
		no_of_params = 0;
		break;
	case SYSCALL_MMAP:
		no_of_params = 4;
		break;
	case SYSCALL_MUNMAP:
		no_of_params = 2;
		break;
	case SYSCALL_MPROTECT:
		no_of_params = 3;
		break;
	case SYSCALL_PMAP:
		no_of_params = 1;
		break;
	case SYSCALL_VFORK:
		no_of_params = 0;
		break;
	case SYSCALL_GET_USER_P:
		no_of_params = 0;
		break;
	case SYSCALL_GET_COW_F:
		no_of_params = 0;
		break;
	case SYSCALL_OPEN:
		no_of_params = 2;
		break;
	case SYSCALL_READ:
		no_of_params = 3;
		break;
	case SYSCALL_WRITE:
		no_of_params = 3;
		break;
	case SYSCALL_DUP:
		no_of_params = 1;
		break;
	case SYSCALL_DUP2:
		no_of_params = 2;
		break;
	case SYSCALL_CLOSE:
		no_of_params = 1;
		break;
	case SYSCALL_LSEEK:
		no_of_params = 3;
		break;
	case SYSCALL_FTRACE:
		no_of_params = 4;
		break;
	case SYSCALL_TRACE_BUFFER:
		no_of_params = 1;
		break;
	case SYSCALL_START_STRACE:
		no_of_params = 2;
		break;
	case SYSCALL_END_STRACE:
		no_of_params = 0;
		break;
	case SYSCALL_READ_STRACE:
		no_of_params = 3;
		break;
	case SYSCALL_STRACE:
		no_of_params = 2;
		break;
	case SYSCALL_READ_FTRACE:
		no_of_params = 3;
		break;
	default:
		return 0;
		break;
	}
	struct trace_buffer_info *trace_buffer = current->files[current->st_md_base->strace_fd]->trace_buffer;
	//we have to fill all the values in 8 bytes so we are using a 64 bit integer
	*(u64*)(trace_buffer->buff + trace_buffer->W) = syscall_num;
	// printk("syscall num in trace buffer is %u\n",*(u64*)(trace_buffer->buff + trace_buffer->W));
	trace_buffer->W = (trace_buffer->W + 8)%TRACE_BUFFER_MAX_SIZE;
	trace_buffer->space -= 8;
	//printing the parameters	
	u64 params[4]={param1, param2, param3, param4};
	// printk("The syscall num is %u and no of param = %d with params %u %u %u %u\n",syscall_num,no_of_params,param1,param2,param3,param4);
	for (int i=0; i<no_of_params; i++){
		*(u64*)(trace_buffer->buff + trace_buffer->W) = params[i];
		// printk("parameter %d is %u\n",i,*(u64*)(trace_buffer->buff + trace_buffer->W));
		trace_buffer->W = (trace_buffer->W + 8)%TRACE_BUFFER_MAX_SIZE;
		trace_buffer->space -= 8;
		// printk("trace_buffer->W is %d and space  is %d\n",trace_buffer->W,trace_buffer->space);
	}
    return 0;
}


int sys_strace(struct exec_context *current, int syscall_num, int action)
{	
	// printk("sys_strace called \n");
	struct strace_head *st_md_base;
	if (current->st_md_base == NULL){ //if the struct is not allocated then initialising.
		st_md_base= os_alloc(sizeof(struct strace_head));
		if(!st_md_base)
			return -EINVAL;
		st_md_base->count = 0;
		st_md_base->is_traced = 0; 
		st_md_base->strace_fd = -1;
		st_md_base->tracing_mode = -1;
		st_md_base->next = NULL;
		st_md_base->last = NULL;
		current->st_md_base = st_md_base;
	}
	else {
	st_md_base = current->st_md_base;
	}
	if (action == ADD_STRACE){
		//if the syscall is already traced then return
		struct strace_info *st_info = st_md_base->next;
		while(st_info != NULL){
			if (st_info->syscall_num == syscall_num)
				return -EINVAL;
			st_info = st_info->next;
		}
		//adding the syscall to the list of traced syscalls
		st_info = os_alloc(sizeof(struct strace_info));
		st_info->syscall_num = syscall_num;
		st_info->next = NULL;
		if (st_md_base->count == 0){
			st_md_base->next = st_info;
		}
		else{
			st_md_base->last->next = st_info;	
		}
		st_md_base->last = st_info;
		st_md_base->count++;
	}
	else if (action == REMOVE_STRACE){
		//if the syscall is not traced then return
		struct strace_info *st_info = st_md_base->next;
		struct strace_info *prev = NULL;
		while(st_info != NULL){
			if (st_info->syscall_num == syscall_num)
				break;
			prev = st_info;
			st_info = st_info->next;
		}
		if (st_info == NULL)
			return -EINVAL;
		//removing the syscall from the list of traced syscalls
		if (st_info == st_md_base->next){
			st_md_base->next = st_info->next;
			if (st_info == st_md_base->last)
				st_md_base->last = NULL;
		}
		else if (st_info == st_md_base->last){
			st_md_base->last = prev;
			prev->next = NULL;
		}
		else{
			prev->next = st_info->next;
		}
		os_free(st_info,sizeof(struct strace_info));
		st_info = NULL;
		st_md_base->count--;
	}
	return 0;
}

int sys_read_strace(struct file *filep, char *buff, u64 count)
{	
	// printk("read_strace called\n");
	if (filep->mode == O_WRITE)
		return -EINVAL;
	// if (is_valid_mem_range((unsigned long)buff, count, filep->mode) == -1)
	// 	return -EBADMEM;
	int ct=count;
	// printk("count is %d\n",ct);
	int pos_in_buff =0;
	while(ct--){
	int syscall_num = *(u64*)(filep->trace_buffer->buff + filep->trace_buffer->R);
	// printk("syscall_num is %d\n",syscall_num);
	if (syscall_num == 0)
		return pos_in_buff;
	filep->trace_buffer->R = (filep->trace_buffer->R + 8)%TRACE_BUFFER_MAX_SIZE;
	filep->trace_buffer->space += 8;
	//printing the syscall number
	*(u64*)(buff + pos_in_buff) = (u64)syscall_num;
	pos_in_buff += 8;
	//printing the parameters
	int no_of_params = 0;
	switch (syscall_num)
	{
	case SYSCALL_EXIT:
		no_of_params = 1;
		break;
	case SYSCALL_GETPID:
		no_of_params = 0;
		break;
	case SYSCALL_GETPPID:
		no_of_params = 0;
		break;
	case SYSCALL_EXPAND:
		no_of_params = 2;
		break;
	case SYSCALL_SHRINK:
		no_of_params = 2;
		break;
	case SYSCALL_ALARM:
		no_of_params = 1;
		break;
	case SYSCALL_SLEEP:
		no_of_params = 1;
		break;
	case SYSCALL_SIGNAL:
		no_of_params = 2;
		break;
	case SYSCALL_CLONE:
		no_of_params = 2;
		break;
	case SYSCALL_FORK:
		no_of_params = 0;
		break;
	case SYSCALL_STATS:
		no_of_params = 0;
		break;
	case SYSCALL_CONFIGURE:
		no_of_params = 1;
		break;
	case SYSCALL_PHYS_INFO:
		no_of_params = 0;
		break;
	case SYSCALL_DUMP_PTT:
		no_of_params = 1;
		break;
	case SYSCALL_CFORK:
		no_of_params = 0;
		break;
	case SYSCALL_MMAP:
		no_of_params = 4;
		break;
	case SYSCALL_MUNMAP:
		no_of_params = 2;
		break;
	case SYSCALL_MPROTECT:
		no_of_params = 3;
		break;
	case SYSCALL_PMAP:
		no_of_params = 1;
		break;
	case SYSCALL_VFORK:
		no_of_params = 0;
		break;
	case SYSCALL_GET_USER_P:
		no_of_params = 0;
		break;
	case SYSCALL_GET_COW_F:
		no_of_params = 0;
		break;
	case SYSCALL_OPEN:
		no_of_params = 2;
		break;
	case SYSCALL_READ:
		no_of_params = 3;
		break;
	case SYSCALL_WRITE:
		no_of_params = 3;
		break;
	case SYSCALL_DUP:
		no_of_params = 1;
		break;
	case SYSCALL_DUP2:
		no_of_params = 2;
		break;
	case SYSCALL_CLOSE:
		no_of_params = 1;
		break;
	case SYSCALL_LSEEK:
		no_of_params = 3;
		break;
	case SYSCALL_FTRACE:
		no_of_params = 4;
		break;
	case SYSCALL_TRACE_BUFFER:
		no_of_params = 1;
		break;
	case SYSCALL_START_STRACE:
		no_of_params = 2;
		break;
	case SYSCALL_END_STRACE:
		no_of_params = 0;
		break;
	case SYSCALL_READ_STRACE:
		no_of_params = 3;
		break;
	case SYSCALL_STRACE:
		no_of_params = 2;
		break;
	case SYSCALL_READ_FTRACE:
		no_of_params = 3;
		break;
	default:
		return 0;
		break;
	}
	for (int i=0;i<no_of_params;i++){
		//printing the parameters
		u64 param = *(u64*)(filep->trace_buffer->buff + filep->trace_buffer->R);
		filep->trace_buffer->R = (filep->trace_buffer->R + 8)%TRACE_BUFFER_MAX_SIZE;
		filep->trace_buffer->space += 8;
		*(u64*)(buff + pos_in_buff) = (u64)param;
		pos_in_buff += 8;
	}
	}
	return pos_in_buff;
	return 0;
}

int sys_start_strace(struct exec_context *current, int fd, int tracing_mode)
{	
	// printk("start_strace called\n");
	if (current->st_md_base != NULL)
		{
			current->st_md_base->strace_fd = fd;
			current->st_md_base->tracing_mode = tracing_mode;
			current->st_md_base->is_traced = 1;
			return 0;
		}
	struct strace_head *st_md_base= os_alloc(sizeof(struct strace_head));
	if(!st_md_base)
		return -EINVAL;
	st_md_base->count = 0;
	st_md_base->is_traced = 1; //1 for enabled and 0 for disabled
	st_md_base->strace_fd = fd;
	st_md_base->tracing_mode = tracing_mode;
	st_md_base->next = NULL;
	st_md_base->last = NULL;
	current->st_md_base = st_md_base;
	return 0;
}

int sys_end_strace(struct exec_context *current)
{	
	// printk("end_strace called\n");
	if (current->st_md_base == NULL)
		return -EINVAL;
	current->st_md_base->strace_fd = -1;
	current->st_md_base->tracing_mode = -1;
	current->st_md_base->count = 0;
	current->st_md_base->is_traced = 0;
	//freeing all strace info
	struct strace_info* st_info =  current->st_md_base->next;
	struct strace_info* st_temp;
	while(st_info != NULL){
		st_temp = st_info;
		st_info = st_info->next;
		os_free(st_temp,sizeof(struct strace_info));
		st_temp = NULL;
	}
	current->st_md_base->next = NULL;
	current->st_md_base->last = NULL;
	os_free(current->st_md_base,sizeof(struct strace_head));
	current->st_md_base = NULL;
	return 0;
}

///////////////////////////////////////////////////////////////////////////
//// 		Start of ftrace functionality 		      	      /////
///////////////////////////////////////////////////////////////////////////


long do_ftrace(struct exec_context *ctx, unsigned long faddr, long action, long nargs, int fd_trace_buffer)
{	

	if (ctx->ft_md_base == NULL){ //if the struct is not allocated then initialising.
		// printk("new ft_md_base is created\n");
		struct ftrace_head *ft_md_base= os_alloc(sizeof(struct ftrace_head));
		if(!ft_md_base)
			return -EINVAL;
		ft_md_base->count = 0;
		ft_md_base->next = NULL;
		ft_md_base->last = NULL;
		ctx->ft_md_base = ft_md_base;
	}
	struct ftrace_head *ft_md_base = ctx->ft_md_base;
	if (action == ADD_FTRACE){
		//if the count of traced functions exceed FTRACE_MAX then return
		if (ft_md_base->count == FTRACE_MAX)
			return -EINVAL;
		//if the function is already traced then return
		struct ftrace_info *ft_info = ft_md_base->next;
		while(ft_info != NULL){
			if (ft_info->faddr == faddr)
				return -EINVAL;
			ft_info = ft_info->next;
		}
		//adding the function to the list of traced functions
		ft_info = os_alloc(sizeof(struct ftrace_info));
		ft_info->faddr = faddr;
		ft_info->num_args = nargs;
		ft_info->fd = fd_trace_buffer;
		
		ft_info->next = NULL;
		if (ft_md_base->count == 0){
			ft_md_base->next = ft_info;
		}
		else{
			ft_md_base->last->next = ft_info;
		}
		ft_md_base->last = ft_info;
		ft_md_base->count++;
		return 0;
	}
	else if (action == REMOVE_FTRACE){
		//if the function is not traced then return
		struct ftrace_info *ft_info = ft_md_base->next;
		struct ftrace_info *prev = NULL;
		while(ft_info != NULL){
			if (ft_info->faddr == faddr)
				break;
			prev = ft_info;
			ft_info = ft_info->next;
		}
		if (ft_info == NULL)
			return -EINVAL;
		//before removing we need to remove tracing from the function
		if (*(u8*)ft_info->faddr == INV_OPCODE){
			for(int i=0;i<4;i++){
			*(u8*)(ft_info->faddr + i) = ft_info->code_backup[i];
		}
		}
		//removing the function from the list of traced functions
		if (ft_info == ft_md_base->next){
			ft_md_base->next = ft_info->next;
			if (ft_info == ft_md_base->last)
				ft_md_base->last = NULL;
		}
		else if (ft_info == ft_md_base->last){
			ft_md_base->last = prev;
			prev->next = NULL;
		}
		else{
			prev->next = ft_info->next;
		}
		os_free(ft_info,sizeof(struct ftrace_info));
		ft_info = NULL;
		ft_md_base->count--;
		return 0;
	}
	else if (action == ENABLE_FTRACE){
		//for an already added function we need to enable tracing
		// printk("reaching enable ftrace\n");
		struct ftrace_info *ft_info = ft_md_base->next;
		while(ft_info != NULL){
			if (ft_info->faddr == faddr)
				break;
			ft_info = ft_info->next;
		}
		if (ft_info == NULL){
			// printk("ft_info is null inside enble ftrace\n");
			return -EINVAL;
		}
		//we need to change the first 4 bytes of the function to INV_OPCODE and store the original opcode in the ft_info->code_backup 0 till 4
		for(int i=0;i<4;i++){
			ft_info->code_backup[i] = *(u8*)(ft_info->faddr + i);
			*(u8*)(ft_info->faddr + i) = INV_OPCODE;
		}
		return 0;
	}
	else if (action == DISABLE_FTRACE){
		//for an already added function we need to disable tracing
		struct ftrace_info *ft_info = ft_md_base->next;
		while(ft_info != NULL){
			if (ft_info->faddr == faddr)
				break;
			ft_info = ft_info->next;
		}
		if (ft_info == NULL)
			return -EINVAL;
		//we need to restore the original opcode from the ft_info->code_backup
		for(int i=0;i<4;i++){
			*(u8*)(ft_info->faddr + i) = ft_info->code_backup[i];
		}
		return 0;
	}
	else if (action == ENABLE_BACKTRACE){
		//for an already added function we need to enable backtracing
		struct ftrace_info *ft_info = ft_md_base->next;
		while(ft_info != NULL){
			if (ft_info->faddr == faddr)
				break;
			ft_info = ft_info->next;
		}
		if (ft_info == NULL)
			return -EINVAL;
		//if before backtracing the function was not traced then we need to enable normal tracing
		if (*(u8*)ft_info->faddr != INV_OPCODE){
			for(int i=0;i<4;i++){
			ft_info->code_backup[i] = *(u8*)(ft_info->faddr + i);
			*(u8*)(ft_info->faddr + i) = INV_OPCODE;
		}
		}
		ft_info->capture_backtrace = 1;
		return 0;
	}
	else if (action == DISABLE_BACKTRACE){
		//for an already added function we need to disable backtracing
		struct ftrace_info *ft_info = ft_md_base->next;
		while(ft_info != NULL){
			if (ft_info->faddr == faddr)
				break;
			ft_info = ft_info->next;
		}
		if (ft_info == NULL)
			return -EINVAL;
		//if before backtracing the function was traced then we need to disable normal tracing as well
		if (*(u8*)ft_info->faddr == INV_OPCODE){
			for(int i=0;i<4;i++){
			*(u8*)(ft_info->faddr + i) = ft_info->code_backup[i];
		}
		}
		ft_info->capture_backtrace = 0;
		return 0;
	}
		
    return 0;
}

//Fault handler
long handle_ftrace_fault(struct user_regs *regs)
{		
	struct exec_context *ctx = get_current_ctx();
	struct ftrace_head *ft_md_base = ctx->ft_md_base;
	struct ftrace_info *ft_info = ft_md_base->next;
	while(ft_info != NULL){
		if (ft_info->faddr == regs->entry_rip)
			break;
		ft_info = ft_info->next;
	}
	if (ft_info == NULL)
		return -EINVAL;
	// printk("yes we are inside the fault handler for faddr %u\n",ft_info->faddr);
	//if we have reached here means we have to atleast perform the normal tracing
	struct trace_buffer_info *trace_buffer = ctx->files[ft_info->fd]->trace_buffer;
	//checking if the trace buffer is full
	if (trace_buffer->space == 0)
		return -EINVAL;
	//we need to save function address and the arguments passed to the function to the trace buffer
	*(u64*)(trace_buffer->buff + trace_buffer->W) = ft_info->faddr;
	trace_buffer->W = (trace_buffer->W + 8)%TRACE_BUFFER_MAX_SIZE;
	trace_buffer->space -= 8;
	//saving the arguments max 5 allowed but doing it for 6 anyway
	u64 arg_reg[6] = {regs->rdi,regs->rsi,regs->rdx,regs->rcx,regs->r8,regs->r9};
	for(int i=0;i<ft_info->num_args;i++){
		*(u64*)(trace_buffer->buff + trace_buffer->W) = arg_reg[i];
		trace_buffer->W = (trace_buffer->W + 8)%TRACE_BUFFER_MAX_SIZE;
		trace_buffer->space -= 8;
	}
	//if backtrace is enabled then we need to save the backtrace
	if (ft_info->capture_backtrace == 1){
		//checking if the trace buffer is full
		if (trace_buffer->space == 0)
			return -EINVAL;
		//we need to save the backtrace to the trace buffer
		//say main->f1->f2 and backtracing for f2.
		//first save the address of the first instruction of f2, then the return address in f1, then the return address in main
		*(u64*)(trace_buffer->buff + trace_buffer->W) = regs->entry_rip;
		trace_buffer->W = (trace_buffer->W + 8)%TRACE_BUFFER_MAX_SIZE;
		trace_buffer->space -= 8;
		//the return address is stored in rbp + 8 so lets go to that address.
		// printk("address of first instruction is %d\n",regs->entry_rip);
		u64 return_address = *(u64*)(regs->entry_rsp);
		u64 prev_rbp = (regs->rbp);
		while (return_address!=END_ADDR)
		{	
			// printk("return_address is %d\n",return_address);
			if (trace_buffer->space == 0)
			return -EINVAL;
			*(u64*)(trace_buffer->buff + trace_buffer->W) = return_address;
			trace_buffer->W = (trace_buffer->W + 8)%TRACE_BUFFER_MAX_SIZE;
			trace_buffer->space -= 8;
			return_address = *(u64*)(prev_rbp + 8);
			prev_rbp = *(u64*)(prev_rbp);
		}
	}
	//now we need to add a delimiter in the trace buffer to identify that this is the end of the function
	if (trace_buffer->space == 0)
		return -EINVAL;
	*(u64*)(trace_buffer->buff + trace_buffer->W) = END_ADDR;
	trace_buffer->W = (trace_buffer->W + 8)%TRACE_BUFFER_MAX_SIZE;
	trace_buffer->space -= 8;

	//since we had changed the first 2 instruction of function so we need to make the required changes
	//so basically we need to do 2 instructions :
	// push rbp
	*(u64*)(regs->entry_rsp - 8) = regs->rbp;
	regs->entry_rsp -= 8;
	// mov rsp, rbp
	regs->rbp = regs->entry_rsp;
	//now we need to change the entry_rip to the next instruction that is the 3rd instruction
	regs->entry_rip += 4;
	// printk("entry_rip is %d\n",regs->entry_rip);
    return 0;
}

int sys_read_ftrace(struct file *filep, char *buff, u64 count)
{	
	struct trace_buffer_info *trace_buffer = filep->trace_buffer;
	int bytes_written = 0;
	while(count--){
		if (trace_buffer->space == TRACE_BUFFER_MAX_SIZE)
			break;
		//reading the func addr;
		*(u64*)(buff + bytes_written) = *(u64*)(trace_buffer->buff + trace_buffer->R);
		trace_buffer->R = (trace_buffer->R + 8)%TRACE_BUFFER_MAX_SIZE;
		trace_buffer->space += 8;
		bytes_written += 8;
		//reading the arguments
		while(*(u64*)(trace_buffer->buff + trace_buffer->R) != END_ADDR){
			*(u64*)(buff + bytes_written) = *(u64*)(trace_buffer->buff + trace_buffer->R);
			trace_buffer->R = (trace_buffer->R + 8)%TRACE_BUFFER_MAX_SIZE;
			trace_buffer->space += 8;
			bytes_written += 8;
		}
		//reading the delimiter but it is not written.
		trace_buffer->R = (trace_buffer->R + 8)%TRACE_BUFFER_MAX_SIZE;
		trace_buffer->space += 8;
	}
	return bytes_written;
    // return 0;
}


