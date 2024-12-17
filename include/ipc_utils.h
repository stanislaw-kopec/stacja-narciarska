#ifndef IPC_UTILS_H
#define IPC_UTILS_H

#include <sys/ipc.h>
#include <sys/types.h>

// Tworzenie i obsługa semaforów
int create_semaphore(key_t key, int initial_value);
void remove_semaphore(int sem_id);
void semaphore_wait(int sem_id);
void semaphore_signal(int sem_id);

// Tworzenie i obsługa pamięci dzielonej
int create_shared_memory(key_t key, size_t size);
void *attach_shared_memory(int shm_id);
void detach_shared_memory(void *shm_ptr);
void remove_shared_memory(int shm_id);

// Tworzenie i obsługa kolejek komunikatów
int create_message_queue(key_t key);
void send_message(int msg_id, long msg_type, const char *msg_text);
ssize_t receive_message(int msg_id, long msg_type, char *buffer, size_t buffer_size);
void remove_message_queue(int msg_id);

#endif // IPC_UTILS_H
