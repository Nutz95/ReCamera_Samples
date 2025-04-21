# stop node-red
/etc/init.d/S03node-red stop

# kill previous instances of custom app:
echo "Searching for sscma-node processes..."

# Get PIDs of sscma-node processes (excluding the grep process itself)
PIDS=$(ps aux | grep "sscma-node" | grep -v "grep" | awk '{print $1}')

if [ -z "$PIDS" ]; then
    echo "No sscma-node processes found."
else
    echo "Found sscma-node processes with PIDs: $PIDS"
    
    # Try to terminate gracefully first
    for PID in $PIDS; do
        echo "Terminating process $PID..."
        kill $PID
    done
    # Wait briefly then check if they're still running
    sleep 2
    
    # Check if any processes are still running
    REMAINING=$(ps aux | grep "sscma-node" | grep -v "grep" | awk '{print $1}')
    
    if [ ! -z "$REMAINING" ]; then
        echo "Some processes still running. Forcing termination..."
        for PID in $REMAINING; do
            echo "Force killing process $PID..."
            kill -9 $PID
        done
    fi
    
    echo "All sscma-node processes terminated."
fi

# Now you can start your custom application
echo "Starting custom node application..."
# Add your startup command here

#start application: 
/usr/local/bin/sscma-node -c /userdata/flow.json --start