#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "defMyStructs.h"

struct cpu cpus[NCPU];

struct proc proc[NPROC];


int used[64];
struct namespace_node nodes[NNMND];

struct proc *initproc;

int nextpid = 1;
struct spinlock pid_lock;

extern void forkret(void);
static void freeproc(struct proc *p);

extern char trampoline[]; // trampoline.S

// helps ensure that wakeups of wait()ing
// parents are not lost. helps obey the
// memory model when using p->parent.
// must be acquired before any p->lock.
struct spinlock wait_lock;



// do job ??
// check for two node - is one the heir of other
int 
is_heir(struct namespace_node* one, struct namespace_node* other) {
  struct namespace_node* local_node = one; 
  while (local_node->depth > 0)
  {
    if (local_node == other) {
      return 1;
    }
    local_node = local_node->head;
  }
  return local_node == other;
  
}


// do job ??
// add paramaetrn for choose namespace

// fix call this func 
struct proc* 
find_proc(int pid, struct namespace_node* node) { // programist have to release this proc
	for (int i = 0; i < NPROC; ++i) {
		acquire(&proc[i].lock);
		if (proc[i].namespace_pids[node->depth] == pid && is_heir(proc[i].node, node)) {
			return &proc[i];
		}
		release(&proc[i].lock);	
	}
	return 0;
}


// do job ???
// get pid/ppid in namespace
int 
get_pid(struct proc* p, struct namespace_node* node) {
  if (is_heir(p->node, node)) {
    return p->namespace_pids[node->depth];
  }
  else {

    return 0;
  }
}

int 
get_ppid(struct proc* p, struct namespace_node* node) {

  return (p == initproc) ? 0 : get_pid(p->parent, node);
}


int 
ps_list(int limit, uint64 pids)
{
  // do job ???
  // fix code for work in namespace
  struct proc *mp = myproc();
  int cntProcces[6] = {0, 0, 0, 0, 0, 0};
  struct proc* p;
  for(p = proc; p < &proc[NPROC]; p++)
  {
    acquire(&p->lock);
	  ++cntProcces[p->state];
    release(&p->lock);
  }
    
  // int totalCnt = 
	//   cntProcces[USED]     +
  //   cntProcces[SLEEPING] +
  //   cntProcces[RUNNABLE] +
  //   cntProcces[RUNNING]  + 
  //   cntProcces[ZOMBIE];
  int cntPrint = 0;

  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    // do job ???
    // check p is heir of mp;
    if (p->state != 0 && is_heir(p->node, mp->node)) {
      // ("%d ", p->namespace_pids[mp->node->depth]);
      copyout(mp->pagetable, pids + 4 * cntPrint, (char*)(&p->namespace_pids[mp->node->depth]), 4); 
      ++cntPrint;
    }
    release(&p->lock);
    if (cntPrint == limit)
      break;
  }
  return cntPrint;
}

int 
ps_info(int pid, uint64 pinfoAddr) {
  struct proc *myp = myproc(), *p = find_proc(pid, myp->node);
		
  if (p->namespace_pids[myp->node->depth] != pid) {
    release(&p->lock);
    return -1;
  }
	p->pinfo.sz = p->sz;
  p->pinfo.state = p->state;
  p->pinfo.cntOfile = 0;

	for (int i = 0; i < NOFILE; ++i) {
		p->pinfo.cntOfile += (p->ofile[i] != 0);
  }

  for (int i = 0; i < 16; ++i) {
		p->pinfo.name[i] = p->name[i];
  }

  p->pinfo.pid = get_pid(p, myp->node);
  p->pinfo.parent_pid = get_ppid(p, myp->node);
  copyout(myp->pagetable, pinfoAddr, (char*)&(p->pinfo), sizeof(struct proccess_info));
  release(&p->lock);
  return 0;
}


pagetable_t
copy_from_level(pagetable_t pagetable, uint64 va, int target_level) {
	if (va >= MAXVA) 
		panic("ps_pt");
	
	for (int level = 2; level > target_level; --level) {
		pte_t *pte = &pagetable[PX(level, va)];
		if (*pte & PTE_V) {
			pagetable = (pagetable_t)PTE2PA(*pte);
		} else {
			return 0;	
		}
	}

	return pagetable;
}	

