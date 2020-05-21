#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <pthread.h>
#include <errno.h>



void child_process();


#define N 10

int * countdown;
int * shutdown;
int * process_counter;


sem_t * semaphore;

int main(void) {

	countdown = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1 ,0);
	if (countdown == MAP_FAILED) {
		perror("mmap()");
		exit(EXIT_FAILURE);
	}
	*countdown = -1;

	shutdown = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1 ,0);
	if (shutdown == MAP_FAILED) {
		perror("mmap()");
		exit(EXIT_FAILURE);
	}
	*shutdown = 0;


	process_counter = calloc(N, sizeof(int));
	if(process_counter == NULL){
		perror("calloc()");
		exit(1);
	}


	process_counter = mmap(process_counter, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1 ,0);
	if (process_counter == MAP_FAILED) {
		perror("mmap()");
		exit(EXIT_FAILURE);
	}


	semaphore = mmap(NULL, // NULL: è il kernel a scegliere l'indirizzo
			sizeof(sem_t), // dimensione della memory map
			PROT_READ | PROT_WRITE, // memory map leggibile e scrivibile
			MAP_SHARED | MAP_ANONYMOUS, // memory map condivisibile con altri processi e senza file di appoggio
			-1,
			0); // offset nel file
	if (semaphore == MAP_FAILED) {
		perror("mmap()");
		exit(EXIT_FAILURE);
	}
	int res = sem_init(semaphore,
			1, // 1 => il semaforo è condiviso tra processi, 0 => il semaforo è condiviso tra threads del processo
			1 // valore iniziale del semaforo
		  );
	if(res == -1){
		perror("sem_init()");
		exit(1);
	}


	// create N children
	for (int i = 0; i < N; i++) {
		switch (fork()) {
		  case 0:
			child_process(i);
			break;
		  case -1:
			perror("fork()");
			exit(1);
		}
	}

  // dopo avere avviato i processi figli, il processo padre dorme 1 secondo
  int sec = 1;
  printf("wait %d second\n", sec);
  sleep(sec);
  *countdown = 100000;
  printf("countdown set to: %d\n", *countdown);

  while(1){
		if( *countdown == 0){
			*shutdown = 1;
			break;
		}
  }

  // wait N children
  for(int i=0 ; i<N ; i++){
	  wait(NULL);
  }

  printf("countdown at the end: %d\n", *countdown);

  for(int i=0 ; i<N ; i++){
	  printf("child %d has decrement countdown %d times\n", i, process_counter[i]);
  }


  printf("end\n");

  return 0;
}

void child_process(int child_index) {

	while(1){
		if (sem_wait(semaphore) == -1) {
			perror("sem_wait");
			exit(EXIT_FAILURE);
		}
		//printf("child : %d start \n",child_index);



		// sezione critica
		if( *countdown > 0 ){
			*countdown = (*countdown -1);
			process_counter[child_index]++;

		}


		//printf("child : %d, end \n",child_index);
		if (sem_post(semaphore) == -1) {
			perror("sem_post");
			exit(EXIT_FAILURE);
		}



		if( *shutdown != 0){
			exit(0);
		}
	}
	exit(0);
}
