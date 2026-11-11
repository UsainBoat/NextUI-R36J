#!/bin/sh
# governor.sh - CPU governor controller for R36J (Allwinner A33 / sun8iw5p1)
# Usage: governor.sh <mode>
# Modes: auto, performance, powersave

MODE="$1"
[ -z "$MODE" ] && MODE="auto"

CPUFREQ="/sys/devices/system/cpu/cpu0/cpufreq"
[ -d "$CPUFREQ" ] || exit 0

case "$MODE" in
	auto)
		echo "interactive" > "$CPUFREQ/scaling_governor" 2>/dev/null || true
		echo "120000" > "$CPUFREQ/scaling_min_freq" 2>/dev/null || true
		echo "1200000" > "$CPUFREQ/scaling_max_freq" 2>/dev/null || true
		;;
	performance)
		echo "performance" > "$CPUFREQ/scaling_governor" 2>/dev/null || true
		echo "120000" > "$CPUFREQ/scaling_min_freq" 2>/dev/null || true
		echo "1344000" > "$CPUFREQ/scaling_max_freq" 2>/dev/null || true
		;;
	powersave)
		echo "conservative" > "$CPUFREQ/scaling_governor" 2>/dev/null || true
		echo "120000" > "$CPUFREQ/scaling_min_freq" 2>/dev/null || true
		echo "1008000" > "$CPUFREQ/scaling_max_freq" 2>/dev/null || true
		;;
	*)
		exit 0
		;;
esac
