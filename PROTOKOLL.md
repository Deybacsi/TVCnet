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
| 01h | `PING` | - | `OK` + 1 byte: protokoll verzió (02h) |
| 02h | `ECHO` | bármi | `OK` + ugyanaz |
| 03h | `STATUS` | - | `OK` + WiFi (1 byte: 0/1), IPv4 (4 byte), feladat állapota (1 byte: `NONE`, `BUSY`, `OK` = TCP kapcsolat él, `DATA`, `END` vagy hibakód), teljes hossz (4 byte, alsó byte először; FFFFFFFFh: ismeretlen) |
| 04h | `WIFI` | `SSID,jelszó` | `OK` / hiba; elmenti, indításkor csatlakozik |
| 10h | `GET` | URL (`http://` vagy `https://`) | `OK` (elindítva) / hiba |
| 11h | `NEXT` | - vagy 1 byte: legfeljebb ennyit kér | `BUSY` / `DATA` / `END` / `NONE` / hiba |
| 12h | `CLOSE` | - | `OK` |
| 20h | `OPEN` | `host:port` | `OK` (elindítva) / hiba |
| 21h | `SEND` | adat | `OK` / `NONE` (nincs TCP kapcsolat) / `BUSY` (tele a küldési sor, küldd újra) / hiba |
| 30h | `TERM` | a begépelt byte-ok (lehet üres) | `OK` / `BUSY` / `DATA` + a képernyőre szánt byte-ok (lásd lent) |

`GET` után `NEXT`-tel olvasd a választ: `BUSY` = várj és kérd újra, `DATA` = egy darab, jön még, `END` = az utolsó darab (lehet üres).

`OPEN` után a `SEND` küld, a `NEXT` olvas: `BUSY` = most nincs adat (vagy még kapcsolódik), `DATA` = jött adat, `END` = a túloldal bontott, ez az utolsó darab. A `SEND` már kapcsolódás közben is mehet, a TVCnet sorba teszi. Egyszerre egy feladat van: egy új `GET`, `OPEN` vagy `ATD` az előzőt lezárja.

## Hayes modem (`TERM`)

A `TERM` egy lekérdezett soros vonal egy Hayes modemhez: a DATA-ban az megy, amit a felhasználó begépelt, a válaszban az jön, amit ki kell írni. Ha nincs mit küldeni, üres `TERM`-mel kell lekérdezni. A válasz állapota a modem módja: `OK` = parancsmód, `BUSY` = tárcsáz (bármelyik billentyű megszakítja), `DATA` = online (TCP kapcsolat vagy `AT&G` letöltés).

| Parancs | Hatás |
|---|---|
| `AT` | `OK` |
| `ATE0` / `ATE1` | visszhang ki / be (alapból be) |
| `ATV0` / `ATV1` | számkódos / szöveges válaszok (alapból szöveges) |
| `ATQ0` / `ATQ1` | válaszok be / ki |
| `ATZ`, `AT&F` | alapbeállítás; az `ATZ` a kapcsolatot is bontja |
| `ATI`, `ATI0`, `ATI2`, `ATI4` | protokoll verzió és WiFi állapot; IP cím; verzió |
| `ATW"SSID,jelszó"` | WiFi, mint a `WIFI` parancs |
| `ATD"host:port"` (`ATDT`, `ATDP` is, idézőjel nélkül is) | TCP kapcsolat; `CONNECT` után online mód, a byte-ok változtatás nélkül mennek át |
| `+++` | 1 s csend, `+++`, 1 s csend: vissza parancsmódba (`OK`), a kapcsolat megmarad |
| `ATO` | vissza online módba (`CONNECT`), vagy `NO CARRIER`, ha nincs kapcsolat |
| `ATH` | bontás |
| `AT&G"url"` | HTTP vagy HTTPS GET: kiírja a választ, a végén `OK` (hibánál `ERROR`) |
| `A/` | az előző sor újra |

- Egy sorban több parancs is lehet (`ATE0V0`); a sor CR-rel (0Dh) zárul, a backspace (08h, 7Fh) töröl.
- Válaszok: `OK` (0), `CONNECT` (1), `NO CARRIER` (3), `ERROR` (4); szövegesen CR LF + szó + CR LF, számkóddal szám + CR.
- `NO CARRIER` jön, ha a kapcsolat nem jön létre (30 s után is), ha a túloldal bont, vagy ha egy billentyű megszakítja a tárcsázást.
- A Telnet vezérlő byte-okat (FFh ...) a TVCnet nem dolgozza fel, azokat a TVC program kapja.
- A `+++` csendjét a TVCnet a TVC-ből jövő adaton méri; az üres `TERM` lekérdezések nem számítanak.

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

## TVC programok

A `tvc/` mappában sjasmplus-szal fordítható TVC oldali példák és tesztek vannak, a `tvcnet.inc` rutinjaival (`tn_cmd`: egy kérés és a válasza, ismétléssel): `ping`, `get`, `tcp`, `hayes`. Leírás: [tvc/README.md](tvc/README.md). TVC BASIC példák (`PING`, `WIFI`, `GET`, `TERM`, CAS fájlként is): [tvc/basic/README.md](tvc/basic/README.md).

40h-tól felfelé minden hiba.
