Import("env")
import os
import shutil

# Backup original and use test version
src_dir = os.path.join(env.get("PROJECT_DIR"), "src")
original = os.path.join(src_dir, "CMakeLists.txt")
test_version = os.path.join(src_dir, "CMakeLists_test_websocket.txt")
backup = os.path.join(src_dir, "CMakeLists_backup.txt")

# Backup original if not already backed up
if not os.path.exists(backup) and os.path.exists(original):
    shutil.copy(original, backup)
    print("Backed up original CMakeLists.txt")

# Use test version
if os.path.exists(test_version):
    shutil.copy(test_version, original)
    print("Using CMakeLists_test_websocket.txt for build")
