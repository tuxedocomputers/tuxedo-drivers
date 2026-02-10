#!/bin/bash

set -eu

status="$(dkms status)"

echo $status | grep "tuxedo-drivers" # Make sure dkms module is installed
