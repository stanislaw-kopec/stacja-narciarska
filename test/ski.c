#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <fcntl.h>
#include <stdarg.h>

// Definicje stałych
#define MAX_PEOPLE_ON_PLATFORM 120     // Maksymalna liczba osób na platformie
#define MAX_CHAIRS 80                 // Maksymalna liczba krzeseł na wyciągu
#define MAX_SKIERS_PER_CHAIR 3        // Maksymalna liczba narciarzy na jednym krześle
#define NUM_GATES 4                   // Liczba bramek wejściowych
#define NUM_EXIT_PATHS 2              // Liczba ścieżek wyjazdowych
#define SHM_KEY 1234                  // Klucz dla pamięci dzielonej
#define MSG_KEY 5678                  // Klucz dla kolejki komunikatów

// Czasy zjazdu narciarzy (w minutach)
#define T1 1  // Krótka trasa (1 minuta)
#define T2 2  // Średnia trasa (2 minuty)
#define T3 3  // Długa trasa (3 minuty)

// Ceny biletów
#define DAILY_TICKET_PRICE 100  // Cena biletu dziennego
#define TK1_PRICE 50            // Cena biletu typu 1
#define TK2_PRICE 75            // Cena biletu typu 2
#define TK3_PRICE 90            // Cena biletu typu 3

// Struktura reprezentująca narciarza
typedef struct {
    int id;             // Unikalny identyfikator narciarza
    int age;            // Wiek narciarza
    int is_vip;         // Czy narciarz jest VIP-em
    int is_child;       // Czy narciarz jest dzieckiem
    int is_senior;      // Czy narciarz jest seniorem
    int ticket_type;    // Typ biletu (0: dzienny, 1: TK1, 2: TK2, 3: TK3)
    time_t entry_time;  // Czas wejścia na stok
    int is_adult;       // Czy narciarz jest dorosły
    int children_count; // Liczba dzieci przypisanych do dorosłego
    int is_frozen;      // Czy narciarz jest zamrożony (1: tak, 0: nie)
} Skier;

// Struktura reprezentująca krzesło na wyciągu
typedef struct {
    int id;             // Unikalny identyfikator krzesła
    int is_occupied;    // Czy krzesło jest zajęte
    Skier skiers[MAX_SKIERS_PER_CHAIR]; // Narciarze na krześle
} Chair;

// Struktura reprezentująca wyciąg krzesełkowy
typedef struct {
    Chair chairs[MAX_CHAIRS];  // Tablica krzeseł
    int available_chairs;      // Liczba dostępnych krzeseł
    int occupied_chairs;       // Liczba zajętych krzeseł
    sem_t chair_sem;           // Semafor do zarządzania dostępnymi krzesłami
    pthread_mutex_t chair_mutex; // Mutex do synchronizacji dostępu do krzeseł
} ChairLift;

// Struktura reprezentująca platformę (dolną lub górną)
typedef struct {
    Skier skiers[MAX_PEOPLE_ON_PLATFORM]; // Narciarze na platformie
    int count;                            // Liczba narciarzy na platformie
    pthread_mutex_t platform_mutex;       // Mutex do synchronizacji dostępu do platformy
    pthread_cond_t platform_cond;         // Zmienna warunkowa do synchronizacji platformy
} Platform;

// Struktura reprezentująca bramkę wejściową
typedef struct {
    int id;                     // Unikalny identyfikator bramki
    pthread_mutex_t gate_mutex; // Mutex do synchronizacji dostępu do bramki
} Gate;

// Struktura reprezentująca ścieżkę wyjazdową
typedef struct {
    int id;                     // Unikalny identyfikator ścieżki
    pthread_mutex_t exit_mutex; // Mutex do synchronizacji dostępu do ścieżki
} ExitPath;

// Struktura reprezentująca kolejkę narciarzy
typedef struct {
    Skier skiers[MAX_PEOPLE_ON_PLATFORM]; // Narciarze w kolejce
    int count;                            // Liczba narciarzy w kolejce
    pthread_mutex_t queue_mutex;          // Mutex do synchronizacji dostępu do kolejki
    sem_t queue_sem;                      // Semafor do zarządzania kolejką
} SkierQueue;

// Struktura reprezentująca bilet
typedef struct {
    int id;       // Unikalny identyfikator biletu
    int is_valid; // Czy bilet jest ważny
} Ticket;

// Struktura reprezentująca rekord przejścia przez bramkę
typedef struct {
    int ticket_id;    // ID biletu
    time_t entry_time; // Czas wejścia
} GateRecord;

