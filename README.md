An interpreter for the Lox language in C

# Introduction
This project is an exploration for me to learn more about how interpreters,
compilers, and virtual machines work. It may or may not conform to the exact
specifications of the Lox language as I commonly read details from the source code
of other programming language compilers such as Python, and, based on inspiration,
may bend my Lox interpreter to do things in addition or in a different way from the
original description of the language. It's also a personal goal to solve the Advent
of Code challenges for a year (https://adventofcode.com) with my own interpreter.

# Getting Started
Clone the project and update the submodules

    git clone https://github.com/greezybacon/clox
    cd clox
    git submodule init && git submodule update

Then build it and run it

    cd src
    make
    ./lox
