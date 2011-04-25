#!/bin/bash
# prints the CMS structures that would be created by signing the given files,
# without actually writing them to FILE.sgn
# usage: ./testsign.sh FILE...
stty -echo
read -p "Enter pass phrase:" pass
stty echo
echo
for msg in "$@"
do
  echo `basename "$msg"`.sgn:
  openssl cms -sign -in "$msg" -binary -nocerts -signer stelcert.pem -inkey stelkey.pem -passin pass:"$pass" -nosmimecap -nodetach -noout -print
done
