#!/bin/sh
CONFIG_PATH=/root/bin/shell/mailvalidator.conf
INPUT_DIR=$(awk -F "=" '/^input_dir/{print $2}' $CONFIG_PATH | tr -d ' ')
COMPLETED_DIR=$(awk -F "=" '/^completed_dir/{print $2}' $CONFIG_PATH | tr -d ' ')
# main loop fetching all except already cleared files in a directory
for entry in $(find $INPUT_DIR -type f | egrep -v '*.clear$')
do
  # clear all the files - leave emails only + remove dubplicates + lowercase 
  FILENAME=$(basename $entry)
  grep -EiEio "\b[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,6}\b" "$INPUT_DIR/$FILENAME" | sort | uniq | tr '[:upper:]' '[:lower:]' > "$INPUT_DIR/$FILENAME.clear" 
  touch "$COMPLETED_DIR/$FILENAME"
  rm "$INPUT_DIR/$FILENAME" 
done

