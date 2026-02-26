# AGENTS: Better Markers

## Co to za projekt

Better Markers to plugin do OBS, ktory pozwala dodawac markery podczas nagrywania i automatycznie eksportowac je do:
- Adobe Premiere Pro (XMP sidecar + embed do MP4/MOV),
- DaVinci Resolve (FCPXML z markerami timeline),
- Final Cut Pro na macOS (FCPXML z markerami clip).

Plugin dziala tylko dla aktywnego nagrywania (nie podczas pauzy) i zapisuje artefakty obok pliku nagrania.

## Jak to dziala (wysoki poziom)

1. OBS emituje eventy nagrywania (`started/stopped/paused/unpaused` i `file_changed` przy split recording).
2. `RecordingSessionTracker` trzyma aktualny stan sesji, fps i sciezke pliku.
3. `MarkerController` tworzy marker (z UI, hotkeya albo szablonu), wylicza frame i przekazuje dane do sinkow eksportu.
4. Sinki dzialaja niezaleznie:
   - `PremiereXmpSink` zapisuje sidecar i probuje embed do MP4/MOV,
   - `ResolveFcpxmlSink` generuje plik `.better-markers.resolve.fcpxml`,
   - `FinalCutFcpxmlSink` (macOS) generuje `.better-markers.fcp.fcpxml`.
5. Gdy embed sie nie uda, zadanie trafia do `RecoveryQueue` i jest ponawiane po starcie pluginu.

## Najwazniejsze katalogi i pliki

- `src/plugin-main.cpp`: wejscie pluginu, menu/dock, podpiecie callbackow OBS, inicjalizacja controllera i hotkeyow.
- `src/bm-recording-session-tracker.*`: stan nagrania, path aktualnego pliku, timing oparty o packet DTS + fallback monotoniczny.
- `src/bm-marker-controller.*`: glowna orkiestracja markerow i eksportu.
- `src/bm-settings-dialog.*` + `src/bm-template-editor-dialog.*`: UI ustawien i szablonow.
- `src/bm-scope-store.*` + `src/bm-models.*`: zapis/odczyt konfiguracji i modeli.
- `src/bm-*-sink.*`, `src/bm-fcpxml-writer.*`, `src/bm-xmp-sidecar-writer.*`, `src/bm-mp4-mov-embed-engine.*`: eksport i serializacja.
- `tests/`: testy konfiguracji i serializacji FCPXML.
- `data/locale/*.ini`: lokalizacje UI.

## Dane i artefakty

- Konfiguracja pluginu:
  - `.../obs-studio/plugin_config/better-markers/stores/global-store.json`
  - `.../obs-studio/plugin_config/better-markers/stores/profiles/<profil>/profile-store.json`
- Kolejka recovery embedu:
  - `pending-embed.json` w katalogu stores.
- Dla nagrania `<name>.mp4/.mov`:
  - `<name>.xmp`
  - `<name>.better-markers.resolve.fcpxml`
  - `<name>.better-markers.fcp.fcpxml` (macOS)

## Build, uruchomienie, testy

- Szybki dev flow na macOS:
  - `./scripts/dev/configure.sh`
  - `./scripts/dev/build-install.sh`
- Auto-przebudowa:
  - `./scripts/dev/watch-install.sh`
- Testy:
  - `cmake --build build_macos --config RelWithDebInfo --target better-markers-tests`
  - `ctest --test-dir build_macos -C RelWithDebInfo --output-on-failure`

## Zasady pracy nad repo

- Rób male, czeste commity dla logicznych zmian.
- Nie mieszaj refaktoru z nowa funkcja w jednym commicie.
- Przy zmianach w eksporcie sprawdzaj wszystkie aktywne targety (Premiere/Resolve/Final Cut).
- Przy zmianach w hotkeyach/focus policy przejdz checklista z `docs/focus-qa-checklist.md`.

## Final Code Formatting Gate

- Before finishing any task, always run `clang-format-19` and `gersemi` on changed code.
- These tools are the baseline for GitHub Actions validation, so code must always pass both checks.
- If either tool reports issues, fix the code and rerun checks until everything is clean.

## Localization Maintenance

- Keep `data/locale/en-US.ini` as the source key set.
- If any locale key is added, removed, or renamed in `en-US.ini`, apply the same key change and provide actual translations (not placeholders or default English strings) in all maintained locale files in the same change set, ensuring 100% translation coverage.
- Maintained locale files are:
  - `an-ES.ini`
  - `ar-SA.ini`
  - `az-AZ.ini`
  - `be-BY.ini`
  - `bg-BG.ini`
  - `bn-BD.ini`
  - `ca-ES.ini`
  - `cs-CZ.ini`
  - `da-DK.ini`
  - `de-DE.ini`
  - `el-GR.ini`
  - `en-GB.ini`
  - `en-US.ini`
  - `es-ES.ini`
  - `et-EE.ini`
  - `eu-ES.ini`
  - `fa-IR.ini`
  - `fi-FI.ini`
  - `fil-PH.ini`
  - `fr-FR.ini`
  - `gd-GB.ini`
  - `gl-ES.ini`
  - `he-IL.ini`
  - `hi-IN.ini`
  - `hr-HR.ini`
  - `hu-HU.ini`
  - `hy-AM.ini`
  - `id-ID.ini`
  - `it-IT.ini`
  - `ja-JP.ini`
  - `ka-GE.ini`
  - `kab-KAB.ini`
  - `kmr-TR.ini`
  - `ko-KR.ini`
  - `lo-LA.ini`
  - `ms-MY.ini`
  - `nb-NO.ini`
  - `nl-NL.ini`
  - `pl-PL.ini`
  - `pt-BR.ini`
  - `pt-PT.ini`
  - `ro-RO.ini`
  - `ru-RU.ini`
  - `si-LK.ini`
  - `sk-SK.ini`
  - `sl-SI.ini`
  - `sr-CS.ini`
  - `sr-SP.ini`
  - `sv-SE.ini`
  - `th-TH.ini`
  - `tl-PH.ini`
  - `tr-TR.ini`
  - `ug-CN.ini`
  - `uk-UA.ini`
  - `vi-VN.ini`
  - `zh-CN.ini`
  - `zh-TW.ini`
