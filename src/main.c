#include "ipc_utils.h"
#include "worker.h"
#include "ski_station.h"
#include <sys/wait.h>
#include <sys/ipc.h>
#include <unistd.h>
#include <stdio.h>

int main() {
    printf("Inicjalizacja stacji narciarskiej\n");

    // Inicjalizacja IPC
    key_t sem_key = ftok("ipcfile", 1);
    int sem_id = create_semaphore(sem_key, 1);

    // Tworzenie procesów
    if (fork() == 0) kasjer();
    if (fork() == 0) pracownik1(sem_id, -1);
    if (fork() == 0) pracownik2(sem_id, -1);

    for (int i = 0; i < 10; i++) {
        if (fork() == 0) narciarz(i, -1, sem_id);
    }

    // Czekanie na zakończenie procesów
    while (wait(NULL) > 0);

    // Usuwanie zasobów IPC
    remove_semaphore(sem_id);

    printf("Zakończenie działania stacji narciarskiej\n");
    return 0;
}