int 
ps_pt0(int pid, uint64 tableAddr) { 
  struct proc *myp = myproc(), *p = find_proc(pid, myp->node);
  
	if (p == 0) return -1;
	
	acquire(&myp->lock);
	copyout(myp->pagetable, tableAddr, (char*) p->pagetable, 512 * 8);
	release(&p->lock);
	release(&myp->lock);
	return 0; 
}

int 
ps_pt1(int pid, uint64 addr, uint64 tableAddr) { 
	struct proc *myp = myproc(), *p = find_proc(pid, myp->node);

	if (p == 0) return -1;	
	
	acquire(&myp->lock);
	pagetable_t pagetable = copy_from_level(p->pagetable, addr, 1);
	copyout(myp->pagetable, tableAddr, (char*) pagetable, 512 * 8);
	release(&p->lock);
	release(&myp->lock);
	return 0; 
}


int 
ps_pt2(int pid, uint64 addr, uint64 tableAddr) { 
	struct proc *myp = myproc(), *p = find_proc(pid, myp->node);
	if (p == 0) return -1;	
	
	acquire(&myp->lock);
	pagetable_t pagetable = copy_from_level(p->pagetable, addr, 0);
	copyout(myp->pagetable, tableAddr, (char*) pagetable, 512 * 8);
	release(&p->lock);
	release(&myp->lock);
	return 0; 
	
}


int 
ps_copy(int pid, uint64 addr, int size, uint64 copyAddr) {
	struct proc *myp = myproc(), *p = find_proc(pid, myp->node);
  acquire(&myp->lock);
  while(size > 0){
    pagetable_t pagetable = copy_from_level(p->pagetable, addr, 0);
    pte_t* pte = &pagetable[PX(0, addr)];
	  if (* pte && PTE_V) {
      char * pa = (char*) PTE2PA(*pte);

      int n = PGSIZE - PGROUNDDOWN((uint64) pa) + (uint64) pa;
      copyout(myp->pagetable, copyAddr, pa, n);
      size -= n;
      addr += n;
      copyAddr += n;
      
    } else {
      return -1;
    }
  }
  release(&myp->lock);
	release(&p->lock);
	return 0;
}


int
ps_sleep_info(int pid, uint64 infoAddr) {
	struct proc *myp = myproc(), *p = find_proc(pid, myp->node);
	
	if (p == 0) return -1;
	struct write_info w = {p->state, p->trapframe->a7, p->trapframe->a0, (char*) p->trapframe->a1, p->trapframe->a2};
	
  if (myp != p) acquire(&myp->lock);
	copyout(myp->pagetable, infoAddr, (char*) &w, sizeof(struct write_info));
	release(&p->lock);
	if (myp != p) release(&myp->lock);
	return 0;
}


