#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <time.h>

#define MAX_CHAIRS 80
#define ACTIVE_CHAIRS 40
#define MAX_PERON 100
#define NUM_BARRIERS 4

// Typy karnetów
#define TK1 1
#define TK2 2
#define TK3 3
#define DAILY 4

// Struktura dla kolejki komunikatów
struct message {
    long type;
    int id;        // Unikalny identyfikator narciarza
    int action;    // Akcja (np. wejście na peron, użycie wyciągu)
    time_t timestamp; // Czas akcji
};

// Struktura dla narciarza
typedef struct {
    int id;          // Unikalny identyfikator narciarza
    int age;         // Wiek narciarza
    int is_vip;      // Czy narciarz jest VIP (1 - tak, 0 - nie)
    int ticket_type; // Typ karnetu (TK1, TK2, TK3, DAILY)
    int zjazdy;      // Liczba zjazdów wykonanych przez narciarza
    int is_child;    // Czy narciarz jest dzieckiem (1 - tak, 0 - nie)
    int has_adult;   // Czy dziecko ma opiekuna (1 - tak, 0 - nie)
} Narciarz;

struct ticket_request {
    long mtype;          // Typ komunikatu (1 - żądanie, 2 - odpowiedź)
    int id;              // ID narciarza
    int age;             // Wiek narciarza
    int is_vip;          // Czy narciarz jest VIP
    int ticket_type;     // Typ karnetu (TK1, TK2, TK3, DAILY)
};

// Semafory
int sem_peron;   // Kontrola dostępu do peronu
int sem_chairs;  // Kontrola liczby aktywnych krzesełek
int sem_children; // Kontrola liczby dzieci na opiekuna

// Kolejka komunikatów
int msg_queue;

// Plik logów
FILE *log_file;

// Czas pracy wyciągu
time_t start_time = 9 * 3600; // 9:00 AM
time_t end_time = 17 * 3600;  // 5:00 PM

// Funkcja sprawdzająca, czy wyciąg jest aktywny
int is_lift_active() {
    // Używamy czasu symulowanego zamiast rzeczywistego czasu systemowego
    static time_t simulated_time = 9 * 3600; // Start symulacji o 9:00
    simulated_time += 60; // Przesuń czas symulacji o 1 minutę przy każdym wywołaniu

    // Sprawdź, czy czas symulowany mieści się w godzinach pracy wyciągu
    return (simulated_time >= 9 * 3600 && simulated_time <= 17 * 3600);
}

//stara wersja
//int is_lift_active() {
//    time_t now = time(NULL);
//    struct tm *tm_now = localtime(&now);
//    time_t current_time = tm_now->tm_hour * 3600 + tm_now->tm_min * 60 + tm_now->tm_sec;
//
//    return (current_time >= start_time && current_time <= end_time);
//}

// Funkcja do zatrzymania wyciągu
void stop_lift(int sig) {
    printf("[Sygnał] Wyciąg zatrzymany z powodu bezpieczeństwa.\n");
    // Logika zatrzymania wyciągu
}

// Funkcja do wznowienia wyciągu
void start_lift(int sig) {
    printf("[Sygnał] Wyciąg wznowił działanie.\n");
    // Logika wznowienia wyciągu
}

// Inicjalizacja semaforów
void init_semaphores() {
    sem_peron = semget(IPC_PRIVATE, 1, IPC_CREAT | 0666);
    sem_chairs = semget(IPC_PRIVATE, 1, IPC_CREAT | 0666);
    sem_children = semget(IPC_PRIVATE, 1, IPC_CREAT | 0666);

    semctl(sem_peron, 0, SETVAL, MAX_PERON);
    semctl(sem_chairs, 0, SETVAL, ACTIVE_CHAIRS);
    semctl(sem_children, 0, SETVAL, 2); // Maksymalnie 2 dzieci na opiekuna
}

// Operacja P na semaforze
void P(int sem_id) {
    struct sembuf sb = {0, -1, 0};
    semop(sem_id, &sb, 1);
}

// Operacja V na semaforze
void V(int sem_id) {
    struct sembuf sb = {0, 1, 0};
    semop(sem_id, &sb, 1);
}

