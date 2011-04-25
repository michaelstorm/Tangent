#!/bin/bash
# displays stelcert.pem in human-readable format
openssl x509 -in stelcert.pem -noout -text -nameopt oneline -nameopt -esc_msb
