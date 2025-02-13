#!/bin/bash

SCRIPT_PATH=$(readlink -f "$0")
OPEN_SOURCE_AE_REPO_PATH=$(dirname "$SCRIPT_PATH")

rm -rf "$OPEN_SOURCE_AE_REPO_PATH/ExternalLibs"
wget "http://downloads.popcornfx.com/Plugins/ExternalLibs/ExternalLibs_AfterEffects_2.20.7-23076_x64_vs2019_macosx.zip" -O "$OPEN_SOURCE_AE_REPO_PATH/ExternalLibs.zip"
unzip "$OPEN_SOURCE_AE_REPO_PATH/ExternalLibs.zip"
rm -f "$OPEN_SOURCE_AE_REPO_PATH/ExternalLibs.zip"
