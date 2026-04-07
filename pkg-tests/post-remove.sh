#!/bin/bash

set -eu

status="$(dkms status)"

echo $status | grep -v "tuxedo-drivers" # Make sure dkms module is removed
