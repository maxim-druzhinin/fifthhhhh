#pragma once

typedef uint64 *pagetable_t;

enum procstate { UNUSED, USED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE }; 


struct  namespace_node
{
	int cnt_proc;
	int last_pid;
	struct proc* init;
	struct namespace_node* head;
	int depth;
};


struct proccess_info {
	enum procstate state;
	int pid;
	int parent_pid;
	uint64 sz;
	int cntOfile;
	char name[16];
	uint startTime;
	uint lastStartTime;
	int cntChange;
	int totalTime;
};


struct write_info {
	enum procstate state;
	int a7;
	uint64 file_ptr;
	char* str_ptr;
	int buf_size;
}; 