// Funkcja logująca akcje
void log_action(int id, int action) {
    time_t now = time(NULL);
    fprintf(log_file, "ID: %d, Akcja: %d, Czas: %s", id, action, ctime(&now));
    fflush(log_file);
}

// Funkcja do określania typu karnetu i ceny na podstawie wieku i statusu
int determine_ticket(int age, int is_vip) {
    int ticket_type;
    if (age < 12 || age > 65) {
        ticket_type = rand() % 3 + 1; // Wybierz TK1, TK2 lub TK3
        printf("Kasjer: Sprzedano bilet ulgowy (typ %d) narciarzowi w wieku %d.\n", ticket_type, age);
    } else {
        ticket_type = DAILY; // Domyślnie karnet dzienny
        printf("Kasjer: Sprzedano bilet dzienny narciarzowi w wieku %d.\n", age);
    }

    if (is_vip) {
        printf("Kasjer: Narciarz VIP obsłużony priorytetowo.\n");
    }

    return ticket_type;
}

// Proces kasjera
void kasjer() {
    printf("Kasjer: Gotowy do sprzedaży biletów.\n");
    while (1) {
        struct ticket_request req;

        // Odbierz żądanie od narciarza
        if (msgrcv(msg_queue, &req, sizeof(req) - sizeof(long), 1, 0) == -1) {
            perror("Kasjer: Błąd odbierania żądania");
            continue;
        }

        printf("Kasjer: Otrzymano żądanie od narciarza %d (wiek: %d, VIP: %d).\n", req.id, req.age, req.is_vip);

        // Wygeneruj karnet
        req.ticket_type = determine_ticket(req.age, req.is_vip);
        req.mtype = 2; // Typ komunikatu odpowiedzi

        // Wyślij odpowiedź do narciarza
        if (msgsnd(msg_queue, &req, sizeof(req) - sizeof(long), 0) == -1) {
            perror("Kasjer: Błąd wysyłania odpowiedzi");
        }

        printf("Kasjer: Wysłano karnet typu %d do narciarza %d.\n", req.ticket_type, req.id);
    }
}

// Proces narciarza
void narciarz(Narciarz *n) {
    sleep(rand() % 5 + 1); // Symulacja czasu przybycia

    if (!is_lift_active()) {
        printf("Narciarz %d: Wyciąg nieczynny. Opuszcza stację.\n", n->id);
        exit(0);
    }

    printf("Narciarz %d: Przybył na stację.\n", n->id);

    // Przygotuj żądanie do kasjera
    struct ticket_request req;
    req.mtype = 1; // Typ komunikatu żądania
    req.id = n->id;
    req.age = n->age;
    req.is_vip = n->is_vip;

    // Wyślij żądanie do kasjera
    if (msgsnd(msg_queue, &req, sizeof(req) - sizeof(long), 0) == -1) {
        perror("Narciarz: Błąd wysyłania żądania");
        exit(1);
    }

    printf("Narciarz %d: Wysłał żądanie zakupu karnetu.\n", n->id);

    // Odbierz odpowiedź od kasjera
    if (msgrcv(msg_queue, &req, sizeof(req) - sizeof(long), 2, 0) == -1) {
        perror("Narciarz: Błąd odbierania odpowiedzi");
        exit(1);
    }

    n->ticket_type = req.ticket_type;
    printf("Narciarz %d: Otrzymał karnet typu %d.\n", n->id, n->ticket_type);

    // Reszta funkcji narciarz...
    if (n->ticket_type > 0) {
        printf("Narciarz %d: Czeka na wejście na peron.\n", n->id);
        if (n->is_vip) {
            printf("Narciarz %d (VIP): Pomija kolejkę.\n", n->id);
        } else {
            P(sem_peron); // Czekaj na miejsce na peronie
        }

        if (n->age >= 4 && n->age <= 8) {
            n->is_child = 1;
            P(sem_children); // Czekaj na opiekuna
            printf("Narciarz %d (dziecko): Czeka na opiekuna.\n", n->id);
        }

        // Przechodzenie przez bramkę
        printf("Narciarz %d: Przechodzi przez bramkę, skanując karnet typu %d.\n", n->id, n->ticket_type);
        log_action(n->id, 1); // Log wejścia na peron

        P(sem_chairs); // Czekaj na wolne krzesełko
        printf("Narciarz %d: Wsiada na wyciąg.\n", n->id);
        log_action(n->id, 2); // Log wsiadania na wyciąg

        // Symulacja czasu podróży wyciągiem
        sleep(rand() % 3 + 1);

        // Wybór trasy zjazdu
        int trasa = rand() % 3 + 1; // Losowo wybierz trasę (1, 2 lub 3)
        printf("Narciarz %d: Zjeżdża trasą %d.\n", n->id, trasa);
        log_action(n->id, 3); // Log zjazdu trasą

        // Symulacja czasu zjazdu
        switch (trasa) {
            case 1:
                sleep(2); // Krótki czas zjazdu dla trasy 1
                break;
            case 2:
                sleep(4); // Średni czas zjazdu dla trasy 2
                break;
            case 3:
                sleep(6); // Długi czas zjazdu dla trasy 3
                break;
        }

        printf("Narciarz %d: Dotarł do górnej stacji.\n", n->id);
        log_action(n->id, 4); // Log dotarcia na górę

        V(sem_chairs); // Zwolnij krzesełko

        if (n->is_child) {
            V(sem_children); // Zwolnij opiekuna
        }

        if (!n->is_vip) {
            V(sem_peron); // Opuść peron
        }

        n->zjazdy++; // Zwiększ liczbę zjazdów
    } else {
        printf("Turysta %d: Odwiedza stację bez korzystania z wyciągu.\n", n->id);
    }

    exit(0);
}

