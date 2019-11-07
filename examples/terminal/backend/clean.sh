#!/bin/bash
set -e
cd "$( dirname "${BASH_SOURCE[0]}" )"

rm -rf ./node_modules
rm -f ./package-lock.json
rm -f ./main.js ./quotes.js
