#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <errno.h>
#include <semaphore.h>

#define N 10
sem_t * mutex;

int *countdown;
int *process_counter;
int *shutdown;

#define CHECK_ERR_MMAP(a,msg) {if ((a) == MAP_FAILED) { perror((msg)); exit(EXIT_FAILURE); } }
#define CHECK_ERR(a,msg) {if ((a) == -1) { perror((msg)); exit(EXIT_FAILURE); } }

void child_process(int n_children) {

	while(1){

	  			if (sem_wait(mutex) == -1) {
	  				perror("sem_wait");
	  				exit(EXIT_FAILURE);
	  			}

	  			if((*countdown)>0)
	  			{
	  			(*countdown)--;
	  			process_counter[n_children]++;
	  			}

	  			if (sem_post(mutex) == -1) {
	  			  	perror("sem_post");
	  			  	exit(EXIT_FAILURE);
	  			 }

	  			if(*shutdown !=0)
	  			{
	  				exit(EXIT_SUCCESS);

	  			}

	}

	exit(EXIT_SUCCESS);

}

int main(void) {

	int res;

	countdown = mmap(NULL, // NULL: è il kernel a scegliere l'indirizzo
			sizeof(int)*2, // dimensione della memory map
			PROT_READ | PROT_WRITE, // memory map leggibile e scrivibile
			MAP_SHARED | MAP_ANONYMOUS, // memory map condivisibile con altri processi e senza file di appoggio
			-1,
			0);

	CHECK_ERR_MMAP(countdown,"mmap");

	*countdown = -1;

	shutdown = (countdown + 1);

	process_counter = mmap(NULL, // NULL: è il kernel a scegliere l'indirizzo
							sizeof(int), // dimensione della memory map
							PROT_READ | PROT_WRITE, // memory map leggibile e scrivibile
							MAP_SHARED | MAP_ANONYMOUS, // memory map condivisibile con altri processi e senza file di appoggio
							-1,
							0);
	CHECK_ERR_MMAP(process_counter,"mmap");

	mutex = mmap(NULL, // NULL: è il kernel a scegliere l'indirizzo
				sizeof(sem_t), // dimensione della memory map
				PROT_READ | PROT_WRITE, // memory map leggibile e scrivibile
				MAP_SHARED | MAP_ANONYMOUS, // memory map condivisibile con altri processi e senza file di appoggio
				-1,
				0);

	CHECK_ERR_MMAP(mutex,"mmap")


	res = sem_init(mutex,
						1, // 1 => il semaforo è condiviso tra processi, 0 => il semaforo è condiviso tra threads del processo
						1 // valore iniziale del semaforo (se mettiamo 0 che succede?)
					  );

	CHECK_ERR(res,"sem_init");

  // voglio creare N processi figli
  for (int i = 0; i < N; i++) {
    switch (fork()) {
      case 0: // child process
        child_process(i);
        break;
      case -1:
        perror("fork");
        exit(EXIT_FAILURE);
      default:
        ;
    }
  }


  sleep(1);
  (*countdown) = 100000;
   printf("Countdown: %d\n", *countdown );

   while(1){

	   if( *countdown == 0){
	        	*shutdown = 1;
	        	break;
	        }

   }


  for (int i = 0; i < N; i++) {
  if (wait(NULL) == -1)
  {    perror("wait error");
   }
  }



  	for(int i=0 ; i<N ; i++){
  	  printf("Child's %d decrement countdown %d times\n",i, process_counter[i]);
  	}
  	printf("Countdown: %d\n", *countdown);

  printf("fine\n");

  return 0;
}
