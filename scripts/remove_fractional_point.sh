
#!/bin/bash

# This script is used to remove the fractional part of a decimal number.
# e.g. 12.456 --> 12

FILE=$1

cp $FILE $FILE".orig"

sed -re 's/([0-9]+\.[0-9]{0})[0-9]+/\1/g' $FILE &> temp.txt
cat temp.txt
sed 's/\.//' temp.txt > $FILE

rm temp.txt
