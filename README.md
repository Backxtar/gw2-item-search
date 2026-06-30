# Item Search

A [Nexus](https://raidcore.gg/Nexus) addon for **Guild Wars 2** that lets you search for items across your **entire account** — bank, material storage, shared inventory, every character's bags and equipped gear, and the legendary armory — all from a single in-game window.

## Features

- **Account-wide search** — type part of an item name and instantly see everywhere you own it, with stack counts.
- **All storage locations in one place:**
  - Account bank (grouped by bank tab)
  - Material storage
  - Shared inventory slots
  - Every character's bags
  - Equipped gear
  - Legendary armory
- **Per-character tabs** with class/elite-spec icons and signature profession colours. Tabs are grouped by base profession, then elite specialization, so characters never appear in a random order.
- **Rich tooltips** — rarity, level, stat prefix with computed attribute values, defense/weapon strength, food/utility effects, and nested runes, sigils and infusions slotted into gear.
- **Quick access** via the Nexus toolbar icon or a configurable keybind.
- **German & English** localization (selectable in the options).
- **Local caching** of item metadata to keep things fast and reduce API calls.

## Installation

1. Install the [Nexus](https://raidcore.gg/Nexus) addon loader for Guild Wars 2.
2. Download the latest `ItemSearch.dll` from the [Releases](../../releases) page.
3. Drop it into your `addons` folder (e.g. `…/Guild Wars 2/addons/`).
4. Launch the game — *Item Search* loads automatically and appears in the Nexus Quick Access bar.

## Setup

The addon needs a Guild Wars 2 API key to read your account.

1. Create a key at the [official ArenaNet API Key Manager](https://account.arena.net/applications).
2. Grant at least these permissions:
   - `account`
   - `inventories`
   - `characters`
   - `builds` *(optional — enables detection of the active elite specialization per character; without it characters fall back to their core profession)*
3. Open the *Item Search* options in Nexus and paste your API key.
4. Pick your language (German / English) if needed.

Your key and settings are stored locally in `addons/ItemSearch/settings.json`. Item metadata is cached in `addons/ItemSearch/items_cache.json`.

## Usage

- Click the toolbar icon or press your bound key to toggle the search window.
- Use the **Account** tab to search everything at once, the **Bank** tab for bank/material storage, or a **character** tab to inspect a single character's gear and inventory.
- Start typing to filter results live by item name.

## Building from source

The project targets Windows and builds with **Visual Studio 2022+** (MSVC, C++17, x64).

```sh
git clone --recurse-submodules https://github.com/Backxtar/gw2-item-search.git
```

Dependencies are vendored as git submodules (run `git submodule update --init --recursive` if you cloned without `--recurse-submodules`):

- [Nexus API](https://github.com/RaidcoreGG/RCGG-lib-nexus-api)
- [Dear ImGui (RaidcoreGG fork)](https://github.com/RaidcoreGG/imgui)
- [nlohmann/json](https://github.com/nlohmann/json)

Then open `ItemSearch.sln` and build the **Release / x64** configuration, or from the command line:

```sh
msbuild ItemSearch.sln /p:Configuration=Release /p:Platform=x64
```

The output `ItemSearch.dll` can be copied straight into your GW2 `addons` folder.

## Credits

- Author: **Backxtar**
- Built on the [Nexus](https://raidcore.gg/Nexus) addon framework by Raidcore.
- Item, character and account data provided by the official [Guild Wars 2 API](https://wiki.guildwars2.com/wiki/API:Main).

> *Guild Wars 2 and all associated trademarks are property of ArenaNet / NCSoft. This is an unofficial, fan-made addon.*
