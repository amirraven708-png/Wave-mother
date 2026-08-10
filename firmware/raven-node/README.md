# Raven Node Firmware v1.0

ESP32-based LoRa + BLE mesh node for the Wave Mother ecosystem.

## Features

- BLE Bridge for mobile connectivity
- LoRa Mesh for long-range communication
- BSW (Blue Silence Weight) for signal obfuscation
- FHSS (Frequency Hopping Spread Spectrum)
- Venderselender Engine hook for future AI
- Message Queue for traffic management

## Quick Start

1. Install PlatformIO
2. Copy `RavenNode.ino` to `firmware/raven-node/`
3. Connect ESP32 via USB
4. Run: `pio run -t upload`

## Pin Configuration

| Component | ESP32 Pin |
|-----------|-----------|
| LoRa NSS  | GPIO5     |
| LoRa RST  | GPIO14    |
| LoRa DIO0 | GPIO2     |
| BLE       | Internal  |

## Integration with Wave Mother

This firmware is part of the Wave Mother Hardware Abstraction Layer.
It communicates with the Wave Mother Orchestrator via BLE or LoRa.
