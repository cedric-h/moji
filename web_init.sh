mkdir webbuild
cd webbuild
cp ../client/spritesheet.png ./
rm ../client.glsl.h
source ~/emsdk/emsdk_env.sh
emcmake cmake -G"Unix Makefiles" -DCMAKE_BUILD_TYPE=MinSizeRel ..
cmake --build .
