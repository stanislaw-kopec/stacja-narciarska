#include "ipc_utils.h"
#include "ski_station.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>  // Nagłówek dla key_t i ftok
#include <sys/types.h>
#include <unistd.h> // Dla fork()
#include <sys/wait.h> // Dla funkcji wait


int main() {
    key_t key = ftok("ipcfile", 1);
    if (key == -1) {
        perror("ftok");
        return 1;
    }

    // Tworzenie zasobów IPC
    int shmid = create_shared_memory(key, 1024);
    int msgid = create_message_queue(key);
    if (shmid == -1 || msgid == -1) return 1;

    printf("Zasoby IPC utworzone.\n");

    // Procesy potomne
    pid_t pid1 = fork();
    if (pid1 == 0) {
        // Proces kasjeraaa
        cashier_process();
        exit(0);
    }

    pid_t pid2 = fork();
    if (pid2 == 0) {
        // Proces pracownika
        worker_process();
        exit(0);
    }

    pid_t pid3 = fork();
    if (pid3 == 0) {
        // Proces narciarzy
        skier_process();
        exit(0);
    }

    // Oczekiwanie na zakończenie procesów potomnych
    wait(NULL);
    wait(NULL);
    wait(NULL);

    // Usuwanie zasobów
    remove_shared_memory(shmid);
    remove_message_queue(msgid);

    printf("Zasoby IPC usunięte.\n");
    return 0;
}