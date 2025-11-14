# MCMGR Heartbeat Test

## Overview

This test project implements a heartbeat mechanism between primary and secondary cores using the MCMGR (Multicore Manager) library. The secondary core periodically sends heartbeat signals to the primary core to indicate that it is alive and functioning properly.

## Test Description

### Primary Core
- Initializes MCMGR and starts the secondary core
- Registers event handlers for heartbeat and exception events
- Monitors heartbeat signals from the secondary core
- Implements timeout detection to identify unresponsive secondary core

### Secondary Core
- Initializes MCMGR on the secondary core
- Sends periodic heartbeat signals to the primary core
- Simulates work being performed while maintaining heartbeat
- Runs for a specified duration then terminates gracefully

## Key Features

1. **Heartbeat Mechanism**: Secondary core sends heartbeat every 1 second
2. **Timeout Detection**: Primary core detects if heartbeat stops for more than 5 seconds
3. **Exception Handling**: Primary core monitors for secondary core exceptions
4. **Statistics**: Both cores provide statistics on heartbeats sent/received

## Configuration

### Timing Parameters
- `TEST_HEARTBEAT_INTERVAL_MS`: 1000ms (heartbeat frequency)
- `TEST_HEARTBEAT_TIMEOUT_MS`: 5000ms (timeout threshold)
- `TEST_TEST_DURATION_MS`: 30000ms (total test duration)

### Event Data
- `TEST_HEARTBEAT_EVENT_DATA`: 0xBEAE (heartbeat identifier)
