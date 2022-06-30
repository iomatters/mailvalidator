#!/bin/sh
CONFIG_PATH=/usr/local/mailvalidator/csh/mailvalidator.conf
INPUT_DIR=$(awk -F "=" '/^input_dir/{print $2}' $CONFIG_PATH | tr -d ' ')
BUF_DIR=$(awk -F "=" '/^buf_dir/{print $2}' $CONFIG_PATH | tr -d ' ')
LINES_NUM=$(awk -F "=" '/^lines_num/{print $2}' $CONFIG_PATH | tr -d ' ')
I=0
while [ $I -le 2 ]
do
  find $INPUT_DIR -type f -exec basename {} \; | xargs -I {} split -d -a 3 -l $LINES_NUM "$INPUT_DIR/{}" "$BUF_DIR/{}"
  find $INPUT_DIR -type f -exec rm {} \;
  I=`expr $I + 1`
  sleep 10
done
