root@web:/home/mantas/www/wp/wp-content/themes/custom # cat /usr/local/mailvalidator/csh/3_process_them_all.sh
#!/bin/sh
CONFIG_PATH=/usr/local/mailvalidator/csh/mailvalidator.conf
BUF_DIR=$(awk -F "=" '/^buf_dir/{print $2}' $CONFIG_PATH | tr -d ' ')
OUTPUT_DIR=$(awk -F "=" '/^output_dir/{print $2}' $CONFIG_PATH | tr -d ' ')
MAILVALIDATOR_PATH=$(awk -F "=" '/^mailvalidator_path/{print $2}' $CONFIG_PATH | tr -d ' ')
BLACKLIST_PATH=$(awk -F "=" '/^blacklist_path/{print $2}' $CONFIG_PATH | tr -d ' ')
PROC_MAX_N=$(awk -F "=" '/^proc_max_n/{print $2}' $CONFIG_PATH | tr -d ' ')
I=0
while [ $I -le 1 ]
do
  sleep 15
  # main loop fetching all the files in a buf directory
  for entry in $(find $BUF_DIR -type f)  #$BUF_DIR/*
  do
    # check if we overrun PROC_MAX_N
    PROC_N=$( ps auxwww | grep $MAILVALIDATOR_PATH | wc -l | awk '{print $1}' )
    if [ "$PROC_N" -ge "$PROC_MAX_N" ]; then
          break
    fi
    # check if the file is already in processing
    FILENAME=$(basename $entry)
    if [ -f "$OUTPUT_DIR/$FILENAME.output" ]; then
          continue
    else
          $MAILVALIDATOR_PATH -l -p mail.iomatters.com -s admin@iomatters.com -catchall -t 10 -mx 1 -xlist $BLACKLIST_PATH -flist $entry > $OUTPUT_DIR/$FILENAME.output &
    fi
  done
  I=`expr $I + 1`
done
