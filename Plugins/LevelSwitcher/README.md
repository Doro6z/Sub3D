# Level Switcher

![Level Switcher overview](Docs/LevelSwitcherOverview.png)


> Editor tool for fast level navigation in Unreal Editor

## Overview
Level Switcher is an editor tool designed to improve ergonomics and reduce navigation time in multi-map projects.
It scans the project for all level assets and centralizes them in a dedicated panel with favorites and startup configuration.
Access it via Window > Level Switcher or the toolbar icon.

## Key Features
- Scan and list all levels (UWorld assets) in one panel
- Search levels by name or path
- Favorites with drag-and-drop reordering
- Startup level selection (dropdown, drag and drop, or context menu)
- Context actions: set startup level, add/remove favorites, open in Content Browser, copy reference, reveal in Explorer (Windows)

## Installation
### Option 1: Install via Fab
1. Install from Fab
2. Enable in Edit > Plugins > Level Switcher
3. Restart Unreal Editor

### Option 2: Manual
1. Copy the `LevelSwitcher` folder to your project `Plugins` directory
2. Enable in Edit > Plugins > Level Switcher
3. Restart Unreal Editor

## Quick Start
1. Open Window > Level Switcher (or use the toolbar icon)
2. Double-click a level to open it
3. Click the star to add or remove favorites
4. Use the Startup dropdown or right-click to set the startup level

## Technical Specifications
- Engine: Unreal Engine 5.0+
- Type: Editor-only utility (zero runtime overhead)
- Platform: Win64
- Dependencies: None

## Included Content
- Level Switcher editor tool
- Documentation (README)
- Source code (C++)

## Documentation
- Docs/LevelSwitcher_UserGuide.md

## Changelog
### v1.0.0 - YYYY-MM-DD
- Initial release
