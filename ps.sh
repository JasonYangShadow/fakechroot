#!/bin/bash

pid=`echo $$`
`ps -ef|grep $pid|awk '{printf}'`
