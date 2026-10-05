#!/bin/bash

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/.." && pwd)"

dir="$repo_root/data/test_benchmark"
echo "" > "$script_dir/output_summary.txt"
# Using find command to get all files and iterate over them
for file in $(find "$dir" -type f -name "*.qasm")
do
  echo "===== Processing $file: ====="
  # echo "Processing $file :" >> output_summary.txt
  "$repo_root/build/QASMTrans" -i "$file" -c "$repo_root/data/devices/ibmq_toronto.json" -o "$script_dir/output.qasm" -v 1 -limited >> "$script_dir/output_summary.txt"
  echo " ===== end ======="
  # echo "with limited coupling graph" >> output_summary.txt
  # ./../build/nwq_qasm -q $file -limited | grep "time is" >> output_summary.txt
  # Add your processing commands here
done

echo "Finish all test files, result store in the output_summary.txt"
