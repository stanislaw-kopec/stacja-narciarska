#include "ski_station.h"
#include "ipc_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

void kasjer() {
    printf("Kasjer sprzedaje karnety\n");
    // Logika sprzedaży karnetów
}

void narciarz(int id, int shm_id, int sem_id) {
    printf("Narciarz %d wchodzi na peron\n", id);
    // Logika przejścia przez bramki
}

void generuj_raport() {
    printf("Generowanie raportu dziennego\n");
    // Logika raportowania
}
