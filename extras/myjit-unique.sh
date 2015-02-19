#!/bin/bash

PREFIX="jit_internal_"
TARGET_DIR="../myjit-unique"
SCRIPT=`mktemp`

echo $SCRIPT

cat ../myjit/*.c ../myjit/*.h | \
sed -n -e '/^[[:alpha:]].*(/p' | \
sed -e 's/\([^ ]*\)(.*/#\1/' -e 's/.*#//' | \
grep -v "^jit" | sort | uniq | \
while read FUNC; do
echo "s/$FUNC/$PREFIX$FUNC/g"
done > "$SCRIPT"

mkdir -p "$TARGET_DIR"

cd ../myjit
for FILE in *.c *.h
do
	echo $FILE
	sed -f "$SCRIPT" $FILE > "$TARGET_DIR/$FILE"
done

rm -f "$SCRIPT"
