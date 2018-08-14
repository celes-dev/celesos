#!/bin/bash
  
current=`date "+%Y%m%d%H%M%S"`
fileName="celesos."${current}".zip"
echo ${fileName}
zip -q -r ${fileName} ./build