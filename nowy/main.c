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
#include <errno.h>
#include <sys/shm.h>
#include <stdbool.h>
#include <pthread.h>

#define NUM_SKIERS 50
#define PLATFORM_LIMIT 6
#define CHAIR_LIMIT 3
#define QUEUE_KEY 1234
#define SEM_KEY 5678
#define SHM_KEY 7777  // Klucz do pamięci współdzielonej

// Typ karnetu
#define T1 "T1"
#define T2 "T2"
#define T3 "T3"
#define T4 "T4"

// Ceny karnetów
#define T1_PRICE 50
#define T2_PRICE 70
#define T3_PRICE 90
#define T4_PRICE 120

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
    int opiekun_id;  // ID opiekuna (0 jeśli brak)
    float cena_karnetu; // Cena karnetu po uwzględnieniu zniżki
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

// Funkcja obliczająca cenę karnetu z uwzględnieniem zniżki
float oblicz_cene_karnetu(struct Narciarz *skier) {
    float cena;
    if (strcmp(skier->karnet, T1) == 0) {
        cena = T1_PRICE;
    } else if (strcmp(skier->karnet, T2) == 0) {
        cena = T2_PRICE;
    } else if (strcmp(skier->karnet, T3) == 0) {
        cena = T3_PRICE;
    } else if (strcmp(skier->karnet, T4) == 0) {
        cena = T4_PRICE;
    } else {
        cena = 0; // Domyślna cena, jeśli karnet nie jest rozpoznany
    }

    // Zniżka 25% dla dzieci do 12 lat i seniorów od 65 lat
    if (skier->wiek <= 12 || skier->wiek >= 65) {
        cena *= 0.75;
    }

    return cena;
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
    skier->cena_karnetu = oblicz_cene_karnetu(skier);
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

// Globalna flaga do kontrolowania stanu wyciągu
volatile sig_atomic_t czy_wyciag_zatrzymany = 0;

// Semafor binarny do synchronizacji dostępu do wyciągu
int sem_wyciag_id;

// PID pracownika2
pid_t pracownik2_pid;

// Funkcja obsługująca sygnał zatrzymania wyciągu
void zatrzymaj_wyciag(int sig) {
    char czas[20];
    aktualny_czas(czas);
    printf("[%s] Pracownik zatrzymał wyciąg z powodu zagrożenia.\n", czas);
    czy_wyciag_zatrzymany = 1; // Ustawienie flagi zatrzymania

    // Blokowanie dostępu do wyciągu
    struct sembuf sem_op = {0, -1, 0};
    semop(sem_wyciag_id, &sem_op, 1);

    sleep(3);  // Symulacja zatrzymania wyciągu na 3 sekundy
    printf("[%s] Pracownik wysyła sygnał do drugiego pracownika, aby wznowić wyciąg.\n", czas);
    kill(pracownik2_pid, SIGUSR2);  // Wysyłanie sygnału do pracownika2
}

// Funkcja obsługująca sygnał wznowienia wyciągu
void wznow_wyciag(int sig) {
    char czas[20];
    aktualny_czas(czas);
    printf("[%s] Pracownik otrzymał sygnał i wznowił działanie wyciągu.\n", czas);
    czy_wyciag_zatrzymany = 0; // Wznowienie wyciągu

    // Odblokowanie dostępu do wyciągu
    struct sembuf sem_op = {0, 1, 0};
    semop(sem_wyciag_id, &sem_op, 1);
}

// Funkcja realizująca proces narciarza
void narciarz_proces(struct Narciarz skier, int msg_queue_id, int sem_id) {
    struct message msg;
    char czas[20];

    // Dołączanie pamięci współdzielonej
    int shm_id = shmget(SHM_KEY, sizeof(int), 0666);
    int *licznik_narciarzy = (int *)shmat(shm_id, NULL, 0);
    if (licznik_narciarzy == (void *)-1) {
        perror("Błąd dołączania pamięci współdzielonej");
        exit(1);
    }

    // Rejestracja obsługi sygnału SIGTERM
    signal(SIGTERM, handle_sigterm);

    // Inkrementacja licznika narciarzy
    __sync_fetch_and_add(licznik_narciarzy, 1);

    aktualny_czas(czas);
    printf("Narciarz %d kupił karnet %s za %.2f zł o godzinie %s.\n", skier.id, skier.karnet, skier.cena_karnetu, czas);

    while (!czy_zakonczyc) {
        if (!sprawdz_waznosc_karnetu(&skier)) {
            printf("Narciarz %d ma nieważny karnet %s. Opuszcza stację.\n", skier.id, skier.karnet);
            break;
        }

        // Czekaj, jeśli wyciąg jest zatrzymany
        while (czy_wyciag_zatrzymany) {
            sleep(1);
        }

        int czas_zjazdu = wybierz_trase();
        
        struct sembuf sem_op = {0, -1, 0};
        semop(sem_id, &sem_op, 1);

        sem_op.sem_num = 1;
        semop(sem_id, &sem_op, 1);

        aktualny_czas(czas);
        printf("Narciarz %d siada na krzesełko o czasie %s z karnetem %s.\n",
               skier.id, czas, skier.karnet);

        msg.msg_type = 1;
        msg.skier = skier;
        aktualny_czas(msg.time);
        msgsnd(msg_queue_id, &msg, sizeof(msg) - sizeof(long), 0);

        sleep(czas_zjazdu);

        sem_op.sem_num = 1;
        sem_op.sem_op = 1;
        semop(sem_id, &sem_op, 1);

        sem_op.sem_num = 0;
        semop(sem_id, &sem_op, 1);

        printf("Narciarz %d zakończył zjazd.\n", skier.id);
    }

    printf("Narciarz %d opuszcza stację narciarską.\n", skier.id);

    // Dekrementacja licznika narciarzy
    __sync_fetch_and_sub(licznik_narciarzy, 1);

    // Odłączenie pamięci współdzielonej
    shmdt(licznik_narciarzy);

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
        int result = msgrcv(msg_queue_id, &msg, sizeof(msg) - sizeof(long), 1, 0);

        if (result < 0) {
            if (errno == EINTR) {
                continue;  // Ponów odczyt wiadomości po przerwaniu sygnałem
            }
            perror("Błąd odczytu wiadomości");
            break;
        }

        fprintf(plik, "Czas: %s, Karnet ID: %d, Wiek: %d, Status: %s, Karnet: %s, Cena: %.2f zł\n",
                msg.time, msg.skier.id, msg.skier.wiek,
                msg.skier.status_vip ? "VIP" : "zwykły", msg.skier.karnet, msg.skier.cena_karnetu);
        fflush(plik);
    }

    fclose(plik);
    printf("Kasjer zakończył pracę.\n");
    exit(0);
}

