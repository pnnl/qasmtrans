#!/bin/bash

dir="../data/test_benchmark"  # replace with your directory path
echo "" > output_summary.txt
# Using find command to get all files and iterate over them
for file in $(find "$dir" -type f -name "*.qasm")
do
  echo "===== Processing $file: ====="
  # echo "Processing $file :" >> output_summary.txt
  ./../build/qasmtrans -i $file -c ../data/devices/ibmq_toronto.json -o output.qasm -v 1 -limited >> output_summary.txt
  echo " ===== end ======="
  # echo "with limited coupling graph" >> output_summary.txt
  # ./../build/nwq_qasm -q $file -limited | grep "time is" >> output_summary.txt
  # Add your processing commands here
done

echo "Finish all test files, result store in the output_summary.txt"
