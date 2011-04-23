#!/bin/sh
openssl cms -sign -in "$1" -binary -out "`basename $1`.sgn" -outform der -nocerts -signer stelcert.pem -inkey stelkey.pem -nosmimecap -nodetach
