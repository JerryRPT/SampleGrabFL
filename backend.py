import json
import math
import os
import re
import shutil
import sys
import time
import warnings


def is_tool_installed(name):
    return shutil.which(name) is not None


def normalize_vector(vector, np):
    vector = np.asarray(vector, dtype=float)
    total = float(np.sum(vector))
    if total > 0.0:
        vector = vector / total

    norm = float(np.linalg.norm(vector))
    if norm > 0.0:
        vector = vector / norm

    return vector


def key_name(tonic_index, is_major):
    major_names = ["C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"]
    minor_names = ["C", "C#", "D", "Eb", "E", "F", "F#", "G", "G#", "A", "Bb", "B"]
    tonic_index %= 12
    return f"{major_names[tonic_index] if is_major else minor_names[tonic_index]} {'Maj' if is_major else 'Min'}"


def relative_key_name(tonic_index, is_major):
    if is_major:
        return key_name(tonic_index - 3, False)
    return key_name(tonic_index + 3, True)


def tuning_to_hz(tuning_cents):
    return 440.0 * math.pow(2.0, tuning_cents / 1200.0)


def score_profiles(chroma_vector, major_profile, minor_profile, np):
    major_profile = normalize_vector(major_profile, np)
    minor_profile = normalize_vector(minor_profile, np)

    best = {
        "score": float("-inf"),
        "tonic_index": 0,
        "is_major": True,
    }

    for tonic_index in range(12):
        major_score = float(np.dot(chroma_vector, np.roll(major_profile, tonic_index)))
        if major_score > best["score"]:
            best = {
                "score": major_score,
                "tonic_index": tonic_index,
                "is_major": True,
            }

        minor_score = float(np.dot(chroma_vector, np.roll(minor_profile, tonic_index)))
        if minor_score > best["score"]:
            best = {
                "score": minor_score,
                "tonic_index": tonic_index,
                "is_major": False,
            }

    return best


def estimate_key(y_harmonic, sr, librosa, np, median_filter):
    tuning_bins = float(librosa.estimate_tuning(y=y_harmonic, sr=sr, bins_per_octave=36))
    chroma = librosa.feature.chroma_cqt(
        y=y_harmonic,
        sr=sr,
        hop_length=512,
        bins_per_octave=36,
        n_chroma=12,
        tuning=tuning_bins,
    )

    if chroma.size == 0:
        raise RuntimeError("No harmonic chroma data was produced for key detection.")

    chroma = median_filter(chroma, size=(1, 9), mode="nearest")
    rms = librosa.feature.rms(y=y_harmonic, frame_length=4096, hop_length=512)[0]
    rms = rms[: chroma.shape[1]]

    if rms.size == 0:
        weights = np.ones(chroma.shape[1], dtype=float)
    else:
        chroma_focus = np.max(chroma, axis=0)
        weights = np.clip(rms, 0.0, None) * np.clip(chroma_focus, 0.0, None)
        positive_weights = weights[weights > 0.0]
        if positive_weights.size > 0:
            floor = float(np.percentile(positive_weights, 35))
            weights = np.where(weights >= floor, weights, 0.0)

        if not np.any(weights > 0.0):
            weights = np.ones(chroma.shape[1], dtype=float)

    chroma_vector = np.average(chroma, axis=1, weights=weights)
    chroma_vector = normalize_vector(chroma_vector, np)

    major_profile = np.array([6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88])
    minor_profile = np.array([6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17])
    best = score_profiles(chroma_vector, major_profile, minor_profile, np)

    primary_key = key_name(best["tonic_index"], best["is_major"])
    alternate_key = relative_key_name(best["tonic_index"], best["is_major"])
    tuning_cents = tuning_bins * 100.0
    tuning_hz = tuning_to_hz(tuning_cents)

    return {
        "primary_key": primary_key,
        "alternate_key": alternate_key,
        "display_key": primary_key,
        "tuning_cents": round(tuning_cents, 1),
        "tuning_hz": round(tuning_hz, 1),
        "tuning_display": f"A={int(round(tuning_hz))}Hz",
    }


