#! /bin/bash

binaries=(clceles
          celes-abigen
          celes-launcher
          eosio-s2wasm
          eosio-wast2wasm
          eosiocpp
          kcelesd
          nodceles
          eosio-applesdemo)

if [ -d "/usr/local/celesos" ]; then
   printf "\tDo you wish to remove this install? (requires sudo)\n"
   select yn in "Yes" "No"; do
      case $yn in
         [Yy]* )
            if [ "$(id -u)" -ne 0 ]; then
               printf "\n\tThis requires sudo, please run ./celesos_uninstall.sh with sudo\n\n"
               exit -1
            fi

            pushd /usr/local &> /dev/null
            rm -rf celesos
            pushd bin &> /dev/null
            for binary in ${binaries[@]}; do
               rm ${binary}
            done
            # Handle cleanup of directories created from installation
            if [ "$1" == "--full" ]; then
               if [ -d ~/Library/Application\ Support/celesos ]; then rm -rf ~/Library/Application\ Support/celesos; fi # Mac OS
               if [ -d ~/.local/share/celesos ]; then rm -rf ~/.local/share/celesos; fi # Linux
            fi
            popd &> /dev/null
            break;;
         [Nn]* )
            printf "\tAborting uninstall\n\n"
            exit -1;;
      esac
   done
fi
