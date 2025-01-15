#include "ski_station.h"
#include "ipc_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

// Stałe konfiguracyjne
#define MAX_PASSENGERS 120
#define MAX_CHAIRS 40
#define MAX_QUEUE 50
#define DISCOUNT_CHILD 0.25
#define DISCOUNT_SENIOR 0.25

void cashier_process() {
    printf("Kasjer obsługuje karnety.\n");

    // Symulacja sprzedaży karnetów
    int skier_id = 1;
    while (skier_id <= 10) {
        int is_child = rand() % 2;
        int is_senior = rand() % 2;

        double price = 50.0;  // Standardowa cena karnetu
        if (is_child) {
            price *= (1.0 - DISCOUNT_CHILD);
            printf("Sprzedano karnet dziecku (ID: %d). Cena: %.2f PLN.\n", skier_id, price);
        } else if (is_senior) {
            price *= (1.0 - DISCOUNT_SENIOR);
            printf("Sprzedano karnet seniorowi (ID: %d). Cena: %.2f PLN.\n", skier_id, price);
        } else {
            printf("Sprzedano karnet dorosłemu (ID: %d). Cena: %.2f PLN.\n", skier_id, price);
        }

        skier_id++;
        sleep(5); // Symulacja czasu obsługi
    }
}

void worker_process() {
    printf("Pracownik obsługuje stację.\n");

    key_t key = ftok("ipcfile", 1);
    if (key == -1) {
        perror("ftok");
        return;
    }

    int msgid = create_message_queue(key);
    if (msgid == -1) return;

    char buffer[100];
    while (1) {
        printf("Pracownik czeka na komunikaty.\n");
        if (receive_message(msgid, 1, buffer, sizeof(buffer)) == -1) {
            break;
        }

        if (strcmp(buffer, "STOP") == 0) {
            printf("Otrzymano polecenie zatrzymania kolejki linowej.\n");
            break;
        }

        printf("Pracownik obsługuje komunikat: %s\n", buffer);
        sleep(5);
    }

    printf("Pracownik zakończył obsługę stacji.\n");
}


void skier_process() {
    printf("Narciarz korzysta ze stacji.\n");

    key_t key = ftok("ipcfile", 1); // Klucz dla pamięci dzielonej
    if (key == -1) {
        perror("ftok");
        return;
    }

    // Inicjalizacja pamięci dzielonej
    int shmid = create_shared_memory(key, sizeof(int) * MAX_PASSENGERS);
    if (shmid == -1) return;

    int* shared_data = (int*)attach_shared_memory(shmid);
    if (shared_data == NULL) return;

    // Inicjalizacja danych w pamięci dzielonejjj
    for (int i = 0; i < MAX_PASSENGERS; i++) {
        shared_data[i] = 0;
    }

    // Tworzenie kolejki komunikatów
    int msgid = create_message_queue(key);
    if (msgid == -1) {
        detach_shared_memory(shared_data);
        return;
    }

    // Symulacja narciarzy
    for (int i = 0; i < 10; i++) {
        int skier_id = rand() % MAX_PASSENGERS;
        shared_data[skier_id]++;
        printf("Narciarz ID %d wykonał %d przejazd(y).\n", skier_id, shared_data[skier_id]);

        // Co pewien czas wysyłamy komunikat do pracownika
        if (rand() % 5 == 0) {
            send_message(msgid, 1, "Problemy techniczne");
        }

        sleep(15);
    }

    // Wyświetlenie raportu
    printf("\n=== Raport narciarzy ===\n");
    for (int i = 0; i < MAX_PASSENGERS; i++) {
        if (shared_data[i] > 0) {
            printf("Narciarz ID %d: %d przejazd(y)\n", i, shared_data[i]);
        }
    }

    // Sprzątanie zasobów
    detach_shared_memory(shared_data);
    remove_message_queue(msgid);
}