// Allocate a page for each process's kernel stack.
// Map it high in memory, followed by an invalid
// guard page.
void
proc_mapstacks(pagetable_t kpgtbl)
{
  struct proc *p;
  
  for(p = proc; p < &proc[NPROC]; p++) {

    char *pa = kalloc();
    if(pa == 0)
      panic("kalloc");
    uint64 va = KSTACK((int) (p - proc));
    kvmmap(kpgtbl, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
  }
}

// initialize the proc table.
void
procinit(void)
{
  struct proc *p;
  
  initlock(&pid_lock, "nextpid");
  initlock(&wait_lock, "wait_lock");
  for(p = proc; p < &proc[NPROC]; p++) {
      initlock(&p->lock, "proc");
      p->state = UNUSED;
      p->kstack = KSTACK((int) (p - proc));
  }
}

// Must be called with interrupts disabled,
// to prevent race with process being moved
// to a different CPU.
int
cpuid()
{
  int id = r_tp();
  return id;
}

// Return this CPU's cpu struct.
// Interrupts must be disabled.
struct cpu*
mycpu(void)
{
  int id = cpuid();
  struct cpu *c = &cpus[id];
  return c;
}

// Return the current struct proc *, or zero if none.
struct proc*
myproc(void)
{
	push_off();
	struct cpu *c = mycpu();
	struct proc *p = c->proc;
	pop_off();
	return p;
}

	int
allocpid()
{
	int pid;

	acquire(&pid_lock);
	pid = nextpid;
	nextpid = nextpid + 1;
	release(&pid_lock);
	
	return pid;
}

// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel,
// and return with p->lock held.
// If there are no free procs, or a memory allocation fails, return 0.
	static struct proc*
allocproc(void)
{
	struct proc *p;

	for(p = proc; p < &proc[NPROC]; p++) {
		acquire(&p->lock);
		if(p->state == UNUSED) {
			goto found;
		} else {
			release(&p->lock);
		}
	}
	return 0;

found:
	p->pid = allocpid();
	p->state = USED;

	// Allocate a trapframe page.

    
	if((p->trapframe = (struct trapframe *)buddy_alloc(1)) == 0){
	// if((p->trapframe = (struct trapframe *)kalloc()) == 0){
		freeproc(p);
		release(&p->lock);
		return 0;
	}
  // printf("alloc in %p\n", p->trapframe);


	// An empty user page table.
	p->pagetable = proc_pagetable(p);
	if(p->pagetable == 0){
		freeproc(p);
		release(&p->lock);
		return 0;
	}

	// Set up new context to start executing at forkret,
	// which returns to user space.
	memset(&p->context, 0, sizeof(p->context));
	p->context.ra = (uint64)forkret;
	p->context.sp = p->kstack + PGSIZE;

	return p;
}

// free a proc structure and the data hanging from it,
// including user pages.
// p->lock must be held.
	static void
freeproc(struct proc *p)
{
	if(p->trapframe)
		buddy_free((void*)p->trapframe);
	p->trapframe = 0;
	if(p->pagetable)
		proc_freepagetable(p->pagetable, p->sz);
	p->pagetable = 0;
	p->sz = 0;
	p->pid = 0;
	p->parent = 0;
	p->name[0] = 0;
	p->chan = 0;
	p->killed = 0;
	p->xstate = 0;
	p->state = UNUSED;

  p->pinfo.state = UNUSED;
  p->pinfo.pid = 0;
  p->pinfo.parent_pid = 0;
  p->pinfo.sz = 0;
  p->pinfo.cntOfile = 0;
  p->pinfo.name[0] = 0;
  p->pinfo.startTime = 0;
  p->pinfo.lastStartTime = 0;
  p->pinfo.cntChange = 0;
  p->pinfo.totalTime = 0;
  
}

// Create a user page table for a given process, with no user memory,
// but with trampoline and trapframe pages.
	pagetable_t
proc_pagetable(struct proc *p)
{
	pagetable_t pagetable;

	// An empty page table.
	pagetable = uvmcreate();
	if(pagetable == 0)
		return 0;

	// map the trampoline code (for system call return)
	// at the highest user virtual address.
	// only the supervisor uses it, on the way
	// to/from user space, so not PTE_U.
	if(mappages(pagetable, TRAMPOLINE, PGSIZE,
				(uint64)trampoline, PTE_R | PTE_X) < 0){
		uvmfree(pagetable, 0);
		return 0;
	}

	// map the trapframe page just below the trampoline page, for
	// trampoline.S.
	if(mappages(pagetable, TRAPFRAME, PGSIZE,
				(uint64)(p->trapframe), PTE_R | PTE_W) < 0){
		uvmunmap(pagetable, TRAMPOLINE, 1, 0);
		uvmfree(pagetable, 0);
		return 0;
	}

	return pagetable;
}

// Free a process's page table, and free the
// physical memory it refers to.
	void
proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
	uvmunmap(pagetable, TRAMPOLINE, 1, 0);
	uvmunmap(pagetable, TRAPFRAME, 1, 0);
	uvmfree(pagetable, sz);
}

// a user program that calls exec("/init")
// assembled from ../user/initcode.S
// od -t xC ../user/initcode
uchar initcode[] = {
	0x17, 0x05, 0x00, 0x00, 0x13, 0x05, 0x45, 0x02,
	0x97, 0x05, 0x00, 0x00, 0x93, 0x85, 0x35, 0x02,
	0x93, 0x08, 0x70, 0x00, 0x73, 0x00, 0x00, 0x00,
	0x93, 0x08, 0x20, 0x00, 0x73, 0x00, 0x00, 0x00,
	0xef, 0xf0, 0x9f, 0xff, 0x2f, 0x69, 0x6e, 0x69,
	0x74, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00
};



