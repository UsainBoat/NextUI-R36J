#!/bin/sh
cd "$(dirname "$0")" || exit 1
battery.elf > "$(dirname "$0")/battery.log" 2>&1
