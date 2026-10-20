# Commander — aktywny panel i zaznaczanie plików

24.09.2026. Aktywny panel ma podwójną ramkę; drugi pojedynczą. Tab i kliknięcie przełączają także ramkę, przy zachowaniu przyrostowego rysowania.

- + zaznacza wskazany plik, - odznacza. Obsługiwany jest również blok numeryczny.
- Gwiazdka przed nazwą oznacza zaznaczenie. Kursor nie przesuwa się automatycznie.
- Zaznaczenia są osobne dla paneli i przeżywają przewijanie. Zmiana katalogu usuwa zaznaczenia tego panelu. Limit: 256 plików; katalogi nie są zaznaczane.
- F5 kopiuje grupę do katalogu drugiego panelu, po potwierdzeniu liczby plików i celu. Bez zaznaczeń zachowuje dotychczasowe kopiowanie pojedynczego pliku.
- Anulowanie zachowuje grupę. Sukces usuwa oznaczenie skopiowanego pliku. Błąd zatrzymuje operację i pozostawia resztę zaznaczoną, więc ponowienie nie kopiuje wcześniej udanych pozycji.
- Nowe komunikaty i pomoc są w EN.LNG/PL.LNG; zachowany wybór języka z SETUP.

Testy: build/install PASS; 21 testów Python i zestawy C PASS. ASan/UBSan dla rzeczywistych handlerów Commandera: ramki/przyrostowe rysowanie, znaki fontu, zaznaczanie po przewinięciu, osobne panele, anulowanie, błąd, częściowy sukces i ponowienie.

Hatari, własny BIOS, STE/030 16 MHz mono: wizualnie potwierdzono ramki, plus/minus oraz F5 grupy dwóch plików. W nowym katalogu C:\MCOPY są kopie DESKTOP.APP (2896 B) i DESKOLD.APP (3376 B). Oryginały pozostały w SYSTEM\APP; aplikacje nie były uruchamiane podczas próby kopiowania.

Emulator pozostawiono z Commanderem: lewy panel MCOPY, prawy SYSTEM\APP. Jest to aktualizacja względem wcześniejszego archiwum stabilności; samo tamto archiwum nie zawiera tej zmiany. Patch commander-norton.patch obejmuje kod, testy i pakiety językowe.
