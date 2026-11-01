# Discadia

Un launcher universale, modulare e leggero per console retrogaming basate su supporti fisici (CD/DVD/USB).
Scritto in C e SDL2, progettato per essere estremamente veloce anche su hardware minimo (es. Raspberry Pi Zero/1).

## Caratteristiche
- **UI in C/SDL2**: Avvio in pochi secondi, footprint di memoria minimo.
- **Sistema Engine Universale**: Configura qualsiasi emulatore, porta nativa o tool (Proton, RetroArch) tramite file JSON.
- **Temi Completamente Custom**: Colori, layout, font e animazioni gestiti via JSON, modificabili dalle impostazioni.
- **Supporto Media Fisici**: Rilevamento automatico e montaggio di CD/DVD/USB con metadati.
- **Store Integrato**: Scarica temi, engine e app homebrew testate da un indice GitHub controllato.

## Installazione Rapida
```bash
./scripts/fetch-deps.sh
make
sudo make install
```

