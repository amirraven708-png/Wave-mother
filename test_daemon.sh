#!/bin/bash

# Kill old daemon
pkill -f wave_daemon 2>/dev/null || true
rm -f ./wave_daemon.sock

# Start daemon
echo "Starting Wave Daemon..."
./services/wave_daemon &
DAEMON_PID=$!
sleep 2

# Run multiple queries to trigger learning
echo ""
echo "Running 150 queries to trigger sleep/wake cycles..."
for i in {1..150}; do
    ./services/terminal we://ABCD1234 > /dev/null 2>&1
    if [ $((i % 30)) -eq 0 ]; then
        echo "  Query $i complete"
    fi
done

# Final query with full output
echo ""
echo "Final query result:"
./services/terminal we://ABCD1234

# Cleanup
kill $DAEMON_PID 2>/dev/null
echo ""
echo "✅ Daemon test complete"
