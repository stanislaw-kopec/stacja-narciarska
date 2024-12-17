#include "worker.h"
#include "ipc_utils.h"
#include <stdio.h>
#include <unistd.h>
#include <signal.h>

void pracownik1(int sem_id, int msg_id) {
    while (1) {
        // Symulacja pracy pracownika 1
        printf("Pracownik 1 kontroluje dolną stację\n");
        sleep(5); // Symulacja działania
    }
}

void pracownik2(int sem_id, int msg_id) {
    while (1) {
        // Symulacja pracy pracownika 2
        printf("Pracownik 2 kontroluje górną stację\n");
        sleep(5); // Symulacja działania
    }
}
