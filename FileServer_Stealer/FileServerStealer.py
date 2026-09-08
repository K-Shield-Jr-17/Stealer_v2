import ctypes
import string
import sys
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED
from datetime import datetime


# ==========================================
# LAB Configuration
# ==========================================

OUTPUT_DIR = Path(r"C:\Windows\Temp\Kisec")

TARGET_EXTENSIONS = {
    ".xlsx",
    ".xlsm",
    ".xls",
    ".csv",
    ".hwp",
    ".hwpx",
    ".pdf", ".ppt",
}

DRIVE_REMOTE = 4


# ==========================================
# Windows Drive Type
# ==========================================

def get_drive_type(path):

    return ctypes.windll.kernel32.GetDriveTypeW(
        ctypes.c_wchar_p(path)
    )


# ==========================================
# Find Network Drives
# ==========================================

def find_network_drives():

    network_drives = []

    print("[+] Checking Windows drives...")

    for letter in string.ascii_uppercase:

        drive = f"{letter}:\\"

        if not Path(drive).exists():
            continue

        drive_type = get_drive_type(drive)

        if drive_type == DRIVE_REMOTE:

            print(f"[NETWORK] {drive}")
            network_drives.append(
                Path(drive)
            )

    return network_drives


# ==========================================
# Automatically Select Network Drive
# ==========================================

def auto_select_network_drive():

    network_drives = find_network_drives()

    if len(network_drives) == 0:

        print(
            "[-] No mapped network drive found."
        )

        return None

    if len(network_drives) > 1:

        print(
            "[-] More than one network drive was found."
        )

        for drive in network_drives:

            print(f"    {drive}")

        print(
            "[-] LAB safety: automatic collection stopped."
        )

        return None

    source = network_drives[0]

    print(
        f"[+] Automatically selected: {source}"
    )

    return source


# ==========================================
# Find Target Documents
# ==========================================

def find_documents(source):

    files = []

    print(
        f"\n[+] Searching target documents in {source}"
    )

    for path in source.rglob("*"):

        try:

            if (
                path.is_file()
                and path.suffix.lower()
                in TARGET_EXTENSIONS
            ):

                files.append(path)

                print(
                    f"[FOUND] "
                    f"{path.relative_to(source)}"
                )

        except (PermissionError, OSError):

            print(
                f"[SKIP] {path}"
            )

    return files


# ==========================================
# Create ZIP
# ==========================================

def create_zip(source, files):

    try:

        OUTPUT_DIR.mkdir(
            parents=True,
            exist_ok=True
        )

    except PermissionError:

        print(
            f"[-] Cannot create: {OUTPUT_DIR}"
        )

        return None

    timestamp = datetime.now().strftime(
        "%Y%m%d_%H%M%S"
    )

    zip_path = (
        OUTPUT_DIR /
        f"Kisec_Documents_{timestamp}.zip"
    )

    print("\n[+] Creating ZIP...")

    with ZipFile(
        zip_path,
        "w",
        ZIP_DEFLATED
    ) as archive:

        for file_path in files:

            try:

                relative_path = (
                    file_path.relative_to(source)
                )

                archive.write(
                    file_path,
                    arcname=relative_path
                )

                print(
                    f"[+] Added: {relative_path}"
                )

            except (PermissionError, OSError) as e:

                print(
                    f"[!] Skipped: {file_path}"
                )

                print(
                    f"    {e}"
                )

    return zip_path


# ==========================================
# Main
# ==========================================

def main():

    print("=" * 55)
    print(
        " KISEC VMware LAB - Automatic Network Collector"
    )
    print("=" * 55)

    # 1. 네트워크 공유 드라이브 자동 확인
    source = auto_select_network_drive()

    if source is None:

        sys.exit(1)

    # 2. 지정 확장자 탐색
    files = find_documents(source)

    print(
        f"\n[+] Found {len(files)} target file(s)."
    )

    if not files:

        print(
            "[-] No target documents found."
        )

        sys.exit(0)

    # 3. ZIP 생성
    zip_path = create_zip(
        source,
        files
    )

    if zip_path is None:

        sys.exit(1)

    # 4. 결과
    print("\n" + "=" * 55)
    print("[+] LAB collection completed")
    print(f"[+] Source : {source}")
    print(f"[+] Files  : {len(files)}")
    print(f"[+] ZIP    : {zip_path}")
    print("=" * 55)


if __name__ == "__main__":
    main()