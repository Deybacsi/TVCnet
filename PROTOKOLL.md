# TVCnet: protokoll

Ez a leírás TVC programíróknak szól: azt mutatja be, hogyan beszélget a TVC a TVCnet-tel. A bekötés a [HARDVER.md](HARDVER.md)-ben van.

**Ha csak használni akarod**, nem kell minden részletet ismerned. A `tvc/tvcnet.inc` `tn_cmd` rutinja elintézi a vonalat, a keretet és az ismétlést, neked csak a parancsot és az adatot kell megadnod. A BASIC példák is ezt használják ([tvc/README.md](tvc/README.md)).

## Áttekintés

- A beszélgetés **félduplex**: egyszerre csak az egyik fél beszél. Mindig a TVC kérdez, a TVCnet csak válaszol, magától soha nem küld semmit.
- A kérés soros byte-okként megy ki a magnó **motorvezérlő** vonalán.
- A válasz impulzuskódként jön vissza a **magnó bemeneten**, hasonlóan, mint egy kazetta jele.
- Mindkét irányban ugyanolyan szerkezetű **keret** megy: jelölő byte, hossz, parancs vagy állapot, adat, ellenőrzőösszeg.
- A lassú műveleteket (letöltés, TCP kapcsolat) egy parancs elindítja, az eredményt pedig a TVC darabonként kéri le a `NEXT` paranccsal. Így a TVC soha nem vár sokáig tiltott megszakítással.

Egy kérés menete:

1. A TVC letiltja a megszakításokat (`DI`), és kiküldi a kérést a motor vonalon.
2. Azonnal figyelni kezdi a magnó bemenetet (`IN (59h)` b5).
3. A TVCnet a kérés utolsó byte-ja után 2 ms-mal elkezdi a választ.
4. A TVC dekódolja a választ, ellenőrzi az összeget, és engedélyezi a megszakításokat (`EI`).

## TVC -> TVCnet: soros jel a motor vonalon

A TVC szoftverből billenti a motorbitet, mint egy UART adó:

| Beállítás | Érték |
|---|---|
| sebesség | 2400 baud: egy bit 417 µs, 3,125 MHz-en 1302 órajel |
| formátum | 8N1: start bit, 8 adatbit (LSB először), stop bit |
| bit | `OUT (05h)` b7 (CH12 aljzat) vagy b6 (CH13 aljzat) |

- **A logika fordított:** ha a bitbe 1-et írsz, a vonal 0 V lesz (bekapcsol a motor tranzisztora). Ezért:
  - nyugalomban és a stop bitnél 0-t írj (a RESET utáni állapot is ez);
  - a start bitnél 1-et írj;
  - adatbitnél a bit ellentettjét írd.
- A 05h többi bitje a hangé, ezeket őrizd meg. Frissítsd a RAM-ban lévő másolatot is (0B12h), különben az operációs rendszer visszaírja a régi értéket.
- Ha a b7-et és a b6-ot egyszerre billented, mindegy, melyik aljzatban van a TVCnet.
- A kérés két byte-ja között legfeljebb 20 ms telhet el. Ha több, a TVCnet eldobja a félig vett kérést, és újra a jelölő byte-ra vár. (A ROM kazettás műveletei is kapcsolgatják a motorbitet; ezt is így szűri ki.)

## TVCnet -> TVC: félperiódus-kód a magnó bemeneten

A magnó bemenet egy kondenzátoron át jut a TVC-be, ezért egy tartós szint (mint egy UART nyugalmi szintje) nem jut át rajta. A TVCnet ezért folyamatosan billegő jelet küld, és **a két él közötti idő hossza a bit**:

| Két él közötti idő | Bit |
|---|---|
| T = 250 µs (3,125 MHz-en 781 órajel) | 1 |
| 2T = 500 µs | 0 |

- A határ 1,5T: ami ennél rövidebb, az 1, ami hosszabb, az 0.
- Csak az élek számítanak, a szint nem, így nem baj, ha a jel fordítva érkezik.
- A byte-on belül itt is LSB megy először.

Példa: a 6Ah byte bitjei b0-tól b7-ig 0, 1, 0, 1, 0, 1, 1, 0. A vonalon ez így megy: 2T, T, 2T, T, 2T, T, T, 2T.

Egy válasz a vonalon:

