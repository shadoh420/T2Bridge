# T2Bridge AutoKit

T1-style automatic repair kit usage for Tribes 2.

## Features

- Automatically uses repair kits when health drops below a configurable threshold
- Default threshold: 65% health
- Configurable via console commands
- Toggle on/off with a keybind

## Installation

1. Download the latest release
2. Copy `bin/version.dll` to your `GameData/` folder (same folder as Tribes2.exe)
3. Copy `bin/T2Bridge.dll` to your `GameData/` folder
4. Copy `t2bridge_autokit.cs` to `GameData/base/scripts/autoexec/`
5. Start the game - the autokit activates automatically

## Usage

### Console Commands

| Command | Description |
|---------|-------------|
| `AutoKit_Status()` | Show current health and status |
| `AutoKit_Toggle(1)` | Toggle autokit on/off |
| `AutoKit_SetThreshold(N)` | Set health threshold 1-99% (default: 65%) |

### Keybind

Bind a key in Options > Controls > find "AutoKitBind", or use console:
```
bindCommand(keyboard, "k", make, "AutoKitBind", true);
```

## How It Works

Tribes 2's TorqueScript doesn't expose player health on the client side. This project uses a DLL that hooks into the game engine to read the player's damage value from memory, then writes it to a file that the TorqueScript reads.

- `version.dll` - Auto-loader proxy (loads T2Bridge.dll when game starts)
- `T2Bridge.dll` - Hooks the game and reads player health
- `t2bridge_autokit.cs` - TorqueScript that polls health and uses repair kits

## Building from Source

### Requirements

- Visual Studio 2022 (Community or higher)
- Windows SDK 10.0

### Build

1. Open `T2Bridge.sln` in Visual Studio
2. Select Release | x86 configuration
3. Build Solution (Ctrl+Shift+B)

Output files will be in `bin/`:
- `T2Bridge.dll` - Main bridge DLL
- `version.dll` - Auto-loader proxy

### Project Structure

```
T2Bridge/
├── dllmain.cpp           # Main bridge DLL source
├── version_proxy.cpp     # Auto-loader proxy source
├── version.def           # Export definitions for version.dll
├── T2Bridge.sln          # Visual Studio solution
├── T2Bridge.vcxproj      # T2Bridge project
├── versionproxy.vcxproj  # version.dll proxy project
├── t2bridge_autokit.cs   # TorqueScript autokit
├── detours/
│   ├── include/          # Detours headers
│   └── lib/              # Detours static library (x86)
└── bin/                  # Build output (pre-built binaries included)
```

## Technical Details

**Hook Target:** `SetRenderPosition` at `0x005D98C0`
- Called every frame when rendering the player
- First parameter is the player object pointer

**Damage Offset:** `0x7F0` (mDamage field)
- Value range: 0.0 (full health) to 1.0 (dead)
- Health % = (1.0 - mDamage) × 100

**Validation Offsets:**
- `0x800`: Must equal 0 (distinguishes player from other objects)
- `0x26C`: Must be non-zero (ensures valid player state)

**Why version.dll proxy?**
- `dinput.dll` and `dinput8.dll` are protected by Windows KnownDLLs
- `version.dll` is not in KnownDLLs, so the game loads ours first
- Our proxy forwards all calls to the real system version.dll

## Future Improvements

The current implementation uses file-based IPC (DLL writes to file, script polls file). A cleaner approach would be to find `Con::setVariable()` in Tribes2.exe and call it directly from the DLL to set TorqueScript global variables like `$T2Bridge::Health` in memory. This would eliminate disk I/O and allow exposing more values (energy, velocity, etc.).

## Requirements

- Tribes 2 (TribesNext or similar)
- Windows

## License

MIT License - See LICENSE file

## Credits

Built using [Microsoft Detours](https://github.com/microsoft/Detours) library for function hooking.