// Struktura reprezentująca liczbę zjazdów dla każdego karnetu
typedef struct {
    int ticket_id;
    int ride_count;
} TicketRideCount;

// Struktura dla kolejki komunikatów
typedef struct {
    long mtype;       // Typ komunikatu
    int skier_id;     // ID narciarza
    char message[100]; // Wiadomość
} MsgBuffer;

// Globalne zmienne
ChairLift chair_lift;               // Wyciąg krzesełkowy
Platform lower_platform;            // Dolna platforma
Platform upper_platform;            // Górna platforma
Gate gates[NUM_GATES];              // Bramki wejściowe
ExitPath exit_paths[NUM_EXIT_PATHS]; // Ścieżki wyjazdowe
SkierQueue skier_queue;             // Kolejka narciarzy
Ticket active_tickets[MAX_PEOPLE_ON_PLATFORM]; // Aktywne bilety
int active_tickets_count = 0;       // Liczba aktywnych biletów
pthread_mutex_t tickets_mutex;      // Mutex do synchronizacji dostępu do biletów
GateRecord gate_records[MAX_PEOPLE_ON_PLATFORM]; // Rekordy przejść przez bramki
int gate_records_count = 0;         // Liczba rekordów przejść
pthread_mutex_t gate_records_mutex; // Mutex do synchronizacji dostępu do rekordów
TicketRideCount ticket_ride_counts[MAX_PEOPLE_ON_PLATFORM]; // Liczba zjazdów dla karnetów
int ticket_ride_counts_count = 0;   // Liczba rekordów zjazdów
pthread_mutex_t ticket_ride_counts_mutex; // Mutex do synchronizacji dostępu do rekordów zjazdów
int are_gates_open = 1;             // Czy bramki są otwarte
int is_lift_stopped = 0;            // Czy wyciąg jest zatrzymany
pthread_mutex_t lift_stop_mutex;    // Mutex do synchronizacji flagi zatrzymania
sem_t lift_freeze_sem;              // Semafor do zamrażania/odmrażania narciarzy
int shmid;                          // ID segmentu pamięci dzielonej
int msqid;                          // ID kolejki komunikatów
pthread_cond_t lift_resumed_cond;   // Zmienna warunkowa do sygnalizacji wznowienia wyciągu
pthread_mutex_t lift_resumed_mutex; // Mutex do synchronizacji dostępu do zmiennej warunkowej
int remaining_skiers = 0;           // Liczba narciarzy na stoku
pthread_mutex_t remaining_skiers_mutex; // Mutex do synchronizacji dostępu do remaining_skiers

// Funkcja do drukowania aktualnego czasu
void print_time() {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char buffer[20];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
    printf("[%s] ", buffer);
}

// Funkcja do drukowania kolorowego tekstu
void print_colored(const char* color, const char* format, ...) {
    va_list args;
    va_start(args, format);
    printf("%s", color);
    vprintf(format, args);
    printf("\033[0m");
    va_end(args);
}

// Funkcja obsługująca sygnały (zatrzymanie/wznowienie wyciągu)
void handle_signal(int signal) {
    print_time();
    if (signal == SIGUSR1) {
        printf("Wyciąg krzesełkowy zatrzymany przez pracownika.\n");
    } else if (signal == SIGUSR2) {
        printf("Wyciąg krzesełkowy wznowiony.\n");
    }
}

// Inicjalizacja wyciągu krzesełkowego
void initialize_chair_lift() {
    chair_lift.available_chairs = MAX_CHAIRS;
    chair_lift.occupied_chairs = 0;
    if (sem_init(&chair_lift.chair_sem, 0, MAX_CHAIRS) == -1) {
        print_time();
        perror("Nie udało się zainicjować semafora");
        exit(EXIT_FAILURE);
    }
    pthread_mutex_init(&chair_lift.chair_mutex, NULL);
    for (int i = 0; i < MAX_CHAIRS; i++) {
        chair_lift.chairs[i].id = i;
        chair_lift.chairs[i].is_occupied = 0;
    }
}

// Inicjalizacja platformy
void initialize_platform(Platform *platform) {
    platform->count = 0;
    pthread_mutex_init(&platform->platform_mutex, NULL);
    pthread_cond_init(&platform->platform_cond, NULL);
}

// Inicjalizacja bramek
void initialize_gates() {
    for (int i = 0; i < NUM_GATES; i++) {
        gates[i].id = i + 1;
        pthread_mutex_init(&gates[i].gate_mutex, NULL);
    }
}

