#!/bin/bash
trap "" SIGHUP
java -jar webcontrol.jar 8080