1. **Előtag:** 32 darab 1-es bit (rövid félperiódusok). Ebből a TVC megmérheti T-t. A `tvcnet.inc` ehelyett fix küszöbbel dolgozik, ami 3,125 MHz-re van kiszámolva.
2. **Szinkron:** a TVC a beérkező biteket egy regiszterbe lépteti, amíg ki nem jön a 6Ah jelölő byte.
3. **A keret többi byte-ja** ezután szünet nélkül jön, 8 bitenként egy byte.
4. **Lezárás:** egy záró él (hogy az utolsó bitnek is legyen vége), néhány 1-es bit, aztán a vonal megáll.

- Átlagosan kb. 330 byte/s jön át; a ROM kazettás sebessége kb. 266 byte/s.
- Egy teljes, 255 adatbyte-os keret legrosszabb esetben kb. 1 s.
- Ha a kérés után rövid időn belül (pl. 50 ms) nem jön él, nem jön válasz: küldd újra a kérést.

## Keret

| Mező | Méret | Tartalom |
|---|---|---|
| SYNC | 1 | jelölő byte: A5h (TVC -> TVCnet), 6Ah (TVCnet -> TVC) |
| LEN | 1 | a DATA hossza, 0-255 |
| CTRL | 1 | b7: váltakozó bit (lásd lent); b6-b0: kérésben a parancs, válaszban az állapot |
| DATA | LEN | adat |
| CHK | 1 | a LEN, a CTRL és a DATA byte-ok 8 bites összege |

- A vevő a byte-okat a helyük alapján értelmezi: a SYNC után a LEN jön, utána a CTRL, utána pontosan LEN darab adatbyte, végül a CHK.
- Ezért a DATA-ban bármilyen byte lehet, a SYNC értéke is; nincs escape.
- 255 byte-nál több adat egy keretbe nem fér: a nagyobb adat több `NEXT` válaszban jön.

Példa: egy `PING` és a válasza, a váltakozó bit 0:

```
TVC:     A5 00 01 01       SYNC, LEN = 0, CTRL = 01h (PING), CHK = 00 + 01
TVCnet:  6A 01 00 02 03    SYNC, LEN = 1, CTRL = 00h (OK), DATA = 02h (verzió), CHK = 01 + 00 + 02
```

Ugyanez 1-es váltakozó bittel: `A5 00 81 81`, a válasz `6A 01 80 02 83`.

## Váltakozó bit (CTRL b7)

Ha egy válasz elveszik, a TVC nem tudhatja, hogy a TVCnet végrehajtotta-e a kérést. Egy `SEND` ismétlése például kétszer küldené el ugyanazt az adatot. Ezt a CTRL b7 dönti el:

- A TVCnet megjegyzi a legutóbbi kérést és a rá adott választ.
- Ha a következő kérés byte-ra ugyanaz (a váltakozó bittel együtt), az ismétlés: a TVCnet nem hajtja végre újra, csak elküldi az eltárolt választ.
- Ha bármiben eltér, az új kérés.
- A válasz CTRL b7-e mindig ugyanaz, mint a kérésé.

A TVC teendője:

- **Minden jó válasz után billentsd át a bitet**, a `BUSY` után is. Különben a következő `NEXT` ismétlésnek számítana, és újra meg újra a régi `BUSY`-t kapnád.
- **Hibás összegű vagy elmaradt válasz után** küldd újra a kérést ugyanazzal a bittel. Néhány (a `tn_cmd`-ben 8) sikertelen próba után add fel.
- Hibás összegű kérésre a TVCnet nem válaszol; ezt a TVC időtúllépésként látja, és ismétel.
- **A program elején küldj egy `PING`-et.** Ha egy előző futásból épp egy ugyanilyen `PING` maradt a TVCnet-ben, a tárolt (ugyanolyan) választ kapod, és utána a bitek már rendben vannak.

## Parancsok

| Kód | Parancs | DATA | Válasz |
|---|---|---|---|
| 01h | `PING` | - | `OK` + 1 byte: protokoll verzió (02h) |
| 02h | `ECHO` | bármi | `OK` + ugyanaz (teszteléshez) |
| 03h | `STATUS` | - | `OK` + 10 byte (lásd lent) |
| 04h | `WIFI` | `SSID,jelszó` | `OK` / hiba |
| 10h | `GET` | URL (`http://` vagy `https://`) | `OK` (elindítva) / hiba |
| 11h | `NEXT` | - vagy 1 byte: legfeljebb ennyi adatot kér | `BUSY` / `DATA` / `END` / `NONE` / hiba |
| 12h | `CLOSE` | - | `OK` |
| 20h | `OPEN` | `host:port` | `OK` (elindítva) / hiba |
| 21h | `SEND` | adat | `OK` / `NONE` / `BUSY` / hiba |
| 30h | `TERM` | a begépelt byte-ok (lehet üres) | `OK` / `BUSY` / `DATA` + a kiírandó byte-ok |

