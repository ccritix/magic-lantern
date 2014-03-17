#!/bin/bash
# Run a ML script on the desktop TCC compiler.

tcc -Iinclude -Ilib lib/desktop.c $1 -run