struct namespace_node* 
namespace_malloc(void) {
  for (int i = 0; i < 64; ++i) {
    if (!used[i]) {
      printf("Alloc node %d\n", i);
      used[i] = 1;
      return &nodes[i++];
    }
  }
  return 0;
}

void
namespace_free(struct namespace_node* node) {
  for (int i = 0; i < 64; ++i) {
    if (&nodes[i] == node) {
      printf("Free %d\n", i);
      used[i] = 0;
    }
  }
}

// Set up first user process.
	void
userinit(void)
{
	struct proc *p;

	p = allocproc();
	initproc = p;



	// allocate one user page and copy initcode's instructions
	// and data into it.
	uvmfirst(p->pagetable, initcode, sizeof(initcode));
	p->sz = PGSIZE;

	// prepare for the very first "return" from kernel to user.
	p->trapframe->epc = 0;      // user program counter
	p->trapframe->sp = PGSIZE;  // user stack pointer

	safestrcpy(p->name, "initcode", sizeof(p->name));
	p->cwd = namei("/");

	p->state = RUNNABLE;
	
	uint xticks;
{
	acquire(&tickslock);
	xticks = ticks;
	release(&tickslock);	
  p->pinfo.parent_pid = -1;
  p->pinfo.pid = p->pid;
 	p->pinfo.startTime = xticks; 


  // do job ??
  // create first namespace and add here init
  p->node = namespace_malloc(); // !!!
  // if (p->node == 0) {
  //   panic("Panic namespace create");
  // }
  {
  p->node->cnt_proc = 1;
  p->node->head = 0;
  p->node->init = p;
  p->node->last_pid = 2;
  p->node->depth = 0;
  p->namespace_pids[0] = 1;
  }

}

	
	release(&p->lock);
}

// Grow or shrink user memory by n bytes.
// Return 0 on success, -1 on failure.
	int
growproc(int n)
{
	uint64 sz;
	struct proc *p = myproc();

  sz = p->sz;
  if(n > 0){
    if((sz = uvmalloc(p->pagetable, sz, sz + n, PTE_W)) == 0) {
      return -1;
    }
  } else if(n < 0){
    sz = uvmdealloc(p->pagetable, sz, sz + n);
  }
  p->sz = sz;
  return 0;
}

// Create a new process, copying the parent.
// Sets up child kernel stack to return as if from fork() system call.
int
fork(void)
{


  int i, pid;
  struct proc *np;
  struct proc *p = myproc();


  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy user memory from parent to child.
  if(uvmcopy(p->pagetable, np->pagetable, p->sz) < 0){
    freeproc(np);
    release(&np->lock);
    return -1;
  }
  np->sz = p->sz;

  // copy saved user registers.
  *(np->trapframe) = *(p->trapframe);

  // Cause fork to return 0 in the child.
  np->trapframe->a0 = 0;

  // increment reference counts on open file descriptors.
  for(i = 0; i < NOFILE; i++)
    if(p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  np->cwd = idup(p->cwd);

  safestrcpy(np->name, p->name, sizeof(p->name));

{
// do job ??
// add to np current p->namespace 
{
  np->node = p->node;

  struct namespace_node *local_node = p->node;
  for (int i = np->node->depth; i >= 0; --i) {
    ++local_node->cnt_proc;
    np->namespace_pids[local_node->depth] = local_node->last_pid++;
    local_node = local_node->head;
  }
}
  pid = np->pid;
  uint xticks;
  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);	

  np->pinfo.parent_pid = p->pid;
  np->pinfo.pid = np->pid;
  np->pinfo.startTime = xticks;  
}
  release(&np->lock);

  acquire(&wait_lock);
  np->parent = p;
  release(&wait_lock);

  acquire(&np->lock);
  np->state = RUNNABLE;
  release(&np->lock);



  return pid;
}


