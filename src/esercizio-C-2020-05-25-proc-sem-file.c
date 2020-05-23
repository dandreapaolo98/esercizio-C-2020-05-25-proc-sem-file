/*
 * esercizio-C-2020-05-25-proc-sem-file.c
 *
 *  Created on: May 22, 2020
 *      Author: marco
 */


/***************TESTO ESERCIZIO***************
il processo principale crea un file "output.txt" di dimensione FILE_SIZE (all'inizio ogni byte del file deve avere valore 0)

#define FILE_SIZE (1024*1024)

#define N 4

è dato un semaforo senza nome: proc_sem

il processo principale crea N processi figli

i processi figli aspettano al semaforo proc_sem.

ogni volta che il processo i-mo riceve semaforo "verde", cerca il primo byte del file che abbia valore 0 e ci scrive il valore ('A' + i). La scrittura su file è concorrente e quindi va gestita opportunamente (ad es. con un mutex).

se il processo i-mo non trova una posizione in cui poter scrivere il valore, allora termina.

il processo padre:

per (FILE_SIZE+N) volte, incrementa il semaforo proc_sem

aspetta i processi figli e poi termina.

risolvere il problema in due modi:

soluzione A:

usare le system call open(), lseek(), write()

soluzione B:

usare le system call open(), mmap()

int main() {
    printf("ora avvio la soluzione_A()...\n");
    soluzione_A();

    printf("ed ora avvio la soluzione_B()...\n");
    soluzione_B();

    printf("bye!\n");
    return 0;
}

*/

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
#include <pthread.h>

#define FILE_SIZE (1024*1024)

#define N 4

#define CHECK_ERR(a,msg) {if ((a) == -1) { perror((msg)); exit(EXIT_FAILURE); } }
#define CHECK_ERR_MMAP(a,msg) {if ((a) == MAP_FAILED) { perror((msg)); exit(EXIT_FAILURE); } }

sem_t * proc_sem;
sem_t * mutex;

int create_file_set_size(char * file_name, unsigned int file_size);
void soluzioneA();
void soluzioneB();

int main(int argc, char * argv[]) {

	printf("ora avvio la soluzione_A()...\n");
	soluzioneA();

	printf("ed ora avvio la soluzione_B()...\n");
	soluzioneB();

	printf("bye\n");

	return 0;
}

int create_file_set_size(char * file_name, unsigned int file_size) {

	// apriamo il file in scrittura, se non esiste verrà creato,
	// se esiste già la sua dimensione viene troncata a 0

	// tratto da man 2 open
	// O_CREAT  If pathname does not exist, create it as a regular file.
	// O_TRUNC  If the file already exists and is a regular file and the access mode allows writing ... it will be truncated to length 0.
	// O_RDONLY, O_WRONLY, or O_RDWR  These request opening the file read-only, write-only, or read/write, respectively.

	int fd = open(file_name,
				  O_CREAT | O_RDWR,
				  S_IRUSR | S_IWUSR // l'utente proprietario del file avrà i permessi di lettura e scrittura sul nuovo file
				 );

	if (fd == -1) { // errore!
			perror("open()");
			return -1;
		}

	int res = ftruncate(fd, file_size);

	if (res == -1) {
		perror("ftruncate()");
		return -1;
	}

	return fd;
}

void soluzioneA(){
	char * file_name = "output.txt";
	unsigned int file_size = FILE_SIZE;
	int fd, res;
	off_t file_offset, eof;

	fd = create_file_set_size(file_name, file_size);

	if (fd == -1) {
		exit(EXIT_FAILURE);
	}

	proc_sem = mmap(NULL, // NULL: è il kernel a scegliere l'indirizzo
			sizeof(sem_t), // dimensione della memory map
			PROT_READ | PROT_WRITE, // memory map leggibile e scrivibile
			MAP_SHARED | MAP_ANONYMOUS, // memory map condivisibile con altri processi e senza file di appoggio
			-1,
			0); // offset nel file

	CHECK_ERR_MMAP(proc_sem,"mmap")

	res = sem_init(proc_sem,
					1, // 1 => il semaforo è condiviso tra processi, 0 => il semaforo è condiviso tra threads del processo
					0 // valore iniziale del semaforo
				  );

	CHECK_ERR(res,"sem_init")

	mutex = mmap(NULL, // NULL: è il kernel a scegliere l'indirizzo
			sizeof(sem_t), // dimensione della memory map
			PROT_READ | PROT_WRITE, // memory map leggibile e scrivibile
			MAP_SHARED | MAP_ANONYMOUS, // memory map condivisibile con altri processi e senza file di appoggio
			-1,
			0); // offset nel file

	CHECK_ERR_MMAP(mutex,"mmap")

	res = sem_init(mutex,
					1, // 1 => il semaforo è condiviso tra processi, 0 => il semaforo è condiviso tra threads del processo
					1 // valore iniziale del semaforo
				  );

	CHECK_ERR(res,"sem_init")

	char msg[1] = "A";

	eof = lseek(fd, 0, SEEK_END);
	if (eof == -1) {
		perror("lseek()");
		exit(EXIT_FAILURE);
	}
	file_offset = lseek(fd, 0, SEEK_SET);
	if (file_offset == -1) {
		perror("lseek()");
		exit(EXIT_FAILURE);
	}
	for(int i = 0; i < N; i++){
		if((res = fork()) == 0){

			msg[0] += i;
			while(1){
				if (sem_wait(proc_sem) == -1) {
					perror("sem_wait");
					exit(EXIT_FAILURE);
				}

				if (sem_wait(mutex) == -1) {
					perror("sem_wait");
					exit(EXIT_FAILURE);
				}

				if(lseek(fd, 0, SEEK_CUR) == eof){
					if (sem_post(mutex) == -1) {
						perror("sem_post");
						exit(EXIT_FAILURE);
					}
					break;
				}

				res = write(fd, msg, 1);

				if (res == -1) {
					perror("write()");
					exit(EXIT_FAILURE);
				}

				if (sem_post(mutex) == -1) {
					perror("sem_post");
					exit(EXIT_FAILURE);
				}

			}


			exit(EXIT_SUCCESS);
		}
	}

	for(int i = 0; i < FILE_SIZE + N; i++){
		if (sem_post(proc_sem) == -1) {
			perror("sem_post");
			exit(EXIT_FAILURE);
		}
	}

	for(int i = 0; i < N; i++){	//aspetto la terminazione di tutti i figli
		if (wait(NULL) == -1) {
			perror("wait error");
		}
	}

	if (close(fd) == -1) {
		perror("close");
		exit(EXIT_FAILURE);
	}
}

