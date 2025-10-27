# WLED On/Off Automation for Windows

This repository contains a small set of Windows-native utilities that keep a [WLED](https://kno.wled.ge/) controller in sync with the power state of a PC. It includes:

- **WLEDService** – a Windows service that turns the light strip on when the machine boots or resumes and off when it shuts down or sleeps.
- **WLEDHelper** – a background helper that reacts to user session lock/unlock and display power events to toggle the lights accordingly.
- **WLEDCommon** – a shared static library that wraps the HTTP calls to the WLED JSON API, basic logging, and network readiness checks.

Together they ensure the WLED device mirrors the system’s activity so you never leave your LEDs on unintentionally.

## Project Structure

- `WLED-ON-OFF.sln` – Visual Studio 2022 solution tying the three projects together.
- `WLEDCommon/` – Shared helpers (`WLEDCommon.cpp/.h`).
- `WLEDService/` – Windows service entry point and control handler.
- `WLEDHelper/` – Per-user message-only window that subscribes to display and session notifications.

## Features

- Uses the WLED JSON API (`/json/state`) to switch the controller on (`{"on":true}`) or off (`{"on":false}`).
- Waits for a working network connection before sending commands, reducing failures on resume.
- Logs all activity to `C:\ProgramData\WLEDService.log`.
- Handles user session changes (lock, unlock, logon, logoff) as well as power broadcast notifications.
- Keeps the system awake briefly on shutdown to make sure the `off` command is delivered.

## Prerequisites

- Windows 10/11 with the Desktop Experience.
- A reachable WLED controller configured on your network.
- Visual Studio 2022 (or newer) with the **Desktop development with C++** workload.
- Windows 10/11 SDK (installed with Visual Studio).

## Configuration

The WLED endpoint and log file location are defined in `WLEDCommon/WLEDCommon.cpp`:

```cpp
const char* LOG_PATH  = "C:\\ProgramData\\WLEDService.log";
```

1. Use the **WLEDSettings** app to update the hostname (`HKLM\SOFTWARE\WLEDController\hostname`) and port (`…\port`). The helper and service read these values at runtime (falling back to `192.168.1.191:80` if unavailable).
2. Adjust `LOG_PATH` if you prefer a different location; the process must have write access.
3. Rebuild both the **WLEDCommon** library and the two executables so they pick up the new settings.

Refer to the [WLED API reference](https://kno.wled.ge/interfaces/json-api/) for additional payload options if you want to extend the behaviour beyond simple on/off commands.

## Building

1. Open `WLED-ON-OFF.sln` in Visual Studio 2022.
2. Select the desired configuration (e.g. `Release | x64`).
3. Build the entire solution (`Build > Build Solution`).
4. The compiled binaries land under `x64\<Configuration>\`.

> Tip: Because the projects link against Win32 system libraries (`wininet`, `powrprof`, `wtsapi32`, `iphlpapi`), make sure the Windows SDK is properly installed, otherwise linking will fail.

## Deployment

### Install the Windows Service

1. Copy the compiled `WLEDService.exe` to the target machine.
2. Open an elevated PowerShell prompt.
3. Register the service:

   ```powershell
   sc.exe create WLEDService binPath= "C:\Path\To\WLEDService.exe" start= auto
   ```

4. Start it with `sc.exe start WLEDService`.

To update the binary later, stop the service (`sc.exe stop WLEDService`), replace the executable, and start it again. To remove it completely run `sc.exe delete WLEDService`.

### Run the User Helper

`WLEDHelper.exe` must run in the user context to receive session and display notifications. The easiest options are:

1. Add a Task Scheduler entry that launches `WLEDHelper.exe` at logon with “Run only when user is logged on”.
2. Place a shortcut in the `shell:startup` folder so it starts with the current user session.

The helper creates an invisible, message-only window and keeps running silently in the background.

## Behaviour Overview

| Event | Component | Action |
|-------|-----------|--------|
| System boots or resumes | Service | Waits up to 120 s for networking, then turns WLED on. |
| System is shutting down or suspending | Service | Sends the off command, keeps the system awake briefly to ensure delivery. |
| User logs on or unlocks session | Helper | Waits for networking, turns WLED on. |
| User locks or logs off session | Helper | Turns WLED off immediately. |
| Display powers off (sleep) | Helper | Turns WLED off. |
| Display powers on | Helper | Waits for networking, delays 1.5 s, then turns WLED on. |

All events are timestamped in the shared log file.

## Troubleshooting

- **Commands fail on resume:** Increase the `WaitForNetworkReady` timeout in `WLEDCommon.cpp`, or confirm the PC obtains network connectivity quickly enough.
- **Service fails to install:** Run the PowerShell prompt elevated and double-check the `binPath` you provide to `sc.exe`.
- **Light stays on after shutdown:** Verify the service is installed and running (`sc.exe query WLEDService`); the helper alone cannot catch shutdown events.

## License

MIT
