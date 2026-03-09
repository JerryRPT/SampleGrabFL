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
    timestamp = int(time.time())
    output_template = os.path.join(out_dir, f"%(id)s_{timestamp}.%(ext)s")
    
    try:
        ydl_opts = {
            'format': 'bestaudio/best',
            'postprocessors': [{
                'key': 'FFmpegExtractAudio',
                'preferredcodec': 'wav',
            }],
            'outtmpl': output_template,
            'quiet': True,
            'no_warnings': True,
            'noplaylist': True
        }
        with yt_dlp.YoutubeDL(ydl_opts) as ydl:
            info = ydl.extract_info(url, download=True)
            filename = ydl.prepare_filename(info)
            wav_file = os.path.splitext(filename)[0] + ".wav"
    except Exception as e:
        print(json.dumps({"error": f"yt-dlp error: {str(e)}"}))
        return
        
    if not os.path.exists(wav_file):
        print(json.dumps({"error": "Downloaded wav file not found."}))
        return

    try:
        # Load audio
        y, sr = librosa.load(wav_file, sr=None)
        
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
        
        # Print JSON ensuring it is the only stdout block starting and ending with braces
        print(json.dumps({
            "file": wav_file,
            "bpm": round(bpm, 2),
            "key": dual_key_str
        }))
    except Exception as e:
        print(json.dumps({"error": f"librosa error: {str(e)}"}))

if __name__ == "__main__":
    main()
