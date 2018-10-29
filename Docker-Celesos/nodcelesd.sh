#!/bin/sh
cd /opt/celesos/bin

if [ ! -d "/opt/celesos/bin/data-dir" ]; then
    mkdir /opt/celesos/bin/data-dir
fi

if [ -f '/opt/celesos/bin/data-dir/config.ini' ]; then
    echo
  else
    cp /config.ini /opt/celesos/bin/data-dir
fi

if [ -d '/opt/celesos/bin/data-dir/contracts' ]; then
    echo
  else
    cp -r /contracts /opt/celesos/bin/data-dir
fi

while :; do
    case $1 in
        --config-dir=?*)
            CONFIG_DIR=${1#*=}
            ;;
        *)
            break
    esac
    shift
done

if [ ! "$CONFIG_DIR" ]; then
    CONFIG_DIR="--config-dir=/opt/celesos/bin/data-dir"
else
    CONFIG_DIR=""
fi

exec /opt/celesos/bin/nodceles $CONFIG_DIR "$@"
