#!/bin/bash
set -e

PROJECT_DIR="/Users/ushorer/code/WifiLedWatch"
USE_OTA=false
DO_LIST=false
WATCH_NAME=""

# Parse arguments
for arg in "$@"; do
    case "$arg" in
        --ota)  USE_OTA=true ;;
        --list) DO_LIST=true ;;
        --help|-h)
            echo "Usage: $0 [--list] [--ota <name>]"
            echo "  (no args)       Build + flash over USB (auto-detect port)"
            echo "  --list          Discover NeoTick clocks on the network and exit"
            echo "  --ota <name>    Build + flash over the air to neotick-<name>.local"
            exit 0 ;;
        --*)    echo "Unknown option: $arg (try --help)"; exit 1 ;;
        *)      WATCH_NAME="$arg" ;;
    esac
done

# Resolve a "<host>.local" mDNS name to an IPv4 address (empty if it doesn't resolve).
resolve_ip() {
    local host="$1" TMP PID IP
    TMP=$(mktemp)
    dns-sd -G v4 "${host}.local" 2>/dev/null > "$TMP" &
    PID=$!
    sleep 2
    kill "$PID" 2>/dev/null || true
    wait "$PID" 2>/dev/null || true
    IP=$(awk '$2=="Add"{for(i=1;i<=NF;i++) if($i ~ /^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$/){print $i; exit}}' "$TMP")
    rm -f "$TMP"
    printf '%s' "$IP"
}

# Discover NeoTick clocks via mDNS (they advertise _neotick._tcp), then resolve each.
list_clocks() {
    echo "Scanning for NeoTick clocks on the network (3s)..."
    local TMP; TMP=$(mktemp)
    dns-sd -B _neotick._tcp local 2>/dev/null > "$TMP" &
    local PID=$!
    sleep 3
    kill "$PID" 2>/dev/null || true
    wait "$PID" 2>/dev/null || true
    local NAMES; NAMES=$(awk '$2=="Add"{print $NF}' "$TMP" | sort -u)
    rm -f "$TMP"
    if [ -z "$NAMES" ]; then
        echo "  No NeoTick clocks found (are they on this WiFi, and is mDNS allowed?)."
        return 0
    fi
    echo ""
    echo "Resolving addresses..."
    echo ""
    local live=0 stale=0
    while IFS= read -r n; do
        [ -z "$n" ] && continue
        local ip; ip=$(resolve_ip "$n")
        if [ -n "$ip" ]; then
            printf '  [LIVE]   %-18s http://%-24s ( http://%s/ )    flash: %s --ota %s\n' \
                   "$n" "${n}.local" "$ip" "$0" "$n"
            live=$((live + 1))
        else
            printf '  [stale]  %-18s not resolving — cached/offline entry\n' "$n"
            stale=$((stale + 1))
        fi
    done <<< "$NAMES"
    echo ""
    echo "Live: $live    Stale/cached: $stale"
    if [ "$stale" -gt 0 ]; then
        echo ""
        echo "To clear stale/phantom entries from this Mac's mDNS cache:"
        echo "  sudo dscacheutil -flushcache; sudo killall -HUP mDNSResponder"
    fi
    echo ""
}

cd "$PROJECT_DIR"

# --list: just discover and exit (no build/upload)
if [ "$DO_LIST" = true ]; then
    list_clocks
    exit 0
fi

# Resolve OTA target host from the supplied name (accepts "gym1" or "neotick-gym1")
if [ "$USE_OTA" = true ]; then
    if [ -z "$WATCH_NAME" ]; then
        echo "OTA requires a watch name:  $0 --ota <name>"
        echo "Run '$0 --list' to see clocks on the network."
        exit 1
    fi
    case "$WATCH_NAME" in
        neotick|neotick-*) OTA_HOST="${WATCH_NAME}.local" ;;
        *)                 OTA_HOST="neotick-${WATCH_NAME}.local" ;;
    esac
fi

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
