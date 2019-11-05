#!/bin/bash
set -e
cd "$( dirname "${BASH_SOURCE[0]}" )"

# build MinSizeRel
./build.sh MinSizeRel

# create cli runtime
rm -rf ./dist/runtime-cli
cp -r ./dist/MinSizeRel/bin ./dist/runtime-cli
rm ./dist/runtime-cli/libaudience_shared.*

# create dynamic runtime
rm -rf ./dist/runtime-dynamic
cp -r ./dist/MinSizeRel/bin ./dist/runtime-dynamic
rm ./dist/runtime-dynamic/audience

# create static runtime
rm -rf ./dist/runtime-static
cp -r ./dist/MinSizeRel/bin ./dist/runtime-static
rm ./dist/runtime-static/audience
rm ./dist/runtime-static/libaudience_shared.*
