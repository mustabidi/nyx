import os
import glob
import subprocess

src_dir = "src/bin/nyx-app"
files = glob.glob(f"{src_dir}/nyx-app-*")

for file in files:
    new_name = file.replace("nyx-app-", "nyx-app-")
    subprocess.run(["git", "mv", file, new_name])

print("Renamed source files.")
