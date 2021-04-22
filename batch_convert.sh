#!bash

cat runstoconv_LDS.txt | while read LINE; 
do 
echo "${LINE}"
#FNAME='datalog_'${LINE:4:-3}".h5"
FNAME='0a7a314c_'${LINE}
echo "Converting " $FNAME

./ADC64Viewer -g 0 -f /data/LRS/$FNAME -m x -w -e300000

echo "Conversion complete, copying to /data/LRS/Converted"

mv *.root /data/LRS/Converted/

done
