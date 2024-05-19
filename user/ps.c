#include "kernel/types.h"
#include "user/user.h"
#include "kernel/riscv.h"

static char *states[] = {
[UNUSED]	"unused  ",	
[USED]		"used    ",
[SLEEPING]	"sleeping",
[RUNNABLE]	"runnable",
[RUNNING]	"running ",
[ZOMBIE]	"zombie  ",
};

static char *flags[] = {
[0] "VALID",
[1]	"READABLE",
[2] "WRITE",
[3] "EXECUTABLE",
[4] "USER",
[5] "GLOBAL",
[6] "ACCESSED",
[7] "DIRTY",
};

static char *sys_call[] = {
[1 ] "fork",   
[2 ] "exit",   
[3 ] "wait",   
[4 ] "pipe",   
[5 ] "read",   
[6 ] "kill",   
[7 ] "exec",   
[8 ] "fstat",  
[9 ] "chdir",  
[10]  "dup",    
[11]  "getpid", 
[12]  "sbrk",   
[13]  "sleep",  
[14]  "uptime", 
[15]  "open",   
[16]  "write",  
[17]  "mknod",  
[18]  "unlink", 
[19]  "link",   
[20]  "mkdir",  
[21]  "close",  
[22]  "dummy",  
[23]  "ps_list", 
[24]  "ps_info", 
[25]  "ps_pt0", 
[26]  "ps_pt1", 
[27]  "ps_pt2", 
[28]  "ps_copy", 
[29]  "ps_sleep_info", 
};


uint64 strtoull(char* s) {
	uint64 ans = 0;
	while('0' <= *s && *s <= '9') ans = ans * 10 + *s++ - '0';
	return ans;

}

void test_1();
void test_2();
void test_3();
void test_4();
void test_5();
void test_6();
void test_7();

void
main(int argc, char *argv[]) {
	int pids[64], limit = 64;

	if (argc == 2 && 0 == strcmp("mem_dump", argv[1])) {
		mem_dump();
	}

	if (argc == 2 && 0 == strcmp("count", argv[1])) {
		printf("%d\n", ps_list(limit, pids));	
	}
	if (argc >= 2 && 0 == strcmp("pids", argv[1])) {
		limit = ps_list(limit, pids);
		if (argc >= 3) {
			limit = atoi(argv[2]);
		}
		for (int i = 0; i < limit; ++i) {
			printf("Pid %d = %d\n", i, pids[i]);
		}
	}


	if (argc >= 2 && 0 == strcmp("list", argv[1])) {
		for (int i = 0; i < argc; ++i) {
			if (! strcmp(argv[i], "-t")) {
				switch (atoi(argv[i + 1]))
				{
				case 1:
					test_1();
					break;
				case 2:
					test_2();
					break;
				case 3:
					test_3();
					break;
				case 4:
					test_4();
					break;
				case 5:
					test_5();
					break;
					
				case 6:
					test_6();
					break;
				case 7:
					test_7();
					break;
				}


			}
		}

		for (int i = 0; i < argc; ++i)
			if (! strcmp(argv[i], "-l"))
				limit = atoi(argv[i + 1]);

		limit = ps_list(limit, pids);
		struct proccess_info pinfo;
		printf("state\t\tpid\tUptime\tP_pid\tMem size\tcnt open file\t\tWork Time\tSwitch cnt\tName\n");

		for (int i = 0; i < limit; ++i) {
			if (! ps_info(pids[i], &pinfo)) {

				printf("%s\t%d\t%d\t%d\t%d\t\t%d\t\t\t%d\t\t%d\t\t%s\n",
					states[pinfo.state],
					pinfo.pid,
					uptime() - pinfo.startTime,		
					pinfo.parent_pid,		
					pinfo.sz,		
					pinfo.cntOfile,		
					pinfo.totalTime,
					pinfo.cntChange,	
					pinfo.name
				);
			}
			else {
				printf("Have some troubles\n");
			}

		}
	}
	if (argc >= 4 && strcmp(argv[1], "pt") == 0) {
		int pid = atoi(argv[3]);
		int need_valid = 1 - (strcmp(argv[argc - 1], "-v") == 0);
		uint64* table = (uint64*) malloc(512 * 8);
		int status = 0;
		if (strcmp(argv[2], "0") == 0) {
			status = ps_pt0(pid,  table) == 0;
		} else if (strcmp(argv[2], "1") == 0) {

			status = ps_pt1(pid, (void*) strtoull(argv[4]), table) == 0;	
		} else if (strcmp(argv[2], "2") == 0) {
			status = ps_pt2(pid, (void*) strtoull(argv[4]), table) == 0;
		}
		if (status != 0) {
			printf("#\tPNN\t\tFlags\n");
			for (int i = 0; i < 512; ++i) {
				if ((table[i] & PTE_V) != 0 || need_valid == 0) { // just for testing
					printf("%d\t: ", i);
					printf("%x\t", PTE2PA(table[i]));
					//printf("MY FLAGS");
					int last = 0;
					for (int j = need_valid; j < 8; ++j) {
						if (((1L << j) & table[i]) != 0) {
							last = j;
						}
					}
					for (int j = need_valid; j < 8; ++j) {
						if (((1L << j) & table[i]) != 0) {
							printf("%s", flags[j]);
							if (j != last) printf(",");
						}
					}
					printf("\n");
				}
			}
		}
		free(table);
	}

	if (argc == 5 && strcmp(argv[1], "dump") == 0) {
		int pid = atoi(argv[2]);
		void* addr = (void*) strtoull(argv[3]);
		int size = atoi(argv[4]);
		void* inform = malloc(size);
		int code;
		if ((code = ps_copy(pid, addr, size, inform)) == 0) {
			for (int i = 0; i < size; ++i) {
				char data = ((char*)inform)[i];
				if (data < 0x10) printf("0");
				printf("%x", data);
				if (i % 4 == 3) printf(" ");
				if (i % 16 == 15) printf("\n");
			}
			if (size % 16 != 0) printf("\n");
		} else {
			printf("I have a error code: %d\n", code);
		}
		free(inform);
	}
	
	if (argc == 3 && strcmp(argv[1], "sleep") == 0) {
		int pid = atoi(argv[2]);
		struct write_info info = {0, 0, 0ULL, 0ULL, 0}; 
		ps_sleep_info(pid, &info);

		char target_state[] = "sleeping", target_call[] = "write";
		if (strcmp(states[info.state], target_state) == 0 && strcmp(sys_call[info.a7], target_call) == 0) {
			char inform[info.buf_size];
			int ret_code = 0;
			if( (ret_code = ps_copy(pid, (void*) info.str_ptr, info.buf_size, inform)) == 0) {
				const int cnt_bit = 16;
				printf("File ptr = %p\n", info.file_ptr);
				printf("Size = %d\n", info.buf_size);
				for (int i = 0; i < info.buf_size / cnt_bit + 1; ++i) {
					printf("[");
					for (int j = 0; j < cnt_bit; ++j) {
						char data;
						if (i * 8 + j < info.buf_size ) 
							data = ((char*)inform)[i * 8 + j];
						else 
							data = 0;
						if (data < 0x10) printf("0");
						printf("%c", data < 32 ? '.' : data);
					}
					printf("]\t");

					for (int j = 0; j < cnt_bit; ++j) {
						char data;
						if (i * 8 + j < info.buf_size ) 
							data = ((char*)inform)[i * 8 + j];
						else 
							data = 0;
						if (data < 0x10) printf("0");
						printf("%x", data);
						if (j % 4 == 3) printf(" ");
					}
					printf("\n");
				}
			} else {
				printf("Return code = %d\n", ret_code);
			}
		} 

		printf("%s, %s\n", states[info.state], sys_call[info.a7]);		
	}

   	exit(0);
}



