#include "ipc_utils.h"
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

// Tworzenie semaforów
int create_semaphore(key_t key, int initial_value) {
    int sem_id = semget(key, 1, IPC_CREAT | 0666);
    if (sem_id == -1) {
        perror("Error creating semaphore");
        exit(EXIT_FAILURE);
    }
    union semun {
        int val;
    } sem_union;
    sem_union.val = initial_value;
    if (semctl(sem_id, 0, SETVAL, sem_union) == -1) {
        perror("Error initializing semaphore");
        exit(EXIT_FAILURE);
    }
    return sem_id;
}

void semaphore_wait(int sem_id) {
    struct sembuf operation = {0, -1, 0};
    if (semop(sem_id, &operation, 1) == -1) {
        perror("Error waiting on semaphore");
        exit(EXIT_FAILURE);
    }
}

void semaphore_signal(int sem_id) {
    struct sembuf operation = {0, 1, 0};
    if (semop(sem_id, &operation, 1) == -1) {
        perror("Error signaling semaphore");
        exit(EXIT_FAILURE);
    }
}

void remove_semaphore(int sem_id) {
    if (semctl(sem_id, 0, IPC_RMID) == -1) {
        perror("Error removing semaphore");
    }
}

// Analogiczne implementacje dla pamięci dzielonej i kolejek komunikatów
