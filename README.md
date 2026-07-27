This Readme is written by AI.

# PluginAnalyzer

A professional-grade audio plugin analysis tool built with the JUCE framework. This application allows you to load audio plugins (VST3, VST, AU, etc.) and perform various technical measurements to analyze their characteristics, performance, and audio quality.

## Features

**PluginAnalyzer** offers a comprehensive suite of analysis modes:

*   **Linear Analysis:** Measures the impulse response and frequency response.
*   **Harmonic Analysis:** Analyzes Total Harmonic Distortion (THD) using a sine wave.
*   **THD Sweep:** Measures THD+N across the frequency spectrum.
*   **IMD:** Intermodulation Distortion analysis (SMPTE method).
*   **Hammerstein:** Non-linear impulse response analysis.
*   **White Noise:** Frequency response analysis using white noise.
*   **Sine Sweep:** Traditional frequency sweep analysis.
*   **Oscilloscope:** Real-time waveform visualization.
*   **Dynamics:** Analyzes compression/expansion ratios and envelope characteristics (Attack/Release).
*   **Performance:** Real-time monitoring of CPU usage, average/peak processing times.

**UI & UX:**
*   **SSL-Style Look and Feel:** A dark, professional, and high-contrast interface inspired by classic studio consoles.
*   **Real-time Visualization:** High-performance graphing for spectrums and waveforms.
*   **Plugin Scanning:** Built-in scanner to find and manage your plugin collection.

## Getting Started

### Prerequisites

*   **CMake:** Version 3.22 or newer.
*   **C++ Compiler:** Visual Studio 2026 (Windows), Xcode (macOS), or a C++17-capable GCC/Clang toolchain (Linux).
*   **Git:** Required by CMake to fetch the pinned JUCE dependency on the first configure.

JUCE 8.0.13 is fetched automatically and pinned by `CMakeLists.txt`. A separately
installed Projucer or VST3 SDK is not required for the normal CMake build.

### Building on Windows

```powershell
git clone https://github.com/koto-thing/PluginAnalyzer.git
cd PluginAnalyzer
cmake --preset vs2026
cmake --build --preset vs2026-debug
```

The application is generated under:

```text
out/build/vs2026/PluginAnalyzer_artefacts/Debug/
```

Use `vs2026-release` instead of `vs2026-debug` for a Release build.

### Testing

The Phase 6 calibration suite covers test-signal generation, FFT transfer
measurements, THD/IMD, deterministic Gain/Delay/Clipper/Waveshaper/Compressor
processors, processor lifecycle, FIFO ordering, every analysis mode, and a
headless application startup/settings/shutdown smoke test.

```powershell
cmake --preset vs2026
cmake --build --preset vs2026-debug
ctest --preset vs2026-debug
```

An opt-in five-second quality run also checks sustained processing, dropout
limits, peak working set, and handle count:

```powershell
cmake --preset vs2026 -DPLUGIN_ANALYZER_ENABLE_STRESS_TESTS=ON
cmake --build --preset vs2026-release
ctest --test-dir out/build/vs2026 -C Release -L long --output-on-failure
```

### Building with Ninja

Install Ninja and a suitable compiler environment, then run:

```bash
cmake --preset ninja-debug
cmake --build --preset ninja-debug
ctest --preset ninja-debug
```

`PluginAnalyzer.jucer` is retained temporarily for migration compatibility.
CMake is the authoritative build definition.

### Continuous integration and releases

GitHub Actions runs source-quality checks, Release builds, and CTest on Windows,
macOS, and Linux for pull requests and pushes to `master` or `main`. Linux also
runs the calibrated tests with AddressSanitizer and UndefinedBehaviorSanitizer.
A Windows packaging job runs the opt-in five-second quality test and verifies
the installed executable.

Pushing a semantic-version tag matching the CMake project version creates
Windows, macOS, and Linux packages, SHA-256 checksum files, and SPDX 2.3 SBOMs.
The same workflow can be started manually with a version such as `v1.0.0`.
After every platform passes, the workflow creates a draft GitHub Release. The
`production` deployment then publishes it after any configured approval.

For release approval, create a GitHub Environment named `production` and add a
required reviewer. Code signing is optional by default; set the repository
Actions variable `REQUIRE_CODE_SIGNING` to `true` to make missing credentials
fail the release. Configure these repository Actions secrets as applicable:

* Windows: `WINDOWS_CERTIFICATE_BASE64`, `WINDOWS_CERTIFICATE_PASSWORD`
* macOS: `MACOS_CERTIFICATE_BASE64`, `MACOS_CERTIFICATE_PASSWORD`,
  `MACOS_SIGNING_IDENTITY`, `APPLE_ID`, `APPLE_TEAM_ID`,
  `APPLE_APP_PASSWORD`

External Actions are pinned to full commit SHAs, and Dependabot checks them
weekly. CI receives read-only repository access; only the final release job is
granted `contents: write`.

### Using an existing JUCE checkout

To avoid downloading JUCE, configure with an absolute path to a JUCE 8.0.13
checkout:

```powershell
cmake --preset vs2026 -DFETCHCONTENT_SOURCE_DIR_JUCE=C:/path/to/JUCE
```

### Current implementation status

The build system and core audio/plugin lifecycle have been stabilised. Hosted
mono and stereo effects are prepared with the active device sample rate and
block size, and their processed signal is returned to the audio device.
Analysis samples now cross a fixed-capacity FIFO into a dedicated worker thread;
FFT, distortion, dynamics, and performance aggregation do not run in the audio
callback. The UI reads immutable result snapshots and live parameters cross the
audio boundary atomically. Linear analysis reports the input/output transfer
function with plugin latency removed from phase. Harmonic analysis uses
FFT-bin-aligned tones, Hann-window amplitude correction, a 20 Hz–20 kHz
measurement band, and guarded THD/THD+N calculations. Performance results
include average, peak, p95, and p99 processing time as well as FIFO drop counts.
Plug-in discovery runs in the background and probes candidates in an isolated
child process. Scan results, blacklists, paths, FFT preferences, and the active
audio-device state persist between launches. The settings dialog controls the
live JUCE device manager, and mode-specific controls stay out of unrelated
analysis views. THD Sweep, IMD, Hammerstein, White Noise, Sine Sweep, and
Dynamics have bounded measurement implementations. The Phase 6
reference-processor suite now guards the shared signal, FFT, distortion,
lifecycle, FIFO, and application-lifecycle paths; these modes remain marked
experimental while broader third-party plug-in compatibility data is collected.

## Usage

1.  **Launch the Application:** Run `PluginAnalyzer.exe`.
2.  **Settings:** Click the **Settings** button to configure:
    *   Audio Device (Input/Output)
    *   Sample Rate & Buffer Size
    *   FFT Order (Resolution)
    *   Plugin Scan Paths
3.  **Load a Plugin:**
    *   Click **Load Plugin...** to open a file browser.
    *   Or click **Browser** to scan specific directories.
4.  **Select Analysis Mode:** Use the tabs at the top to switch between measurement modes (e.g., *LinearAnalysis*, *HarmonicAnalysis*, *Oscilloscope*).
5.  **Control Signal:**
    *   Adjust **Amplitude** and **Frequency** sliders for test signals (sine sweeps, THD tests).
    *   Toggle **Show Phase** to view phase response in graphs.

## License

This project is licensed under the [MIT License](LICENSE).
