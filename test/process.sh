#build library
make -C../src clean
make -C../src install COVERAGE_BUILD=1

#build testsuite
make clean && make

./libprimis_testsuite

gcov ../src/engine/*/* -r > output.txt

grep 'Lines' output.txt > output2.txt #only get lines with numbers
grep -Eo '+[.1234567890]*[1234567890]' output2.txt > output3.txt #-Eo: regex & only select grepped text


loop=0 #needed to sort properly between cov%s and line counts
temp=0;
points=0;
total=0;
while read line;
do
    loop=$(($loop + 1))
    if (($loop % 2))
    then
        temp=$line #these are the percents
    else
        total=$(($total + $line)) #total lines of addressable code
        points=$(bc <<< "$points + ( 0.01 * $line * $temp)")
    fi
done < output3.txt

#rounding
total1=$(bc -l <<< "1000 * 100 * $points / $total")
total2=$(bc <<< "$total1 / 1")

#final output
echo $(bc -l <<< "$total2 / 1000")