A szöveges paraméterek (URL, `host:port`, `SSID,jelszó`) sima ASCII byte-ok, lezáró byte nélkül: a hosszukat a LEN adja meg.

Egyszerre egy feladat fut: egy új `GET`, `OPEN` vagy Hayes tárcsázás (`ATD`) az előzőt lezárja.

### WiFi beállítása: `WIFI`, `STATUS`

- A `WIFI` paraméterében az első vessző választja el a hálózat nevét a jelszótól. A névben tehát nem lehet vessző, a jelszóban lehet.
- A TVCnet elmenti az adatokat, és bekapcsoláskor magától csatlakozik. Elég egyszer megadni.
- A csatlakozás eltart egy ideig. A `STATUS` megmondja, sikerült-e.

A `STATUS` válaszának 10 byte-ja:

| Byte | Tartalom |
|---|---|
| 0 | WiFi: 0 = nincs, 1 = csatlakozva |
| 1-4 | IPv4 cím, pl. C0 A8 00 0A = 192.168.0.10 |
| 5 | a futó feladat állapota: `NONE`, `BUSY`, `OK` (a TCP kapcsolat él, de nincs adat), `DATA`, `END` vagy hibakód |
| 6-9 | a letöltés teljes hossza, alsó byte először; FFFFFFFFh, ha nem ismert |

### Letöltés: `GET` és `NEXT`

A `GET` csak elindítja a letöltést, és azonnal `OK`-t (vagy hibát) ad. A tartalmat utána `NEXT`-ekkel kell lekérni:

| A `NEXT` válasza | Jelentés | Teendő |
|---|---|---|
| `BUSY` | még nincs adat (kapcsolódik, vár a szerverre) | várj egy kicsit, és kérd újra |
| `DATA` | egy darab a tartalomból, jön még | tárold el, kérd a következőt |
| `END` | az utolsó darab (lehet üres is) | kész |
| `NONE` | nincs futó feladat | |
| hiba | pl. 43h DNS, 45h HTTP hiba | a 45h DATA-jában a HTTP státuszkód van (2 byte, alsó byte először), pl. 94h 01h = 404 |

- Egy darab legfeljebb 255 byte. Ha kevesebb hely van a pufferben, a `NEXT` DATA-jában add meg, legfeljebb mennyit kérsz.
- A TVCnet a hálózatról csak annyit olvas, amennyit a TVC elvisz, így a tartalom akármilyen nagy lehet.
- HTTPS-nél a titkosítást a TVCnet intézi, a TVC a sima tartalmat kapja. A tanúsítványt egyelőre nem ellenőrzi.
- A `CLOSE` félbeszakítja a letöltést.

### TCP kapcsolat: `OPEN`, `SEND`, `NEXT`, `CLOSE`

- Az `OPEN` elindítja a kapcsolódást, és azonnal `OK`-t ad. Ha a kapcsolódás nem sikerül (DNS, kapcsolódási hiba), azt a következő `NEXT` mondja meg.
- A `SEND` elküldi az adatot. Már kapcsolódás közben is mehet: a TVCnet sorba teszi (legfeljebb 4096 byte-ot), és a kapcsolat felépülése után küldi el.
  - `BUSY`: tele a sor, küldd újra később.
  - `NONE`: nincs TCP kapcsolat.
- A `NEXT` olvassa, ami a túloldalról jött:
  - `BUSY`: most nincs adat (vagy még kapcsolódik);
  - `DATA`: jött adat;
  - `END`: a túloldal bontott, és ez az utolsó darab.
- A `CLOSE` bontja a kapcsolatot.

## Hayes modem (`TERM`)

A `TERM` szöveges felület a TVCnet-hez: úgy viselkedik, mint egy régi Hayes modem AT parancsokkal. Terminálprogramhoz vagy BASIC-hez kényelmes, mert nem kell kereteket és állapotkódokat kezelni, csak szöveget küldeni és kiírni.

Mivel a TVCnet magától nem szól, a `TERM` lekérdezve működik:

