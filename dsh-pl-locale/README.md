# dsh-locale-pl — polski pakiet językowy dla DSH

Pakiet kliencki, który dodaje **polski (`pl`)** do przełącznika języka w interfejsie
DSH. Język jest rejestrowany na usłudze `locale` jako definicja z fallbackiem do
`en`, a słowniki głównych ekranów są rejestrowane per przestrzeń nazw. Klucze,
których nie tłumaczymy, nadal wyświetlają się po angielsku — dzięki temu pakiet
można rozbudowywać partiami bez psucia interfejsu.

## Co tłumaczy

| Grupa | Przestrzenie nazw |
|---|---|
| Wspólne | `common` |
| Ustawienia | `settings`, `settings.locale`, `settings.theme`, `settings.models`, `settings.agentPreset`, `settings.permission` |
| Czat i rozmowa | `chat`, `conversation` |
| Panele boczne | `sidebar`, `sidebarRight`, `sidebarFiles`, `sidebarDocumentPreview`, `sidebarImage`, `sidebarPdf`, `sidebarCodePreview`, `documentMarkdown`, `documentHtml` |
| Dialogi i stany | `model`, `command`, `slash.menu`, `question`, `approval`, `plan`, `job`, `goal`, `feedback`, `permission.access`, `reference` |

Poza zakresem pierwszej partii (świadomie, wyświetlają się po angielsku):
`trajectory`, `workspace`, `cordis`, `deliverables`, `subagent`,
`settings.plugins`, `settings.pluginInventory`, `schedule.catalog`,
`workflowRun`, `skill`, `open-in-app`, `session-log-download`.

## Jak to działa

- `lib/index.js` — połowa hostowa: pusta, istnieje po to, by pakiet dał się
  zamontować jako zwykły wiersz Loadera.
- `lib/client.js` — **plik generowany**: pojedyncze wywołanie
  `window.__ModuleLoader__.load(...)` rejestrujące język i słowniki. Bez importów,
  więc nie wymaga bundlera.
- `translations/<przestrzeń>.json` — źródło prawdy dla tłumaczeń.
- `catalog/<przestrzeń>.json` — wyekstrahowane, wysyłane słowniki `en`/`zh`
  z pakietów `@deepseek-ai` (materiał odniesienia, nie edytujemy).

Montowanie jako wiersz Loadera (`insert` w warstwie łaty profilu) jest tym, co
powoduje, że host składa graf startowy i serwuje bundel pod `/plugins`.

## Instalacja

```sh
cd /Users/xxx/Documents/Deepseek_test/dsh-pl-locale
python3 tools/build_bundle.py          # generuje lib/client.js
python3 tools/install.py --dry-run     # pokaż, co zostanie zapisane
python3 tools/install.py               # symlink + wpis w cordis.patch.yml
```

Potem **zrestartuj `dsh web`** i wybierz język w *Settings → General*.
Sam odśwież strony nie wystarczy: graf modułów klienckich jest składany przy
starcie hosta.

Odinstalowanie:

```sh
python3 tools/install.py --uninstall
```

## Rozbudowa o kolejne ekrany

```sh
python3 tools/extract_dictionaries.py --out catalog   # odśwież materiał źródłowy
# przetłumacz catalog/<ns>.json → translations/<ns>.json
python3 tools/build_bundle.py
```

`build_bundle.py` odrzuci tłumaczenie, którego zbiór kluczy nie zgadza się ze
wysłanym słownikiem angielskim, więc literówka w kluczu nie przejdzie po cichu.

## Ograniczenia

- Runtime zna tylko formy liczby mnogiej `.one` i `.other`; nie ma polskich
  form `.few`/`.many`, więc dla 2–4 użyta jest forma dla 2+.
- Teksty rejestrowane poza ścieżką renderowania slotów (np. opisy komend
  zapisane przy rejestracji) zachowują język z chwili rejestracji.
- Pakiet nie tłumaczy treści generowanych przez model — tylko interfejs.
