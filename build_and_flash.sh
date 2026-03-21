#!/bin/bash
set -e

PROJECT_DIR="/Users/ushorer/code/WifiLedWatch"
OTA_HOST="neotick.local"
USE_OTA=false

# Parse arguments
for arg in "$@"; do
    case "$arg" in
        --ota) USE_OTA=true ;;
    esac
done

cd "$PROJECT_DIR"

echo "========================================="
echo "  NeoTick LED Watch"
echo "========================================="
if [ "$USE_OTA" = true ]; then
    echo "Upload:  OTA ($OTA_HOST)"
else
    echo "Upload:  USB (auto-detect)"
fi
echo ""

echo "-----------------------------------------"
echo "  Step 1: Compiling..."
echo "-----------------------------------------"
pio run
echo ""
echo "Compilation complete."
echo ""

echo "-----------------------------------------"
echo "  Step 2: Uploading to ESP32..."
echo "-----------------------------------------"
if [ "$USE_OTA" = true ]; then
    pio run -t upload --upload-port "$OTA_HOST"
else
    pio run -t upload
fi
echo ""
echo "Upload complete."
echo ""

echo "-----------------------------------------"
echo "  Step 3: Monitor"
echo "-----------------------------------------"
if [ "$USE_OTA" = true ]; then
    echo "Waiting for ESP32 to reboot..."
    sleep 5
    echo "Monitoring logs from http://$OTA_HOST/logs"
    echo "Press Ctrl+C to exit."
    echo ""
    while true; do
        curl -s "http://$OTA_HOST/logs" 2>/dev/null || echo "(waiting for ESP32...)"
        echo "--- $(date +%H:%M:%S) ---"
        sleep 3
    done
else
    echo "Press Ctrl+C to exit."
    echo ""
    pio device monitor
fi
