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

/*
N = 10

un processo padre crea N processi figli ( https://repl.it/@MarcoTessarotto/crea-n-processi-figli )

shared variables: countdown, process_counter[N], shutdown

usare mutex per regolare l'accesso concorrente a countdown

dopo avere avviato i processi figli, il processo padre dorme 1 secondo e
poi imposta il valore di countdown al valore 100000.

quando countdown == 0, il processo padre imposta shutdown a 1.

aspetta che terminino tutti i processi figli e poi stampa su stdout process_counter[].

i processi figli "monitorano" continuamente countdown:

    processo i-mo: se countdown > 0, allora decrementa countdown ed incrementa process_counter[i]

    se shutdown != 0, processo i-mo termina



è un problema del tipo "produttore-consumatore"
Little Book of Semaphores, pag. 55-59
 */

#define N 10

#define INIT_VALUE 100000


sem_t * items; // producer-consumer

sem_t * start;


int * process_counter; // int [N]



#define CHECK_ERR(a,msg) {if ((a) == -1) { perror((msg)); exit(EXIT_FAILURE); } }

#define CHECK_ERR_MMAP(a,msg) {if ((a) == MAP_FAILED) { perror((msg)); exit(EXIT_FAILURE); } }

void child_process(int i);

int main() {

	int res;

	items = mmap(NULL, // NULL: è il kernel a scegliere l'indirizzo
			sizeof(sem_t) * 2 + sizeof(int) * (N+1), // dimensione della memory map
			PROT_READ | PROT_WRITE, // memory map leggibile e scrivibile
			MAP_SHARED | MAP_ANONYMOUS, // memory map condivisibile con altri processi e senza file di appoggio
			-1, // nessun file di appoggio alla memory map
			0); // offset nel file
	CHECK_ERR_MMAP(items,"mmap")

	start = items + 1;

	process_counter = (int *) (items + 2);


	printf("items = %p\n", items);
	printf("start = %p\n", start);
	printf("process_counter = %p\n", process_counter);
	printf("\n");

	res = sem_init(items,
						1, // 1 => il semaforo è condiviso tra processi, 0 => il semaforo è condiviso tra threads del processo
						INIT_VALUE // valore iniziale del semaforo
					  );
	CHECK_ERR(res,"sem_init")

	res = sem_init(start,
						1, // 1 => il semaforo è condiviso tra processi, 0 => il semaforo è condiviso tra threads del processo
						0 // valore iniziale del semaforo
					  );
	CHECK_ERR(res,"sem_init")

	// voglio creare N processi figli

	for (int i = 0; i < N; i++) {
		switch (fork()) {
		  case 0: // child process
			child_process(i);
		  case -1:
			perror("fork");
			exit(EXIT_FAILURE);
		  default:
			;
		}
	}

	printf("[parent] before sleep 1 second\n");
	sleep(1);

	// facciamo partire tutti i processi figli
	for (int i = 0; i < N; i++) {
		if (sem_post(start) == -1) {
			perror("sem_post");
			exit(EXIT_FAILURE);
		}
	}


	while (wait(NULL) != -1) ; // aspettiamo la terminazione di tutt i processi figli

	res = sem_destroy(items); // ok per distruggere il semaforo
	CHECK_ERR(res,"sem_destroy")

	res = sem_destroy(start); // ok per distruggere il semaforo
	CHECK_ERR(res,"sem_destroy")

	long sum = 0;
	for (int i = 0; i < N; i++) {
		printf("[parent] process_counter[%d] = %d\n", i, process_counter[i]);

		sum += process_counter[i];
	}

	printf("[parent] sum of process_counter[] = %ld\n", sum);

	printf("[parent] bye\n");

	return 0;
}

void child_process(int proc_id) {
/*
i processi figli "monitorano" continuamente countdown:

    processo i-mo: se countdown > 0, allora decrementa countdown ed incrementa process_counter[i]

    se shutdown != 0, processo i-mo termina
 */

	if (sem_wait(start) == -1) {
		perror("sem_wait");
		exit(EXIT_FAILURE);
	}

	//printf("proc_id %d starts working\n", proc_id);

	while (1) {

		if (sem_trywait(items) == -1 && errno == EAGAIN) {
			// items == 0
			break;
		}

		process_counter[proc_id]++;

	}

	//printf("proc_id %d terminating\n", proc_id);

	exit(EXIT_SUCCESS);
}