def save_history(result_data):
    try:
        appdata = os.environ.get("APPDATA", "")
        if not appdata:
            return

        history_dir = os.path.join(appdata, "jerryrpt", "SampleGrab")
        os.makedirs(history_dir, exist_ok=True)
        history_file = os.path.join(history_dir, "history.json")

        history_list = []
        if os.path.exists(history_file):
            with open(history_file, "r", encoding="utf-8") as handle:
                try:
                    history_list = json.load(handle)
                except json.JSONDecodeError:
                    history_list = []

        history_list.insert(0, result_data)
        history_list = history_list[:50]

        with open(history_file, "w", encoding="utf-8") as handle:
            json.dump(history_list, handle, indent=4)
    except Exception:
        pass


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
        from scipy.ndimage import median_filter
    except ImportError:
        print(json.dumps({"error": "analysis dependencies not installed. Run: pip install librosa numpy soundfile scipy"}))
        return

    warnings.filterwarnings("ignore", category=UserWarning, module="librosa")

    timestamp = int(time.time())
    output_template = os.path.join(out_dir, f"%(title).150s_{timestamp}.%(ext)s")

    def my_hook(download_state):
        if download_state["status"] == "downloading":
            percent_str = download_state.get("_percent_str", "")
            if percent_str:
                ansi_escape = re.compile(r"\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~])")
                clean_str = ansi_escape.sub("", percent_str).strip().replace("%", "")
                try:
                    percent = float(clean_str)
                    print(json.dumps({"progress": percent, "status": "Downloading..."}), flush=True)
                except Exception:
                    pass
        elif download_state["status"] == "finished":
            print(json.dumps({"progress": 100.0, "status": "Processing audio..."}), flush=True)

    audio_file = None

    if os.path.isfile(url):
        print(json.dumps({"status": "Local file detected. Skipping download..."}), flush=True)
        audio_file = url
    else:
        try:
            ydl_opts = {
                "format": "bestaudio/best",
                "postprocessors": [
                    {
                        "key": "FFmpegExtractAudio",
                        "preferredcodec": "flac",
                    }
                ],
                "outtmpl": output_template,
                "restrictfilenames": True,
                "quiet": True,
                "no_warnings": True,
                "noplaylist": True,
                "progress_hooks": [my_hook],
            }
            with yt_dlp.YoutubeDL(ydl_opts) as ydl:
                info = ydl.extract_info(url, download=True)
                filename = ydl.prepare_filename(info)
                audio_file = os.path.splitext(filename)[0] + ".flac"
        except Exception as exc:
            print(json.dumps({"error": f"yt-dlp error: {str(exc)}"}))
            return

        if not os.path.exists(audio_file):
            print(json.dumps({"error": "Downloaded flac file not found."}))
            return

    print(json.dumps({"status": "Analyzing BPM and Key..."}), flush=True)

    try:
        y, sr = librosa.load(audio_file, sr=None)
        y_harmonic, y_percussive = librosa.effects.hpss(y)

        onset_env = librosa.onset.onset_strength(y=y_percussive, sr=sr)
        tempogram = librosa.feature.tempogram(onset_envelope=onset_env, sr=sr)
        freqs = librosa.tempo_frequencies(tempogram.shape[0], hop_length=512, sr=sr)
        mean_tempogram = np.mean(tempogram, axis=1)

        valid_idx = (freqs > 60) & (freqs < 200)
        freqs = freqs[valid_idx]
        mean_tempogram = mean_tempogram[valid_idx]

        top_indices = np.argsort(mean_tempogram)[::-1][:5]
        best_tempos = freqs[top_indices]
        tempo = best_tempos[0] if len(best_tempos) > 0 else 120.0

        for candidate_tempo in best_tempos[1:]:
            ratio = tempo / candidate_tempo
            if abs(ratio - 1.3333) < 0.05 or abs(ratio - 1.5) < 0.05:
                tempo = candidate_tempo
                break

        bpm = float(round(tempo))
        key_data = estimate_key(y_harmonic, sr, librosa, np, median_filter)

        result_data = {
            "file": audio_file,
            "bpm": round(bpm, 2),
            "key": key_data["display_key"],
            "primary_key": key_data["primary_key"],
            "alternate_key": key_data["alternate_key"],
            "display_key": key_data["display_key"],
            "tuning_hz": key_data["tuning_hz"],
            "tuning_cents": key_data["tuning_cents"],
            "tuning_display": key_data["tuning_display"],
            "timestamp": timestamp,
        }

        save_history(result_data)
        print(json.dumps(result_data), flush=True)
    except Exception as exc:
        import traceback

        err_msg = "".join(traceback.format_exception(type(exc), exc, exc.__traceback__))
        print(json.dumps({"error": f"librosa error: {str(exc)}", "traceback": err_msg}), flush=True)


if __name__ == "__main__":
    main()
