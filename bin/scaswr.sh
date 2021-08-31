#!/usr/bin/env bash
# Scoarers Converter - assembler and writer

if [[ "$1" == "" ]] ; then
  echo "Please provide the name of the Soarer's Converter code to compile."
  exit 1
fi

# See if the scas/scwr are in the path or not.
WSCAS="$(which scas)"
WSCWR="$(which scwr)"

# If 'scas'/'scwr' is in the path use it, otherwise see if it is in
# the current directory.
SCAS=${WSCAS:-"./scas"}
SCWR=${WSCWR:-"./scwr"}

# Exit on undefined variable (-u) or any error (-e)
set -u
set -e

SOURCE=$1
SOURCE_PATH=$(dirname ${SOURCE})
SOURCE_FILE=$(basename ${SOURCE})

# Run scas with a default target filename...
echo "Assembling $1..."
${SCAS} ${SOURCE_PATH}/${SOURCE_FILE} ${SOURCE_FILE}.scb

# Run scwr with that default filename...
echo "Writing $1.scb using sudo command."
sudo ${SCWR} ${SOURCE_FILE}.scb
EC=$?

if [[ $EC -eq 0 ]] ; then
  echo Compiled and uploaded successfully.
else
  echo Errors uploading - review output above.
fi