// Inicjalizacja ścieżek wyjazdowych
void initialize_exit_paths() {
    for (int i = 0; i < NUM_EXIT_PATHS; i++) {
        exit_paths[i].id = i + 1;
        pthread_mutex_init(&exit_paths[i].exit_mutex, NULL);
    }
}

// Inicjalizacja kolejki narciarzy
void initialize_skier_queue() {
    skier_queue.count = 0;
    pthread_mutex_init(&skier_queue.queue_mutex, NULL);
    sem_init(&skier_queue.queue_sem, 0, 0);
}

// Inicjalizacja biletów
void initialize_tickets() {
    pthread_mutex_init(&tickets_mutex, NULL);
    active_tickets_count = 0;
}

// Inicjalizacja rekordów przejść przez bramki
void initialize_gate_records() {
    pthread_mutex_init(&gate_records_mutex, NULL);
    gate_records_count = 0;
}

// Inicjalizacja rekordów zjazdów
void initialize_ticket_ride_counts() {
    pthread_mutex_init(&ticket_ride_counts_mutex, NULL);
    ticket_ride_counts_count = 0;
}

// Inicjalizacja semafora do zamrażania/odmrażania narciarzy
void initialize_freeze_semaphore() {
    if (sem_init(&lift_freeze_sem, 0, 0) == -1) {
        print_time();
        perror("Nie udało się zainicjować semafora zamrażania");
        exit(EXIT_FAILURE);
    }
}

// Inicjalizacja pamięci dzielonej
void initialize_shared_memory() {
    shmid = shmget(SHM_KEY, sizeof(int), 0666 | IPC_CREAT);
    if (shmid == -1) {
        perror("shmget failed");
        exit(EXIT_FAILURE);
    }
}

// Inicjalizacja kolejki komunikatów
void initialize_message_queue() {
    msqid = msgget(MSG_KEY, 0666 | IPC_CREAT);
    if (msqid == -1) {
        perror("msgget failed");
        exit(EXIT_FAILURE);
    }
}

// Inicjalizacja zmiennych warunkowych i mutexów
void initialize_conditions() {
    pthread_cond_init(&lift_resumed_cond, NULL);
    pthread_mutex_init(&lift_resumed_mutex, NULL);
}

// Dodanie biletu do listy aktywnych biletów
void add_ticket(int ticket_id) {
    pthread_mutex_lock(&tickets_mutex);
    if (active_tickets_count < MAX_PEOPLE_ON_PLATFORM) {
        active_tickets[active_tickets_count].id = ticket_id;
        active_tickets[active_tickets_count].is_valid = 1;
        active_tickets_count++;
    }
    pthread_mutex_unlock(&tickets_mutex);
}

// Sprawdzenie, czy bilet jest ważny
int is_ticket_valid(int ticket_id) {
    pthread_mutex_lock(&tickets_mutex);
    for (int i = 0; i < active_tickets_count; i++) {
        if (active_tickets[i].id == ticket_id && active_tickets[i].is_valid) {
            pthread_mutex_unlock(&tickets_mutex);
            return 1;
        }
    }
    pthread_mutex_unlock(&tickets_mutex);
    return 0;
}

// Dodanie narciarza do kolejki
void add_skier_to_queue(Skier skier) {
    pthread_mutex_lock(&skier_queue.queue_mutex);
    if (skier_queue.count < MAX_PEOPLE_ON_PLATFORM) {
        skier_queue.skiers[skier_queue.count++] = skier;
        sem_post(&skier_queue.queue_sem);
        print_time();
        print_colored("\033[33m", "Narciarz %d dodany do kolejki.\n", skier.id);
    } else {
        print_time();
        print_colored("\033[33m", "Kolejka jest pełna. Narciarz %d nie może zostać dodany.\n", skier.id);
    }
    pthread_mutex_unlock(&skier_queue.queue_mutex);
}

// Pobranie narciarza z kolejki
Skier get_skier_from_queue() {
    sem_wait(&skier_queue.queue_sem);
    pthread_mutex_lock(&skier_queue.queue_mutex);
    if (skier_queue.count == 0) {
        pthread_mutex_unlock(&skier_queue.queue_mutex);
        Skier empty_skier = {0};
        return empty_skier;
    }
    Skier skier = skier_queue.skiers[--skier_queue.count];
    pthread_mutex_unlock(&skier_queue.queue_mutex);
    return skier;
}

