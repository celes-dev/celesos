#!/bin/bash

# Usage:
# Go into cmd loop: sudo ./clceles.sh
# Run single cmd:  sudo ./clceles.sh <clceles paramers>

PREFIX="docker-compose exec nodcelesd clceles"
if [ -z $1 ] ; then
  while :
  do
    read -e -p "clceles " cmd
    history -s "$cmd"
    $PREFIX $cmd
  done
else
  $PREFIX "$@"
fi