void 
test_1() {
	int  a = -1;
	a = clone();
	if (a == 0) sleep(200); 
}

void 
test_2() {
	int  a = -1, res = 0;
	a = clone();
	if (a == 0) exit(0); else wait(&res); 
}


void 
test_3() {
	int  a = -1, b = -1;
	a = clone();
	if (a == 0) b = fork();

	if (a == 0 && b != 0) printf("b = %d\n", b);
	if (a == 0) sleep(300); 
	if (b == 0) sleep(200); 
	sleep(100);
}


void 
test_4() {
	int  a = -1, b = -1, res = 0;
	a = clone();
	if (a != 0) {
		wait(&res); 
		printf("Exit code = %d\n", res);
	}
	else b = fork();

	if (a == 0 && b != 0) printf("b = %d\n", b);
	if (b != 0 && a == 0) exit(0); 
	if (a == 0) sleep(300); 
	if (b == 0) sleep(200); 
	sleep(100);
}



void 
test_5() {
	int  a = -1, b = -1, c = -1, res = 0;
	a = clone();
	if (a == 0) b = fork();
	if (a == 0 && b == 0) c = fork();

	if (a == 0 && b != 0) { 
		wait(&res);
		printf("Exit code = %d\n", res);
		wait(&res);
		printf("Exit code = %d\n", res);
	}

	if (a == 0 && b == 0 && c != 0) exit(0); 

	if (a == 0) sleep(1000); 
	sleep(100);
}


void 
test_6() {
	int  a = -1, b = -1, c = -1, res = 0;
	a = clone();
	if (a == 0) b = clone();
	if (a == 0 && b == 0) c = fork();

	if (a == 0 && b != 0) { 
		wait(&res);
		printf("Exit code = %d\n", res);
		wait(&res);
		printf("Exit code = %d\n", res);
	}

	if (a == 0 && b == 0 && c != 0) exit(0); 

	if (a == 0) sleep(1000); 
	sleep(100);
}



void 
test_7() {
	int  a = -1, b = -1, c = -1, res = 0;
	a = clone();
	if (a == 0) b = clone();
	if (a == 0 && b == 0) c = clone();

	if (a == 0 && b != 0) { 
		wait(&res);
		printf("Exit code = %d\n", res);
		wait(&res);
		printf("Exit code = %d\n", res);
	}

	if (a == 0 && b == 0 && c != 0) exit(0); 

	if (a == 0) sleep(1000); 
	sleep(100);
}

