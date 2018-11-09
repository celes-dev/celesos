#!/bin/bash
##########################################################################
# This is the CELESOS automated install script for Linux and Mac OS.
# This file was downloaded from https://github.com/CELESOS/celes
#
# Copyright (c) 2017, Respective Authors all rights reserved.
#
# After June 1, 2018 this software is available under the following terms:
# 
# The MIT License
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#
# https://github.com/CELESOS/celes/blob/master/LICENSE.txt
##########################################################################

if [ "$(id -u)" -ne 0 ]; then
        printf "\n\tThis requires sudo. Please run with sudo.\n\n"
        exit -1
fi   

   CWD="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
   if [ "${CWD}" != "${PWD}" ]; then
      printf "\\n\\tPlease cd into directory %s to run this script.\\n \\tExiting now.\\n\\n" "${CWD}"
      exit 1
   fi

   BUILD_DIR="${PWD}/build"
   CMAKE_BUILD_TYPE=Release
   TIME_BEGIN=$( date -u +%s )
   INSTALL_PREFIX="/usr/local/celesos"
   VERSION=1.2

   txtbld=$(tput bold)
   bldred=${txtbld}$(tput setaf 1)
   txtrst=$(tput sgr0)

   create_symlink() {
      pushd /usr/local/bin &> /dev/null
      ln -sf ../celesos/bin/$1 $1
      popd &> /dev/null
   }

   create_cmake_symlink() {
      mkdir -p /usr/local/lib/cmake/celesos
      pushd /usr/local/lib/cmake/celesos &> /dev/null
      ln -sf ../../../celesos/lib/cmake/celesos/$1 $1
      popd &> /dev/null
   }

   install_symlinks() {
      printf "\\n\\tInstalling CELESOS Binary Symlinks\\n\\n"
      create_symlink "clceles"
      create_symlink "celes-abigen"
      create_symlink "celes-launcher"
      create_symlink "eosio-s2wasm"
      create_symlink "eosio-wast2wasm"
      create_symlink "eosiocpp"
      create_symlink "kcelesd"
      create_symlink "nodceles"
   }

   if [ ! -d "${BUILD_DIR}" ]; then
      printf "\\n\\tError, celesos_build.sh has not ran.  Please run ./celesos_build.sh first!\\n\\n"
      exit -1
   fi

   ${PWD}/scripts/clean_old_install.sh
   if [ $? -ne 0 ]; then
      printf "\\n\\tError occurred while trying to remove old installation!\\n\\n"
      exit -1
   fi

   if ! pushd "${BUILD_DIR}"
   then
      printf "Unable to enter build directory %s.\\n Exiting now.\\n" "${BUILD_DIR}"
      exit 1;
   fi
   
   if ! make install
   then
      printf "\\n\\t>>>>>>>>>>>>>>>>>>>> MAKE installing CELESOS has exited with the above error.\\n\\n"
      exit -1
   fi
   popd &> /dev/null 

   install_symlinks
   create_cmake_symlink "celesos-config.cmake"

   printf "\n\n${bldred}"
   printf "\t #####  ####### #       #######  #####  #######  #####\n"
   printf "\t#     # #       #       #       #     # #     # #     #\n"
   printf "\t#       #       #       #       #       #     # #\n"
   printf "\t#       #####   #       #####    #####  #     #  #####\n"
   printf "\t#       #       #       #             # #     #       #\n"
   printf "\t#     # #       #       #       #     # #     # #     #\n"
   printf "\t #####  ####### ####### #######  #####  #######  #####\n"
   printf "${txtrst}"

   printf "\\tFor more information:\\n"
   printf "\\tCELESOS website: https://celes.os\\n"
   printf "\\tCELESOS resources: https://celes.os/resources/\\n"
   printf "\\tCELESOS wiki: https://github.com/CELESOS/celes/wiki\\n\\n\\n"
