import json
import math
import os
import re
import shutil
import sys
import time
import warnings
from urllib.parse import parse_qs, urlparse


def is_tool_installed(name):
    return shutil.which(name) is not None


YOUTUBE_HOSTS = {
    "youtube.com",
    "www.youtube.com",
    "m.youtube.com",
    "music.youtube.com",
    "youtube-nocookie.com",
    "www.youtube-nocookie.com",
    "youtu.be",
    "www.youtu.be",
}


class SourceValidationError(ValueError):
    pass


def normalize_source(source):
    """Validate a local file/web URL and canonicalize supported YouTube links."""
    source = source.strip().strip('"').strip("'")
    if not source:
        raise SourceValidationError("Enter a YouTube URL or drop an existing audio file.")

    if os.path.isfile(source):
        return source

    lowered = source.lower()
    if lowered.startswith(("youtube.com/", "www.youtube.com/", "m.youtube.com/", "music.youtube.com/", "youtu.be/")):
        source = "https://" + source

    parsed = urlparse(source)
    if parsed.scheme.lower() not in {"http", "https"} or not parsed.hostname:
        raise SourceValidationError("That is not a valid web URL or existing audio file.")

    host = parsed.hostname.lower()
    if host not in YOUTUBE_HOSTS:
        return source

    path_parts = [part for part in parsed.path.split("/") if part]
    video_id = ""

    if host in {"youtu.be", "www.youtu.be"} and path_parts:
        video_id = path_parts[0]
    elif path_parts and path_parts[0].lower() == "watch":
        video_id = parse_qs(parsed.query).get("v", [""])[0]
    elif len(path_parts) >= 2 and path_parts[0].lower() in {"shorts", "embed", "live"}:
        video_id = path_parts[1]

    if not re.fullmatch(r"[A-Za-z0-9_-]{6,20}", video_id):
        raise SourceValidationError("This YouTube link does not contain a valid video ID.")

    return f"https://www.youtube.com/watch?v={video_id}"


def sanitize_url(url):
    """Backward-compatible alias used by older callers and tests."""
    return normalize_source(url)


def emit_error(code, message, details=None):
    payload = {"error": message, "error_code": code}
    if details:
        payload["details"] = details
    print(json.dumps(payload), flush=True)


def friendly_download_error(exc):
    message = str(exc)
    lowered = message.lower()

    if "http error 403" in lowered or "forbidden" in lowered:
        return (
            "youtube_forbidden",
            "YouTube blocked the download (403). Update SampleGrab or disable any VPN/proxy, then retry.",
        )
    if "private video" in lowered:
        return "private_video", "That video is private and cannot be downloaded."
    if "video unavailable" in lowered or "not available" in lowered:
        return "video_unavailable", "That video is unavailable or restricted in your region."
    if "sign in" in lowered or "confirm you’re not a bot" in lowered or "confirm you're not a bot" in lowered:
        return "youtube_verification", "YouTube requires browser verification for this video. Try another public video or retry later."
    if "timed out" in lowered or "temporary failure" in lowered or "name resolution" in lowered:
        return "network_error", "The network request timed out. Check your connection and try again."
    if "unsupported url" in lowered:
        return "unsupported_url", "This URL is not supported. Paste a direct YouTube video link."
    if "ffmpeg" in lowered:
        return "ffmpeg_error", "FFmpeg could not process the audio. Reinstall SampleGrab and try again."

    return "download_failed", "The video could not be downloaded. Check that it is public and try again."


def normalize_vector(vector, np):
    vector = np.asarray(vector, dtype=float)
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


def pearson_correlation(x, y, np):
    """Compute Pearson correlation coefficient between two vectors."""
    x = np.asarray(x, dtype=float)
    y = np.asarray(y, dtype=float)
    x_mean = np.mean(x)
    y_mean = np.mean(y)
    x_centered = x - x_mean
    y_centered = y - y_mean
    numerator = float(np.dot(x_centered, y_centered))
    denominator = float(np.sqrt(np.dot(x_centered, x_centered) * np.dot(y_centered, y_centered)))
    if denominator < 1e-12:
        return 0.0
    return numerator / denominator


def score_key_correlation(chroma_vector, np):
    """Score chroma vector against key profiles using Pearson correlation
    (the standard Krumhansl-Schmuckler algorithm)."""

    # Profile sets: (major, minor)
    profile_sets = [
        # Krumhansl-Kessler (1990)
        (
            [6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88],
            [6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17],
        ),
        # Temperley MIREX (2005)
        (
            [5.0, 2.0, 3.5, 2.0, 4.5, 4.0, 2.0, 4.5, 2.0, 3.5, 1.5, 4.0],
            [5.0, 2.0, 3.5, 4.5, 2.0, 3.5, 2.0, 4.5, 3.5, 2.0, 1.5, 4.0],
        ),
        # Sha'ath (2011)
        (
            [6.6, 2.0, 3.5, 2.3, 4.6, 4.0, 2.5, 5.2, 2.4, 3.7, 2.3, 3.4],
            [6.5, 2.7, 3.5, 5.4, 2.6, 3.5, 2.5, 5.2, 4.0, 2.7, 4.3, 3.2],
        ),
    ]

    # scores[0..11] = major keys C..B, scores[12..23] = minor keys C..B
    scores = np.zeros(24, dtype=float)

    for major_p, minor_p in profile_sets:
        major_p = np.array(major_p, dtype=float)
        minor_p = np.array(minor_p, dtype=float)
        for tonic in range(12):
            scores[tonic] += pearson_correlation(chroma_vector, np.roll(major_p, tonic), np)
            scores[12 + tonic] += pearson_correlation(chroma_vector, np.roll(minor_p, tonic), np)

    best_idx = int(np.argmax(scores))
    is_major = best_idx < 12
    tonic_index = best_idx if is_major else best_idx - 12

    return {
        "score": float(scores[best_idx]),
        "tonic_index": tonic_index,
        "is_major": is_major,
        "all_scores": scores,
    }


