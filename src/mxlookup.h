#ifndef MXLOOKUP_H
#define MXLOOKUP_H

// Parameters of DNS records
#define RECORDS_COUNT 10 
#define RECORDS_LEN 100 
// Types of DNS resource records
#define T_A 1                   // Ipv4 address
#define T_NS 2                  // Nameserver
#define T_CNAME 5               // canonical name
#define T_SOA 6                 // start of authority zone
#define T_PTR 12                // domain name pointer
#define T_MX 15                 // Mail server


typedef struct _DNS_RECORDS
{
   unsigned short len;
   char record[RECORDS_COUNT][RECORDS_LEN];
} DNS_RECORDS;

// Function Prototypes
void ngethostbyname (DNS_RECORDS&, unsigned char*, char*, int);

#endif
