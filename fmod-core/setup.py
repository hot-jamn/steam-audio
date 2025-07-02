#
# Copyright 2017-2023 Valve Corporation.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

import os
import shutil
import urllib.request, urllib.error, urllib.parse
import zipfile
import subprocess
import sys

version = "4.6.1"

def download_file(url):
    remote_file = urllib.request.urlopen(url)
    with open(os.path.basename(url), "wb") as local_file:
        while True:
            data = remote_file.read(1024)
            if not data:
                break
            local_file.write(data)

def create_import_library(dll_path, lib_path):
    """Create import library from DLL using lib.exe"""
    try:
        # Try to create import library using lib.exe
        cmd = f'lib /def /out:"{lib_path}" /machine:x64 "{dll_path}"'
        result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
        if result.returncode == 0:
            print(f"Created import library: {lib_path}")
            return True
        else:
            print(f"Warning: Could not create import library for {dll_path}")
            print(f"lib.exe output: {result.stderr}")
            return False
    except Exception as e:
        print(f"Error creating import library: {e}")
        return False

print("Downloading steamaudio_" + version + ".zip...")
url = "https://github.com/ValveSoftware/steam-audio/releases/download/v" + version + "/steamaudio_" + version + ".zip"
download_file(url)

print("Extracting steamaudio_" + version + ".zip...")
with zipfile.ZipFile(os.path.basename(url), "r") as zip:
    zip.extractall()

print("Creating directories...")
if not os.path.exists("lib/windows-x86"):
    os.makedirs("lib/windows-x86")
if not os.path.exists("lib/windows-x64"):
    os.makedirs("lib/windows-x64")
if not os.path.exists("lib/linux-x86"):
    os.makedirs("lib/linux-x86")
if not os.path.exists("lib/linux-x64"):
    os.makedirs("lib/linux-x64")
if not os.path.exists("lib/osx"):
    os.makedirs("lib/osx")
if not os.path.exists("lib/android-armv7"):
    os.makedirs("lib/android-armv7")
if not os.path.exists("lib/android-armv8"):
    os.makedirs("lib/android-armv8")
if not os.path.exists("lib/android-x86"):
    os.makedirs("lib/android-x86")

print("Copying files...")
shutil.copy("steamaudio/lib/windows-x86/phonon.dll", "lib/windows-x86")
shutil.copy("steamaudio/lib/windows-x64/phonon.dll", "lib/windows-x64")
shutil.copy("steamaudio/lib/linux-x86/libphonon.so", "lib/linux-x86")
shutil.copy("steamaudio/lib/linux-x64/libphonon.so", "lib/linux-x64")
try:
    shutil.rmtree("lib/osx/phonon.bundle")
except:
    pass
shutil.copytree("steamaudio/lib/osx/phonon.bundle", "lib/osx/phonon.bundle")
shutil.copy("steamaudio/lib/android-armv7/libphonon.so", "lib/android-armv7")
shutil.copy("steamaudio/lib/android-armv8/libphonon.so", "lib/android-armv8")
shutil.copy("steamaudio/lib/android-x86/libphonon.so", "lib/android-x86")

# Copy import libraries if they exist in the SDK
print("Copying import libraries...")
if os.path.exists("steamaudio/lib/windows-x86/phonon.lib"):
    shutil.copy("steamaudio/lib/windows-x86/phonon.lib", "lib/windows-x86")
    print("Copied phonon.lib for windows-x86")
else:
    print("Creating import library for windows-x86...")
    create_import_library("lib/windows-x86/phonon.dll", "lib/windows-x86/phonon.lib")

if os.path.exists("steamaudio/lib/windows-x64/phonon.lib"):
    shutil.copy("steamaudio/lib/windows-x64/phonon.lib", "lib/windows-x64")
    print("Copied phonon.lib for windows-x64")
else:
    print("Creating import library for windows-x64...")
    create_import_library("lib/windows-x64/phonon.dll", "lib/windows-x64/phonon.lib")

# Copy headers if they exist
print("Copying headers...")
if os.path.exists("steamaudio/include"):
    if not os.path.exists("include"):
        os.makedirs("include")
    if os.path.exists("steamaudio/include/phonon"):
        if os.path.exists("include/phonon"):
            shutil.rmtree("include/phonon")
        shutil.copytree("steamaudio/include/phonon", "include/phonon")
        print("Copied phonon headers")

print("Cleaning up...")
shutil.rmtree("steamaudio")
os.remove("steamaudio_" + version + ".zip")

print("Setup complete!")
print("Note: If import library creation failed, you may need to:")
print("1. Install Visual Studio Build Tools")
print("2. Run this script from a Developer Command Prompt")
print("3. Or manually create import libraries using lib.exe")