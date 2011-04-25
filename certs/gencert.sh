#!/bin/bash
# generates a new certificate in stelcert.pem and corresponding private key in stelkey.pem
openssl req -x509 -newkey rsa:1024 -keyout stelkey.pem -out stelcert.pem -days 7300 -utf8 -subj "/C=FR/L=Guéreins/O=Stellarium/CN=Sky Data CA"
