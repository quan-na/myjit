#!/bin/bash

PREFIX="jit_internal_"
TARGET_DIR="../myjit-unique"
SCRIPT=`mktemp`


#
# lists all functions in the source codes passed to the stdin
#
function list_functions {
	sed -n -e '/^[[:alpha:]].*(/p' | \
	sed -e 's/\([^ ]*\)(.*/#\1/' -e 's/.*#//' | \
	grep -v "^jit" | sort | uniq 
}


#
# lists all types in the source codes passed to the stdin
#
function list_types {
	tr '\r\n' '  ' | sed -e 's/{[^}]*}//g' | tr ';' '\n' | sed -n -e '/typedef/ {s/.* //p}' | \
	grep -v "^jit" | sort | uniq | grep -v "value"
}


function list_constants {
	echo "OUT_REGS"
	echo "x86_cc_unsigned_map"
	echo "x86_cc_signed_map"
}

#
# reads a list of symbols from stdin and prints sed script
#
function create_sed_script {
	while read SYMBOL; do
		echo "s/$SYMBOL/$PREFIX$SYMBOL/g"
	done

}

cat ../myjit/*.c ../myjit/*.h | list_functions | create_sed_script > "$SCRIPT"
cat ../myjit/*.c ../myjit/*.h | list_types  | create_sed_script >> "$SCRIPT"
cat ../myjit/*.c ../myjit/*.h | list_constants | create_sed_script >> "$SCRIPT"



mkdir -p "$TARGET_DIR"

cd ../myjit
for FILE in *.c *.h
do
	echo $FILE
	sed -f "$SCRIPT" $FILE > "$TARGET_DIR/$FILE"
done

rm -f "$SCRIPT"
