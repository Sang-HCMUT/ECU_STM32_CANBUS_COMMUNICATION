# Automotive ECU System (STM32) & Dashboard

This repository contains the complete source code for an automotive ECU simulation system using STM32 microcontrollers, communicating via CAN bus, along with a Dashboard interface for monitoring and control.

## Directory Structure

- `ABS/`: Firmware source code for the Anti-lock Braking System.
- `ADAS/`: Firmware source code for Advanced Driver Assistance Systems.
- `EPS/`: Firmware source code for Electronic Power Steering.
- `dashboard/`: Dashboard application used to display parameters and control the system.
- `common/`: Shared libraries and common code for the ECUs.
- `.md` files: Contains comprehensive report documents and system design details (in Vietnamese).

## How to Use

### 1. Flashing Firmware to ECUs (STM32)
- **Prerequisites**: STM32CubeIDE (or Keil C/VS Code depending on project configuration) and an ST-Link/J-Link programmer.
- **Steps**:
  1. Open STM32CubeIDE.
  2. Select `File > Open Projects from File System...` and point to the respective ECU directory (e.g., `ABS`, `ADAS`, or `EPS`).
  3. Build the project (click the hammer icon or `Project > Build All`).
  4. Connect the STM32 board to your computer via the ST-Link programmer.
  5. Click the Run/Debug button to flash the program onto the microcontroller.
  6. Repeat this process for the remaining ECU nodes.

### 2. Running the Dashboard
1. Open a terminal (Command Prompt / PowerShell) and navigate to the `dashboard/` directory.
2. Depending on the technology used for the Dashboard:
   - If it's a **Node.js/React/Vue** project: run `npm install` to install dependencies, then run `npm start` or `npm run dev`.
   - If it's a **Python** project: set up the environment and run `pip install -r requirements.txt`, then execute the main file (e.g., `python main.py`).
3. Ensure the hardware signal converter (CAN to USB or UART to USB) is connected to your computer and the correct COM port is selected on the interface so the Dashboard can send and receive data from the CAN network.

## References
Read the Markdown documents in the root directory (especially `BAO_CAO_TONG_HOP_HOAN_CHINH.md` and `DATN_TAI_LIEU_TONG_HOP.md`) to better understand:
- System block diagrams
- Hardware/Software architecture
- ID mapping and CAN message data flow (CAN Matrix).
