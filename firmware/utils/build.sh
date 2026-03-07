#!/usr/bin/env bash
set -euo pipefail

# =============================================================================
#                                 !!Important!!
#   This script is NOT meant to be run standalone and is instead called from
#   a docker container with the relavant environment variables set. Please do
#   refrain from running this script if you are not absolutely sure of what y
#   -ou are doing.
# =============================================================================

# create secrets dir if it doesn't exist
mkdir -p $PROJECT_DIR/secrets

# generate secrets header
python3 utils/derive_secrets.py /secrets/global.secrets ${HSM_PIN} ${PERMISSIONS}

# build script
rm -rf $BUILD_DIR/*
cmake -B $BUILD_DIR
cmake --build $BUILD_DIR

# copy output to output dir
mkdir -p $OUTPUT_DIR
cp $BUILD_DIR/shift.hex $BUILD_DIR/shift.bin $BUILD_DIR/shift $OUTPUT_DIR
mv $OUTPUT_DIR/shift.hex $OUTPUT_DIR/hsm.hex
mv $OUTPUT_DIR/shift.bin $OUTPUT_DIR/hsm.bin
mv $OUTPUT_DIR/shift $OUTPUT_DIR/hsm
