#!/bin/sh
set -eu

WORKINGDIR=${1}
shift
OUTPUT="${PWD}/${1}"
shift

echo "#ifndef LIB_SLAPT" > "${OUTPUT}"
echo "#define LIB_SLAPT 1" >> "${OUTPUT}"
cd "${WORKINGDIR}"
cat $@ | grep -v '#include \"' >> "${OUTPUT}"
echo "#endif" >> "${OUTPUT}"