int
clone(void) 
{

  int i, pid;
  struct proc *np;
  struct proc *p = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy user memory from parent to child.
  if(uvmcopy(p->pagetable, np->pagetable, p->sz) < 0){
    freeproc(np);
    release(&np->lock);
    return -1;
  }
  np->sz = p->sz;

  // copy saved user registers.
  *(np->trapframe) = *(p->trapframe);

  // Cause fork to return 0 in the child.
  np->trapframe->a0 = 0;

  // increment reference counts on open file descriptors.
  for(i = 0; i < NOFILE; i++)
    if(p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  np->cwd = idup(p->cwd);

  safestrcpy(np->name, p->name, sizeof(p->name));


{

//do job ??
// fork, with creating new namespace
{
  np->node = namespace_malloc();
  np->node->cnt_proc = 0;
  np->node->last_pid = 1;
  np->node->init = np;
  np->node->head = p->node;
  np->node->depth = p->node->depth + 1;

  struct namespace_node *local_node = np->node;
  for (int i = np->node->depth; i >= 0; --i) {
    ++local_node->cnt_proc;
    np->namespace_pids[local_node->depth] =  local_node->last_pid++;
    local_node = local_node->head;
  }
}

  pid = np->pid;
  uint xticks;
  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);	

  np->pinfo.parent_pid = p->pid;
  np->pinfo.pid = np->pid;
  np->pinfo.startTime = xticks;  
}

  release(&np->lock);
  acquire(&wait_lock);
  np->parent = p;
  release(&wait_lock);

  acquire(&np->lock);
  np->state = RUNNABLE;
  release(&np->lock);

  return pid;
}


// Pass p's abandoned children to init.
// Caller must hold wait_lock.
void
reparent(struct proc *p)
{
  struct proc *pp;

  struct proc *new_init = initproc;
  if (p->node->init == p) {
    p->node->init = 0;
  }
  struct namespace_node *local_node = p->node;
  while(local_node != 0) {
    if (local_node->init != 0) {
      new_init = local_node->init;
      break;
    }
    local_node = local_node->head;
  }

  for(pp = proc; pp < &proc[NPROC]; pp++){
    if(pp->parent == p){
      // do job ??
      // reparent to correct init    
      pp->parent = new_init;
      wakeup(initproc);
    }
  }
}



