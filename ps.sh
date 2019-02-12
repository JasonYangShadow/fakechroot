#!/bin/bash

pid=`echo $$`
`ps -p $pid|grep -v grep`
