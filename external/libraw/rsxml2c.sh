#!/bin/sh

echo "const char *_rawspeed_data_xml[]={"
cat $1 | tr -d '\015' | sed -e 's/\\/\\\\/g;s/"/\\"/g;s/  /\\t/g;s/^/"/;s/$/\\n",/'
echo "0"
echo "};"
