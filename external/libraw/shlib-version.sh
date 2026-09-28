#!/bin/sh

vfile=./libraw/libraw_version.h

major=`grep LIBRAW_SHLIB_CURRENT $vfile |head -1 | awk '{print $3}'`
minor=`grep LIBRAW_SHLIB_REVISION $vfile | head -1 | awk '{print $3}'`
patch=`grep LIBRAW_SHLIB_AGE $vfile | head -1 | awk '{print $3}'`

echo "$major:$minor:$patch" | awk '{printf $1}'


