# TVCnet: protokoll

TVC programíróknak. Félduplex: a TVC küld egy kérést, a TVCnet válaszol; a TVCnet magától soha nem küld semmit.

## Vonal

| Irány | Jel | Beállítás |
|---|---|---|
| TVC -> TVCnet | UART a motor lábon: `OUT (05h)` b7 (CH12) vagy b6 (CH13) | 2400 baud, 8N1, LSB először; a bitbe írt 1 = 0 V (start bit: 1, stop bit: 0) |
| TVCnet -> TVC | félperiódus-kód a magnó bemeneten: `IN (59h)` b5 | minden félperiódus egy bit: T = 1, 2T = 0; T = 250 µs; LSB először |

- A 05h többi bitjét (hang) őrizd meg, és frissítsd a 0B12h árnyékmásolatot is.
- A válasz a kérés utolsó byte-ja után 2 ms-mal indul: 32 db 1-es bit (ebből mérhető T, a küszöb 1,5T), aztán a SYNC byte, majd a keret, a végén néhány 1-es bit.
- A kérés két byte-ja között legfeljebb 20 ms telhet el, különben a TVCnet eldobja.

## Keret

| Mező | Méret | Tartalom |
|---|---|---|
| SYNC | 1 | A5h (TVC -> TVCnet), 6Ah (TVCnet -> TVC) |
| LEN | 1 | a DATA hossza, 0-255 |
| CTRL | 1 | b7: váltakozó bit; b6-b0: parancs vagy állapot |
| DATA | LEN | adat |
| CHK | 1 | LEN + CTRL + DATA 8 bites összege |

A DATA-ban bármilyen byte lehet, nincs escape.

## Váltakozó bit (CTRL b7)

- Minden jó válasz után billentsd át (a `BUSY` után is).
- Hibás vagy hiányzó válasz után küldd újra ugyanazzal a bittel: a TVCnet nem hajtja végre újra, csak megismétli a választ.
- A program elején küldj egy `PING`-et.

## Parancsok

| Kód | Parancs | DATA | Válasz |
|---|---|---|---|
| 01h | `PING` | - | `OK` + 1 byte: protokoll verzió (01h) |
| 02h | `ECHO` | bármi | `OK` + ugyanaz |
| 03h | `STATUS` | - | `OK` + WiFi (1 byte: 0/1), IPv4 (4 byte), feladat állapota (1 byte), teljes hossz (4 byte, alsó byte először; FFFFFFFFh: ismeretlen) |
| 04h | `WIFI` | `SSID,jelszó` | `OK` / hiba; elmenti, indításkor csatlakozik |
| 10h | `GET` | URL (`http://` vagy `https://`) | `OK` (elindítva) / hiba |
| 11h | `NEXT` | - vagy 1 byte: legfeljebb ennyit kér | `BUSY` / `DATA` / `END` / `NONE` / hiba |
| 12h | `CLOSE` | - | `OK` |

`GET` után `NEXT`-tel olvasd a választ: `BUSY` = várj és kérd újra, `DATA` = egy darab, jön még, `END` = az utolsó darab (lehet üres).

## Állapotkódok

| Kód | Állapot |
|---|---|
| 00h | `OK` |
| 01h | `NONE`: nincs futó feladat |
| 02h | `BUSY` |
| 03h | `DATA` |
| 04h | `END` |
| 40h | hiba: ismeretlen parancs |
| 41h | hiba: hibás paraméter |
| 42h | hiba: nincs WiFi |
| 43h | hiba: DNS |
| 44h | hiba: kapcsolódás |
| 45h | hiba: HTTP; DATA: a státuszkód (2 byte, alsó byte először) |
| 46h | hiba: megszakadt kapcsolat / időtúllépés |

40h-tól felfelé minden hiba.