// Dodanie rekordu przejścia przez bramkę
void add_gate_record(int ticket_id) {
    pthread_mutex_lock(&gate_records_mutex);
    if (gate_records_count < MAX_PEOPLE_ON_PLATFORM) {
        gate_records[gate_records_count].ticket_id = ticket_id;
        gate_records[gate_records_count].entry_time = time(NULL);
        gate_records_count++;
    }
    pthread_mutex_unlock(&gate_records_mutex);
}

// Dodanie rekordu zjazdu dla karnetu
void add_ticket_ride_count(int ticket_id) {
    pthread_mutex_lock(&ticket_ride_counts_mutex);
    for (int i = 0; i < ticket_ride_counts_count; i++) {
        if (ticket_ride_counts[i].ticket_id == ticket_id) {
            ticket_ride_counts[i].ride_count++;
            pthread_mutex_unlock(&ticket_ride_counts_mutex);
            return;
        }
    }
    if (ticket_ride_counts_count < MAX_PEOPLE_ON_PLATFORM) {
        ticket_ride_counts[ticket_ride_counts_count].ticket_id = ticket_id;
        ticket_ride_counts[ticket_ride_counts_count].ride_count = 1;
        ticket_ride_counts_count++;
    }
    pthread_mutex_unlock(&ticket_ride_counts_mutex);
}

// Funkcja wątku kasjera
void* kasjer(void* arg) {
    while (1) {
        // Sprawdź, czy bramki są otwarte
        if (!are_gates_open) {
            print_time();
            print_colored("\033[34m", "Kasjer: Stok zamknięty. Sprzedaż biletów wstrzymana.\n");
            sleep(1); // Czekaj przed ponownym sprawdzeniem
            continue; // Nie sprzedawaj biletów, jeśli stok jest zamknięty
        }

        Skier skier;
        skier.id = rand() % 1000;
        skier.age = rand() % 80;
        skier.is_vip = rand() % 2;
        skier.is_child = skier.age < 12;
        skier.is_senior = skier.age > 65;
        skier.ticket_type = rand() % 4;
        skier.entry_time = time(NULL);
        skier.is_adult = skier.age >= 18;
        skier.children_count = 0;
        skier.is_frozen = 0; // Narciarz początkowo nie jest zamrożony

        int price = 0;
        switch (skier.ticket_type) {
            case 0: price = DAILY_TICKET_PRICE; break;
            case 1: price = TK1_PRICE; break;
            case 2: price = TK2_PRICE; break;
            case 3: price = TK3_PRICE; break;
        }

        if (skier.is_child || skier.is_senior) {
            price = price * 0.75;
        }

        char buffer[256];
        snprintf(buffer, sizeof(buffer), "Kasjer: Narciarz %d (wiek: %d) kupił bilet typu %d za %d PLN.\n", skier.id, skier.age, skier.ticket_type, price);
        print_time();
        print_colored("\033[34m", buffer); // Niebieski tekst

        add_ticket(skier.id);
        add_skier_to_queue(skier);

        sleep(rand() % 3);
    }
    return NULL;
}

