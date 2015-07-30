#!/bin/bash


./genFasta | ./validate
./validate test/pass*


for filename in test/fail*
do 
	! ./validate $filename
done
