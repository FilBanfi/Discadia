# Discadia Architecture

## Core Principles
1. **No Hardcoded Emulators**: Everything is an 'engine' defined by JSON.
2. **Minimal Footprint**: C + SDL2, designed to run on 512MB RAM devices.
3. **Universal Media**: Treats CD-ROMs, USB drives, and internal storage with the same metadata-driven logic.

## UI Layout
- **Top Bar**: Game Title (Left) | Settings Icon + Date/Time (Right)
- **Center**: Horizontal carousel of covers. Selected item is scaled up.
- **Bottom**: Description and metadata (platform, year, publisher, etc.)
- **Sorting**: Disc-based games first (alphabetical), then installed apps/games (alphabetical). No visual dividers.
