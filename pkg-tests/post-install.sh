#!/bin/bash

set -eu

if [ "$1" = "alpm" ];
    # Don't check on Arch (no dkms install is done according to guidelines)
    exit 0
fi

status="$(dkms status)"

echo $status | grep "tuxedo-drivers" # Make sure dkms module is installed
