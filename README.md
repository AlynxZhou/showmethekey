Show Me The Key
===============

Show keys you typed on screen.
------------------------------

[Project Website](https://showmethekey.alynx.one/)

A SUSE Hack Week 20 Project: [Show Me The Key: A screenkey alternative that works under Wayland via libinput](https://hackweek.suse.com/20/projects/a-screenkey-alternative-that-works-under-wayland-via-reading-evdev-directly).

# Install

## Distribution Package (Recommanded)

### AOSC OS

Just run following command to install from offcial repository:

```
# apt install showmethekey
```

### Arch Linux

#### Install From AUR

```
$ yay -S showmethekey
```

Or use other AUR helpers.

#### Install From `archlinuxcn`

First [add archlinuxcn repo to your system](ttps://www.archlinuxcn.org/archlinux-cn-repo-and-mirror/).

```
# pacman -S showmethekey
```

### openSUSE

#### Install from OBS

Packages can be found in [my OBS project](https://build.opensuse.org/package/show/home:AZhou/showmethekey).

```
# zypper ar https://download.opensuse.org/repositories/home:/AZhou/openSUSE_Tumbleweed/home:AZhou.repo
# zypper in showmethekey showmethekey-lang
```

Leap users please replace URL for Tumbleweed with URL for your Leap version.

### Other Distributions

Please help package showmethekey to your distribution!

## Build From Source

### Dependencies

- libevdev
- udev (or systemd)
- libinput
- glib2
- gtk3
- json-glib
- cairo
- pango
- libxkbcommon
- meson
- ninja
- gcc

### Build

```
$ git clone https://github.com/AlynxZhou/showmethekey.git
$ cd showmethekey
$ mkdir build && cd build && meson setup --prefix=/usr . .. && meson compile && meson install
$ showmethekey-gtk
```

# Usage

For detailed usage please run usage dialog from app menu!

You need to toggle the switch to start it manually and need to input your password to `pkexec`'s dialog, because we need superuser permission to read keyboard events (this program does not handle your password so it is safe). Wayland does not allow a client to set its position, so this program does not set its position in preference, and you can click the "Clickable Area" in titlebar and drag the floating window to anywhere you want.

## Special Notice for Wayland Session Users

There is no official Wayland protocol allowing toplevel clients to set their own position and layer, only users can change those things. But don't worry, users are always allowed to do those things by themselves if their compositors support it.

For example if you are using GNOME Shell (Wayland), you can right click the "Clickable Area" on title bar to show a window manager menu and check "Always on Top" and "Always on Visible Workspace" in it.

If you are using KDE Plasma (Wayland), you can right click "Floating Window - Show Me The Key" on task bar, check "Move to Desktop" -> "All Desktops" and "More Actions" -> "Keep Above Others".

# Feature

[screenkey](https://gitlab.com/screenkey/screenkey) is a popular project for streamers or tutorial recorders because it can make your typing visual on screen, but it only works under X11, not Wayland because it uses X11 functions to get keyboard event.

This program, instead, reads key events via libinput directly, and then put it on screen, so it will not depend on X11 or special Wayland Compositors and will work across them.

# Project Structure

## CLI

This part exists because of Wayland's security policy, which means you cannot run a GUI program with `sudo` (see <https://wiki.archlinux.org/index.php/Running_GUI_applications_as_root#Wayland>). It's suggested to split your program into a GUI frontend and a CLI backend that do privileged operations, and this is the backend, a custom re-write of <https://gitlab.freedesktop.org/libinput/libinput/-/blob/master/tools/libinput-debug-events.c>, based on [libinput](https://www.freedesktop.org/wiki/Software/libinput/), [libudev](https://www.freedesktop.org/software/systemd/man/libudev.html) and [libevdev](https://www.freedesktop.org/wiki/Software/libevdev/).

It generates JSON in lines like `{"event_name": "KEYBOARD_KEY", "event_type": 300, "device_name": "USB Keyboard USB Keyboard", "time_stamp": 39869802, "key_name": "KEY_C", "key_code": 46, "state_name": "PRESSED", "state_code": 1}`.

## GTK

A GUI frontend based on GTK, will run CLI backend as root via `pkexec`, and show a transparent floating window to display events.

# FAQ

## Why your program needs root permission? screenkey never asks for it!

If you debug with libinput, you'll find it needs root permission, too. Because this program support both Wayland and X11, it does not get input events via display protocol, actually it's reading directly from evdev interface under `/dev`. **And if you want to interact with files under `/dev`, you need root permission.** screenkey does not needs root permission because it's heavily X11-based, **it gets input events from X server** instead of `/dev`, which already done it. And because of this it will never support Wayland.

# Translate

If you changed translatable strings, don't forget to run `meson compile showmethekey-update-po` in build directory and then edit po files, and please check if there are `fuzzy` tag in comment, you should remove them and make translation exact, otherwise it will not work.

If you added new source files with translatable strings, don't forget to add it to `showmethekey-gtk/po/POTFILES.in` before running `meson compile showmethekey-update-po`. File paths in `POTFILES.in` should be relative to project directory.

If you want to add languages, first add a country code in `showmethekey-gtk/po/LINGUAS`, then run `meson compile showmethekey-update-po`, you will get a new `.po` file with your added country code. If this language needs UTF-8 encoding, don't use words like `zh_CN.UTF-8` in `showmethekey-gtk/po/LINGUAS` or file name, because RPM's find\_lang script may ignore them sometimes, and you should change to `charset=UTF-8` manually in the header.

# Name

As I want some clear name that hints its usage, but `screenkey` is already taken and I think `visualkey` sounds like `Visual Studio` and it's horrible. My friend [@LGiki](https://github.com/LGiki) suggests `Show Me The Key` which sounds like "Show me the code" from Linus Torvalds. At first I think it's a little bit long, but now it is acceptable so it's called `showmethekey` or `Show Me The Key`.

The Chinese translate of this program name should be `让我看键`, and it's only used for window title, debug output, package name and desktop entry name should not be translated.

# Icon

Program icon made by <a href="https://www.freepik.com" title="Freepik">Freepik</a> from <a href="https://www.flaticon.com/" title="Flaticon">www.flaticon.com</a>.
