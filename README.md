# SampleGrab

A VST3 plugin that downloads audio straight from YouTube, finds the BPM, detects the Key, and lets you drag and drop the WAV file directly into your DAW project.

No more sketchy MP3 converter sites or digging through your downloads folder.

## Features
- **URL to DAW:** Paste a YouTube link, click download, and drag the audio right into your playlist.
- **Auto BPM & Key Detection:** Automatically detects tempo and both Major/Relative Minor keys.
- **Zero weird dependencies:** The Windows installer handles all the python/ffmpeg stuff under the hood so you don't have to.

## Download & Install (For Producers)
1. Go to the [Releases Tab](../../releases).
2. Download `SampleGrab_Windows_Installer.exe`.
3. Run it, open your DAW, and scan for new VST3 plugins.

## Building from Source (For Devs)

If you want to compile the C++ code yourself:

**Requirements:**
- CMake (v3.20+)
- Visual Studio 2022 Build Tools (C++ workload)
- Python 3.10+
- Inno Setup 6 (to build the `.exe` installer)

**Build Steps:**
```powershell
git clone https://github.com/jerryrpt/SampleGrab.git
cd SampleGrab
cmake -B build
cmake --build build --config Release
```
This automatically fetches JUCE via CMake and builds the `.vst3` target into `build\SampleGrab_artefacts\Release\VST3\SampleGrab.vst3`.

## License
MIT License. Built with the JUCE Framework.
