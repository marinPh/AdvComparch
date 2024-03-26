
# build the FullyFunk executable

# Path: HW1/build.sh

# compile the FullyFunk.c file

cmake ./build
cd build 
make
mv Simulate ../
cd ..