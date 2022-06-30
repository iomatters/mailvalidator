#!/bin/sh
CONFIG_PATH=/usr/local/mailvalidator/csh/mailvalidator.conf
MYSQL_CNF=$(awk -F "=" '/^mysql_cnf/{print $2}' $CONFIG_PATH | tr -d ' ')
BUF_DIR=$(awk -F "=" '/^buf_dir/{print $2}' $CONFIG_PATH | tr -d ' ')
OUTPUT_DIR=$(awk -F "=" '/^output_dir/{print $2}' $CONFIG_PATH | tr -d ' ')
# main loop fetching all the files waiting to be completed
for entry in $(mysql --defaults-file=$MYSQL_CNF -sN -e "select file_name from wp_user_files where file_status=1;")
do
  # cleaning buffer
  find $BUF_DIR -type f -name $entry'*' -exec rm -f {} \; > /dev/null 2>&1
  # cleaning output
  find $OUTPUT_DIR -type f -name $entry'*' -exec rm -f {} \; > /dev/null 2>&1
  # updating status
  # mysql --defaults-file=$MYSQL_CNF -sN -e "update wp_user_files set file_status=1 where file_name='$entry';"
done