// do job ?? - if cnt_proc == 0 -> clean this node and cole from head
void 
clean_node(struct namespace_node* node) {
  struct namespace_node* for_delete;
  while (node->depth != 0 && node->cnt_proc == 0) {
    for_delete = node;
    node = node->head;
    namespace_free(for_delete); // !!!
  }
} 

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait().
void
exit(int status)
{
  struct proc *p = myproc();

  if(p == initproc)
    panic("init exiting");

  // Close all open files.
  for(int fd = 0; fd < NOFILE; fd++){
    if(p->ofile[fd]){
      struct file *f = p->ofile[fd];
      fileclose(f);
      p->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(p->cwd);
  end_op();
  p->cwd = 0;

  acquire(&wait_lock);


  // do job ??
  // decreament cnt_proc for this node and paranets, call clean_node


  struct namespace_node* local_node = p->node;
  for (int i = p->node->depth; i >= 0; --i) {
    --local_node->cnt_proc;
    local_node = local_node->head;
  }
  clean_node(p->node);

  // Give any children to init.
  reparent(p);

  // Parent might be sleeping in wait().
  wakeup(p->parent);
  
  acquire(&p->lock);

  p->xstate = status;
  p->state = ZOMBIE;

  release(&wait_lock);

  // Jump into the scheduler, never to return.
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(uint64 addr)
{
  struct proc *pp;
  int havekids, pid;
  struct proc *p = myproc();

  acquire(&wait_lock);

  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(pp = proc; pp < &proc[NPROC]; pp++){
      if(pp->parent == p){
        // make sure the child isn't still in exit() or swtch().
        acquire(&pp->lock);

        havekids = 1;
        if(pp->state == ZOMBIE){
          // Found one.
          pid = pp->pid;
          if(addr != 0 && copyout(p->pagetable, addr, (char *)&pp->xstate,
                                  sizeof(pp->xstate)) < 0) {
            release(&pp->lock);
            release(&wait_lock);
            return -1;
          }
          freeproc(pp);
          release(&pp->lock);
          release(&wait_lock);
          return pid;
        }
        release(&pp->lock);
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || killed(p)){
      release(&wait_lock);
      return -1;
    }
    
    // Wait for a child to exit.
    sleep(p, &wait_lock);  //DOC: wait-sleep
  }
}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run.
//  - swtch to start running that process.
//  - eventually that process transfers control
//    via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();

  // int cnt = 0;
  c->proc = 0;
  for(;;){
    // Avoid deadlock by ensuring that devices can interrupt.
    intr_on();

    for(p = proc; p < &proc[NPROC]; p++) {

      acquire(&p->lock);
      if(p->state == RUNNABLE) {
        // Switch to chosen process.  It is the process's job
        // to release its lock and then reacquire it
        // before jumping back to us.

        p->state = RUNNING;
        c->proc = p;
        // printf("%d %d\n", cnt++, p->pid);
        // printf("%d %d %d\n", p->trapframe->a0, p->trapframe->a1, p->trapframe->a7);


        acquire(&tickslock);
        uint xticksStart = ticks;
        release(&tickslock);

        ++p->pinfo.cntChange;
        swtch(&c->context, &p->context);
        p->pinfo.lastStartTime = xticksStart;		


        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
      }
      release(&p->lock);
    }
  }
}

// Switch to scheduler.  Must hold only p->lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->noff, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&p->lock))
    panic("sched p->lock");
  if(mycpu()->noff != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(intr_get())
    panic("sched interruptible");

  intena = mycpu()->intena;
  swtch(&p->context, &mycpu()->context);
  

  acquire(&tickslock);
  uint xticks = ticks;
  release(&tickslock);
  p->pinfo.totalTime += xticks - p->pinfo.lastStartTime;
  p->pinfo.lastStartTime = 0;
  
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  struct proc *p = myproc();
  acquire(&p->lock);
  p->state = RUNNABLE;
  sched();
  release(&p->lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void
forkret(void)
{
  static int first = 1;

  // Still holding p->lock from scheduler.
  release(&myproc()->lock);

  if (first) {
    // File system initialization must be run in the context of a
    // regular process (e.g., because it calls &leep), and thus cannot
    // be run from main().
    first = 0;
    fsinit(ROOTDEV);
  }

  usertrapret();
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  // Must acquire p->lock in order to
  // change p->state and then call sched.
  // Once we hold p->lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup locks p->lock),
  // so it's okay to release lk.

  acquire(&p->lock);  //DOC: sleeplock1
  release(lk);

  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  release(&p->lock);
  acquire(lk);
}

// Wake up all processes sleeping on chan.
// Must be called without any p->lock.
void
wakeup(void *chan)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    if(p != myproc()){
      acquire(&p->lock);
      if(p->state == SLEEPING && p->chan == chan) {
        p->state = RUNNABLE;
      }
      release(&p->lock);
    }
  }
  
}

// Kill the process with the given pid.
// The victim won't exit until it tries to return
// to user space (see usertrap() in trap.c).
int
kill(int pid)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    if(p->pid == pid){
      p->killed = 1;
      if(p->state == SLEEPING){
        // Wake process from sleep().
        p->state = RUNNABLE;
      }
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }
  return -1;
}

void
setkilled(struct proc *p)
{
  acquire(&p->lock);
  p->killed = 1;
  release(&p->lock);
}

int
killed(struct proc *p)
{
  int k;
  
  acquire(&p->lock);
  k = p->killed;
  release(&p->lock);
  return k;
}

// Copy to either a user address, or kernel address,
// depending on usr_dst.
// Returns 0 on success, -1 on error.
int
either_copyout(int user_dst, uint64 dst, void *src, uint64 len)
{
  struct proc *p = myproc();
  if(user_dst){
    return copyout(p->pagetable, dst, src, len);
  } else {
    memmove((char *)dst, src, len);
    return 0;
  }
}

// Copy from either a user address, or kernel address,
// depending on usr_src.
// Returns 0 on success, -1 on error.
int
either_copyin(void *dst, int user_src, uint64 src, uint64 len)
{
  struct proc *p = myproc();
  if(user_src){
    return copyin(p->pagetable, dst, src, len);
  } else {
    memmove(dst, (char*)src, len);
    return 0;
  }
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [USED]      "used",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  struct proc *p;
  char *state;

  printf("\n");
  for(p = proc; p < &proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    printf("%d %s %s", p->pid, state, p->name);
    printf("\n");
  }
}
