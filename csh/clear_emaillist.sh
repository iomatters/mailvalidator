#!/bin/sh
CONFIG_PATH=/usr/local/mailvalidator/csh/mailvalidator.conf
INPUT_DIR=$(awk -F "=" '/^input_dir/{print $2}' $CONFIG_PATH | tr -d ' ')
FILENAME=$(basename $1)
LINES_NUM=$2
# setting defaults
if [ -z "$LINES_NUM" ]; then
  LINES_NUM=500
elif [ "$LINES_NUM" == "0" ]; then
  LINES_NUM=500
fi
grep -EiEio "\b[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,6}\b" "$1" | sort | uniq | tr '[:upper:]' '[:lower:]' | head -n "$LINES_NUM" > "$INPUT_DIR/$FILENAME.clear"
wc -l "$INPUT_DIR/$FILENAME.clear" | awk '{print $1}'
