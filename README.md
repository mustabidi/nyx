# Nyx Media Player
[![Build Status](https://github.com/moon/nyx/actions/workflows/build.yml/badge.svg)](https://github.com/moon/nyx)

**Nyx** is a celestial, dark-themed, modern Linux media player built for visual excellence and premium user experiences. Nyx brings a cohesive, night-themed celestial aesthetic forced into Libadwaita dark mode, alongside powerful background traversal automation.

Powered by [GStreamer](https://gstreamer.freedesktop.org/) and built for the GNOME desktop environment using [GTK4](https://www.gtk.org/) and [Libadwaita](https://gitlab.gnome.org/GNOME/libadwaita), Nyx provides a gorgeous, state-of-the-art interface that makes enjoying your favorite media absolute bliss.

---

## ✨ Features

### 🌌 Celestial Night Styling
Nyx features a tailored, custom color palette (`#1a1f3a`) forcing Adwaita dark mode with glassmorphic accents, vibrant glows, and responsive micro-animations that make the interface feel incredibly responsive and alive.

### 📁 Open Folder as Playlist
Scan and play entire directories asynchronously in the background:
- **Background Traversal Pipeline**: Traverses deep folder structures in background worker threads (`GTask` thread pool) keeping the GTK4 main thread perfectly fluid.
- **Natural Numeric Collation**: media files are naturally sorted via a zero-allocation, case-insensitive, UTF-8 folded digit sequence comparator (sorting `Part 2` correctly before `Part 10`).
- **Interactive Queue Merging**: When opening a folder, if the current playlist is not empty, Nyx prompts you with a sleek, responsive dialog offering to either **Replace** the existing queue (clearing it and playing the folder immediately) or **Append** the new files to the end.

---

## ⌨️ Keyboard Shortcuts & Gestures

Nyx features a comprehensive set of keyboard shortcuts and overlays to speed up your interactions:

| Action | Shortcut | Description |
|---|---|---|
| **Add Files** | `<Ctrl>O` | Prompt file dialog to choose individual files |
| **Open Folder** | `<Ctrl><Shift>O` or `<Ctrl>d` | Prompt directory dialog to scan and play folder contents |
| **Add URI** | `<Ctrl>U` | Prompt dialog to add network stream |
| **New Window** | `<Ctrl>N` | Open a new independent window |
| **Toggle Play** | `Space` or `k` | Pause or resume playback |
| **Volume Control** | `Up` / `Down` | Increase / decrease audio volume |
| **Fullscreen** | `F11` or `f` | Toggle fullscreen mode |

---

## 📦 Compilation and Installation

Building Nyx from source is straightforward using the Meson build system:

```sh
# Setup the build directory
meson setup builddir

# Compile the project
meson compile -C builddir

# Install the application
sudo meson install -C builddir
```

To automatically rebuild and run the local developer build from the root workspace directory without system installation, simply execute the convenient developer run script:
```sh
./run
```


---

## 🛡️ License
Nyx is licensed under `GPL-3.0-or-later` for the binary layer, while the underlying playback and GTK integration libraries (`Nyx` and `NyxGtk`) are licensed under `LGPL-2.1-or-later`.
