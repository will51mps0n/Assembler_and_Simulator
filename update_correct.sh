#!/bin/bash
make clean
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

echo "Generating .correct files..."
for test in "${tests[@]}"; do
  echo "Processing $test..."
  ../assembler ${test}.as ${test}.mc
  ../simulator ${test}.mc > ${test}Out.correct
done

echo "Done generating .correct outputs."
