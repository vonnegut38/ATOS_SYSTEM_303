# Glosariusz i zasady tłumaczenia DSH na polski

Dokument obowiązujący wszystkie partie tłumaczenia pakietu `dsh-locale-pl`.
Cel: spójna terminologia i styl w całym interfejsie.

## Zasady ogólne

1. **Tłumacz na naturalny polski**, nie dosłownie. Interfejs ma brzmieć jak polski
   produkt, nie jak tłumaczenie maszynowe.
2. **Forma**: bezosobowa i bez „Pan/Pani”. Przyciski i pozycje menu — tryb
   rozkazujący, krótko: `Zapisz`, `Anuluj`, `Usuń`, `Kopiuj`, `Ponów`.
   Etykiety i opisy — rzeczownikowo lub zdaniowo, zwięźle.
3. **Wielkość liter**: w polskim nie stosuj Title Case. Pisz `Ustawienia`,
   `Obszar roboczy`, `Zadania w tle` — tylko pierwsza litera zdania/etykiety.
4. **Placeholdery zostaw dokładnie tak, jak są**: `{count}`, `{name}`, `{path}`,
   `{seconds}`, `{model}` itd. Nie zmieniaj ich nazw ani nie dodawaj spacji w środku.
   Kolejność w zdaniu możesz zmienić, jeśli polski tego wymaga.
5. **Nie tłumacz**: nazw produktów i technologii (`DSH`, `DeepSeek`, `Atari`,
   `Hatari`, `GitHub`, `MCP`, `API`, `JSON`, `FAT16`, `TOS`), identyfikatorów
   technicznych, nazw plików, komend (`/model`, `/export`), skrótów klawiszowych
   (`Ctrl+C`), oraz symboli i emoji.
6. **Liczby mnogie**: runtime zna tylko klucze `.one` i `.other`. Dla `.one`
   użyj liczby pojedynczej, dla `.other` formy dla 2+ („2 zadania w tle”,
   „5 plików”). Nie próbuj obejść tego ograniczenia.
7. **HTML/znaczniki**: jeśli w wartości są znaczniki (np. `<b>`, `<code>`),
   zachowaj je bez zmian.
8. **Interpunkcja**: używaj polskich cudzysłowów „…”, półpauzy – i wielokropka …
   Dopuszczalne są też zwykłe `"` gdy wartość jest kodem/ścieżką.

## Terminologia (trzymaj się ściśle)

| Angielski | Polski |
|---|---|
| session | sesja |
| conversation | rozmowa |
| chat | czat |
| turn | tura |
| message | wiadomość |
| workspace | obszar roboczy |
| working directory | katalog roboczy |
| tool | narzędzie |
| tool call | wywołanie narzędzia |
| background job | zadanie w tle |
| task | zadanie |
| agent | agent |
| subagent | podagent |
| skill | umiejętność |
| goal | cel |
| plan | plan |
| step | krok |
| deliverable | plik wynikowy |
| artifact | artefakt |
| approval | zatwierdzenie |
| permission | uprawnienie |
| access | dostęp |
| provider | dostawca |
| model | model |
| reasoning effort | nakład rozumowania |
| context window | okno kontekstu |
| max tokens | limit tokenów |
| token | token |
| API key | klucz API |
| transcript | transkrypt |
| sidebar | panel boczny |
| attachment | załącznik |
| schedule | harmonogram |
| reference | odwołanie |
| stream | strumień |
| retry | ponów |
| compact | kompaktowy |
| verbose | szczegółowy |
| plugin | wtyczka |
| preset | preset |
| settings | ustawienia |
| general | ogólne |
| theme | motyw |
| appearance | wygląd |
| mount | montowanie |
| gateway | brama |
| workflow | przepływ pracy |
| checkpoint | punkt kontrolny |
| snapshot | zrzut |
| queue | kolejka |
| pending | oczekujący |
| running | w trakcie |
| completed | ukończone |
| failed | niepowodzenie |
| cancelled | anulowane |
| idle | bezczynny |
| stop | zatrzymaj |
| dismiss | odrzuć |
| copy | kopiuj |
| paste | wklej |
| expand | rozwiń |
| collapse | zwiń |
| search | szukaj |
| filter | filtruj |
| reset | przywróć domyślne |
| apply | zastosuj |
| save | zapisz |
| delete | usuń |
| edit | edytuj |
| add | dodaj |
| remove | usuń |
| close | zamknij |
| open | otwórz |
| loading | wczytywanie |
| error | błąd |
| warning | ostrzeżenie |
| hint | podpowiedź |
| empty state | (opisz naturalnie: „Brak …”) |
| not configured | nieskonfigurowane |
| read-only | tylko do odczytu |
| unavailable | niedostępne |
| unsupported | nieobsługiwane |

## Format pliku wyjściowego

Zwykły obiekt JSON, płaski, klucze **identyczne** jak w źródle, wartości — tekst
polski. Bez komentarzy, bez dodatkowych kluczy, bez `null`, bez pustych wartości.
Plik musi być poprawnym JSON w UTF-8, z polskimi znakami diakrytycznymi
wpisanymi dosłownie (nie jako `\uXXXX`).
