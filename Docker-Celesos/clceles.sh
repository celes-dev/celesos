#!/bin/bash

# Usage:
# Go into cmd loop: sudo ./clceles.sh
# Run single cmd:  sudo ./clceles.sh <cleos paramers>

PREFIX="docker-compose exec nodeosd cleos"
if [ -z $1 ] ; then
  while :
  do
    read -e -p "cleos " cmd
    history -s "$cmd"
    $PREFIX $cmd
  done
else
  $PREFIX "$@"
fi
