#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <time.h>
#include <signal.h>
#include <string.h>

#define NUM_SKIERS 50
#define PLATFORM_LIMIT 6
#define CHAIR_LIMIT 3
#define QUEUE_KEY 1234
#define SEM_KEY 5678

// Typ karnetu
#define T1 "T1"
#define T2 "T2"
#define T3 "T3"
#define T4 "T4"

// Trasy z różnymi czasami przejazdu
#define T1_TIME 2   // Czas przejazdu na trasie 1
#define T2_TIME 3   // Czas przejazdu na trasie 2
#define T3_TIME 4   // Czas przejazdu na trasie 3

// Czas ważności karnetów (w sekundach)
#define T1_VALIDITY 10
#define T2_VALIDITY 15
#define T3_VALIDITY 20
#define T4_VALIDITY 25

// Struktura narciarza
struct Narciarz {
    int id;
    int wiek;
    int status_vip; // 1 dla VIP, 0 dla zwykłego narciarza
    char karnet[3]; // Typ karnetu (T1, T2, T3, T4)
    time_t czas_waznosci; // Czas ważności karnetu
};

// Struktura wiadomości dla kolejki komunikatów
struct message {
    long msg_type;
    struct Narciarz skier;
    char time[20];
};

// Funkcja pobierająca aktualny czas
void aktualny_czas(char *bufor) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(bufor, 20, "%H:%M:%S", tm_info);
}

// Funkcja przydzielająca karnet
void przydziel_karnet(struct Narciarz *skier) {
    int rand_karnet = rand() % 4;
    switch (rand_karnet) {
        case 0: 
            strcpy(skier->karnet, T1);
            skier->czas_waznosci = time(NULL) + T1_VALIDITY;
            break;
        case 1: 
            strcpy(skier->karnet, T2);
            skier->czas_waznosci = time(NULL) + T2_VALIDITY;
            break;
        case 2: 
            strcpy(skier->karnet, T3);
            skier->czas_waznosci = time(NULL) + T3_VALIDITY;
            break;
        case 3: 
            strcpy(skier->karnet, T4);
            skier->czas_waznosci = time(NULL) + T4_VALIDITY;
            break;
    }
}

// Funkcja sprawdzająca ważność karnetu
int sprawdz_waznosc_karnetu(struct Narciarz *skier) {
    return time(NULL) <= skier->czas_waznosci;
}

// Funkcja losująca trasę i zwracająca czas przejazdu
int wybierz_trase() {
    int rand_trasa = rand() % 3; // Losowanie trasy (0, 1, 2)
    switch (rand_trasa) {
        case 0: return T1_TIME;
        case 1: return T2_TIME;
        case 2: return T3_TIME;
        default: return T1_TIME;
    }
}

// Funkcja obsługująca sygnał SIGTERM
volatile sig_atomic_t czy_zakonczyc = 0;

void handle_sigterm(int sig) {
    czy_zakonczyc = 1; // Ustawienie flagi zakończenia
}

// Funkcja realizująca proces narciarza
void narciarz_proces(struct Narciarz skier, int msg_queue_id, int sem_id) {
    struct message msg;
    char czas[20];

    // Rejestracja obsługi sygnału SIGTERM
    signal(SIGTERM, handle_sigterm);

    // Narciarz kupuje karnet
    aktualny_czas(czas);
    printf("Narciarz %d kupił karnet %s o godzinie %s.\n", skier.id, skier.karnet, czas);

    // Narciarz od razu idzie na wyciąg
    while (!czy_zakonczyc) {  // Narciarz zjeżdża, dopóki nie otrzyma sygnału SIGTERM
        // Sprawdzenie ważności karnetu przed wejściem na platformę
        if (!sprawdz_waznosc_karnetu(&skier)) {
            printf("Narciarz %d ma nieważny karnet %s. Opuszcza stację.\n", skier.id, skier.karnet);
            break;
        }

        int czas_zjazdu = wybierz_trase(); // Losowanie trasy

        // Narciarz wchodzi na peron
        struct sembuf sem_op = {0, -1, 0}; // Zajmij miejsce na peronie
        semop(sem_id, &sem_op, 1);

        // Narciarz siada na krzesełko
        sem_op.sem_num = 1;
        semop(sem_id, &sem_op, 1);

        aktualny_czas(czas);
        printf("Narciarz %d (%s, wiek: %d) siada na krzesełko o czasie %s z karnetem %s.\n",
               skier.id, skier.status_vip ? "VIP" : "zwykły", skier.wiek, czas, skier.karnet);

        // Rejestracja przejścia w kolejce komunikatów
        msg.msg_type = 1;
        msg.skier = skier;
        aktualny_czas(msg.time);
        msgsnd(msg_queue_id, &msg, sizeof(msg) - sizeof(long), 0);

        // Symulacja czasu jazdy na wyciągu (czas zależny od wybranej trasy)
        sleep(czas_zjazdu);

        // Zwolnienie krzesełka i miejsca na peronie
        sem_op.sem_num = 1;
        sem_op.sem_op = 1;
        semop(sem_id, &sem_op, 1);

        sem_op.sem_num = 0;
        semop(sem_id, &sem_op, 1);

        printf("Narciarz %d zakończył zjazd (czas: %d sekund).\n", skier.id, czas_zjazdu);
    }

    // Narciarz opuszcza stację narciarską
    printf("Narciarz %d opuszcza stację narciarską.\n", skier.id);

    exit(0);
}

