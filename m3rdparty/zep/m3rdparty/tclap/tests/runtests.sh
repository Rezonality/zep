#!/bin/bash

let "suc = 0"
let "fail = 0"
NUMTEST=82

for (( tno = 1 ; $tno <= $NUMTEST ; tno = $tno + 1 )); do     
    ./testCheck.sh $tno
    if [ "$?" -eq "0" ]; then 
	echo "OK" 
	let "suc = $(($suc + 1))"
    else
	echo "FAIL" 
	let "fail = $(($fail + 1))"
    fi
done

let "total = $(($suc + $fail))"
echo "======================"
echo "|     TESTS DONE     |"
echo "======================"
echo " TOTAL: $total"
echo "    OK: $suc"
echo "FAILED: $fail"

if [ $fail -ne 0 ]; then
    exit 1
fi
    
