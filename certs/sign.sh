#!/bin/bash
# asks for stelkey.pem's pass phrase, then signs the given files
# and packs them into der-encoded files named FILE.sgn
# usage: ./sign.sh FILE...
stty -echo
read -p "Enter pass phrase:" pass
stty echo
echo
for msg in "$@"
do
  openssl cms -sign -in "$msg" -binary -out "`basename "$msg"`".sgn -outform der -nocerts -signer stelcert.pem -inkey stelkey.pem -passin pass:"$pass" -nosmimecap -nodetach
done
