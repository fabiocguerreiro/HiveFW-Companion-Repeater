## Overview

HiveFW is first and foremost a **MeshCore Companion Radio**.

The Companion functionality remains the core of the firmware at all times. The **Repeater mode is an additional capability**, which can be enabled directly through the MeshCore application when required.

When Repeater mode is activated from the app, HiveFW enables the additional Repeater-specific functionality, including the Repeater advertisement behaviour, Smart Advert scheduling and Node Discovery features.

In other words:

> **Companion first. Repeater when enabled.**

This approach allows the same device to remain a fully functional Companion Radio while optionally becoming a Repeater when the user enables the feature.

The goal is to provide a practical platform for **remote MeshCore interaction, telemetry, automation and future on-demand bot applications**, while keeping the Companion Radio experience at the centre of the firmware.

---

## Key Features

### Companion First

The defining characteristic of HiveFW is that it remains a **Companion Radio first and always**.

* Full MeshCore Companion Radio functionality remains the foundation of the firmware.
* Repeater functionality is optional.
* The Repeater mode is enabled or disabled through the MeshCore application.
* When Repeater mode is disabled, the device operates as a normal Companion Radio.
* When Repeater mode is enabled, the additional Repeater functionality is activated automatically.
* No separate firmware is required to switch between normal Companion operation and Companion + Repeater operation.

This makes HiveFW a **single firmware solution** that can adapt to the role required by the user.

### Companion Development

The Companion side of HiveFW is not considered a finished feature set.

New functionality and improvements for **Companion mode are planned for future releases**, with the intention of expanding the capabilities of the device while maintaining compatibility with the existing MeshCore Companion workflow.

---

### Companion + Repeater

When Repeater mode is activated through the MeshCore application, HiveFW enables the additional functionality required for Repeater operation.

This currently includes:

* Integrated LoRa Repeater functionality.
* Repeater-aware advertisements.
* Smart Advert scheduling.
* 23-hour advertisement cycle.
* Deterministic advertisement scheduling.
* Small timing jitter to reduce simultaneous advertisements from multiple nodes.
* Node Discovery functionality for Repeater operation.

The device therefore does not need to be permanently configured as a Repeater. The user can continue using it as a normal Companion Radio and enable the additional Repeater functionality whenever required.

---

## Home Assistant Integration

One of the main goals of HiveFW is to provide a bridge between **MeshCore and Home Assistant**.

The Wi-Fi connection allows a permanently connected HiveFW node to communicate with Home Assistant through projects such as **meshcore-ha** and **meshcore-chat-ha**.

This creates a bridge between the LoRa mesh and home automation systems, allowing selected information and commands to be exchanged remotely.

---

## Remote Automation & MeshCore Bots

HiveFW is designed with **controlled, on-demand automation** in mind.

A connected automation platform such as Home Assistant can provide predefined responses to commands received through the MeshCore network.

Bots and automated services should operate primarily **on demand**, responding to explicit requests instead of continuously generating network traffic.

The objective is to provide useful automation without turning the shared LoRa network into a continuous stream of automated traffic.
