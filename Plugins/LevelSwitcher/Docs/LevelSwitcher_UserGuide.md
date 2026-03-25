# Level Switcher User Guide

## Overview
Level Switcher is an editor tool for fast level navigation in Unreal Engine. It scans the project for level assets and centralizes access to levels, favorites, and the startup level in one panel.

## Installation
1. Copy the `LevelSwitcher` folder into your project's `Plugins` directory.
2. In Unreal Editor, open Edit > Plugins and enable Level Switcher.
3. Restart Unreal Editor.

## Access
- Window > Level Switcher
- Toolbar icon (Level Switcher)

## Core Workflow
1. Open the Level Switcher panel.
2. Double-click a level to open it.
3. Click the star to add or remove favorites.
4. Use the Startup dropdown or right-click a level to set the startup level.

## Favorites
Favorite levels are grouped into a dedicated top section for faster access and a streamlined workflow.

## Context Actions
- Set as startup level
- Add/remove favorites
- Open in Content Browser
- Copy reference
- Reveal in Explorer (Windows)

## Notes
- Editor-only utility. No runtime overhead.
- Requires Asset Registry indexing to list maps. This is handled natively by Unreal Engine, but if you use a third-party system to generate or manage levels, ensure those assets are properly indexed.
