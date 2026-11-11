#!/bin/sh
cd "$(dirname "$0")" || exit 1
clock.elf > "$(dirname "$0")/clock.log" 2>&1
