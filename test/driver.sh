#!/bin/bash


./genFasta | ./validate
./validate test/pass*

# head prints header lines, breaking the fasta structure
! ( head --verbose test/pass* | ./validate )


for filename in test/fail*
do 
	! ./validate $filename
done
