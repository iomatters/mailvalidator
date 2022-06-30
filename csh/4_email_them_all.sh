#!/bin/sh
CONFIG_PATH=/usr/local/mailvalidator/csh/mailvalidator.conf
MYSQL_CNF=$(awk -F "=" '/^mysql_cnf/{print $2}' $CONFIG_PATH | tr -d ' ')
BUF_DIR=$(awk -F "=" '/^buf_dir/{print $2}' $CONFIG_PATH | tr -d ' ')
OUTPUT_DIR=$(awk -F "=" '/^output_dir/{print $2}' $CONFIG_PATH | tr -d ' ')
COMPLETED_DIR=$(awk -F "=" '/^completed_dir/{print $2}' $CONFIG_PATH | tr -d ' ')
I=0
while [ $I -le 2 ]
do
  # main loop fetching all the files waiting to be completed
  for entry in $(mysql --defaults-file=$MYSQL_CNF -sN -e "select file_name from wp_user_files where file_status=0;")
  do
    # buffered files number
    BUF_FILES=$( find $BUF_DIR -type f -name $entry'*' | wc -l )
    # output NOT EMPTY files number
    OUTPUT_FILES=$( find $OUTPUT_DIR -type f -not -empty -name $entry'*' | wc -l )
    # file is not yet presented
    if [ $BUF_FILES == "0" ]; then
      continue
    # if buffered files do not match processed files
    elif [ $BUF_FILES -ne $OUTPUT_FILES ]; then
      continue
    else
      # concatenating
      find $OUTPUT_DIR -type f -name $entry'*' -exec cat {} \; > "$COMPLETED_DIR/$entry"
      # calculating deliverable emails  number
      DELIVERABLE_EMAILS=$(awk '/\t250\t/ {print $2}' "$COMPLETED_DIR/$entry" | wc -l)
      echo $DELIVERABLE_EMAILS > "$COMPLETED_DIR/tempas"
      mysql --defaults-file=$MYSQL_CNF -sN -e "update wp_user_files set file_status=1, valid_emails=$DELIVERABLE_EMAILS, completed_at=now() where file_name='$entry';"
    fi
  done
  sleep 15
  I=`expr $I + 1`
done
