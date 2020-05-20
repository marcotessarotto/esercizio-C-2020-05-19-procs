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

typedef struct{
   int countdown;
   int shutdown;
   int * process_counter;
}map_struct;

map_struct * my_stuct;
sem_t * semaphore;

int main(void) {
	my_stuct = mmap(NULL, sizeof(map_struct), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1 ,0);
	if (my_stuct == MAP_FAILED) {
		perror("mmap()");
		exit(EXIT_FAILURE);
	}
	my_stuct->shutdown = 0;


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

	my_stuct->process_counter = calloc(N, sizeof(int));
	if(my_stuct->process_counter == NULL){
		perror("calloc()");
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
  sleep(1);
  my_stuct->countdown = 100000;
  printf("countdown set to: %d\n", my_stuct->countdown);

  while(1){
		if( my_stuct->countdown == 0){
			my_stuct->shutdown = 1;
			break;
		}
  }



  // wait N children
  for(int i=0 ; i<N ; i++){
	  wait(NULL);
  }

  printf("countdown at the end: %d\n", my_stuct->countdown);

  for(int i=0 ; i<N ; i++){
	  printf("child %d has work %d\n", i, my_stuct->process_counter[i]);
  }


  printf("fine\n");

  return 0;
}

void child_process(int child_index) {
	while(1){
		if (sem_wait(semaphore) == -1) {
			perror("sem_wait");
			exit(EXIT_FAILURE);
		}

		// sezione critica
		if( my_stuct->countdown > 0 ){
			my_stuct->countdown--;
			my_stuct->process_counter[child_index]++;
		}

		if (sem_post(semaphore) == -1) {
			perror("sem_post");
			exit(EXIT_FAILURE);
		}

		if(my_stuct->shutdown != 0){
			exit(0);
		}
	}
	exit(0);
}
