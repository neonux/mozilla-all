# update.sh <chromium-src-directory>
cp $1/media/base/yuv_convert.h .
cp $1/media/base/yuv_convert.cc yuv_convert.cpp
cp $1/media/base/yuv_row.h .
cp $1/media/base/yuv_row_table.cc yuv_row_table.cpp
cp $1/media/base/yuv_row_posix.cc yuv_row_posix.cpp
cp $1/media/base/yuv_row_win.cc yuv_row_win.cpp
cp $1/media/base/yuv_row_posix.cc yuv_row_c.cpp
patch -p3 <convert.patch
patch -p3 <win64.patch
patch -p3 <TypeFromSize.patch
patch -p3 <QuellGccWarnings.patch
