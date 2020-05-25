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

#define CHECK_ERR(a,msg) {if ((a) == -1) { perror((msg)); exit(EXIT_FAILURE); } }

#define CHECK_ERR_MMAP(a,msg) {if ((a) == MAP_FAILED) { perror((msg)); exit(EXIT_FAILURE); } }

typedef struct {
	sem_t sem; // semaforo per implementare il meccanismo di mutual exclusion (mutex)
	int countdown; // variabile condivisa
	int shutdown;
	int process_counter[N];
} shared;

shared *variables;

void child_process(int numero) {

	while (1) {

		if (variables->shutdown != 0) {
			exit(EXIT_SUCCESS);
		}

		if (sem_wait(&variables->sem) == -1) {
			perror("sem_wait");
			exit(EXIT_FAILURE);
		}

		if (variables->countdown > 0) {

			variables->countdown--;

			printf("[figlio numero %d]: countdown vale %d\n", numero,
					variables->countdown);

			(variables->process_counter)[numero]++;

		}

		if (sem_post(&variables->sem) == -1) {
			perror("sem_post");
			exit(EXIT_FAILURE);
		}

	}
}

void stampa_process_counter(void) {
	for (int i = 0; i < N; i++) {
		printf("Fiflio %d ha decrementato countdown %d volte\n", i,
				(variables->process_counter)[i]);
	}
}

int main(void) {

	pid_t boss;
	int res;

	boss = getpid(); // il pid del primo processo
	printf("generation 0 process pid: %d\n", boss);

	variables = mmap(NULL, // NULL: è il kernel a scegliere l'indirizzo
			sizeof(shared), // dimensione della memory map
			PROT_READ | PROT_WRITE, // memory map leggibile e scrivibile
			MAP_SHARED | MAP_ANONYMOUS, // memory map condivisibile con altri processi e senza file di appoggio
			-1, // nessun file di appoggio alla memory map
			0); // offset nel file

	CHECK_ERR_MMAP(variables, "mmap")

	res = sem_init(&(variables->sem), 1, // 1 => il semaforo è condiviso tra processi, 0 => il semaforo è condiviso tra threads del processo
			1 // valore iniziale del semaforo (se mettiamo 0 che succede?)
			);

	CHECK_ERR(res, "sem_init")

// voglio creare N processi figli

	for (int figlio = 0; figlio < N; figlio++) {
		switch (fork()) {
		case 0: // child process
			printf("Sono il figlio numero %d\n", figlio);
			child_process(figlio);
			break;
			// oppure:
			// goto fine;
		case -1:
			perror("fork");
			exit(EXIT_FAILURE);
		default:
			;
		}
	}

	if (getpid() == boss) {

		sleep(1);

		variables->countdown = 100000;

		while (1) {
			if (variables->countdown == 0) {
				printf("Countdown ora vale: %d\n", variables->countdown);
				variables->shutdown = 1;
				break;
			}
		}

		int child_counter = 0;

		do {
			res = wait(NULL); // tutti i processi aspettano i processi figli, se he hanno

			if (res > 0) {
				printf("wait ok - parent has waited for process %d\n", res);
				child_counter++;
			} else {
				printf("wait err - parent\n");
			}
		} while (res != -1); //-1 vuol dire che non ci sono più figli da aspettare

		printf("parent has waited for %d processes\n", child_counter);

		if ( (sem_destroy(&variables->sem)) != 0 ) {
			perror("sem_destroy");
			exit(EXIT_FAILURE);
		}

		stampa_process_counter();

	}

//fine:
// ...

	printf("fine\n");

	return 0;
}
