# Discadia - Retro launcher (C11 + SDL2)

Discadia is a lightweight, themeable, controller-first launcher for
retrogaming consoles built on physical media (CD/DVD/USB) and local
storage. It renders with SDL2 at 60fps on minimal hardware
(Raspberry Pi Zero / 512MB RAM), and everything that is user-visible —
colors, layout, animations, sounds, strings, emulators — is data-driven
through JSON files. Nothing is hardcoded.

- **No hardcoded strings**: every label goes through `locale_get()`; default language is English (`en`).
- **No hardcoded emulators**: engines are JSON files in `<data>/engines/`.
- **No hardcoded look**: themes are JSON + optional assets in `<data>/themes/<name>/`.
- **Game metadata is multilingual**: see [docs/DISC-AUTHORING.md](docs/DISC-AUTHORING.md).

## Requirements

Build-time (Debian/Ubuntu/Raspbian/DietPi names):

```bash
sudo apt install build-essential pkg-config git \
    libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev libsdl2-mixer-dev
```

Runtime: SDL2 libraries above, `curl` (store downloads), optionally
`udisks2` + udev rules for automatic disc mounting (polling fallback
works without them), and any TrueType font (DejaVu is used as fallback).

## Build & run

```bash
make            # release build -> ./discadia
make DEBUG=1    # debug build (-g -O0, verbose logging helpers)
make run        # build and run using ./data as the data dir (no install needed)
make test       # build and run unit tests (tests/test_main.c)
make install    # install binary to /usr/local/bin, data to /etc/discadia
```

Run with verbose logs:

```bash
DISCADIA_DEBUG=1 ./discadia
```

Debug mode also shows an FPS counter and layout borders.

## Data directories

At startup Discadia resolves its data directory in this order:

1. `$DISCADIA_DATA_DIR` (development, points at `./data`)
2. `/etc/discadia` (installed system data)
3. `./data` relative to the current working directory

User state always lives under the XDG dirs:

| Path | Content |
|---|---|
| `~/.config/discadia/config.json` | settings (theme, language, audio, ...) |
| `~/.config/discadia/input-map.json` | button remapping |
| `~/.local/share/discadia/games/` | installed games/apps (library root) |
| `~/.local/share/discadia/saves/` | per-game save folders |
| `~/.cache/discadia/store/` | store index cache + downloads |

All config writes are atomic (tmp file + fsync + rename, `.bak` kept).

## Files YOU must add (optional resources)

The launcher runs fully without any of these — missing pieces fall back
to procedural drawing, synthesized beeps and system fonts. Add them to
make it yours:

| File | Purpose | Notes |
|---|---|---|
| `data/themes/arcade-classic/fonts/title.ttf` | display font for titles/boot | any TTF/OTF |
| `data/themes/arcade-classic/fonts/body.ttf` | UI body font | any TTF/OTF |
| `data/themes/arcade-classic/sounds/navigate.wav` | carousel move blip | PCM WAV |
| `data/themes/arcade-classic/sounds/select.wav` | confirm sound | PCM WAV |
| `data/themes/arcade-classic/sounds/back.wav` | back/cancel sound | PCM WAV |
| `data/themes/arcade-classic/sounds/cd_insert.wav` | disc inserted chime | PCM WAV |
| `data/themes/arcade-classic/assets/frame.png` | cabinet frame overlay | transparent PNG |
| `data/themes/arcade-classic/assets/bg-default.jpg` | home background | JPG/PNG |
| `data/themes/arcade-classic/assets/icons/settings.png` | top-bar icon | PNG (else drawn) |
| `data/themes/arcade-classic/assets/icons/cd.png` | disc badge on covers | PNG (else drawn) |
| `store-repo/assets/covers/<pkg>.jpg` | store package covers | square-ish JPG/PNG |
| `store-repo/packages/<id>/<file>.tar.gz` | actual package payloads | you host/push these |

After `make install`, all paths above map to `/etc/discadia/...`.
To ship a new theme/language/engine, drop a folder/file under
`data/` (or install packages via the Store) — no recompilation needed.

## Repository layout

```
src/            C sources (app/, ui/, theme/, input/, library/, media/,
                engine/, settings/, store/, audio/, i18n/, system/, util/)
data/           shipped system data (locales, themes, engines)
third_party/    vendored cJSON
config/         udev rule + systemd service templates
scripts/        dependency fetch, disc authoring, CD simulation
docs/           architecture, themes, engines, disc authoring guides
store-repo/     example store index tree (publish it on GitHub)
tests/          unit tests (make test)
examples/       ready-to-use metadata templates
```

## Documentation

- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) — module map and runtime flow
- [docs/THEMES.md](docs/THEMES.md) — full `theme.json` reference
- [docs/ENGINES.md](docs/ENGINES.md) — engine definitions and variables
- [docs/DISC-AUTHORING.md](docs/DISC-AUTHORING.md) — how to prepare game discs/USBs
- [docs/STORE.md](docs/STORE.md) — publishing your store repository
- [docs/INSTALL-DIETPI.md](docs/INSTALL-DIETPI.md) — setup on DietPi / Raspberry Pi

## Controls

| Action | Controller (default) | Keyboard |
|---|---|---|
| Navigate carousel | D-Pad Left/Right | Arrows |
| Move in lists | D-Pad Up/Down | Arrows |
| Confirm / Launch | A | Enter |
| Back | B | Escape |
| Menu (Start) | START | – |
| Contextual | SELECT | Space |
| Quit anywhere | – | Q |

Remap from Settings → Remap Buttons (persisted immediately).

## License

See [LICENSE](LICENSE).
