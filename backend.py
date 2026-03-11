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
        
        # Separate harmonic and percussive components for much better analysis accuracy
        y_harmonic, y_percussive = librosa.effects.hpss(y)

        # BPM Detection (using percussive component and tempogram to fix polyrhythms)
        onset_env = librosa.onset.onset_strength(y=y_percussive, sr=sr)
        tg = librosa.feature.tempogram(onset_envelope=onset_env, sr=sr)
        freqs = librosa.tempo_frequencies(tg.shape[0], hop_length=512, sr=sr)
        mean_tg = np.mean(tg, axis=1)

        # Filter out extreme frequencies to focus on standard hip-hop/pop ranges
        valid_idx = (freqs > 60) & (freqs < 200)
        freqs = freqs[valid_idx]
        mean_tg = mean_tg[valid_idx]

        top_indices = np.argsort(mean_tg)[::-1][:5]
        best_tempos = freqs[top_indices]

        tempo = best_tempos[0] if len(best_tempos) > 0 else 120.0

        # Check for triplet/dotted-8th polyrhythm confusion
        # Modern trap beats often have bouncy hi-hats that trick the algorithm into picking a 4/3 or 3/2 multiple.
        for t in best_tempos[1:]:
            ratio = tempo / t
            # If dominant tempo is 4/3 or 3/2 of another strong tempo, the slower one is the true quarter note
            if abs(ratio - 1.3333) < 0.05 or abs(ratio - 1.5) < 0.05:
                tempo = t
                break

        # Round to nearest integer (most DAW projects use exact integers like 100, 140, etc.)
        bpm = float(round(tempo))
        
        # Key Estimation (using harmonic component and Krumhansl-Schmuckler profiles)
        pitch_classes = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B']

        # Use CQT for better pitch resolution
        chroma = librosa.feature.chroma_cqt(y=y_harmonic, sr=sr)
        chroma_mean = np.mean(chroma, axis=1)

        # Krumhansl-Schmuckler profiles
        major_profile = np.array([6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88])
        minor_profile = np.array([6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17])

        major_corrs = [np.corrcoef(chroma_mean, np.roll(major_profile, i))[0, 1] for i in range(12)]
        minor_corrs = [np.corrcoef(chroma_mean, np.roll(minor_profile, i))[0, 1] for i in range(12)]

        best_maj_idx = np.argmax(major_corrs)
        best_min_idx = np.argmax(minor_corrs)

        # Compare the correlation coefficients to see which key (Major or Minor) is a better fit
        if major_corrs[best_maj_idx] >= minor_corrs[best_min_idx]:
            # Likely a Major key
            maj_key = pitch_classes[best_maj_idx]
            min_key = pitch_classes[(best_maj_idx - 3) % 12]
            dual_key_str = f"{maj_key} Maj / {min_key} Min"
        else:
            # Likely a Minor key
            min_key = pitch_classes[best_min_idx]
            maj_key = pitch_classes[(best_min_idx + 3) % 12]
            dual_key_str = f"{min_key} Min / {maj_key} Maj"
        
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
        import traceback
        err_msg = "".join(traceback.format_exception(type(e), e, e.__traceback__))
        print(json.dumps({"error": f"librosa error: {str(e)}", "traceback": err_msg}), flush=True)

if __name__ == "__main__":
    main()
