#!/bin/sh

DEST=$1
VERSION=$2
if test x$VERSION = x ; then
 VERSION=`./version.sh`
 echo VERSION set to $VERSION
fi

if test -d $DEST ; then
 echo Using $DEST/$VERSION
else
  echo Usage: $0 destination-dir
  exit 1
fi
cd ..
for dir in LibRaw 
do
 cd $dir
 git pull origin
 cd ..
done
for dir in LibRaw 
do
 cd $dir
 git archive --prefix=$dir-$VERSION/ $VERSION | (cd $DEST; tar xvf - )
 cd ..
done
