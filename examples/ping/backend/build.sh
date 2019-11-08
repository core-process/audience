#!/bin/bash
set -e

# go to project directory
EXAMPLE_PING_PROJECT_ROOT="$(realpath "$( dirname "${BASH_SOURCE[0]}" )")"
echo "EXAMPLE_PING_PROJECT_ROOT=$EXAMPLE_PING_PROJECT_ROOT"
cd "$EXAMPLE_PING_PROJECT_ROOT"

# read build type
declare -a VALID_CMAKE_BUILD_TYPES=("Debug" "Release" "RelWithDebInfo" "MinSizeRel")

if (( $# < 1 )) || [[ ! " ${VALID_CMAKE_BUILD_TYPES[@]} " =~ " $1 " ]];
then
  echo "Usage: $0 <$(IFS="|"; echo "${VALID_CMAKE_BUILD_TYPES[*]}")>"
  exit 1
fi

CMAKE_BUILD_TYPE="$1"

# create build directory
mkdir -p "./build/$CMAKE_BUILD_TYPE"
cd "./build/$CMAKE_BUILD_TYPE"

# initialize cmake cache
if [ ! -f ./CMakeCache.txt ];
then
  cmake -G "Unix Makefiles" "-DCMAKE_BUILD_TYPE=$CMAKE_BUILD_TYPE" "-DCMAKE_INSTALL_PREFIX:PATH=../../dist" ../..
fi

# perform build and install
cmake --build .
cmake --build . --target install
