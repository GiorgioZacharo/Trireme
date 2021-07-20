
#!/bin/bash

# This script is used to remove the fractional part of a decimal number.
# e.g. 12.456 --> 12

FILE=$1

cp $FILE $FILE".orig"

sed -re 's/,85//g' $FILE &> temp.txt
sed -re 's/,86//g' $FILE &> temp.txt
#sed 's/\.//' temp.txt > $FILE
awk ' ($3 > 0 ) ' temp.txt > $FILE

