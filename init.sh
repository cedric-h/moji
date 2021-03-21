git submodule update --init --recursive
mkdir formpack/build
cd formpack/build
cmake ..
cd ../../
mkdir nativebuild
cd nativebuild
cmake ..
cp ../client/spritesheet.png ./
