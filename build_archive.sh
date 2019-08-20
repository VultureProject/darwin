#!/bin/sh

###################################################################
# Script name	: build_archive.sh
# Date          : 14/05/18
# Description   : Build a Darwin package
# Author(s)     : hsoszynski
# Licence       : GPLv3
# Brief         : Copyright (c) 2018 Advens. All rights reserved.
###################################################################

set -e

# pkg install -y cmake libevent boost-libs

# To add filter, simply add its name in the filters string separated by a space
# Example: filters="TOTO TITI TATA"
# Note: You can use '\' to make it multi-line
filters=" \
SESSION \
REPUTATION \
HOSTLOOKUP \
USER_AGENT \
DGA \
LOGS \
CONNECTION \
ANOMALY \
TANOMALY \
END \
CONTENT_INSPECTION
"

dest="pkg/stage/home/darwin/"
build_dir="build/"

rm -rf build
mkdir build
rm -rf ${dest}filters
mkdir ${dest}filters

cd ./build

echo "Building filters"
cmake ..
echo "CMAKE DONE"
make -j4

cd -

for elem in ${filters}
do
    name=`echo -n "darwin_$elem" | tr '[:upper:]' '[:lower:]'`
    cp "${build_dir}${name}" "${dest}filters/"
    echo "home/darwin/filters/$name" >> pkg/stage/plist
    echo "home/darwin/filters/$name"
    echo "Done"
done

echo "All filters built in directory:" ${dest}

cp -r manager pkg/stage/home/darwin/

cd ./pkg/stage
find ./ -type f ! -name "plist" ! -name "+MANIFEST" ! -name "+POST_DEINSTALL" ! -name "+PRE_INSTALL" ! -name "+POST_INSTALL" ! -name ".gitkeep" >> plist
cd ../../

echo 'Package content right before creation:'
ls -R ./pkg/

pkg create -m pkg/stage -r pkg/stage -p pkg/stage/plist -o .

echo "Package created."
