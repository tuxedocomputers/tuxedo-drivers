#!/bin/bash

set -eu

if [ "$1" = "alpm" ]; then
    # Don't check on Arch (no dkms install is done according to guidelines)
    exit 0
fi

status="$(dkms status)"

echo $status | grep -v "tuxedo-drivers" # Make sure dkms module is removed
