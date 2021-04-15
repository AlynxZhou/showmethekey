Show Me The Key
===============

A screenkey alternative that works under Wayland via libinput.
--------------------------------------------------------------

A SUSE Hack Week 20 Project: [Show Me The Key: A screenkey alternative that works under Wayland via libinput](https://hackweek.suse.com/20/projects/a-screenkey-alternative-that-works-under-wayland-via-reading-evdev-directly).

# Install

Clone this repo and run `mkdir build && cd build && meson setup --prefix=/usr . .. && meson compile && meson install` to install, then you can run `showmethekey-gtk` from terminal or click `Show Me The Key` in launcher.

Arch Linux users can also install it from [AUR](https://aur.archlinux.org/packages/showmethekey/).

# Usage

For detailed usage please run usage dialog from app menu!

You need to toggle the switch to start it manually and need to input your password to `pkexec`'s dialog, because we need superuser permission to read keyboard events (this program does not handle your password so it is safe). Wayland does not allow a client to set its position, so this program does not set its position in preference, and you can click the "Clickable Area" in titlebar and drag the floating window to anywhere you want.

## Special Notice for GNOME Wayland Session Users

There is no official Wayland protocol allowing toplevel clients to set their own position and layer, only users can change those things. Also Mutter has no way to let a program to set itself always on top. But don't worry, users are always allowed to do those things by themselves, so after turning on the switch, please right click the "Clickable Area" in titlebar of the floating window and check "Always on Top" and "Always on Visible Workspace", and Mutter will do as what you set!

For other Wayland DEs (personally I only care about KDE Plasma), I will do more test and update here.

# Feature

[screenkey](https://gitlab.com/screenkey/screenkey) is a popular project for streamers or tutorial recorders because it can make your typing visual on screen, but it only works under X11, not Wayland because it uses X11 functions to get keyboard event.

This program, instead, reads key events via libinput directly, and then put it on screen, so it will not depend on X11 or special Wayland Compositors and will work across them.

# Project Structure

## CLI

This part exists because of Wayland's security policy, which means you cannot run a GUI program with `sudo` (see <https://wiki.archlinux.org/index.php/Running_GUI_applications_as_root#Wayland>). It's suggested to split your program into a GUI frontend and a CLI backend that do privileged operations, and this is the backend, a custom re-write of <https://gitlab.freedesktop.org/libinput/libinput/-/blob/master/tools/libinput-debug-events.c>, based on [libinput](https://www.freedesktop.org/wiki/Software/libinput/), [libudev](https://www.freedesktop.org/software/systemd/man/libudev.html) and [libevdev](https://www.freedesktop.org/wiki/Software/libevdev/).

It generates JSON in lines like `{"event_name": "KEYBOARD_KEY", "event_type": 300, "device_name": "USB Keyboard USB Keyboard", "time_stamp": 39869802, "key_name": "KEY_C", "key_code": 46, "state_name": "PRESSED", "state_code": 1}`.

## GTK

A GUI frontend based on GTK, will run CLI backend as root via `pkexec`, and show a transparent floating window to display events.

# Translate

If you changed translatable strings, don't forget to run `meson compile showmethekey-update-po` in build directory and then edit po files.

# Name

As I want some clear name that hints its usage, but `screenkey` is already taken and I think `visualkey` sounds like `Visual Studio` and it's horrible. My friend [@LGiki](https://github.com/LGiki) suggests `Show Me The Key` which sounds like "Show me the code" from Linus Torvalds. At first I think it's a little bit long, but now it is acceptable so it's called `showmethekey` or `Show Me The Key`.

The Chinese translate of this program name should be `让我看键`, and it's only used for window title, debug output, package name and desktop entry name should not be translated.

# Icon

Program icon made by <a href="https://www.freepik.com" title="Freepik">Freepik</a> from <a href="https://www.flaticon.com/" title="Flaticon">www.flaticon.com</a>.
