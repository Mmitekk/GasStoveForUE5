# GasStoveUE5 — Blue Gas Burner Flame for Unreal Engine 5

[🇷🇺 Русский](#-русский) · [🇬🇧 English](#-english)

---

## 🇷🇺 Русский

VFX синего пламени газовой конфорки: кольцо из ~22 объёмных язычков с бело-голубым корнем, мерцанием, покачиванием, свечением основания и синим светом. Без Niagara-системы — пламя собрано на инстансированных меш-плоскостях с аддитивным материалом, поэтому стабильно выглядит с любого ракурса и почти ничего не стоит.

### Требования

- **Unreal Engine 5.2** — `.uasset` материалы собраны под 5.2. Для других версий движка: удалите скопированные `M_*.uasset`, оставьте PNG и пересоберите материалы скриптом (см. ниже).
- Windows, Python 3 — только для запуска установщика.
- Плагин Niagara — установщик включает его сам.

### Состав

| Путь | Что это |
|---|---|
| `Source/UE52VFX/GasStoveBurnerFlame.h/.cpp` | Актор `AGasStoveBurnerFlame`: кольцо язычков, свет, анимация, `SetLit`/`SetGasLevel` |
| `Content/VFX/GasStove/M_GasFlame.uasset` | Материал язычков (Additive/Unlit) |
| `Content/VFX/GasStove/M_GasGlow.uasset` | Материал свечения основания |
| `Content/VFX/GasStove/Textures/*.png` | Процедурные текстуры: язычок, glow, нойз |
| `Python/install_gas_burner.py` | Установщик в чужой проект |
| `Python/build_gas_stove_vfx.py` | Пересборка материалов внутри редактора |
| `Python/diag_gas_flame.py` | Диагностика (режим бленда, текстуры, инстансы) |
| `InstallGasBurner.bat` | Установка в один клик |

### Установка

> ⚠️ Целевой проект должен быть **закрыт** перед установкой.

#### Вариант A — батник (рекомендуется)

```bat
git clone https://github.com/Mmitekk/Gas-stove-for-UE5.git
```

Двойной клик по `InstallGasBurner.bat` (или перетащите `.uproject` на него). Батник сам подтянет свежие изменения из гита (`git pull`), спросит путь к `.uproject`, скопирует код и контент, пропатчит модуль. Дальше:

1. Перекомпилируйте C++ (Visual Studio или Live Coding `Ctrl+Alt+F11`).
2. Перетащите **Gas Stove Burner Flame** на конфорку.
3. По умолчанию пламя погашено (`bLit=false`) — зажигайте через `SetLit` / `SetGasLevel`.

#### Вариант B — из редактора

1. В `Python/install_target.txt` впишите путь к `.uproject` целевого (закрытого!) проекта.
2. `Tools → Execute Python Script` → `Python/run_install_editor.py`.

#### Вариант C — чистый Python

```bat
python Python\install_gas_burner.py --target "D:\MyProject\MyProject.uproject"
```

Скрипт сам определяет имя модуля, меняет `UE52VFX_API` на ваш макрос, добавляет Niagara в `Build.cs`/`.uproject`, всё перезаписываемое бэкапит в `.bak`.

### Использование

Подстройка внешности — в Details актора: `FlameCount`, `BurnerRadius`, `FlameHeight`, `FlameWidth`, `OutwardTiltDeg`, `FlameIntensity`, `FlickerSpeed/Amount`, `SwayAmplitude/Speed`, `TiltWobbleDeg`, `LightIntensity`, `GasLevel`, `bLit`.

Управление в игре (категория Gas Flame):

- `Set Gas Level (0..1)` — газовая ручка: высота, яркость и свет масштабируются живьём, около нуля гаснет. `GasLevel` в Details работает и без Play.
- `Set Lit (bool)` — кран вкл/выкл.
- `Rebuild Flames` — вызвать после смены `FlameCount`/`BurnerRadius` в рантайме.

Производительность: погашенная конфорка стоит ноль (тик выключен, draw calls нет). Горящая — 1 инстансед draw call + 1 свет без теней. Основной расход — точечные света, держите маленьким `AttenuationRadius`.

---

## 🇬🇧 English

Blue gas-burner flame VFX: a ring of ~22 volumetric tongues with a white-hot root, flicker, sway, base glow and blue light. No Niagara system — the flame is built from instanced mesh planes with an additive material, so it reads well from any angle and costs next to nothing.

### Requirements

- **Unreal Engine 5.2** — bundled `.uasset` materials are built for 5.2. For other engine versions: delete the copied `M_*.uasset`, keep the PNGs and rebuild materials with the script (see below).
- Windows, Python 3 — only needed to run the installer.
- Niagara plugin — enabled automatically by the installer.

### Contents

| Path | What it is |
|---|---|
| `Source/UE52VFX/GasStoveBurnerFlame.h/.cpp` | `AGasStoveBurnerFlame` actor: tongue ring, light, animation, `SetLit`/`SetGasLevel` |
| `Content/VFX/GasStove/M_GasFlame.uasset` | Tongue material (Additive/Unlit) |
| `Content/VFX/GasStove/M_GasGlow.uasset` | Base glow material |
| `Content/VFX/GasStove/Textures/*.png` | Procedural textures: tongue, glow, noise |
| `Python/install_gas_burner.py` | Installer into another project |
| `Python/build_gas_stove_vfx.py` | Rebuild materials inside the editor |
| `Python/diag_gas_flame.py` | Diagnostics (blend mode, textures, instances) |
| `InstallGasBurner.bat` | One-click install |

### Installation

> ⚠️ The target project must be **closed** before installing.

#### Option A — batch file (recommended)

```bat
git clone https://github.com/Mmitekk/Gas-stove-for-UE5.git
```

Double-click `InstallGasBurner.bat` (or drag-drop a `.uproject` onto it). The bat pulls the latest pack from git (`git pull`), asks for the `.uproject` path, copies code and content, patches the module. Then:

1. Rebuild C++ (Visual Studio or Live Coding `Ctrl+Alt+F11`).
2. Drag **Gas Stove Burner Flame** onto a burner.
3. The flame is off by default (`bLit=false`) — ignite via `SetLit` / `SetGasLevel`.

#### Option B — from the editor

1. Write the (closed!) target project `.uproject` path into `Python/install_target.txt`.
2. `Tools → Execute Python Script` → `Python/run_install_editor.py`.

#### Option C — plain Python

```bat
python Python\install_gas_burner.py --target "D:\MyProject\MyProject.uproject"
```

The script auto-detects the module name, swaps `UE52VFX_API` for your macro, adds Niagara to `Build.cs`/`.uproject`, and backs up everything it overwrites as `.bak`.

### Usage

Look tuning — actor Details: `FlameCount`, `BurnerRadius`, `FlameHeight`, `FlameWidth`, `OutwardTiltDeg`, `FlameIntensity`, `FlickerSpeed/Amount`, `SwayAmplitude/Speed`, `TiltWobbleDeg`, `LightIntensity`, `GasLevel`, `bLit`.

Runtime control (Gas Flame category):

- `Set Gas Level (0..1)` — gas knob: height, brightness and light scale live, auto-extinguishes near zero. The `GasLevel` slider also previews in Details without Play.
- `Set Lit (bool)` — valve on/off.
- `Rebuild Flames` — call after changing `FlameCount`/`BurnerRadius` at runtime.

Performance: an unlit burner costs zero (tick disabled, no draw calls). A lit one is 1 instanced draw call + 1 shadowless light. Point lights are the main cost — keep `AttenuationRadius` small.
