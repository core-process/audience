#!/bin/bash
set -e
cd "$( dirname "${BASH_SOURCE[0]}" )"

rm -rf ./node_modules
rm -f ./index.d.ts ./postinstall.d.ts
rm -f ./index.js ./postinstall.js