pid_t narciarze_gid;  // ID grupy procesów narciarzy
pid_t pracownik1_pid, kasjer_pid;  // PID-y dla pracowników i kasjera

// Funkcja fabryki dla tworzenia narciarzy w losowych odstępach czasowych
void fabryka_narciarzy(int msg_queue_id, int sem_id) {
    int skier_id = 0;
    int opiekun_id = 0;  // To będzie id dorosłego opiekuna
    narciarze_gid = getpid();  // Ustawienie grupy procesów narciarzy

    while (!czy_zakonczyc) {
        struct Narciarz skier;
        skier.id = skier_id++;
        skier.wiek = rand() % 60 + 8; // Losowy wiek (8 - 67)
        skier.status_vip = rand() % 2; // VIP czy nie
        przydziel_karnet(&skier); // Losowanie karnetu dla narciarza

        // Przypisanie dzieci (wiek 4-8 lat) do opiekunów
        if (skier.wiek >= 4 && skier.wiek <= 8) {
            // Dziecko w wieku 4-8 lat
            skier.opiekun_id = opiekun_id;  // Przypisz dziecku opiekuna
            printf("Dziecko %d przypisane do opiekuna %d\n", skier.id, skier.opiekun_id);

            // Sprawdź, czy opiekun ma już dwóch podopiecznych
            if (rand() % 2 == 0) {  // Jeśli dziecko dostaje opiekuna, zwiększamy liczbę dzieci pod opieką
                opiekun_id++;
            }
        } else {
            skier.opiekun_id = 0;  // Dorosły nie potrzebuje opiekuna
        }

        // Tworzenie procesu dla narciarza
        pid_t pid = fork();
        if (pid == 0) {
            setpgid(0, narciarze_gid);  // Ustawienie procesu narciarza w tej samej grupie
            narciarz_proces(skier, msg_queue_id, sem_id);
        }

        // Czekanie na losowy czas (1-3 sekundy)
        int czas_oczekiwania = rand() % 3 + 1;
        sleep(czas_oczekiwania);
    }
}

