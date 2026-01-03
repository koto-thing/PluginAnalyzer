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

*   **C++ Compiler:** Visual Studio 2022 (Windows), Xcode (macOS), or GCC (Linux).
*   **JUCE Framework:** The Projucer is required to manage the project, or you can use CMake/VS directly if configured.
*   **VST3 SDK:** (Optional but recommended) For hosting VST3 plugins.

### Building

1.  **Clone the repository:**
    ```bash
    git clone https://github.com/yourusername/PluginAnalyzer.git
    ```
2.  **Open the Project:**
    *   Open `PluginAnalyzer.sln` in Visual Studio.
    *   Or open the `.jucer` file in the Projucer and save to your target IDE.
3.  **Build:**
    *   Select your configuration (Debug/Release) and platform (x64).
    *   Build the solution.

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