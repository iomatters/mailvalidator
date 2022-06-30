Command line app that verifies email(s) address existence

As simple as that:
./mailvalidator -h

Name:
	mailvalidator v.1.0.1

Description:
	The mailvalidator utility perfoms email address validation tests such as MX servers existence and 250 SMTP reply code.
	Email(es) for validation can be provided in the form of a single argument or a plain-text file containing list of emails.
	Nameservers from etc/resolv.conf are used or 8.8.8.8 by default.

Usage:
	mailvalidator [@server] [-h] [-l] [-a] [-f filename] [-fsize filesize] [-xlist filename] [-d delay msec.]
	[-t socket timeout sec.] [-mx mail servers number] [-p ptr record] [-s mailfrom] [rcptto]

Options:
	-h
		Output this help screen.
	@server
		IP address of the name server to query, if not stated and resolve.conf is unusable, then 8.8.8.8 to be used by default.
	-a
		Perform FQDN A record test (please note, FQDN without A record can still have MX records), default OFF.
	-d NUM
		Delay in msec. to be set while processing a list of emails, default 100.
	-flist filename
		File name containing a list of emails.
	-fsize NUM
		Lines (i.e. emails) number limit to be used in file mode, default 500.
	-l
		Enable logging, default OFF.
	-mx NUM
		MX servers number to be attempted, default 3.
	-p ptr record
		PTR record to be used for EHLO command, default educause.edu.
	-s mailfrom
		MAIL FROM to be used while checking 250 SMTP reply code, default admin@educause.edu.
	-t NUM
		Socket timeout in sec. to be set while establishing connectivity, default 15.
	-xlist filename
		Filename containig blacklisted domains, to be used excluded from validation.

Arguments:
	rcptto
		Email address to be verified.