// Funkcja wątku bramki
void* bramka(void* arg) {
    Gate* gate = (Gate*)arg;
    while (are_gates_open) {
        Skier skier = get_skier_from_queue();

        // Sprawdź, czy wyciąg jest zatrzymany
        pthread_mutex_lock(&lift_stop_mutex);
        if (is_lift_stopped) {
            pthread_mutex_unlock(&lift_stop_mutex);
            print_time();
            print_colored("\033[33m", "Bramka %d: Wyciąg zatrzymany. Narciarz %d nie może wejść na krzesełko.\n", gate->id, skier.id);
            continue;
        }
        pthread_mutex_unlock(&lift_stop_mutex);

        if (skier.is_vip) {
            pthread_mutex_lock(&gate->gate_mutex);
            print_time();
            print_colored("\033[33m", "Bramka %d: VIP Narciarz %d wszedł na platformę bez czekania.\n", gate->id, skier.id);
            add_gate_record(skier.id);
            pthread_mutex_lock(&lower_platform.platform_mutex);
            if (lower_platform.count < MAX_PEOPLE_ON_PLATFORM) {
                lower_platform.skiers[lower_platform.count++] = skier;
                print_time();
                print_colored("\033[33m", "Bramka %d: VIP Narciarz %d dodany na platformę.\n", gate->id, skier.id);
            } else {
                print_time();
                print_colored("\033[33m", "Bramka %d: Platforma jest pełna. VIP Narciarz %d musi czekać.\n", gate->id, skier.id);
            }
            pthread_mutex_unlock(&lower_platform.platform_mutex);
            pthread_mutex_unlock(&gate->gate_mutex);
            continue;
        }

        if (skier.is_child && skier.age >= 4 && skier.age <= 8) {
            pthread_mutex_lock(&lower_platform.platform_mutex);
            int found_adult = 0;
            for (int i = 0; i < lower_platform.count; i++) {
                if (lower_platform.skiers[i].is_adult && lower_platform.skiers[i].children_count < 2) {
                    lower_platform.skiers[i].children_count++;
                    found_adult = 1;
                    break;
                }
            }
            if (!found_adult) {
                print_time();
                print_colored("\033[33m", "Bramka %d: Narciarz %d (dziecko) nie może wejść bez opiekuna.\n", gate->id, skier.id);
                pthread_mutex_unlock(&lower_platform.platform_mutex);
                continue;
            }
            pthread_mutex_unlock(&lower_platform.platform_mutex);
        }

        pthread_mutex_lock(&gate->gate_mutex);
        print_time();
        print_colored("\033[33m", "Bramka %d: Narciarz %d wszedł na platformę.\n", gate->id, skier.id);
        add_gate_record(skier.id);
        pthread_mutex_lock(&lower_platform.platform_mutex);
        if (lower_platform.count < MAX_PEOPLE_ON_PLATFORM) {
            lower_platform.skiers[lower_platform.count++] = skier;
            print_time();
            print_colored("\033[33m", "Bramka %d: Narciarz %d dodany na platformę.\n", gate->id, skier.id);
        } else {
            print_time();
            print_colored("\033[33m", "Bramka %d: Platforma jest pełna. Narciarz %d musi czekać.\n", gate->id, skier.id);
            add_skier_to_queue(skier);
        }
        pthread_mutex_unlock(&lower_platform.platform_mutex);
        pthread_mutex_unlock(&gate->gate_mutex);

        sleep(rand() % 2);
    }
    print_time();
    print_colored("\033[33m", "Bramka %d: Zamknięta.\n", gate->id);
    return NULL;
}

// Funkcja wątku pracownika1 (stacja dolna)
void* pracownik1(void* arg) {
    while (1) {
        if (rand() % 10 == 0) { // 10% szans na awarię
            pthread_mutex_lock(&lift_stop_mutex);
            if (!is_lift_stopped) { // Tylko jeśli wyciąg nie jest już zatrzymany
                is_lift_stopped = 1; // Zatrzymanie wyciągu
                pthread_mutex_unlock(&lift_stop_mutex);

                char buffer[256];
                snprintf(buffer, sizeof(buffer), "\033[32mPracownik1 (stacja dolna): Awaria! Zatrzymanie wyciągu.\033[0m\n");
                print_time();
                printf("%s", buffer);

                kill(getpid(), SIGUSR1); // Wysyłanie sygnału awarii

                // Komunikacja z pracownikiem2
                print_time();
                printf("\033[32mPracownik1 (stacja dolna): Wysyłanie komunikatu do pracownika2.\033[0m\n");
                sleep(2); // Symulacja czasu komunikacji

                print_time();
                printf("\033[32mPracownik1 (stacja dolna): Otrzymano potwierdzenie od pracownika2. Wznawianie wyciągu.\033[0m\n");

                pthread_mutex_lock(&lift_stop_mutex);
                is_lift_stopped = 0; // Wznowienie wyciągu
                pthread_cond_broadcast(&lift_resumed_cond); // Powiadom wszystkich czekających narciarzy
                pthread_mutex_unlock(&lift_stop_mutex);

                snprintf(buffer, sizeof(buffer), "\033[32mPracownik1 (stacja dolna): Awaria naprawiona. Uruchamianie wyciągu.\033[0m\n");
                print_time();
                printf("%s", buffer);

                kill(getpid(), SIGUSR2); // Wysyłanie sygnału wznowienia
            } else {
                pthread_mutex_unlock(&lift_stop_mutex);
            }
        }
        sleep(2); // Czas między sprawdzaniem awarii
    }
    return NULL;
}

