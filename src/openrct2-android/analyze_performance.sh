#!/bin/bash

# OpenRCT2 Android Performance Analysis Script
# Usage: ./analyze_performance.sh [iterations]

PACKAGE="io.openrct2"
ACTIVITY="io.openrct2/.MainActivity"
ITERATIONS=${1:-3}

echo "=== OpenRCT2 Android Performance Analysis ==="
echo "Running $ITERATIONS cold start tests..."

# Ensure device is ready
echo "Preparing device..."
adb shell input keyevent KEYCODE_WAKEUP
adb shell settings put global window_animation_scale 0
adb shell settings put global transition_animation_scale 0
adb shell settings put global animator_duration_scale 0

total_time=0
declare -a startup_times

for i in $(seq 1 $ITERATIONS); do
    echo ""
    echo "=== Test $i/$ITERATIONS ==="

    # Clear app data for cold start
    echo "Clearing app data..."
    adb shell pm clear $PACKAGE >/dev/null 2>&1

    # Clear logcat
    adb logcat -c

    # Start app and measure
    echo "Starting app..."
    result=$(adb shell am start -W -S -n $ACTIVITY 2>&1)

    # Extract timing
    startup_time=$(echo "$result" | grep "TotalTime" | awk '{print $2}')
    if [ -n "$startup_time" ]; then
        startup_times[$i]=$startup_time
        total_time=$((total_time + startup_time))
        echo "Cold start time: ${startup_time}ms"
    else
        echo "Failed to measure startup time"
        startup_times[$i]=0
    fi

    # Wait for complete startup
    sleep 3

    # Collect profiling data
    echo "Collecting profiling data..."
    adb logcat -d -s StartupProfiler:* AndroidAssetManager:* | grep PERF > "/tmp/openrct2_perf_test${i}.log"

    if [ -s "/tmp/openrct2_perf_test${i}.log" ]; then
        echo "Performance log saved to /tmp/openrct2_perf_test${i}.log"
        asset_init_time=$(grep "Asset manager initialization" "/tmp/openrct2_perf_test${i}.log" | grep -o '[0-9]*ms' | head -1)
        validation_time=$(grep "Critical asset validation" "/tmp/openrct2_perf_test${i}.log" | grep -o '[0-9]*ms' | head -1)

        echo "  - Asset manager init: $asset_init_time"
        echo "  - Asset validation: $validation_time"
    fi

    # Force stop app
    adb shell am force-stop $PACKAGE
    sleep 2
done

# Calculate average
if [ $total_time -gt 0 ]; then
    average=$((total_time / ITERATIONS))
    echo ""
    echo "=== PERFORMANCE SUMMARY ==="
    echo "Cold start times: ${startup_times[@]} ms"
    echo "Average cold start: ${average}ms"
    echo ""

    # Analyze bottlenecks
    echo "=== BOTTLENECK ANALYSIS ==="
    if [ -f "/tmp/openrct2_perf_test1.log" ]; then
        echo "Most time-consuming operations:"
        cat /tmp/openrct2_perf_test*.log | grep "took" | sort -k4 -nr | head -5
        echo ""
        echo "Large asset reads:"
        cat /tmp/openrct2_perf_test*.log | grep "Read large asset" | head -3
    fi

    # Recommendations
    echo ""
    echo "=== OPTIMIZATION RECOMMENDATIONS ==="
    if [ $average -gt 5000 ]; then
        echo "🔴 SLOW STARTUP (>5s) - Critical optimizations needed:"
        echo "  - Consider asset compression/bundling"
        echo "  - Implement lazy loading"
        echo "  - Profile native initialization"
    elif [ $average -gt 3000 ]; then
        echo "🟡 MODERATE STARTUP (3-5s) - Room for improvement:"
        echo "  - Optimize asset validation"
        echo "  - Consider parallel loading"
    else
        echo "🟢 GOOD STARTUP (<3s) - Minor optimizations possible"
    fi
fi

echo ""
echo "Detailed logs available in /tmp/openrct2_perf_test*.log"
