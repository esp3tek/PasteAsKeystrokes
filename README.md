# PasteAsKeystrokes

![PasteAsKeystrokes window](window.png)

Tiny Windows utility (~615 lines of C, no dependencies, no installer, single
~36 KB `.exe`) that types the current clipboard contents as real keyboard
input into the focused window.

Useful where `Ctrl+V` is blocked or unavailable: remote consoles (SSH, KVM,
vSphere, RDP/Citrix), BIOS/UEFI, virtual machine consoles, password fields,
and similar places.

## Download

Grab the pre-built executable from the [latest GitHub release](https://github.com/esp3tek/PasteAsKeystrokes/releases/latest):

**[PasteAsKeystrokes.exe](https://github.com/esp3tek/PasteAsKeystrokes/releases/latest/download/PasteAsKeystrokes.exe)** (~36 KB, no installer).

Just double-click to run.

### Verify the binary (optional)

Current build (v1.1.0) SHA-256:

```
6E2FB24814EBB33569678EA0C6A61D494C56B3581BBE6FDC4F677CB7552755DC
```

On Windows: `Get-FileHash PasteAsKeystrokes.exe -Algorithm SHA256`

### Windows SmartScreen warning

The first time you run the downloaded `.exe`, Windows SmartScreen may
show **"Windows protected your PC"** because this binary is not signed
with a paid code-signing certificate. The file is safe (you can verify
the SHA-256 above and read the entire source in this repo).

To run it: click **More info** → **Run anyway**.

## Usage

1. Run `PasteAsKeystrokes.exe`. The window shows the current input mode
   (default: **Unicode**) and a clickable button to switch it.
2. Copy text to the clipboard.
3. Switch focus to the target window.
4. Press **Ctrl + Alt + V**. The text is typed character by character.

Minimize the window to send it to the system tray. Right-click the tray
icon for `Show` / `Exit`. The clipboard is never modified.

## Input modes

The app offers **two injection modes** because no single mechanism works
for every target — this is the same approach professional tools like
KeePass and 1Password take. Click the button below the keycaps to switch.

### Unicode mode (default)

Injects characters via `KEYEVENTF_UNICODE`. The target receives the
*literal Unicode codepoint*, independent of any keyboard layout.

Best for:
- Local Windows applications (Notepad, browsers, code editors, password fields).
- Remote desktop tools that forward characters (AnyDesk, RDP when configured
  with "Apply Windows key combinations on remote", TeamViewer).
- Anywhere the target speaks plain Unicode text.

Fails or gets dropped in: KVMs and IPMI consoles that only forward raw
keyboard scancodes (most browser-based HTML5 KVMs).

### Scancode US mode

Injects hardcoded US PS/2 scancodes via `KEYEVENTF_SCANCODE`, exactly like a
physical US keyboard would. Before typing, attaches the input queue to the
foreground thread and activates the US layout to align local apps too.

Best for:
- Browser-based KVM / IPMI / iDRAC / iLO HTML5 consoles, where the target
  Linux/Unix server uses the **US layout** (the default for AlmaLinux,
  Rocky, RHEL, Debian, Ubuntu, FreeBSD, etc.).
- BIOS / UEFI screens.
- Virtual machine console viewers (Proxmox, OpenNebula, vSphere) when the
  guest is on US layout.

Fails when: the target system is on a non-US layout. In that case the
emitted US scancodes get reinterpreted by the target's layout and produce
the wrong characters.

### Newlines, tabs, Unicode beyond ASCII

In both modes:
- `\r\n`, `\r`, `\n` are sent as the `Enter` key.
- `\t` is sent as Unicode `U+0009`.
- Non-ASCII characters (accents, CJK, emoji, surrogate pairs) always use
  `KEYEVENTF_UNICODE` — there is no ASCII fallback for those.
- BOM (`U+FEFF`) at the start of the clipboard is stripped silently.

## Build from source

Requires either **MSVC** (`cl.exe`) or **MinGW-w64** (`gcc`) on `PATH`.

```
build.bat
```

Manual MinGW build:

```
windres resources.rc -O coff -o resources.o
gcc paste_as_keystrokes.c resources.o -o PasteAsKeystrokes.exe -mwindows -luser32 -lshell32 -lgdi32 -s -O2
```

## The cross-layout reality

There is no client-side trick that makes keyboard injection work everywhere,
because the receiver — KVM viewer, target OS, USB HID translator — applies
its *own* layout to whatever scancodes you send. KeePass, AutoHotkey and
similar tools all document this limitation and tell users to match layouts
manually. PasteAsKeystrokes follows the same best practice: offer two
honest modes and let you pick.

Universal rule for remote consoles:

> KVM viewer layout = local Windows layout = target OS layout

If all three agree, everything works regardless of which injection mode
you pick.

## Other notes

- Single-instance: launching twice brings the existing window to the front.
- Typing speed: ~5 ms per character.
- Modifier release: the app polls until you physically release `Ctrl`,
  `Alt`, `Shift`, `Win` (max 500 ms) before injecting, so holding the hotkey
  a moment longer doesn't corrupt the first character.
- No logging, no disk writes, no network. The clipboard text only lives in
  process memory until the keystrokes are sent.

## License

MIT — see [LICENSE](LICENSE). Copyright (c) 2026 esp3tek.
