#ifndef TYPES_H
#define TYPES_H

// Struktura dla danych o karnetach
typedef struct {
    int id;
    int typ_karnetu; // 0 = czasowy, 1 = dzienny
    int zjazdy_wykonane;
} Karnet;

// Struktura dla krzesełek
typedef struct {
    int krzeselka[40]; // 1 = zajęte, 0 = wolne
} KolejLinowa;

// Struktura dla danych o peronie
typedef struct {
    int liczba_osob;
    int vip_na_peronie;
    int zwykli_na_peronie;
} Peron;

#endif // TYPES_H