// Funkcja realizująca proces kasjera (rejestracja karnetów)
void kasjer_proces(int msg_queue_id) {
    struct message msg;
    FILE *plik = fopen("raport.txt", "w");
    if (!plik) {
        perror("Błąd otwierania pliku raportu");
        exit(1);
    }

    // Rejestracja obsługi sygnału SIGTERM
    signal(SIGTERM, handle_sigterm);

    fprintf(plik, "Raport z dnia:\n\n");
    fprintf(plik, "Historia przejść przez bramki:\n");

    while (!czy_zakonczyc) {
        if (msgrcv(msg_queue_id, &msg, sizeof(msg) - sizeof(long), 1, 0) < 0) {
            perror("Błąd odczytu wiadomości");
            break;
        }
        fprintf(plik, "Czas: %s, Karnet ID: %d, Wiek: %d, Status: %s, Karnet: %s\n",
                msg.time, msg.skier.id, msg.skier.wiek,
                msg.skier.status_vip ? "VIP" : "zwykły", msg.skier.karnet);
        fflush(plik);
    }

    fclose(plik);
    printf("Kasjer zakończył pracę.\n");
    exit(0);
}

// Funkcja fabryki dla tworzenia narciarzy w losowych odstępach czasowych
void fabryka_narciarzy(int msg_queue_id, int sem_id) {
    int skier_id = 0;
    while (!czy_zakonczyc) {
        struct Narciarz skier;
        skier.id = skier_id++;
        skier.wiek = rand() % 60 + 8; // Losowy wiek (8 - 67)
        skier.status_vip = rand() % 2; // VIP czy nie
        przydziel_karnet(&skier); // Losowanie karnetu dla narciarza

        // Tworzenie procesu dla narciarza
        if (fork() == 0) {
            narciarz_proces(skier, msg_queue_id, sem_id);
        }

        // Czekanie na losowy czas (1-3 sekundy)
        int czas_oczekiwania = rand() % 3 + 1;
        sleep(czas_oczekiwania);
    }
}

// Funkcja obsługująca sygnał zatrzymania wyciągu
void zatrzymaj_wyciag(int sig) {
    printf("Pracownik zatrzymał wyciąg z powodu zagrożenia.\n");
    // Wysyłanie sygnału do drugiego pracownika
    kill(getppid(), SIGUSR2);
}

// Funkcja obsługująca sygnał wznowienia wyciągu
void wznow_wyciag(int sig) {
    printf("Pracownik wznowił działanie wyciągu.\n");
}

// Proces pracownika1 (stacja dolna)
void pracownik1_proces() {
    signal(SIGUSR1, zatrzymaj_wyciag);
    signal(SIGTERM, handle_sigterm);
    while (!czy_zakonczyc) {
        sleep(1); // Symulacja pracy
    }
    printf("Pracownik1 kończy pracę.\n");
    exit(0);
}

// Proces pracownika2 (stacja górna)
void pracownik2_proces() {
    signal(SIGUSR2, wznow_wyciag);
    signal(SIGTERM, handle_sigterm);
    while (!czy_zakonczyc) {
        sleep(1); // Symulacja pracy
    }
    printf("Pracownik2 kończy pracę.\n");
    exit(0);
}

int main() {
    pid_t pids[NUM_SKIERS];
    int msg_queue_id, sem_id;

    // Tworzenie kolejki komunikatów
    msg_queue_id = msgget(QUEUE_KEY, IPC_CREAT | 0666);
    if (msg_queue_id < 0) {
        perror("Błąd tworzenia kolejki komunikatów");
        exit(1);
    }

    // Tworzenie semaforów
    sem_id = semget(SEM_KEY, 2, IPC_CREAT | 0666);
    if (sem_id < 0) {
        perror("Błąd tworzenia semaforów");
        exit(1);
    }
    semctl(sem_id, 0, SETVAL, PLATFORM_LIMIT); // Peron
    semctl(sem_id, 1, SETVAL, CHAIR_LIMIT);   // Krzesełka

    // Tworzenie procesu kasjera
    if (fork() == 0) {
        kasjer_proces(msg_queue_id);
    }

    // Tworzenie procesów pracowników
    if (fork() == 0) {
        pracownik1_proces();
    }
    if (fork() == 0) {
        pracownik2_proces();
    }

    // Tworzenie fabryki narciarzy
    if (fork() == 0) {
        fabryka_narciarzy(msg_queue_id, sem_id);
    }

    // Symulacja czasu pracy wyciągu
    time_t start_time = time(NULL);
    time_t end_time = start_time + 60; // 60 sekund symulacji
    while (time(NULL) < end_time) {
        sleep(1);
    }

    // Wyłączenie wyciągu po upływie czasu
    printf("Czas pracy wyciągu minął. Wyłączanie...\n");

    // Wysłanie sygnału SIGTERM do wszystkich procesów potomnych
    kill(0, SIGTERM);

    // Czekanie na zakończenie procesów potomnych
    while (wait(NULL) > 0);

    // Zatrzymanie wyciągu po 5 sekundach
    sleep(5);
    printf("Wyciąg został zatrzymany.\n");

    return 0;
}