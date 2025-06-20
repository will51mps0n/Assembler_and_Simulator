# LC-2K Assembler & Simulator

This project implements a two-part system for the LC-2K architecture:
1. **Assembler** — converts LC-2K assembly code into machine code (`.mc`)
2. **Simulator** — executes LC-2K machine code and prints output based on program logic

Written entirely in C and tested using a suite of curated test programs.

---

## Features

- Full support for LC-2K instruction set
- Strict error handling and debug-friendly output
- Assembly-to-Machine Code compiler (`assembler.c`)
- Machine Code simulator (`simulator.c`)
- Bash-based automated test suite with pass/fail reporting
- Makefile for simple builds
- Bash script for testing and running : 
./runtests.sh
This will:
Assemble and run each test from tests/
Compare output to expected .correct file
Print pass/fail summary

---


## Setup & Build

You’ll need a basic C compiler (`gcc`) and Bash shell.

To compile:

```bash
make clean
make assembler
make simulator