// Funkcja wątku pracownika2 (stacja górna)
void* pracownik2(void* arg) {
    while (1) {
        if (rand() % 10 == 0) { // 10% szans na awarię
            pthread_mutex_lock(&lift_stop_mutex);
            if (!is_lift_stopped) { // Tylko jeśli wyciąg nie jest już zatrzymany
                is_lift_stopped = 1; // Zatrzymanie wyciągu
                pthread_mutex_unlock(&lift_stop_mutex);

                char buffer[256];
                snprintf(buffer, sizeof(buffer), "\033[32mPracownik2 (stacja górna): Awaria! Zatrzymanie wyciągu.\033[0m\n");
                print_time();
                printf("%s", buffer);

                kill(getpid(), SIGUSR1); // Wysyłanie sygnału awarii

                // Komunikacja z pracownikiem1
                print_time();
                printf("\033[32mPracownik2 (stacja górna): Wysyłanie komunikatu do pracownika1.\033[0m\n");
                sleep(2); // Symulacja czasu komunikacji

                print_time();
                printf("\033[32mPracownik2 (stacja górna): Otrzymano potwierdzenie od pracownika1. Wznawianie wyciągu.\033[0m\n");

                pthread_mutex_lock(&lift_stop_mutex);
                is_lift_stopped = 0; // Wznowienie wyciągu
                pthread_cond_broadcast(&lift_resumed_cond); // Powiadom wszystkich czekających narciarzy
                pthread_mutex_unlock(&lift_stop_mutex);

                snprintf(buffer, sizeof(buffer), "\033[32mPracownik2 (stacja górna): Awaria naprawiona. Uruchamianie wyciągu.\033[0m\n");
                print_time();
                printf("%s", buffer);

                kill(getpid(), SIGUSR2); // Wysyłanie sygnału wznowienia
            } else {
                pthread_mutex_unlock(&lift_stop_mutex);
            }
        }
        sleep(2); // Czas między sprawdzaniem awarii
    }
    return NULL;
}

// Funkcja wątku narciarza
void* narciarz(void* arg) {
    Skier* skier = (Skier*)arg;

    // Zwiększ liczbę narciarzy na stoku
    pthread_mutex_lock(&remaining_skiers_mutex);
    remaining_skiers++;
    pthread_mutex_unlock(&remaining_skiers_mutex);

    print_time();
    print_colored("\033[33m", "Narciarz %d czeka na wyciąg krzesełkowy.\n", skier->id);

    // Czekaj, aż wyciąg zostanie wznowiony, zanim wsiądziesz
    pthread_mutex_lock(&lift_stop_mutex);
    while (is_lift_stopped) {
        skier->is_frozen = 1; // Oznacz narciarza jako zamrożonego
        print_time();
        print_colored("\033[33m", "Narciarz %d czeka na wznowienie wyciągu.\n", skier->id);
        pthread_cond_wait(&lift_resumed_cond, &lift_stop_mutex); // Czekaj na sygnał wznowienia
        skier->is_frozen = 0; // Oznacz narciarza jako odmrożonego
        print_time();
        print_colored("\033[33m", "Narciarz %d wznowił wjazd.\n", skier->id);
    }
    pthread_mutex_unlock(&lift_stop_mutex);

    sem_wait(&chair_lift.chair_sem);
    pthread_mutex_lock(&chair_lift.chair_mutex);
    if (chair_lift.available_chairs > 0) {
        chair_lift.available_chairs--;
        chair_lift.occupied_chairs++;
        pthread_mutex_unlock(&chair_lift.chair_mutex);
    } else {
        pthread_mutex_unlock(&chair_lift.chair_mutex);
        add_skier_to_queue(*skier);
        return NULL;
    }

    print_time();
    print_colored("\033[33m", "Narciarz %d jest na wyciągu krzesełkowym.\n", skier->id);

    // Symulacja przejazdu na górę
    int travel_time = 2; // Czas przejazdu na górę (2 minuty)
    int elapsed_time = 0;

    while (elapsed_time < travel_time) {
        // Sprawdź, czy wyciąg został zatrzymany
        pthread_mutex_lock(&lift_stop_mutex);
        if (is_lift_stopped) {
            skier->is_frozen = 1; // Oznacz narciarza jako zamrożonego
            print_time();
            print_colored("\033[33m", "Narciarz %d czeka na wyciągu (wyciąg zatrzymany).\n", skier->id);

            // Czekaj na sygnał wznowienia
            pthread_cond_wait(&lift_resumed_cond, &lift_stop_mutex);

            skier->is_frozen = 0; // Oznacz narciarza jako odmrożonego
            print_time();
            print_colored("\033[33m", "Narciarz %d kontynuuje wjazd (wyciąg wznowiony).\n", skier->id);
        }
        pthread_mutex_unlock(&lift_stop_mutex);

        if (!skier->is_frozen) {
            sleep(1); // Symulacja upływu czasu tylko jeśli narciarz nie jest zamrożony
            elapsed_time++;
        }
    }

    pthread_mutex_lock(&chair_lift.chair_mutex);
    chair_lift.available_chairs++;
    chair_lift.occupied_chairs--;
    sem_post(&chair_lift.chair_sem);
    pthread_mutex_unlock(&chair_lift.chair_mutex);

    print_time();
    print_colored("\033[33m", "Narciarz %d dotarł na górę.\n", skier->id);

    // Symulacja zjazdu
    int route = rand() % 3 + 1;
    print_time();
    print_colored("\033[33m", "Narciarz %d zjeżdża trasą %d.\n", skier->id, route);
    sleep(route == 1 ? T1 : (route == 2 ? T2 : T3));
    print_time();
    print_colored("\033[33m", "Narciarz %d zakończył zjazd.\n", skier->id);

    // Zwiększ liczbę zjazdów dla karnetu
    add_ticket_ride_count(skier->id);

    // Wybór drogi wyjazdowej
    int exit_path = rand() % NUM_EXIT_PATHS;
    pthread_mutex_lock(&exit_paths[exit_path].exit_mutex);
    print_time();
    print_colored("\033[33m", "Narciarz %d opuszcza stok przez ścieżkę %d.\n", skier->id, exit_path + 1);
    pthread_mutex_unlock(&exit_paths[exit_path].exit_mutex);

    // Zmniejsz liczbę narciarzy na stoku
    pthread_mutex_lock(&remaining_skiers_mutex);
    remaining_skiers--;
    pthread_mutex_unlock(&remaining_skiers_mutex);

    return NULL;
}

