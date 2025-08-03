#!/usr/bin/env bash
# Bash3 Boilerplate. Copyright (c) 2014, kvz.io

set -o errexit
set -o pipefail
set -o nounset
############### end of Boilerplate

# Run the command to read the PMS3005 sensor (which produces JSON 
# output) 
# Use `jq` to add the timestamp and sensor name and pipe to
# mosquitto_pub to publish

# requirements: install mosquitto-clients and jq (Debian package names.)

"/home/$LOGNAME/bin/pms5003" "/dev/ttyAMA0" 4 | \
    /bin/jq -c '. += {"t":'"$(date +%s)"',"sensor":"PMS5003"}'| \
    mosquitto_pub -h mqtt -l -t "HA/$(/bin/hostname)/roamer/particulates"
