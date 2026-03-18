#!/bin/bash

# name of log file
TIMESTAMP=$(date +"%Y-%m-%d-%H%M%S")
FILE="${PWD}/bt-onoff-$TIMESTAMP.log"

WAITTIME=200
TESTTIMES=10

# create log file
touch $FILE
echo "PASS 0 FAIL 0 TOTAL 0 TIME $(date +%F_%T)" > $FILE


for a in $(seq 1 $TESTTIMES);
do
adb shell reboot

echo -ne  "waiting..."
#setting progress dots
i=1
j=0
sp="/-\|"
echo -n ' '
for i in $(seq 1 $WAITTIME);
do
        j=$((j+1))
        printf "\b${sp:i++%${#sp}:1}"
        sleep 0.5
done
#get Bluetooth status
RESULT=$(adb shell dumpsys bluetooth_manager | grep enabled | awk 'NR==1{print $NF}')
#echo "test bt result: $RESULT"
# read log file data
DATA=$(tail -n 1 $FILE)
#echo $DATA
R_PASS=$(echo $DATA | awk '{print $2}')
R_FAIL=$(echo $DATA | awk '{print $4}')
R_TOTAL=$(echo $DATA | awk '{print $6}')
#echo "PASS: $R_PASS"
#echo "FAIL: $R_FAIL"
#echo "TOTAL: $R_TOTAL"
#writing result to log file
if [ "$RESULT" = "true" ]; then
        echo 
        echo "PASS $((R_PASS + 1)) FAIL $R_FAIL TOTAL $((R_TOTAL + 1)) TIME $(date +%F_%T)" >> $FILE
        echo $(tail -n 1 $FILE)
else
        echo 
        echo "PASS $R_PASS FAIL $((R_FAIL + 1)) TOTAL $((R_TOTAL + 1)) TIME $(date +%F_%T)" >> $FILE
        echo $(tail -n 1 $FILE)
fi


done