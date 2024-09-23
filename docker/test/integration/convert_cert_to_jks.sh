#!/bin/bash

# Usage: ./create_jks.sh <directory> <ssl_key_path> <ssl_cert_path> <ca_cert_path>

# Input Arguments
DIR=$1
SSL_KEY_PATH=$2
SSL_CERT_PATH=$3
CA_CERT_PATH=$4

# Keystore and Truststore Files
KEYSTORE="$DIR/keystore.jks"
TRUSTSTORE="$DIR/truststore.jks"
PKCS12_FILE="$DIR/keystore.p12"

# Common password
PASSWORD="passw0rd1!"

cat $CA_CERT_PATH >> $SSL_CERT_PATH

# Check if directory exists
if [ ! -d "$DIR" ]; then
    echo "Directory $DIR does not exist. Creating it."
    mkdir -p "$DIR"
fi

# Step 1: Convert SSL Key and Certificate to PKCS12 format
echo "Converting SSL key and certificate to PKCS12 format..."
openssl pkcs12 -export \
  -inkey "$SSL_KEY_PATH" \
  -in "$SSL_CERT_PATH" \
  -name "nifi-key" \
  -out "$PKCS12_FILE" \
  -password pass:$PASSWORD

# Step 2: Import the PKCS12 file into the JKS Keystore
echo "Creating JKS keystore from PKCS12..."
keytool -importkeystore \
  -destkeystore "$KEYSTORE" \
  -deststoretype jks \
  -destalias nifi-key \
  -srckeystore "$PKCS12_FILE" \
  -srcstoretype pkcs12 \
  -srcalias "nifi-key" \
  -storepass "$PASSWORD" \
  -srcstorepass "$PASSWORD" \
  -noprompt

# Step 3: Import the CA certificate into the Truststore
echo "Creating JKS truststore and importing CA certificate..."
keytool -importcert \
  -alias "nifi-cert" \
  -file "$CA_CERT_PATH" \
  -keystore "$TRUSTSTORE" \
  -storepass "$PASSWORD" \
  -noprompt

echo "Keystore and Truststore creation complete."
echo "Keystore location: $KEYSTORE"
