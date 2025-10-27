<p align="center">
  <img src="icon.png" alt="WLED On/Off Logo" height="160">
</p>

# WLED On/Off Automation for Windows

WLED On/Off keeps your [WLED](https://kno.wled.ge/) strip in step with your Windows PC. A background service and helper watch for startup, sleep, and lock events while the settings app lets you configure and test the connection. 

## Project Structure

- `WLEDCommon/` - Shared library consumed by the service, and helper.
- `WLEDService/` - Service entry point + install/uninstall helpers.
- `WLEDHelper/` - User-mode message window that subscribes to session and display notifications.
- `WLEDSettings/` - Configuration UI app.
- `WLEDInstaller/` - Setup project that produces an MSI, and installs the service.

## Configuration

1. Launch **WLED On-Off** (elevated) and update the hostname/port fields.
2. Use the status info to check if your computer is connected to the WLED controller.
3. Adjust the color and brightness of the LED strip.
4. Press "Save" to save the configuration.

## Behaviour Overview

| Event | Component | Action |
|-------|-----------|--------|
| System boots or resumes | Service | Waits up to 120 seconds for networking, then turns WLED on. |
| System is shutting down or suspending | Service | Sends the off command and holds execution briefly to ensure delivery. |
| User logs on or unlocks session | Helper | Waits for networking, then turns WLED on. |
| User locks or logs off session | Helper | Turns WLED off immediately. |
| Display powers off (sleep) | Helper | Turns WLED off. |
| Display powers on | Helper | Waits for networking, delays 1.5 seconds, then turns WLED on. |



## Building

### Prerequisites
- Windows 11 x64.
- A reachable WLED controller on your network.
- Visual Studio 2022 (or newer) with the **Desktop development with C++** workload.
- (Optional) Visual Studio Installer Projects extension to build `WLEDInstaller`.

### Instructions
1. Open `WLED-ON-OFF.sln` in Visual Studio 2022.
2. Select the desired configuration (for deployment use `Release | x64`).
3. Build the entire solution (`Build > Build Solution`).
4. Optional: build `WLEDInstaller` to produce an MSI at `WLEDInstaller\Release\WLEDInstaller.msi`.
5. Executables and support files land under `x64\<Configuration>\`.


## License

MIT
