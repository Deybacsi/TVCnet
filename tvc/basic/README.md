# TVC BASIC példák

Egyszerű BASIC programok a TVCnet kipróbálásához. A TVCnet a CH12 aljzatban van (05h b7), 2400 baud, T = 250 µs.

| Program | Mit csinál |
|---|---|
| `PING` | `PING`, `ECHO`, `STATUS`: kiírja a protokoll verzióját, a visszakapott szöveget, a WiFi állapotát és az IP címet |
| `WIFI` | bekéri a WiFi nevét és jelszavát, elküldi a TVCnet-nek (`WIFI`), és megvárja a csatlakozást |
| `GET` | bekér egy URL-t (üresen: `http://example.com/`), letölti (`GET`, `NEXT`), és kiírja |
| `TERM` | egyszerű terminál a `TERM` paranccsal: Hayes AT parancsok (`ATI`, `ATDhost:port`, `+++`, `ATH`, `AT&Gurl`), lásd [PROTOKOLL.md](../../PROTOKOLL.md) |

- A `.CAS` fájlok betölthetők (`LOAD "PING"`, utána `RUN`); a `.bas` fájlok ugyanezek szövegként, begépeléshez.
- 1.2-es és 2.x-es ROM-mal is mennek; freeTVC-ben (2.2, a `PING` 1.2-vel is) kipróbálva, valódi TVCnet-tel még nem.

## Hogyan működik

- A TVCnet vonalát BASIC-ből nem lehet kezelni (2400 baud, 250 µs-os félperiódusok), ezért egy kis gépi kódú rutin (`helper.asm`, a `../tvcnet.inc` rutinjaival) végzi a kérést és a választ. A program a 9100-tól kezdődő DATA sorokból POKE-olja be a 6656-os (1A00h) címre.
- `1 LOMEM 8192` a BASIC programot 8192-re (2000h) teszi, így alatta hely marad a rutinnak és a puffereinek.
- A közös alprogramok (minden programban ugyanazok):
  - `GOSUB 8000`: kérés. `C` = a parancs kódja, `D$` = az adat; vissza: `Z` = az állapot (0 = `OK`, -1 = nincs válasz). A válasz a `W` címen van: `PEEK(W)` a hossza, az adat `W+2`-től.
  - `GOSUB 8100`: kiírja a válasz adatát (CR és LF új sort ad).
  - `GOSUB 8200`: kiírja `N`-et szóközök nélkül.
  - `GOSUB 9000`: betölti a gépi kódot, és küld egy `PING`-et.
- A TVC BASIC sajátosságai, amikre figyelni kell: a 18 karakternél hosszabb stringet dimenzionálni kell (`DIM U$*254`); az összefűzés jele a `&`; részstring: `A$(2:4)`; karakterkód: `ORD`; a `FOR I=1 TO 0` egyszer lefut.
- A `TERM` lassú gépelést bír: az `INKEY$` csak egy leütést tárol, és hosszú kiírás közben a leütések elvesznek. Kilépés: CTRL+ESC; a nyitott kapcsolatot előbb `+++` és `ATH` bontja.
- Vesszőt tartalmazó WiFi jelszóval a `WIFI` programot nem próbáltam ki.

## Újragenerálás

```sh
python3 make_bas.py <sjasmplus>
```

A `src/*.bas` a programok saját sorai; a `make_bas.py` hozzáteszi a közös alprogramokat és a gépi kódot DATA sorokként, az eredmény a `*.bas`. A `.CAS` fájlok freeTVC-ben készültek: a listát begépelve, `SAVE`-vel egy `--cas-dir` mappába.
