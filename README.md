# TVCnet

A TVCnet egy WiFi modul a Videoton TV Computerhez (TVC). Egy ESP32-S3 lapkából áll, és a TVC magnó DIN aljzatába kell dugni, a gépet nem kell megbontani hozzá.

Mire jó:

- weboldal vagy fájl letöltése HTTP-n és HTTPS-en;
- TCP kapcsolat egy szerverrel, például egy BBS-sel;
- Hayes modemként AT parancsokkal is kezelhető, így egy egyszerű BASIC program vagy egy terminálprogram is használhatja.

## Állapot

- **Valódi hardveren még nincs kipróbálva.** A protokoll, a firmware közös része és a TVC programok a [freeTVC](https://github.com/LaM3rzLG/FreeTVC) emulátorban működnek. Az emulátor TVCnet támogatása még nincs benne a freeTVC kiadott változatában.
- **A 2400 baud egyelőre tesztérték.** A valódi gépen végzett mérések után a sebesség és az időzítés változhat.
- A HTTPS egyelőre tanúsítvány-ellenőrzés nélkül működik.

## Hogyan működik

- A TVC a magnó két jelvonalán beszélget a TVCnet-tel:
  - a motorvezérlő lábon soros jelként küldi a kérést;
  - a magnó bemenetén impulzuskódként kapja a választ.
- Mindig a TVC kérdez, a TVCnet csak válaszol.
- A hálózati munkát (WiFi, DNS, HTTP, TLS, TCP) a TVCnet végzi. A TVC csak kéréseket küld, és darabonként megkapja az eredményt.

Leírások:

- [HARDVER.md](HARDVER.md): alkatrészek, bekötés, táp, első próba;
- [PROTOKOLL.md](PROTOKOLL.md): a vonal, a keret és a parancsok, TVC programíróknak.

## Mi kell hozzá

Egy ESP32-S3-Zero lap, egy 5 pólusú, 180°-os DIN dugó és két ellenállás. A bekötés és az összeszerelés a [HARDVER.md](HARDVER.md)-ben van.

## A firmware feltöltése

Kész bin fájl még nincs, a firmware-t le kell fordítani a [PlatformIO](https://platformio.org/)-val.

1. Telepítsd a PlatformIO-t: a VS Code bővítményt, vagy a parancssori PlatformIO Core-t. **6.2.0 vagy újabb** változat kell (`pio upgrade`). A 6.1.x és a fejlesztői 6.2.0-post.1 `IncompatiblePlatform` hibával leáll.
2. A modult húzd ki a TVC-ből, és dugd a PC-be USB-C kábellel.
3. Fordítás és feltöltés:

   ```sh
   cd esp32s3
   pio run -t upload
   pio device monitor
   ```

Tudnivalók:

- Az első fordítás letölti a platformot és az eszközöket, ez kb. 1-1,5 GB.
- Ha a feltöltés nem indul, tartsd nyomva a lapon a BOOT gombot, miközben bedugod az USB-t, és próbáld újra.
- Indulás után a soros naplóban (115200 baud) megjelenik a `TVCnet ready` sor.
- A napló utána minden kérést és választ mutat.

## TVC programok

- [tvc/basic/](tvc/basic/README.md): TVC BASIC példák CAS fájlként is:
  - `PING`: kipróbálja a kapcsolatot;
  - `WIFI`: beállítja a WiFi-t;
  - `GET`: letölt egy URL-t;
  - `TERM`: egyszerű terminál.
- [tvc/](tvc/README.md): Z80 assembly rutinok (`tvcnet.inc`) és tesztprogramok, sjasmplus-szal fordíthatók.

Első használatkor a `WIFI` programmal kell megadni a WiFi hálózat nevét és jelszavát. A TVCnet elmenti őket, és bekapcsoláskor magától csatlakozik.

## Fejlesztőknek

| Mappa | Tartalom |
|---|---|
| `common/` | hardverfüggetlen C++17: keret, vonali kód, parancs- és állapotkódok, a modem (váltakozó bit, parancsok, `NEXT`), a Hayes modem (`TERM`), a HAL felület. Ugyanez a kód fordul a freeTVC emulátorba is, ezért Arduino és ESP-IDF fejléc nem lehet benne. |
| `esp32s3/` | az ESP hardverfüggő része (UART vétel, RMT adás, WiFi, HTTP, TCP); PlatformIO projekt |
| `tests/` | a `common/` host tesztjei hamis hálózattal |
| `tvc/` | TVC oldali programok: assembly rutinok és tesztek, BASIC példák |

- A `common/` és a gazda közötti határ a `common/include/tvcnet/hal.hpp`.
- Host tesztek (Linux, CMake, Ninja, C++ fordító):

  ```sh
  cmake -S . -B build -G Ninja && cmake --build build && ./build/tvcnet_tests
  ```

- ESP részletek:
  - platform: pioarduino 55.03.312-1 (Arduino-ESP32 3.3.12);
  - lap: `waveshare_esp32_s3_zero` (4 MB flash, quad PSRAM);
  - a `common/` a `symlink://../common` könyvtárként kerül be.
- freeTVC: a repó gyökerét FetchContenttel húzza be (`tvcnet_common` könyvtár). Fejlesztés közben a helyi mappát így adhatod meg: `-DFETCHCONTENT_SOURCE_DIR_TVCNET=<ez a mappa>`.

## Készítés

A projekt a [Claude Code](https://claude.com/claude-code) (Anthropic) segítségével készült. Mire használtuk:

- **tervezés:** a protokoll (a vonali kód, a keret, a váltakozó bit, a parancsok) és a bekötés;
- **kód:** a firmware, a közös C++ rész és a TVC oldali assembly és BASIC programok;
- **tesztelés:** host tesztek, és a TVC programok futtatása a freeTVC emulátorban;
- **dokumentáció:** a README, a HARDVER.md és a PROTOKOLL.md.

## Licenc

GPL-3.0-or-later, lásd [LICENSE](LICENSE).
