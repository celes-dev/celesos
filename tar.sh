#!/bin/bash
  
current=`date "+%Y%m%d%H%M%S"`
fileName="celesos."${current}".tar.gz"
tar -zcvf ${fileName} ./build