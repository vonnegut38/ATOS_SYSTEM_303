# ATOS 2.1 — diagnoza kolorowych ikon, 2026-10-02

## Potwierdzona przyczyna

Nowe zapytanie o głębię obrazu nie przechodzi przez usługę pośredniczącą GFX.APP.

1. `system/desktop_wm.c:mode_query` wysyła `GFX_VIDEO_INFO = 35`.
2. `system/kernel_exception.c` obsługuje opcode 35: zwraca 8 dla chunky lub 1 dla mono. Dodano go też do dozwolonych zapytań klienta.
3. `system/gfx_service_app.S`, około linii 35, nadal wykonuje `cmpi.l #34,%d2` / `bhi command_invalid`. Polecenie 35 zostaje odrzucone przed wywołaniem kernela; klient otrzymuje `-0x3e2`.
4. Pulpit porównuje odpowiedź z 8. Każda inna wartość, również błąd, ustawia `cur_bpp=1` i `chunky=0`. Renderowanie i karta mogą pracować w 8 bpp, podczas gdy logika pulpitu uważa, że obowiązuje monochrom.
5. `icon_ink` wybiera wtedy wartości dla mono zamiast czerwonego kosza, szarego dysku i niebieskich skrótów. Błędna głębia wpływa też na rozpoznawanie bieżącego trybu w SETTINGS.

To błąd zgodności protokołu desktop → GFX.APP → kernel. Dalsze zmiany palety czy formatu źródła w kompozytorze nie naprawiają odrzuconego zapytania.

## Minimalna poprawka i dowód

W `gfx_service_app.S` zmienić górną granicę z 34 na 35 i zbudować GFX.APP. Patch w tej paczce dotyczy wyłącznie tej instrukcji i komentarza. Nie rozszerza uprawnień klienta do zmiany globalnego trybu; istniejące sprawdzenia kernela pozostają.

Test A/B wykonano w osobnej kopii dysku, na Hatari 68030 32 MHz, 4 MB ST-RAM, 64 MB TT-RAM, VOFA/ET4000 800×600×256, bez fast-forward. Między przebiegami podmieniono **wyłącznie SYSTEM/GFX.APP**. Nie zmieniono kernela ani DESKTOP.APP. Zgodność zainstalowanego DESKTOP.APP z bieżącym build potwierdzono sumą SHA-256.

- Przed: ikony dysków, kosza i aplikacji wyglądają biało/czarno.
- Po: kosz czerwony, skróty aplikacji niebieskie, drugi dysk szary. Zrzuty znajdują się w `evidence/icons-before.png` i `evidence/icons-after.png`.
- Budowa GFX.APP zakończona powodzeniem. Sprawdzono obecność instrukcji `CMP.L #35,D2` w binarium oraz poprawność nagłówka, długości i sumy kontrolnej aplikacji.

Dodatkowy start z BIOS mode=2 i zapisanym RES=0 dotarł do pierwszego batcha, ale headless screenshot mono jest czarny zarówno przed poprawką, jak i po niej. Nie uznaję go za pozytywną weryfikację obrazu mono; ten wariant wymaga testu wizualnego w konfiguracji użytkownika.

Nie należy przy tym obiecywać, że jedna zmiana uruchamia nowe obrazki PNG. Naprawia ona wybór koloru istniejących bitmap.

## Nowe PNG nadal nie są używane

W `system/ICONS` znajdują się PNG 96×96, w wariantach normal/hover. README opisuje plan przypisywania po nazwach. Obecna ścieżka wykonawcza nie realizuje tego planu:

- `icons_build` przypisuje zasoby numerami 0..3, nie nazwami plików.
- `GUI_ICON` przekazuje identyfikator zasobu oraz kolory ink/paper.
- `kgfx_icon` wybiera `rs_iconmask0..3` i `rs_icondata0..3` z zasobów EmuTOS.
- `theme_icon` wybiera między ink i paper na podstawie jednego bitu bitmapy; maska określa widoczność. Nie ma indeksu osobnego koloru dla każdego piksela.
- `tools/mk_icons.py` wprost konwertuje obraz do **1 bpp** przez próg jasności. Taka konwersja usuwa kolory z PNG.
- Brakuje połączenia nowych PNG z kompilacją zasobów i ich wyborem przez pulpit. Sam katalog plików nie zmienia wyświetlanych ikon.

Wniosek: obecnie DeepSeek pracuje nad kolorowaniem starych ikon. Pełnokolorowe nowe ikony to odrębny etap danych i renderera.

## Zalecany następny etap dla DeepSeek

1. Najpierw scalić tę minimalną poprawkę GFX i potwierdzić czerwony kosz/niebieskie skróty na swoim obrazie.
2. W `mode_query` odróżniać odpowiedź 1/8 od kodu błędu. Przy błędzie nie udawać rozpoznania mono; zachować poprzedni poprawny stan i zgłosić diagnostykę. Docelowo współdzielić stałe protokołu między usługą a klientami lub sprawdzać zgodność zakresów w testach.
3. Przygotować konwerter uruchamiany podczas budowania: 96×96 → 32×32, wspólna paleta indeksowana dla wszystkich używanych ikon, osobna maska przezroczystości. Nie zastępować tego konwerterem 1 bpp i nie dodawać kosztownego dekodera PNG do każdego odrysowania na 68030.
4. Zachować indeksy palety używane przez obecne UI (obecnie 0..19), a pozostałą część palety zaplanować wspólnie dla ikon. Nie ustawiać innej globalnej palety przy rysowaniu każdej ikony. Jawnie określić sposób obsługi półprzezroczystości po zmniejszeniu PNG; tło zaznaczenia nie może pozostawiać obwódek w kolorze tła zwykłego pulpitu.
5. Dodać zasób zawierający szerokość, wysokość, stride, maskę i indeksy koloru. Renderer 8 bpp ma kopiować indeksy widocznych pikseli; dla mono pozostawić fallback. Testować clipping do powierzchni, narożniki, granice pamięci i zaznaczenie.
6. Zacząć od kilku ikon rzeczywiście widocznych na pulpicie. Dla 32×32 jedna bitmapa 8 bpp zajmuje 1024 B, a maska 1 bpp 128 B; 50 wariantów to 57 600 B przed tablicami/opisami. Nie ładować wszystkich wariantów przy każdym paint.
7. Dopiero potem dodać mapowanie nazw na zasoby i stan normal/hover. Zmiana stanu ma unieważniać prostokąt jednej ikony, nie cały pulpit.

Nie usuwać maski ani nie traktować koloru 0 jako automatycznie przezroczystego — czarne elementy rysunku muszą nadal być widoczne. Nie zakładać również, że sama rozdzielczość rozstrzyga mono/kolor: 800×600 istnieje w obu głębiach.

## Scalenie

Zastosować `GFX-depth-query.patch` na osobnej kopii projektu, zbudować:

    gmake build/system/GFX.APP

Następnie zainstalować **GFX.APP**, nie tylko KERNEL.SYS i DESKTOP.APP, do zatrzymanego testowego obrazu:

    python3 tools/install_boot_sys.py work/atos-v2.img build/system/GFX.APP

Uruchomić system ponownie. Działająca usługa GFX pozostaje starym procesem do restartu — podmiana pliku na dysku nie podmienia jej kodu w pamięci. Nie przepisywać całych nowszych plików DeepSeek kopią z audytu; patch jest jednolinijkowy.

Aktywne źródła i obraz użytkownika nie zostały zmodyfikowane. W paczce nie ma niewdrożonego renderera PNG przedstawianego jako gotowa funkcja.
