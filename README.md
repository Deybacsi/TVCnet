# TVCnet

A TVCnet a Videoton TV Computer magnó DIN aljzatára dugható ESP32-S3 WiFi modul / WiFi dongle.

A TVC a motorvezérlő lábon (05h b7/b6) UART-szerű jelekkel küld adatot, és a magnó bemeneten (59h b5) impulzusszélesség-kóddal kap választ. A kommunikáció félduplex, vagyis egyszerre csak az egyik fél tud beszélni. Kérés -> válasz alapon működik, mindig a TVC a mester.

A protokoll leírása: [PROTOKOLL.md](PROTOKOLL.md), a hardveré és a bekötésé: [HARDVER.md](HARDVER.md).

## Felépítés

| Mappa | Tartalom |
|---|---|
| `common/` | hardverfüggetlen C++17: keret, vonali kód, parancs- és állapotkódok, a modem (váltakozó bit, parancsok, `NEXT`), a Hayes modem (`TERM`), a HAL felület. Ugyanez a kód fordul a freeTVC emulátorba is, ezért Arduino és ESP-IDF fejléc nem lehet benne. |
| `esp32s3/` | Az ESP hardverfüggő kódrészei. PlatformIO projekt. |
| `tests/` | a `common/` host tesztjei hamis hálózattal |
| `tvc/` | TVC oldali programok (sjasmplus): a kérés-válasz rutinok és tesztek |

A `common/` és a gazda közötti határ: `common/include/tvcnet/hal.hpp`.

## Fordítás

- **Host tesztek** (Linux, CMake + C++ fordító):

  ```sh
  cmake -S . -B build -G Ninja && cmake --build build && ./build/tvcnet_tests
  ```

- **ESP32-S3** (PlatformIO, pl. a VS Code bővítményből): nyisd meg az `esp32s3/` mappát, vagy parancssorból:

  ```sh
  cd esp32s3 && pio run && pio run -t upload && pio device monitor
  ```

  A platform a pioarduino 55.03.312-1 (Arduino-ESP32 3.3.12); az első fordítás letölti (kb. 1–1,5 GB). Ehhez **PlatformIO Core 6.2.0** vagy újabb kell (`pio upgrade`); a 6.1.x és a fejlesztői 6.2.0-post.1 `IncompatiblePlatform` hibával leáll. A lap a `waveshare_esp32_s3_zero` (4 MB flash, quad PSRAM). A `common/` a `symlink://../common` könyvtárként jön.

- **freeTVC-be**: a freeTVC a repó gyökerét FetchContenttel húzza be (`tvcnet_common` könyvtár); fejlesztés közben `-DFETCHCONTENT_SOURCE_DIR_TVCNET=<ez a mappa>`.

## Használat az ESP-n

- A WiFi adatait a TVC a `WIFI` paranccsal adja meg (`SSID,jelszó`); az ESP elmenti, és indításkor csatlakozik.
- Az USB soros naplóban (115200) minden keret látszik mindkét irányban.
- HTTPS egyelőre tanúsítvány-ellenőrzés nélkül (`setInsecure()`).
- Terminálprogramnak a `TERM` parancs Hayes modemként viselkedik (`ATD"host:port"`, `+++`, `ATH`, `AT&G"url"`), lásd [PROTOKOLL.md](PROTOKOLL.md).
