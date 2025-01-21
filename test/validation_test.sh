#!/bin/bash

dir="../data/test_benchmark"
echo "" > compare_summary.txt

for file in $(find "$dir" -type f -name "*.qasm")
do
  # Skip files containing "vqe" in their name
  if [[ $file == *vqe* ]] || [[ $file == *qec* ]] || [[ $file == *bwt* ]]; then
    continue
  fi
  
  echo "===== Processing $file: ====="

  # Execute the C++ program and save the output to a temporary file
  ./../build/qasmtrans -i $file -c ../data/devices/ibmq_toronto.json -o ../build/output.qasm -v 1 -limited > temp_cpp_output.txt
  echo "===== Processing $file: =====" >> compare_summary.txt
  # Execute the Python script and save the output to a temporary file
  python3 test.py $file >>  compare_summary.txt

  echo "===== end ======="
done

# Cleanup temporary files
rm temp_cpp_output.txt 

echo "Finish all test files, result store in the compare_summary.txt"