// Generowanie raportu dziennego
void generate_report() {
    FILE *report_file = fopen("raport_dzienny.txt", "w");
    if (report_file == NULL) {
        perror("Nie udało się otworzyć pliku raportu");
        return;
    }

    fprintf(report_file, "Raport dzienny:\n");
    fprintf(report_file, "Łączna liczba narciarzy: %d\n", gate_records_count);

    int vip_count = 0;
    for (int i = 0; i < gate_records_count; i++) {
        for (int j = 0; j < skier_queue.count; j++) {
            if (skier_queue.skiers[j].id == gate_records[i].ticket_id && skier_queue.skiers[j].is_vip) {
                vip_count++;
            }
        }
    }
    fprintf(report_file, "Łączna liczba VIP-ów: %d\n", vip_count);

    int child_count = 0;
    for (int i = 0; i < gate_records_count; i++) {
        for (int j = 0; j < skier_queue.count; j++) {
            if (skier_queue.skiers[j].id == gate_records[i].ticket_id && skier_queue.skiers[j].is_child) {
                child_count++;
            }
        }
    }
    fprintf(report_file, "Łączna liczba dzieci: %d\n", child_count);

    int senior_count = 0;
    for (int i = 0; i < gate_records_count; i++) {
        for (int j = 0; j < skier_queue.count; j++) {
            if (skier_queue.skiers[j].id == gate_records[i].ticket_id && skier_queue.skiers[j].is_senior) {
                senior_count++;
            }
        }
    }
    fprintf(report_file, "Łączna liczba seniorów: %d\n", senior_count);

    fprintf(report_file, "Łączna liczba przejazdów: %d\n", gate_records_count);

    fprintf(report_file, "Szczegółowe informacje o biletach:\n");
    for (int i = 0; i < gate_records_count; i++) {
        fprintf(report_file, "ID biletu: %d, Czas wejścia: %s", gate_records[i].ticket_id, ctime(&gate_records[i].entry_time));
    }

    fprintf(report_file, "Liczba zjazdów dla poszczególnych karnetów:\n");
    for (int i = 0; i < ticket_ride_counts_count; i++) {
        fprintf(report_file, "ID biletu: %d, Liczba zjazdów: %d\n", ticket_ride_counts[i].ticket_id, ticket_ride_counts[i].ride_count);
    }

    fclose(report_file);
}

