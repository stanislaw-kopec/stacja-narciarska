#ifndef IPC_UTILS_H
#define IPC_UTILS_H

#include <sys/ipc.h>  // Nagłówek dla key_t i ftok
#include <sys/types.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <stddef.h>

struct message {
    long mtype;
    char mtext[100];
};

// Semafory
int create_semaphore(key_t key, int initial_value);
int remove_semaphore(int sem_id);

// Pamięć dzielona
int create_shared_memory(key_t key, size_t size);
void* attach_shared_memory(int shmid);
int detach_shared_memory(void* addr);
int remove_shared_memory(int shmid);

// Kolejki komunikatów
int create_message_queue(key_t key);
int send_message(int msgid, long mtype, const char* text);
int receive_message(int msgid, long mtype, char* buffer, size_t buffer_size);
int remove_message_queue(int msgid);

#endif
