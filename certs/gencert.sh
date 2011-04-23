#!/bin/sh
openssl req -x509 -newkey rsa:1024 -keyout stelkey.pem -out stelcert.pem -days 7300 -subj "/C=FR/L=Gu\\\xC3\\\x83\\\xC2\\\xA9reins/O=Stellarium/CN=stellarium.org\/emailAddress=oopsdude@gmail.com"
