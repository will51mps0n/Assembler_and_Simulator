#!/bin/bash
make assembler
make simulator

cd tests

declare -a tests=(
  "p3spec"
  "test2dAdd"
  "lwdependency"
  "lwdependencynohaz"
  "testLoop"
  "testLwALU"
  "testLwSwAdd"
  "testaddCat"
  "testdoblsw"
  "testlsl"
)

echo "Running test suite..."
for test in "${tests[@]}"; do
  echo "Testing $test..."
  ../assembler ${test}.as ${test}.mc
  ../simulator ${test}.mc > ${test}Out.txt
  diff ${test}Out.txt ${test}Out.correct > /dev/null

  if [ $? -eq 0 ]; then
    echo "[PASS] ${test}"
  else
    echo "[FAIL] ${test} — Output differs from correct"
    echo "Diff:"
    diff ${test}Out.txt ${test}Out.correct
    echo
  fi
done

echo "All tests complete."
