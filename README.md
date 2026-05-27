# PasteAsKeystrokes

![PasteAsKeystrokes window](window.png)

Tiny Windows utility (~575 lines of C, no dependencies, no installer, single
~30 KB `.exe`) that types the current clipboard contents as real keyboard
input into the focused window.

Useful where `Ctrl+V` is blocked or unavailable: remote consoles (SSH, KVM,
vSphere, RDP/Citrix), BIOS/UEFI, virtual machine consoles, password fields,
and similar places.

## Download

Grab the pre-built executable directly from this repo:
**[PasteAsKeystrokes.exe](PasteAsKeystrokes.exe)** (~30 KB, no installer).

Just double-click to run.

## Usage

1. Run `PasteAsKeystrokes.exe`. A small window appears.
2. Copy text to the clipboard.
3. Switch focus to the target window (terminal, console, etc).
4. Press **Ctrl + Alt + V**. The text is typed character by character.

- `Enter` and `Tab` from the clipboard are sent as real `ENTER` / `TAB` keys.
- Printable ASCII is sent using hardcoded US scan codes
  (`KEYEVENTF_SCANCODE`), so what the target receives matches a physical US
  keyboard regardless of the local Windows layout.
- Non-ASCII characters (accents, CJK, emoji, surrogate pairs) fall back to
  Unicode injection (`KEYEVENTF_UNICODE`).
- Before typing, the app attaches its input queue to the foreground thread
  and activates the US layout, so apps that look at translated characters
  rather than scan codes also see correct US characters.
- The clipboard is **not modified**.
- Minimize the window to send it to the system tray. Right-click the tray
  icon for `Show` / `Exit`.

## Build from source

Requires either **MSVC** (`cl.exe`, from Visual Studio Build Tools) or
**MinGW-w64** (`gcc`) on `PATH`.

```
build.bat
```

Manual MinGW build:

```
gcc paste_as_keystrokes.c -o PasteAsKeystrokes.exe -mwindows -luser32 -lshell32 -lgdi32 -s -O2
```

Manual MSVC build:

```
cl /O2 paste_as_keystrokes.c /link /SUBSYSTEM:WINDOWS user32.lib shell32.lib gdi32.lib
```

## Notes on remote consoles (KVMs, RDP)

The app assumes the target system uses a **US keyboard layout** (the default
for most Linux servers, BMCs and BIOSes). For HTML5 / noVNC web consoles
(IONOS, Proxmox, OpenNebula, …), the visor often has its own keyboard
selector. Universal rule:

> Visor layout = local Windows layout = target OS layout

If all three line up, everything works. If any of them disagrees, characters
get mistranslated regardless of the typing tool.

## Other notes

- Single-instance: launching twice brings the existing window to the front.
- Typing speed: ~5 ms per character (fast enough to feel instant, slow enough
  that remote consoles don't drop input).
- Modifier release: the app polls until you physically release `Ctrl`,
  `Alt`, `Shift`, `Win` (max 500 ms) before injecting, so holding the hotkey
  a moment longer doesn't corrupt the first character.
- No logging, no disk writes, no network. The clipboard text only lives in
  process memory until the keystrokes are sent.

## License

MIT — see [LICENSE](LICENSE). Copyright (c) 2026 esp3tek.