// Proces pracownika
void pracownik(int station) {
    signal(SIGUSR1, stop_lift);
    signal(SIGUSR2, start_lift);

    printf("Pracownik %d: Zarządza stacją.\n", station);
    while (1) {
        pause(); // Czekaj na sygnały
    }
}

// Funkcja do zamykania wyciągu
void close_lift() {
    printf("Wyciąg: Zamykanie wyciągu.\n");
    // Logika transportu pozostałych osób na górę
    sleep(5); // Symulacja czasu transportu
    printf("Wyciąg: Wyłączony.\n");
}

// Funkcja do generowania raportu
void generate_report() {
    fclose(log_file);

    FILE *read_log = fopen("lift_log.txt", "r");
    if (!read_log) {
        perror("Nie udało się otworzyć pliku logów.");
        return;
    }

    printf("\n--- Raport dzienny ---\n");
    char line[256];
    while (fgets(line, sizeof(line), read_log)) {
        printf("%s", line);
    }

    fclose(read_log);
}

int main() {
    srand(time(NULL));
    init_semaphores();

    // Otwórz plik logów
    log_file = fopen("lift_log.txt", "w");
    if (!log_file) {
        perror("Nie udało się otworzyć pliku logów.");
        return 1;
    }

    // Utwórz kolejkę komunikatów
    msg_queue = msgget(IPC_PRIVATE, IPC_CREAT | 0666);
    if (msg_queue == -1) {
        perror("Nie udało się utworzyć kolejki komunikatów.");
        return 1;
    }

    // Uruchom procesy kasjera i pracowników
    if (fork() == 0) {
        kasjer();
        exit(0);
    }

    if (fork() == 0) {
        pracownik(1); // Pracownik na dolnej stacji
        exit(0);
    }

    if (fork() == 0) {
        pracownik(2); // Pracownik na górnej stacji
        exit(0);
    }

    // Generuj narciarzy
    for (int i = 0; i < 50; i++) {
        if (fork() == 0) {
            Narciarz n;
            n.id = i;
            n.age = rand() % 70 + 4;
            n.is_vip = rand() % 5 == 0; // 20% szans na VIP
            n.zjazdy = 0;
            n.is_child = 0;
            n.has_adult = 0;

            narciarz(&n);
        }
    }

    // Czekaj na zakończenie procesów potomnych
    while (wait(NULL) > 0);

    // Zamknij wyciąg po godzinach pracy
    close_lift();

    // Wygeneruj raport
    generate_report();

    // Posprzątaj zasoby
    semctl(sem_peron, 0, IPC_RMID);
    semctl(sem_chairs, 0, IPC_RMID);
    semctl(sem_children, 0, IPC_RMID);
    msgctl(msg_queue, IPC_RMID, NULL);

    printf("Symulacja zakończona.\n");
    return 0;
}