EMAIL=$1
RETURN=$(/usr/local/mailvalidator/mailvalidator -p mail.iomatters.com -s admin@iomatters.com -catchall $EMAIL | awk '{print $2}')
echo $RETURN;
