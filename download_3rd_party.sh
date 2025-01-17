#!/bin/bash

SCRIPT_PATH=$(readlink -f "$0")
OPEN_SOURCE_AE_REPO_PATH=$(dirname "$SCRIPT_PATH")

rm -rf "$OPEN_SOURCE_AE_REPO_PATH/ExternalLibs"
wget "http://downloads.popcornfx.com/Plugins/ExternalLibs/ExternalLibs_AfterEffects_2.20.6-22710_x64_macosx_vs2019.zip" -O "$OPEN_SOURCE_AE_REPO_PATH/ExternalLibs.zip"
unzip "$OPEN_SOURCE_AE_REPO_PATH/ExternalLibs.zip"
rm -f "$OPEN_SOURCE_AE_REPO_PATH/ExternalLibs.zip"
