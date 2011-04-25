#!/bin/bash
# verifies the given files' signatures against stelcert.pem, then unpacks
# their signed data to FILE.vrf
# usage: ./verify.sh FILE...
for msg in "$@"
do
  openssl cms -verify -in "$msg" -inform der -certfile stelcert.pem -out "`basename "$msg"`".vrf -CAfile stelcert.pem -nointern
done
