<div align="center">
  <h1>SampleGrab</h1>
  <p><b>The ultimate Youtube audio ripper & analyzer built for FL Studio workflow automation.</b></p>
</div>

SampleGrab is a native VST3 plugin that allows you to instantly download high-quality audio from any YouTube URL directly into your DAW. Instead of relying on slow web rippers and losing downloaded MP3s in your folders, this plugin handles the entire pipeline internally.

It passes the YouTube link to a robust local Python backend utilizing `yt-dlp`, converts the file to `.wav`, analyzes the audio via `librosa` to approximate the Tempo (BPM) and Musical Key (Major/Minor), and then creates a draggable region inside the VST UI so you can toss the `.wav` file straight into your Sequencer or Audio Tracks.

> [!NOTE]
> *This plugin is built with C++ using the JUCE framework on the frontend, and Python on the backend.*

## ✨ Features
- **Instant YouTube-to-WAV Extraction**: Just paste the URL and hit download.
- **Harmonic & BPM Analysis**: The plugin automatically runs mathematical estimations on the chromagram to tell you the root Tempo, the primary Major key, and its Relative Minor.
- **Drag-and-Drop Workflow**: Provides a seamless "DRAG TO PLAYLIST" target that integrates natively with FL Studio's file drop listener.
- **Sleek UI**: Premium glossy interface built entirely natively in JUCE C++. 

## ⬇️ Installation (For Producers)
If you just want to use the plugin, download the latest Release:
1. Go to the [Releases Tab](../../releases).
2. Download the `SampleGrab_Windows_Installer.exe`.
3. Run the installer (it handles moving the `.vst3` file, installing background Python dependencies, and acquiring FFmpeg automatically).
4. Open FL Studio (or your DAW), scan for new plugins, and add **SampleGrab** to an insert or channel rack!

## 🛠️ Building from Source (For Developers)

### Prerequisites
- [CMake](https://cmake.org/download/) (v3.20+)
- Visual Studio 2022 Build Tools (with "Desktop development with C++")
- [Python 3.10+](https://www.python.org/downloads/)
- [Inno Setup 6](https://jrsoftware.org/isdl.php) (if you intend to build the Windows Installer)

### Instructions
1. Clone this repository to your desktop.
```powershell
git clone https://github.com/jerryrpt/SampleGrab.git
cd SampleGrab
```
2. Run CMake to automatically fetch the JUCE submodule and build the `.vst3` target:
```powershell
cmake -B build
cmake --build build --config Release
```
3. The plugin will be compiled to `build\SampleGrab_artefacts\Release\VST3\SampleGrab.vst3`. 
To build the automated installer, compile the `installer.iss` file through Inno Setup Studio (ISCC).

## 🪪 License
This repository is open-sourced under the MIT License.
*Note: Due to the usage of the JUCE Framework, commercial closed-source redistribution requires a separate commercial license from the JUCE team.*
