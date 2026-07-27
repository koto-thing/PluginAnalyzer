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

### Building with Ninja

Install Ninja and a suitable compiler environment, then run:

```bash
cmake --preset ninja-debug
cmake --build --preset ninja-debug
ctest --preset ninja-debug
```

`PluginAnalyzer.jucer` is retained temporarily for migration compatibility.
CMake is the authoritative build definition.

### Using an existing JUCE checkout

To avoid downloading JUCE, configure with an absolute path to a JUCE 8.0.13
checkout:

```powershell
cmake --preset vs2026 -DFETCHCONTENT_SOURCE_DIR_JUCE=C:/path/to/JUCE
```

### Current implementation status

The Linear, Harmonic, Oscilloscope, and Performance paths are being stabilised.
THD Sweep, IMD, Hammerstein, White Noise, Sine Sweep, and Dynamics should
currently be treated as experimental and their results should not yet be used
as calibrated measurements.

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
