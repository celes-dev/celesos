#!/bin/bash
  
current=`date "+%Y%m%d%H%M%S"`
fileName="celesos."${current}".zip"
zip -q -r ${fileName} ./build