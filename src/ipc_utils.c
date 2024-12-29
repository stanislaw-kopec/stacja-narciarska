#include "ipc_utils.h"
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

// Semafory
int create_semaphore(key_t key, int initial_value) {
    int sem_id = semget(key, 1, IPC_CREAT | 0666);
    if (sem_id == -1) {
        perror("semget");
        return -1;
    }
    if (semctl(sem_id, 0, SETVAL, initial_value) == -1) {
        perror("semctl - SETVAL");
        return -1;
    }
    return sem_id;
}

int remove_semaphore(int sem_id) {
    if (semctl(sem_id, 0, IPC_RMID) == -1) {
        perror("semctl - IPC_RMID");
        return -1;
    }
    return 0;
}

// Pamięć dzielona
int create_shared_memory(key_t key, size_t size) {
    int shmid = shmget(key, size, IPC_CREAT | 0666);
    if (shmid == -1) {
        perror("shmget");
        return -1;
    }
    return shmid;
}

void* attach_shared_memory(int shmid) {
    void* addr = shmat(shmid, NULL, 0);
    if (addr == (void*)-1) {
        perror("shmat");
        return NULL;
    }
    return addr;
}

int detach_shared_memory(void* addr) {
    if (shmdt(addr) == -1) {
        perror("shmdt");
        return -1;
    }
    return 0;
}

int remove_shared_memory(int shmid) {
    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror("shmctl - IPC_RMID");
        return -1;
    }
    return 0;
}

// Kolejki komunikatów
int create_message_queue(key_t key) {
    int msgid = msgget(key, IPC_CREAT | 0666);
    if (msgid == -1) {
        perror("msgget");
        return -1;
    }
    return msgid;
}

int send_message(int msgid, long mtype, const char* text) {
    struct message msg;
    msg.mtype = mtype;
    strncpy(msg.mtext, text, sizeof(msg.mtext) - 1);
    msg.mtext[sizeof(msg.mtext) - 1] = '\0';

    if (msgsnd(msgid, &msg, sizeof(msg.mtext), 0) == -1) {
        perror("msgsnd");
        return -1;
    }
    return 0;
}

int receive_message(int msgid, long mtype, char* buffer, size_t buffer_size) {
    struct message msg;
    if (msgrcv(msgid, &msg, sizeof(msg.mtext), mtype, 0) == -1) {
        perror("msgrcv");
        return -1;
    }
    strncpy(buffer, msg.mtext, buffer_size - 1);
    buffer[buffer_size - 1] = '\0';
    return 0;
}

int remove_message_queue(int msgid) {
    if (msgctl(msgid, IPC_RMID, NULL) == -1) {
        perror("msgctl - IPC_RMID");
        return -1;
    }
    return 0;
}