// Czyszczenie zasobów
void cleanup() {
    sem_destroy(&chair_lift.chair_sem);
    pthread_mutex_destroy(&chair_lift.chair_mutex);
    pthread_mutex_destroy(&lower_platform.platform_mutex);
    pthread_mutex_destroy(&upper_platform.platform_mutex);
    for (int i = 0; i < NUM_GATES; i++) {
        pthread_mutex_destroy(&gates[i].gate_mutex);
    }
    for (int i = 0; i < NUM_EXIT_PATHS; i++) {
        pthread_mutex_destroy(&exit_paths[i].exit_mutex);
    }
    pthread_mutex_destroy(&skier_queue.queue_mutex);
    sem_destroy(&skier_queue.queue_sem);
    pthread_mutex_destroy(&tickets_mutex);
    pthread_mutex_destroy(&gate_records_mutex);
    pthread_mutex_destroy(&ticket_ride_counts_mutex);
    pthread_mutex_destroy(&lift_stop_mutex);
    sem_destroy(&lift_freeze_sem);
    pthread_cond_destroy(&lift_resumed_cond);
    pthread_mutex_destroy(&lift_resumed_mutex);
    shmctl(shmid, IPC_RMID, NULL); // Usuń pamięć dzieloną
    msgctl(msqid, IPC_RMID, NULL); // Usuń kolejkę komunikatów
    pthread_mutex_destroy(&remaining_skiers_mutex);
}

// Funkcja główna
int main() {
    srand(time(NULL));
    signal(SIGUSR1, handle_signal);
    signal(SIGUSR2, handle_signal);

    initialize_chair_lift();
    initialize_platform(&lower_platform);
    initialize_platform(&upper_platform);
    initialize_gates();
    initialize_exit_paths();
    initialize_skier_queue();
    initialize_tickets();
    initialize_gate_records();
    initialize_ticket_ride_counts();
    initialize_freeze_semaphore();
    initialize_shared_memory();
    initialize_message_queue();
    initialize_conditions();
    pthread_mutex_init(&lift_stop_mutex, NULL);
    pthread_mutex_init(&remaining_skiers_mutex, NULL);

    pthread_t kasjer_thread, pracownik1_thread, pracownik2_thread;
    pthread_t gate_threads[NUM_GATES];

    if (pthread_create(&kasjer_thread, NULL, kasjer, NULL) != 0) {
        print_time();
        perror("Nie udało się utworzyć wątku kasjera");
        exit(EXIT_FAILURE);
    }
    if (pthread_create(&pracownik1_thread, NULL, pracownik1, NULL) != 0) {
        print_time();
        perror("Nie udało się utworzyć wątku pracownika1");
        exit(EXIT_FAILURE);
    }
    if (pthread_create(&pracownik2_thread, NULL, pracownik2, NULL) != 0) {
        print_time();
        perror("Nie udało się utworzyć wątku pracownika2");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < NUM_GATES; i++) {
        if (pthread_create(&gate_threads[i], NULL, bramka, &gates[i]) != 0) {
            print_time();
            perror("Nie udało się utworzyć wątku bramki");
            exit(EXIT_FAILURE);
        }
    }

    time_t start_time = time(NULL);
    int Tp = 0;
    int Tk = 20;

    while (1) {
        time_t current_time = time(NULL);
        if (current_time - start_time >= Tp && current_time - start_time < Tk) {
            are_gates_open = 1;
        } else if (current_time - start_time >= Tk) {
            are_gates_open = 0;
            print_time();
            printf("Zamykanie wyciągu o godzinie %d\n", Tk);

            // Poczekaj, aż wszyscy narciarze zjadą na dół
            while (1) {
                pthread_mutex_lock(&remaining_skiers_mutex);
                if (remaining_skiers == 0) {
                    pthread_mutex_unlock(&remaining_skiers_mutex);
                    break;
                }
                pthread_mutex_unlock(&remaining_skiers_mutex);
                sleep(1); // Czekaj 1 sekundę przed ponownym sprawdzeniem
            }

            print_time();
            printf("Wszyscy narciarze zjechali na dół. Kończenie programu.\n");

            generate_report();
            cleanup();
            exit(0);
        }

        pthread_mutex_lock(&lower_platform.platform_mutex);
        if (lower_platform.count > 0) {
            Skier skier = lower_platform.skiers[--lower_platform.count];
            pthread_mutex_unlock(&lower_platform.platform_mutex);

            pthread_t narciarz_thread;
            if (pthread_create(&narciarz_thread, NULL, narciarz, &skier) != 0) {
                print_time();
                perror("Nie udało się utworzyć wątku narciarza");
            } else {
                pthread_detach(narciarz_thread);
            }
        } else {
            pthread_mutex_unlock(&lower_platform.platform_mutex);
        }
        sleep(1);
    }

    pthread_join(kasjer_thread, NULL);
    pthread_join(pracownik1_thread, NULL);
    pthread_join(pracownik2_thread, NULL);
    for (int i = 0; i < NUM_GATES; i++) {
        pthread_join(gate_threads[i], NULL);
    }

    return 0;
}