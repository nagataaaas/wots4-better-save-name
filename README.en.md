# Better Save Name

[日本語](README.md) | [Changelog](CHANGELOG.en.md)

> [!NOTE]
> This Mod is for the [Steam version of Way of the Samurai 4](https://store.steampowered.com/app/312780/Way_of_the_Samurai_4/) only.

In the Save, Load and Delete lists, shows how far each save has progressed instead of a file name such as `S4-DATA_01`.

## Features

- **What a save holds, on the first line of its row**: the slot number, the day and time of day, and the latest event, such as `01: Day 1 Night [Frn] The Telegraph`.
  - `[Pra]`, `[Sho]`, `[Frn]` or `[True]` before the event is the route its box belongs to on the event chart (Prajna, Shogunate, Foreigners, or the true route). Common events have none.
  - Event names are the event chart's, in the game's language.
  - A save in the prologue shows "Prologue" instead of a day.
  - A label too long for its row is drawn narrower, and if it still does not fit, the end of the event name is replaced with "...".
- The other parts of a row (date and time, protagonist name, difficulty) do not change, and neither does saving or loading.
- Installed together with [autosave](https://github.com/nagataaaas/wots4-autosave), the autosave row is shown the same way, such as `AUTOSAVE: Day 2 Morning ...`.

The latest event is the one you last accepted or finished. For a save made after the time of day has passed, it is the latest-stage event among those you accepted, finished or failed.

## Installation

### Automatic installation (recommended)

1. Close the game.
2. Download and run [WayOfTheSamurai4ModInstaller.exe](https://github.com/nagataaaas/wots4-mods/releases/latest/download/WayOfTheSamurai4ModInstaller.exe).
   - If "Windows protected your PC" appears, click "More info" → "Run anyway".
   - When asked whether to allow it to make changes as administrator, click "Yes" (it writes to the game folder).
3. Follow the screen and choose `better_save_name` in the list of Mods. Move with Up and Down, choose with Space and confirm with Enter.

The ModLoader is installed along with it. The same steps update the Mod.

<details>
<summary>Manual installation</summary>

> [!WARNING]
> Install [wots4-mods (ModLoader)](https://github.com/nagataaaas/wots4-mods) first.

1. Close the game.
2. Download `wots4-mod-better-save-name-<version>.zip` from [Releases](../../releases) and extract it.
3. Copy the extracted `mods` folder into the game's installation folder (the folder containing `WayOfTheSamurai4.exe`). If a `mods` folder already exists, merge the contents into it.
4. Confirm that `{game installation folder}/mods/better_save_name/mod.dll` exists, then start the game.

</details>

## Uninstallation

1. Close the game.
2. Delete the `{game installation folder}/mods/better_save_name/` folder.

The Mod does not change your saves, so after removing it the lists show the file names again.