- a DATA-ban az megy, amit a felhasználó begépelt;
- a válasz DATA-jában az jön, amit a képernyőre kell írni (visszhang, `OK`, a kapcsolat adata);
- ha nincs mit küldeni, üres `TERM`-mel kérdezz rá, van-e mit kiírni;
- a válasz állapota a modem módja:
  - `OK`: parancsmód;
  - `BUSY`: tárcsáz (bármelyik billentyű megszakítja);
  - `DATA`: online (TCP kapcsolat vagy `AT&G` letöltés).

Egy munkamenet például: `ATI` (kiírja a verziót és a WiFi állapotát), `ATD"bbs.example.com:23"` (`CONNECT`, ettől kezdve a begépelt szöveg a BBS-re megy), `+++` (vissza parancsmódba), `ATH` (bontás).

| Parancs | Hatás |
|---|---|
| `AT` | `OK` |
| `ATE0` / `ATE1` | visszhang ki / be (alapból be) |
| `ATV0` / `ATV1` | számkódos / szöveges válaszok (alapból szöveges) |
| `ATQ0` / `ATQ1` | válaszok be / ki |
| `ATZ`, `AT&F` | alapbeállítás; az `ATZ` a kapcsolatot is bontja |
| `ATI`, `ATI0` | protokoll verzió és WiFi állapot |
| `ATI2` | IP cím |
| `ATI4` | a protokoll verzió száma |
| `ATW"SSID,jelszó"` | WiFi, mint a `WIFI` parancs |
| `ATD"host:port"` (`ATDT`, `ATDP` is, idézőjel nélkül is) | TCP kapcsolat; `CONNECT` után online mód, a byte-ok változtatás nélkül mennek át |
| `+++` | 1 s csend, `+++`, 1 s csend: vissza parancsmódba (`OK`), a kapcsolat megmarad |
| `ATO` | vissza online módba (`CONNECT`), vagy `NO CARRIER`, ha nincs kapcsolat |
| `ATH` | bontás |
| `AT&G"url"` | HTTP vagy HTTPS GET: kiírja a tartalmat, a végén `OK` (hibánál `ERROR`) |
| `A/` | az előző sor újra |

- Egy sorban több parancs is lehet (`ATE0V0`). A sor CR-rel (0Dh) zárul, legfeljebb 80 karakter; a backspace (08h, 7Fh) töröl.
- Válaszok: `OK` (0), `CONNECT` (1), `NO CARRIER` (3), `ERROR` (4). Szövegesen CR LF + szó + CR LF, számkóddal szám + CR.
- `NO CARRIER` jön, ha:
  - a kapcsolat nem jön létre (30 s után is);
  - a túloldal bont;
  - egy billentyű megszakítja a tárcsázást.
- A Telnet vezérlő byte-okat (FFh ...) a TVCnet nem dolgozza fel, azokat a TVC program kapja.
- A `+++` előtti és utáni csendet a TVCnet a TVC-ből jövő adaton méri; az üres `TERM` lekérdezések nem számítanak bele.

## Állapotkódok

| Kód | Állapot |
|---|---|
| 00h | `OK` |
| 01h | `NONE`: nincs futó feladat |
| 02h | `BUSY`: még nincs kész, kérd újra |
| 03h | `DATA`: adat jött, jön még |
| 04h | `END`: az utolsó adat |
| 40h | hiba: ismeretlen parancs |
| 41h | hiba: hibás paraméter |
| 42h | hiba: nincs WiFi |
| 43h | hiba: DNS (nem található a név) |
| 44h | hiba: nem sikerült kapcsolódni |
| 45h | hiba: HTTP; DATA: a státuszkód (2 byte, alsó byte először) |
| 46h | hiba: megszakadt a kapcsolat / időtúllépés |

40h-tól felfelé minden hiba: egy `CP 40h` után `NC` esetén hiba jött.

## TVC programok

A `tvc/` mappában sjasmplus-szal fordítható TVC oldali példák és tesztek vannak, a `tvcnet.inc` rutinjaival (`tn_cmd`: egy kérés és a válasza, ismétléssel): `ping`, `get`, `tcp`, `hayes`. Leírás: [tvc/README.md](tvc/README.md). TVC BASIC példák (`PING`, `WIFI`, `GET`, `TERM`, CAS fájlként is): [tvc/basic/README.md](tvc/basic/README.md).
