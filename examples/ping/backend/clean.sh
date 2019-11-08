#!/bin/bash
set -e
cd "$( dirname "${BASH_SOURCE[0]}" )"

# remove all build and dist files
rm -rf ./build
rm -rf ./dist
