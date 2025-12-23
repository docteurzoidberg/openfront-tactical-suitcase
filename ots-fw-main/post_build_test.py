Import("env")
import os
import shutil

# Restore original CMakeLists
src_dir = os.path.join(env.get("PROJECT_DIR"), "src")
original = os.path.join(src_dir, "CMakeLists.txt")
backup = os.path.join(src_dir, "CMakeLists_backup.txt")

if os.path.exists(backup):
    shutil.copy(backup, original)
    print("Restored original CMakeLists.txt")
