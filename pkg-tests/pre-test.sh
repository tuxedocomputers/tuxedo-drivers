#!/bin/bash

set -eu

# Wonderful workaround for zypper that doesn't work properly if this isn't added.
if [ ! -d /var/tmp/dkms ]; then
    mkdir -p /var/tmp/dkms
fi
