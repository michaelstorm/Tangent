#!/bin/bash
# prints the CMS structures encoded in the given files, but does
# not verify their signed data nor write it to FILE.vrf
# usage: ./testverify.sh FILE...
for msg in "$@"
do
  echo `basename "$msg"`:
  openssl cms -verify -in "$msg" -inform der -certfile stelcert.pem -CAfile stelcert.pem -nointern -cmsout -noout -print
done