void soluzioneB(){
	char * file_name = "output.txt";
	unsigned int file_size = FILE_SIZE;
	int fd, res;
	char * addr;
	int * counter;
	off_t file_offset, eof;

	fd = create_file_set_size(file_name, file_size);

	if (fd == -1) {
		exit(EXIT_FAILURE);
	}

	addr = mmap(NULL, // NULL: è il kernel a scegliere l'indirizzo
				file_size, // dimensione della memory map
				PROT_READ | PROT_WRITE, // memory map leggibile e scrivibile
				MAP_SHARED, // memory map condivisibile con altri processi
				fd,
				0); // offset nel file

	CHECK_ERR_MMAP(addr,"mmap")

	close(fd); // il file descriptor si può chiudere subito dopo mmap, la memory map rimane attiva


	proc_sem = mmap(NULL, // NULL: è il kernel a scegliere l'indirizzo
			sizeof(sem_t), // dimensione della memory map
			PROT_READ | PROT_WRITE, // memory map leggibile e scrivibile
			MAP_SHARED | MAP_ANONYMOUS, // memory map condivisibile con altri processi e senza file di appoggio
			-1,
			0); // offset nel file

	CHECK_ERR_MMAP(proc_sem,"mmap")

	res = sem_init(proc_sem,
					1, // 1 => il semaforo è condiviso tra processi, 0 => il semaforo è condiviso tra threads del processo
					0 // valore iniziale del semaforo
				  );

	CHECK_ERR(res,"sem_init")

	mutex = mmap(NULL, // NULL: è il kernel a scegliere l'indirizzo
			sizeof(sem_t), // dimensione della memory map
			PROT_READ | PROT_WRITE, // memory map leggibile e scrivibile
			MAP_SHARED | MAP_ANONYMOUS, // memory map condivisibile con altri processi e senza file di appoggio
			-1,
			0); // offset nel file

	CHECK_ERR_MMAP(mutex,"mmap")

	res = sem_init(mutex,
					1, // 1 => il semaforo è condiviso tra processi, 0 => il semaforo è condiviso tra threads del processo
					1 // valore iniziale del semaforo
				  );

	CHECK_ERR(res,"sem_init")

	counter = mmap(NULL, // NULL: è il kernel a scegliere l'indirizzo
			sizeof(int), // dimensione della memory map
			PROT_READ | PROT_WRITE, // memory map leggibile e scrivibile
			MAP_SHARED | MAP_ANONYMOUS, // memory map condivisibile con altri processi e senza file di appoggio
			-1,
			0); // offset nel file

	CHECK_ERR_MMAP(counter,"mmap")

	*counter = 0;

	char msg = 'A';

	for(int i = 0; i < N; i++){
		if((res = fork()) == 0){

			msg += i;
			while(1){
				if (sem_wait(proc_sem) == -1) {
					perror("sem_wait");
					exit(EXIT_FAILURE);
				}

				if (sem_wait(mutex) == -1) {
					perror("sem_wait");
					exit(EXIT_FAILURE);
				}

				if(*counter == file_size){
					if (sem_post(mutex) == -1) {
						perror("sem_post");
						exit(EXIT_FAILURE);
					}
					break;
				}

				addr[*counter] = msg;
				(*counter)++;

				if (res == -1) {
					perror("write()");
					exit(EXIT_FAILURE);
				}

				if (sem_post(mutex) == -1) {
					perror("sem_post");
					exit(EXIT_FAILURE);
				}

			}

			exit(EXIT_SUCCESS);
		}
	}

	for(int i = 0; i < FILE_SIZE + N; i++){
		if (sem_post(proc_sem) == -1) {
			perror("sem_post");
			exit(EXIT_FAILURE);
		}
	}

	for(int i = 0; i < N; i++){	//aspetto la terminazione di tutti i figli
		if (wait(NULL) == -1) {
			perror("wait error");
		}
	}
}







