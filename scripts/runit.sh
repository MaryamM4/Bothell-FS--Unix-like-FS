#!/bin/bash

rm -f a.out

gcc -Wall -Wextra -Wno-sign-compare ../src/*.c

./a.out
