#!/bin/sh
cd "$(dirname "$0")" || exit 1
settings.elf > "$(dirname "$0")/settings.log" 2>&1