def estimate_key(y, y_harmonic, sr, librosa, np, median_filter):
    """Estimate musical key using bandpass-filtered chroma with Pearson correlation.
    
    Bandpass filtering to 200-4000Hz removes bass (808, sub) energy that biases
    chroma toward subdominant/4th degree — the #1 cause of key misdetection
    in hip-hop/electronic/trap beats.
    """
    from scipy.signal import butter, sosfilt

    tuning_bins = float(librosa.estimate_tuning(y=y, sr=sr, bins_per_octave=36))

    # --- Bandpass filter: isolate melodic range (200Hz - 4kHz) ---
    nyquist = sr / 2.0
    low = max(200.0 / nyquist, 0.001)
    high = min(4000.0 / nyquist, 0.999)
    sos = butter(4, [low, high], btype='band', output='sos')
    y_melodic = sosfilt(sos, y).astype(np.float32)

    # --- Chromagram 1: CQT on bandpass-filtered signal ---
    chroma_cqt = librosa.feature.chroma_cqt(
        y=y_melodic,
        sr=sr,
        hop_length=512,
        bins_per_octave=36,
        n_chroma=12,
        tuning=tuning_bins,
    )

    if chroma_cqt.size == 0:
        raise RuntimeError("No chroma data was produced for key detection.")

    chroma_cqt = median_filter(chroma_cqt, size=(1, 9), mode="nearest")

    # --- Chromagram 2: CENS on bandpass-filtered signal ---
    chroma_cens_bp = librosa.feature.chroma_cens(
        y=y_melodic,
        sr=sr,
        hop_length=512,
        n_chroma=12,
        bins_per_octave=36,
    )

    # --- Chromagram 3: CENS on full signal (as a tiebreaker) ---
    chroma_cens_full = librosa.feature.chroma_cens(
        y=y,
        sr=sr,
        hop_length=512,
        n_chroma=12,
        bins_per_octave=36,
    )

    # Energy weighting for CQT chroma (on melodic signal)
    rms = librosa.feature.rms(y=y_melodic, frame_length=4096, hop_length=512)[0]
    rms_cqt = rms[: chroma_cqt.shape[1]]

    if rms_cqt.size == 0:
        weights_cqt = np.ones(chroma_cqt.shape[1], dtype=float)
    else:
        chroma_focus = np.max(chroma_cqt, axis=0)
        weights_cqt = np.clip(rms_cqt, 0.0, None) * np.clip(chroma_focus, 0.0, None)
        pos = weights_cqt[weights_cqt > 0.0]
        if pos.size > 0:
            floor = float(np.percentile(pos, 20))
            weights_cqt = np.where(weights_cqt >= floor, weights_cqt, 0.0)
        if not np.any(weights_cqt > 0.0):
            weights_cqt = np.ones(chroma_cqt.shape[1], dtype=float)

    # Build chroma vectors
    vec_cqt = np.average(chroma_cqt, axis=1, weights=weights_cqt)
    vec_cens_bp = np.mean(chroma_cens_bp, axis=1)
    vec_cens_full = np.mean(chroma_cens_full, axis=1)

    # Score all three representations
    result_cqt = score_key_correlation(vec_cqt, np)
    result_cens_bp = score_key_correlation(vec_cens_bp, np)
    result_cens_full = score_key_correlation(vec_cens_full, np)

    # Ensemble: bandpass chromas weighted 2x (they're more reliable), full signal 1x
    combined = (result_cqt["all_scores"] * 2.0
                + result_cens_bp["all_scores"] * 2.0
                + result_cens_full["all_scores"] * 1.0)

    best_idx = int(np.argmax(combined))
    is_major = best_idx < 12
    tonic_index = best_idx if is_major else best_idx - 12

    primary_key = key_name(tonic_index, is_major)
    alternate_key = relative_key_name(tonic_index, is_major)
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

    try:
        url = normalize_source(sys.argv[1])
    except SourceValidationError as exc:
        emit_error("invalid_source", str(exc))
        return

    out_dir = sys.argv[2]

    if not is_tool_installed("ffmpeg"):
        emit_error("ffmpeg_missing", "FFmpeg is missing. Reinstall SampleGrab to repair the audio tools.")
        return

    try:
        import yt_dlp
    except ImportError:
        emit_error("downloader_missing", "SampleGrab's downloader is missing. Reinstall SampleGrab to repair it.")
        return

    try:
        import librosa
        import numpy as np
        from scipy.ndimage import median_filter
    except ImportError:
        emit_error("analysis_missing", "SampleGrab's analysis tools are missing. Reinstall SampleGrab to repair them.")
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
                "source_address": "0.0.0.0",
                "retries": 3,
                "fragment_retries": 3,
                "extractor_retries": 3,
                "file_access_retries": 3,
                "progress_hooks": [my_hook],
            }
            with yt_dlp.YoutubeDL(ydl_opts) as ydl:
                info = ydl.extract_info(url, download=True)
                filename = ydl.prepare_filename(info)
                audio_file = os.path.splitext(filename)[0] + ".flac"
        except Exception as exc:
            code, message = friendly_download_error(exc)
            emit_error(code, message, str(exc))
            return

        if not os.path.exists(audio_file):
            emit_error("output_missing", "The download finished, but the converted audio file could not be found.")
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
        key_data = estimate_key(y, y_harmonic, sr, librosa, np, median_filter)

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
        emit_error("analysis_failed", "The audio downloaded, but BPM and key analysis failed.", err_msg)


if __name__ == "__main__":
    main()
