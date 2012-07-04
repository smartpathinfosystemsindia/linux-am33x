export PATH=$PATH:/opt/arm-none-linux-gnueabi/bin
export PKG_CONFIG_PATH=$PWD/../install/usr/lib/pkgconfig
pkg-config --cflags libxml-2.0 freetype2
export CC=arm-none-linux-gnueabi-gcc
export TOPDIR=$PWD/../install
export CXX=arm-none-linux-gnueabi-g++
./configure --prefix=$TOPDIR/usr --target=arm-none-linux --host=amd64 --disable-python  --without-python --with-arch=arm --with-freetype-config=$TOPDIR/usr/bin/freetype-config
make ARCH=arm CROSS_COMPILE=arm-none-linux-gnueabi-
make install 
