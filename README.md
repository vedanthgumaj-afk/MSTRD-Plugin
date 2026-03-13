# Soft Clipper (MSTRD) — VST3 / AU Plugin

![MSTRD Plugin](assets/plugin_ui.jpg)

A professional saturation plugin built with JUCE 8 for macOS Apple Silicon (M1–M5) and Windows x64. Four distinct clipping modes, 4× IIR polyphase oversampling, VCA compression, and a full preset system.

---

## Modes

| Mode | Character | DSP |
|------|-----------|-----|
| **CLEAN** | Transparent, linear-feeling | Symmetric tanh at threshold |
| **KNOCK** | Bright, snappy, transient-forward | Dual envelope-follower transient boost → tanh |
| **VD** | Warm, vintage, 2nd-harmonic body | Asymmetric tanh + DC block |
| **GRIT** | Pumping VCA crunch | Stereo-linked compressor (8:1) → tanh ceiling + DC block |

### CLEAN
Symmetric tanh waveshaping. Low coloration — works like a transparent limiter. Great for bus processing where you want loudness without character.

### KNOCK
A dual envelope-follower (1 ms fast / 10 ms slow) detects transients and boosts them before the tanh stage. Makes hi-hats cut, snares snap, and 808 attacks pop. Reference: Pi'erre Bourne, OG Parker, Maaly Raw.

### VD (Vintage Drive)
Asymmetric tanh adds 2nd-harmonic content (tube/tape character). A Julius O. Smith DC blocker keeps the signal centered. Reference: J Dilla, Pete Rock, classic MPC boom bap.

### GRIT
A stereo-linked VCA compressor (8:1 ratio, 1 ms attack, 150 ms program-dependent release) hits the signal hard, then a tanh ceiling catches the peaks. The release slows up to 4× when the compressor is deep in gain reduction, creating natural pump and breathe. DC blocked on output.

---

## Parameters

| Parameter | Range | Default | Notes |
|-----------|-------|---------|-------|
| Drive | 0 – 24 dB | 6 dB | Input gain into the clipping stage |
| Threshold | −24 – 0 dB | −6 dB | Clip ceiling |
| Post Gain | −12 – +12 dB | 0 dB | Output trim |
| Mix | 0 – 100 % | 100 % | Dry/wet blend |
| Asymmetry | 0 – 0.3 | 0.08 | 2nd-harmonic bias (advanced) |
| Knee Width | 0 – 20 dB | 0 dB | Soft knee amount (advanced) |

All parameters are fully automatable and saved per DAW session.

---

## Oversampling

The plugin runs at **4× IIR polyphase oversampling** by default to minimize aliasing at high drive settings. Click the **OS** button in the preset bar to toggle between **4× OS** and **2× OS** (lower CPU, useful on older machines). The swap happens at the next audio block with a brief click — intended for setup, not automation.

---

## Preset System

**5 factory presets** (read-only):

| Preset | Mode | Character |
|--------|------|-----------|
| Init | KNOCK | Default starting point |
| Clean Open | CLEAN | Transparent loudness |
| Knock Snap | KNOCK | Aggressive transient crunch |
| VD Warmth | VD | Vintage body blend |
| Grit Crush | GRIT | Heavy VCA pump |

**User presets** are saved as XML files in:
```
~/Library/Application Support/Soft Clipper/Presets/   (macOS)
```

Use the **SAVE** button to name and save your own presets. Use **DEL** to delete the currently selected user preset.

---

## Build

### Prerequisites — macOS

| Tool | Notes |
|------|-------|
| Xcode | Install from App Store, then: `xcode-select --install` |
| CMake ≥ 3.22 | `brew install cmake` or https://cmake.org/download/ |

### Prerequisites — Windows

| Tool | Notes |
|------|-------|
| Visual Studio 2022 | "Desktop development with C++" workload |
| CMake ≥ 3.22 | Download from cmake.org, tick "Add to PATH" |

### Build — macOS (Apple Silicon, M1–M5)

```bash
git clone https://github.com/vedanthgumaj-afk/Soft-Clipper-Vst.git
cd Soft-Clipper-Vst

cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build --config Release --parallel
```

JUCE 8 is downloaded automatically by CMake FetchContent on first build (~200 MB).

**Plugin locations after build (auto-installed):**

| Format | Location |
|--------|----------|
| AU | `~/Library/Audio/Plug-Ins/Components/Soft Clipper.component` |
| VST3 | `~/Library/Audio/Plug-Ins/VST3/Soft Clipper.vst3` |

### Build — Windows (x64, VST3 only)

```powershell
git clone https://github.com/vedanthgumaj-afk/Soft-Clipper-Vst.git
cd Soft-Clipper-Vst

cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
```

VST3 is installed to `C:\Program Files\Common Files\VST3\Soft Clipper.vst3`.
Run PowerShell as Administrator if you get an access denied error.

### Rescan in your DAW after building

- **Logic Pro**: Settings → Plug-in Manager → Reset & Rescan
- **Ableton Live**: Options → Manage Plug-ins → Rescan
- **Reaper**: Options → Preferences → Plug-ins → VST → Re-scan
- **FL Studio**: Options → Manage Plugins → Start Scan

---

## Project Structure

```
Soft-Clipper-Vst/
├── CMakeLists.txt
├── README.md
└── Source/
    ├── PluginProcessor.h/.cpp   — DSP engine (4 modes, oversampling, VCA, meters)
    ├── PluginEditor.h/.cpp      — UI (knobs, mode selector, meters, transfer curve)
    └── PresetManager.h/.cpp     — Factory + user preset system
```

---

## Troubleshooting

### "Cannot be opened because the developer cannot be verified" (macOS Gatekeeper)
```bash
sudo xattr -rd com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/"Soft Clipper.vst3"
sudo xattr -rd com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/"Soft Clipper.component"
```

### AU not showing in Logic Pro
```bash
auval -v aufx Sclp Ynme
```
If validation fails, ensure `BUNDLE_ID`, `PLUGIN_MANUFACTURER_CODE`, and `PLUGIN_CODE` in CMakeLists.txt are set correctly.

### FetchContent slow / timeout
Clone JUCE manually and point CMake at it:
```bash
git clone --depth 1 --branch 8.0.4 https://github.com/juce-framework/JUCE.git
```
Then replace `FetchContent_Declare(JUCE ...)` in CMakeLists.txt with:
```cmake
add_subdirectory(/path/to/JUCE)
```

---

## Tested

| DAW | Format | Platform |
|-----|--------|----------|
| Logic Pro 10.8 | AU | macOS arm64 |
| Ableton Live 12 | VST3 | macOS arm64 |
| Reaper 7 | VST3 + AU | macOS arm64 |
| Ableton Live 12 | VST3 | Windows x64 |
| FL Studio 21 | VST3 | Windows x64 |

> Pro Tools (AAX) requires a paid Avid certificate — not included.
