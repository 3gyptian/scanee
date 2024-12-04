#!/bin/bash
echo "Timestamp           RSSI    Noise    Channel"
while true; do
    # Extract Wi-Fi information in one go
    INFO=$(sudo wdutil info)

    # Extract RSSI, Noise, and Channel with adjusted parsing
    RSSI=$(echo "$INFO" | grep -E "^ *RSSI *:" | awk -F: '{print $2}' | awk '{print $1}')
    NOISE=$(echo "$INFO" | grep -E "^ *Noise *:" | awk -F: '{print $2}' | awk '{print $1}')
    CHANNEL=$(echo "$INFO" | grep -E "^ *Channel *:" | awk -F: '{print $2}' | awk '{print $1}')

    # Ensure values are valid (no empty or invalid fields)
    if [[ "$RSSI" =~ ^-?[0-9]+$ && "$NOISE" =~ ^-?[0-9]+$ && -n "$CHANNEL" ]]; then
        # Get current timestamp
        TIMESTAMP=$(date "+%Y-%m-%d %H:%M:%S")

        # Print the values side by side with the timestamp
        printf "%-20s %-8s %-8s %-8s\n" "$TIMESTAMP" "$RSSI" "$NOISE" "$CHANNEL"
    else
        # Skip this iteration if values are invalid
        echo "Invalid data, skipping..."
    fi

    # Wait 5 seconds before the next check
    sleep 5
done

