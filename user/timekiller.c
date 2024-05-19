#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char* argv[]) {
	int pid, limit1, limit2;
	limit1 = argc == 3 ? atoi(argv[1]) : 1e3;
	limit2 = argc == 3 ? atoi(argv[2]) : 2e3;
	pid = fork();
	fork();
	int sum = 0;
	int limit = pid ? limit1 : limit2;
	
	for(int i = 0; i < limit; ++i) {
		for (int j = 0; j < limit; ++j) {
			sum += i;
		}
	}
	printf("Stop KT with PP=%d, result=%d\n", pid, sum);
	return 0;
}
