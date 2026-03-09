import sys
import json
import os
import shutil

def is_tool_installed(name):
    return shutil.which(name) is not None

def main():
    if len(sys.argv) < 3:
        print(json.dumps({"error": "Usage: python backend.py <url> <output_dir>"}))
        return
        
    url = sys.argv[1]
    out_dir = sys.argv[2]
    
    if not is_tool_installed("ffmpeg"):
        print(json.dumps({"error": "ffmpeg is not installed or not in System PATH. It is required by yt-dlp to extract audio."}))
        return

    try:
        import yt_dlp
    except ImportError:
        print(json.dumps({"error": "yt-dlp not installed. Run: pip install yt-dlp"}))
        return

    try:
        import librosa
        import numpy as np
    except ImportError:
        print(json.dumps({"error": "librosa not installed. Run: pip install librosa numpy soundfile"}))
        return
        
    import time
    import re
    timestamp = int(time.time())
    # Use the video title in the output filename and restrict special characters to prevent C++ pipe issues
    output_template = os.path.join(out_dir, f"%(title).150s_{timestamp}.%(ext)s")
    
    def my_hook(d):
        if d['status'] == 'downloading':
            percent_str = d.get('_percent_str', '')
            if percent_str:
                ansi_escape = re.compile(r'\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~])')
                clean_str = ansi_escape.sub('', percent_str).strip().replace('%', '')
                try:
                    percent = float(clean_str)
                    print(json.dumps({"progress": percent, "status": "Downloading..."}), flush=True)
                except Exception:
                    pass
        elif d['status'] == 'finished':
            print(json.dumps({"progress": 100.0, "status": "Processing audio..."}), flush=True)

    audio_file = None

    if os.path.isfile(url):
        print(json.dumps({"status": "Local file detected. Skipping download..."}), flush=True)
        audio_file = url
    else:
        try:
            ydl_opts = {
                'format': 'bestaudio/best',
            'postprocessors': [{
                'key': 'FFmpegExtractAudio',
                'preferredcodec': 'flac',
            }],
            'outtmpl': output_template,
            'restrictfilenames': True,
            'quiet': True,
            'no_warnings': True,
                'noplaylist': True,
                'progress_hooks': [my_hook]
            }
            with yt_dlp.YoutubeDL(ydl_opts) as ydl:
                info = ydl.extract_info(url, download=True)
                filename = ydl.prepare_filename(info)
                audio_file = os.path.splitext(filename)[0] + ".flac"
        except Exception as e:
            print(json.dumps({"error": f"yt-dlp error: {str(e)}"}))
            return
            
        if not os.path.exists(audio_file):
            print(json.dumps({"error": "Downloaded flac file not found."}))
            return

    print(json.dumps({"status": "Analyzing BPM and Key..."}), flush=True)

    try:
        # Load audio
        y, sr = librosa.load(audio_file, sr=None)
        
        # BPM
        tempo, _ = librosa.beat.beat_track(y=y, sr=sr)
        bpm = float(np.mean(tempo))
        
        # Key Estimation
        chroma = librosa.feature.chroma_stft(y=y, sr=sr)
        chroma_vals = np.sum(chroma, axis=1)
        pitch_classes = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B']
        
        # Calculate Major index and Relative Minor index (3 semitones down)
        maj_idx = np.argmax(chroma_vals)
        min_idx = (maj_idx - 3) % 12
        
        maj_key = pitch_classes[maj_idx] + " Maj"
        min_key = pitch_classes[min_idx] + " Min"
        dual_key_str = f"{maj_key} / {min_key}"
        
        result_data = {
            "file": audio_file,
            "bpm": round(bpm, 2),
            "key": dual_key_str,
            "timestamp": timestamp
        }
        
        # Save to history.json
        try:
            appdata = os.environ.get('APPDATA', '')
            if appdata:
                history_dir = os.path.join(appdata, "jerryrpt", "SampleGrab")
                os.makedirs(history_dir, exist_ok=True)
                history_file = os.path.join(history_dir, "history.json")
                
                history_list = []
                if os.path.exists(history_file):
                    with open(history_file, 'r', encoding='utf-8') as f:
                        try:
                            history_list = json.load(f)
                        except json.JSONDecodeError:
                            pass
                
                # Prepend new entry so newest is first
                history_list.insert(0, result_data)
                
                # Keep only last 50 entries
                if len(history_list) > 50:
                    history_list = history_list[:50]
                    
                with open(history_file, 'w', encoding='utf-8') as f:
                    json.dump(history_list, f, indent=4)
        except Exception:
            pass # Fail silently if history cannot be written

        # Print JSON ensuring it is the only stdout block starting and ending with braces
        print(json.dumps(result_data), flush=True)
    except Exception as e:
        print(json.dumps({"error": f"librosa error: {str(e)}"}), flush=True)

if __name__ == "__main__":
    main()
