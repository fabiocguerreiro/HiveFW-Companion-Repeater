# HiveFW Companion Repeater

HiveFW is a custom firmware based on [MeshCore](https://github.com/meshcore-dev/MeshCore) for the **Heltec WiFi LoRa 32 V3**.

It combines the functionality of a MeshCore Companion Radio with an integrated Repeater mode, providing a practical bridge between the LoRa mesh, Wi-Fi, computers and home automation systems.

The main purpose of HiveFW is to provide a permanently connected Wi-Fi Companion that can also participate in the LoRa mesh as a Repeater, enabling remote interaction with MeshCore channels, automated services and selected Home Assistant information.

## What is HiveFW?

HiveFW extends the MeshCore Companion concept with a focus on **Wi-Fi connectivity, remote interaction and home automation**.

A single Heltec V3 can remain connected to the local network while simultaneously participating in the LoRa mesh, combining local network connectivity with long-range, decentralised communication.

This creates a flexible platform that can act as:

- A MeshCore Companion accessible through Wi-Fi.
- A LoRa Repeater when Repeater mode is enabled.
- A remote interface for MeshCore channels.
- A bridge between the LoRa mesh and Home Assistant.
- A source of remote telemetry and predefined information.
- An automation and bot platform for remote interaction.

## Key Features

### Companion + Repeater

- Operates as a standard MeshCore Companion.
- Can operate as a LoRa Repeater when enabled.
- Normal Companion behaviour is preserved when Repeater mode is disabled.

### Wi-Fi Connectivity

The Companion Radio can connect directly to the local network, allowing external applications and services to communicate with the radio without requiring a permanent USB connection.

This provides the foundation for integration with computers, automation platforms and other network services.

### Smart Advert

- Custom Smart Advert implementation for Repeater mode.
- 23-hour advertisement cycle.
- Deterministic timing with a small jitter to reduce simultaneous advertisements.

### Node Discovery

- Basic Node Discovery support for Repeaters.
- Discovery functionality is limited to the Repeater implementation.

## Home Assistant & Remote Automation

One of the main objectives of HiveFW is to create a bridge between **MeshCore and Home Assistant**.

A permanently connected Heltec V3 can provide a communication interface between the LoRa mesh and a local Home Assistant installation.

This architecture is intended to allow remote MeshCore users to interact with selected Home Assistant information and services through the LoRa network.

Possible applications include:

- Viewing selected Home Assistant sensor values remotely.
- Accessing environmental and temperature information.
- Checking device and system status.
- Reading energy and presence information.
- Receiving predefined telemetry.
- Triggering predefined Home Assistant automations.
- Sending messages to MeshCore channels from Home Assistant.
- Monitoring MeshCore channels from Home Assistant.
- Responding automatically to commands and pings.

This opens the possibility of using the LoRa mesh as a **lightweight remote information and control interface**, allowing selected home automation data to be accessed beyond the normal range of a Wi-Fi network.

## Remote Bot

HiveFW is designed with future automated interaction in mind.

A connected Home Assistant installation can provide predefined responses to commands received through the MeshCore network.

For example, a remote node could request a predefined value, send a command or ping the HiveFW node and receive an automated response.

This makes the system suitable for building lightweight **MeshCore bots and remote automation services** without requiring a conventional internet connection between the participating LoRa nodes.

## Radio Configuration

HiveFW currently includes the Portuguese MeshCore frequency presets:

| Frequency | Bandwidth | SF | CR |
|---|---:|---:|---:|
| 433.375 MHz | 62.5 kHz | 9 | 6 |
| 869.618 MHz | 62.5 kHz | 7 | 6 |

These presets are intended for use in **Portugal**.

The frequency definitions can be adapted for other countries, regions or local regulatory requirements.

## Hardware

Current target hardware:

**Heltec WiFi LoRa 32 V3**

The firmware is based on the MeshCore Companion Radio architecture and is built using PlatformIO.

## MeshCore

HiveFW is based on [MeshCore](https://github.com/meshcore-dev/MeshCore), an open-source LoRa mesh networking system designed for long-range, decentralised communication.

MeshCore provides multi-hop packet routing, Companion Radio support, Repeater functionality and telemetry capabilities across low-power LoRa networks.

For more information, visit the [MeshCore documentation](https://docs.meshcore.io/).

## Version

**HiveFW v1.17.1-hivefw**

## License

HiveFW is based on MeshCore and retains the licensing terms of the original project.

See the [MeshCore repository](https://github.com/meshcore-dev/MeshCore) for the original project and licensing information.
