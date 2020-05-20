/*
 * esercizio-C-2020-05-19-procs.c
 *
 *  Created on: May 19, 2020
 *      Author: marco
 *      N = 10

un processo padre crea N processi figli ( https://repl.it/@MarcoTessarotto/crea-n-processi-figli )

shared variables: countdown, process_counter[N], shutdown

usare mutex per regolare l'accesso concorrente a countdown

dopo avere avviato i processi figli, il processo padre dorme 1 secondo e poi imposta il valore
 di countdown al valore 100000.

quando countdown == 0, il processo padre imposta shutdown a 1.

aspetta che terminino tutti i processi figli e poi stampa su stdout process_counter[].

i processi figli "monitorano" continuamente countdown:

    processo i-mo: se countdown > 0, allora decrementa countdown ed incrementa process_counter[i]

    se shutdown != 0, processo i-mo termina
 *
 */
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>

sem_t* mutex;
//pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

#define N 20
#define TOT_COUNTDOWN 100000
#define CHECK_ERR(a,msg) {if ((a) == -1) { perror((msg)); exit(EXIT_FAILURE); } }
#define CHECK_ERR_MMAP(a,msg) {if ((a) == MAP_FAILED) { perror((msg)); exit(EXIT_FAILURE); } }

int* countdown;
int* process_counter[N];
int* shutdown;

/*
void parent_process_signal_handler(int signum){
	//pid_t child_pid;

	while(waitpid(-1,NULL,WNOHANG) > 0){
		*shutdown = 1;
		continue;
	}


}
*/

void child_process(int nproc) {
	//int s;

  // fai qualcosa
	// 3.4.2 Mutual exclusion solution, pag. 19

	while(1){
		//CHECK_ERR(sem_wait(mutex),"sem_wait()")

		CHECK_ERR(sem_wait(mutex),"sem_wait()")
		//printf("countdown %d: \n",*countdown);
		if(*shutdown == 0){
			if(*countdown > 0){
				(*countdown)--;
				(*process_counter[nproc])++;
			}else if(*countdown == 0){
				// se esco senza aver sbloccato il mutex gli altri procesi non terminano!
				CHECK_ERR(sem_post(mutex),"sem_post()");
				exit(EXIT_SUCCESS);
			}
		} else if(*shutdown != 0){
			// sezione critica
			CHECK_ERR(sem_post(mutex),"sem_post()");
			exit(EXIT_SUCCESS);
			//process_counter[nproc]++;
		}
		CHECK_ERR(sem_post(mutex),"sem_post()")
	}


}

int main(void) {

	int s;
	// creo zona di memoria condivisa tra i processi
	mutex = mmap(NULL, // NULL: è il kernel a scegliere l'indirizzo
			sizeof(sem_t) + sizeof(int) + sizeof(int) + N*sizeof(int), // dimensione della memory map
			PROT_READ | PROT_WRITE, // memory map leggibile e scrivibile
			MAP_SHARED | MAP_ANONYMOUS, // memory map condivisibile con altri processi e senza file di appoggio
			-1,
			0); // offset nel file

	CHECK_ERR_MMAP(mutex,"mmap")

	countdown = (int *) (mutex + 1);
	shutdown = (int*) (mutex + 2);

	printf("[parent] before fork()!\n");

	for (int i = 0; i< N; i++){
		process_counter[i] = (int*) (mutex + (3+i));
		printf("[parent] process_counter [%d] = %d\n", i, *process_counter[i]);
	}

	s = sem_init(mutex,
					1, // 1 => il semaforo è condiviso tra processi,
					   // 0 => il semaforo è condiviso tra threads del processo
					0 // valore iniziale del semaforo (se mettiamo 0 che succede?)
				  );

	CHECK_ERR(s,"sem_init")

  // voglio creare N processi figli
  for (int i = 0; i < N; i++) {
     //childPid = fork();
     switch(fork()){
		 case 0:

			child_process(i);

			break;

		 case -1:

			 perror("fork");
			 exit(EXIT_FAILURE);

		 default:

			 if(i == 0) *countdown = TOT_COUNTDOWN;
			 break;
	}

  }

	// qui esegue solo il padre
  sleep(1);
  printf("initial value of countdown=%d, NUMBER_OF_PROCESSES=%d\n", *countdown, N);

  CHECK_ERR(sem_post(mutex),"sem_post()")

  while(wait(NULL) != -1){
	  continue;
  }

	int tot_counter = 0;
	printf("[parent] after childs termination:\n");

	for (int j = 0; j < N; j++){
		tot_counter = tot_counter + *process_counter[j];
		printf("[parent] process_counter[%d] = %d\n",j, *process_counter[j]);

	}

	printf("tot_counter: %d \n", tot_counter);

  s = sem_destroy(mutex);
  CHECK_ERR(s,"sem_destroy")

  printf("fine\n");

  exit(EXIT_SUCCESS);
}
