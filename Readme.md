# BreakLock Solver — strategia

## Jak działa

Program sonduje planszę numer po numerze, obserwując jak zmieniają się `hits` i `exact` po każdej próbie.

### Start

Tablica wypełniana jest pierwszymi *n* kolejnymi numerami, np. dla `LENGTH 4` i `NUMBERS 1 16` będzie to `[1, 2, 3, 4]`. Wysyłana jest pierwsza próba i zapamiętywane są zwrócone `hits` i `exact`.

### Pętla

W każdej turze program podmienia jeden slot na kolejny numer (`n++`) i porównuje wynik z poprzednim:

- **`exact` spadł** → ten numer był dobry, cofamy zmianę i trwale blokujemy slot (zapamiętujemy wartość).
- **`exact` wzrósł** → nowy numer pasuje na tę pozycję, blokujemy slot i przechodzimy do następnego.
- **`hits` spadł** → numer w ogóle nie należy do wzoru, cofamy i blokujemy slot (ale nie pozycję).
- **`hits` wzrósł** → numer należy do wzoru (choć nie na tej pozycji), blokujemy i przechodzimy dalej.
- **bez zmian** → podmień ten sam slot kolejnym numerem.

Zablokowane sloty są pomijane przy wyborze miejsca następnej zmiany.

### Gdy znamy już wszystkie numery

Gdy `hits == n`, program wie jakie numery tworzą wzór, ale nie zna kolejności. Wtedy iteruje przez kolejne permutacje (`std::next_permutation`) aż do trafienia.

### Błąd 101

Jeśli wstawiony numer już gdzieś w tablicy istnieje, serwer zwraca `ERROR 101`. Program pomija tę próbę (nie liczy się jako ruch) i wstawia kolejny numer.
