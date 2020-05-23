#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>

#define N	10

int *countdown;
int *process_counter;
int *shutdown;
sem_t *sem;

void err_exit(char *msg)
{
	perror(msg);
	exit(EXIT_FAILURE);
}

void child(int i)
{
	while (1) {
		//printf("SHUTDOWN IS %d\n", *shutdown);
		if (sem_wait(sem))
			err_exit("sem_wait() error");
		if (*shutdown) {
			puts("BREAKING");
			if (sem_post(sem))
				err_exit("sem_post() error");
			break;
		} else {
			printf("shutdown is %d\n", *shutdown);
		}
		if (*countdown > 0) {
			//puts("CHILD DECREMENTING");
			(*countdown)--;
			process_counter[i]++;
			//printf("countdown = %d\n", *countdown);
		}
		if (sem_post(sem))
			err_exit("sem_post() error");
		//sleep(1);
	}
	//puts("CHILD EXITING");
	exit(EXIT_SUCCESS);
}

int main()
{
	int map_size = (N + 2) * 4 + sizeof(sem);
	void *map = mmap(NULL, map_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
	if (map == MAP_FAILED)
		err_exit("mmap() error");

	memset(map, 0, map_size);
	countdown = (int *) map;
	shutdown = countdown + 1;
	process_counter = shutdown + 1;
	sem = (sem_t *) (process_counter + N);

	if (sem_init(sem, 1, 1))
		err_exit("sem_init() error");

	for (int i = 0; i < N; i++) {
		switch (fork()) {
		case 0:
			child(i);
			break;
		case -1:
			err_exit("fork() error");
			break;
		default:
			break;
		}
	}
	sleep(1);
	if (sem_wait(sem))
		err_exit("sem_wait() error");
	*countdown = 100;
	if (sem_post(sem))
		err_exit("sem_post() error");
	while (1) {
		if (sem_wait(sem))
			err_exit("sem_wait() error");
		//puts("CHECKING");
		if (!(*countdown)) {
			*shutdown = 1;
			puts("SHUTDOWN SET!");
			if (sem_post(sem))
				err_exit("sem_post() error");
			break;
		}
		if (sem_post(sem))
			err_exit("sem_post() error");
	}
	for (int i = 0; i < N; i++) {
		if (wait(NULL) == -1)
			err_exit("wait() error");
	}

	puts("PROC COUNTER");
	for (int i = 0; i < N; i++) {
		printf("%d, ", process_counter[i]);
	}
	return 0;
}
