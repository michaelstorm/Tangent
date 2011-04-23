#!/bin/sh
openssl cms -verify -in "$1" -inform der -certfile stelcert.pem -out `basename "$1"`.vrf -CAfile stelcert.pem -nointern