// Proces pracownika1 (stacja dolna)
void pracownik1_proces() {
    signal(SIGUSR1, zatrzymaj_wyciag);
    signal(SIGTERM, handle_sigterm);
    while (!czy_zakonczyc) {
        int czas_oczekiwania = rand() % 3 + 15;  // Losowy czas 15-17 sekund
        sleep(czas_oczekiwania);
        printf("Pracownik1 zatrzymuje wyciąg.\n");
        kill(getpid(), SIGUSR1);  // Wysyłanie sygnału do samego siebie
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

int *licznik_narciarzy;  // Wskaźnik do pamięci współdzielonej

int main() {
    int msg_queue_id, sem_id, shm_id;

    // Tworzenie pamięci współdzielonej dla licznika narciarzy
    shm_id = shmget(SHM_KEY, sizeof(int), IPC_CREAT | 0666);
    if (shm_id < 0) {
        perror("Błąd tworzenia pamięci współdzielonej");
        exit(1);
    }
    licznik_narciarzy = (int *)shmat(shm_id, NULL, 0);
    if (licznik_narciarzy == (void *)-1) {
        perror("Błąd dołączania pamięci współdzielonej");
        exit(1);
    }
    *licznik_narciarzy = 0;  // Ustawienie początkowej liczby narciarzy

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
    semctl(sem_id, 0, SETVAL, PLATFORM_LIMIT);
    semctl(sem_id, 1, SETVAL, CHAIR_LIMIT);

    // Tworzenie semafora binarnego do synchronizacji wyciągu
    sem_wyciag_id = semget(SEM_KEY + 1, 1, IPC_CREAT | 0666);
    if (sem_wyciag_id < 0) {
        perror("Błąd tworzenia semafora binarnego");
        exit(1);
    }
    semctl(sem_wyciag_id, 0, SETVAL, 1);  // Inicjalizacja semafora binarnego

    // Tworzenie procesu kasjera
    kasjer_pid = fork();
    if (kasjer_pid == 0) {
        kasjer_proces(msg_queue_id);
    }

    // Tworzenie procesów pracowników
    pracownik1_pid = fork();
    if (pracownik1_pid == 0) {
        pracownik1_proces();
    }
    pracownik2_pid = fork();
    if (pracownik2_pid == 0) {
        pracownik2_proces();
    }

    // Tworzenie fabryki narciarzy
    pid_t narciarze_pid = fork();
    if (narciarze_pid == 0) {
        setpgid(0, 0);  // Tworzymy nową grupę procesów dla narciarzy
        fabryka_narciarzy(msg_queue_id, sem_id);
        exit(0);
    }
    setpgid(narciarze_pid, narciarze_pid);

    // Symulacja czasu pracy wyciągu
    sleep(40);

    printf("Czas pracy wyciągu minął. Wyłączanie...\n");

    // Wysłanie SIGTERM do grupy procesów narciarzy
    killpg(narciarze_pid, SIGTERM);

    // Oczekiwanie na zakończenie wszystkich narciarzy
    while (*licznik_narciarzy > 0) {
        sleep(1);  // Czekamy, aż wszyscy narciarze opuszczą stok
    }

    printf("Wszyscy narciarze opuścili stok.\n");

    // Wysłanie SIGTERM do kasjera i pracowników
    kill(kasjer_pid, SIGTERM);
    kill(pracownik1_pid, SIGTERM);
    kill(pracownik2_pid, SIGTERM);

    // Oczekiwanie na zakończenie procesów kasjera i pracowników
    waitpid(kasjer_pid, NULL, 0);
    waitpid(pracownik1_pid, NULL, 0);
    waitpid(pracownik2_pid, NULL, 0);

    printf("Wszyscy pracownicy zakończyli pracę.\n");

    // Zwolnienie pamięci współdzielonej
    shmdt(licznik_narciarzy);
    shmctl(shm_id, IPC_RMID, NULL);

    // Usunięcie semafora binarnego
    semctl(sem_wyciag_id, 0, IPC_RMID);

    return 0;
}