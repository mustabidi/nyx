# Contributing to Nyx

We love contributions! Since **Nyx** is natively forced into Libadwaita dark mode and implements complex asynchronous worker thread layers for directory playback, please follow these guidelines to keep the codebase elegant and performant.

---

## 🛠️ Development Setup

To compile and run Nyx in a sandboxed developer environment, install dependencies matching GTK4, Libadwaita (1.5+), and GStreamer (1.20+), then execute:

```bash
# Setup build directory
meson setup builddir

# Compile the target
meson compile -C builddir

# Run local build
./run
```

---

## 📐 Coding Guidelines

### 1. C90 Compliance
- All C source files must declare variables strictly at the beginning of local lexical blocks.
- Avoid introducing any mixed declarations or mid-block initializations.

### 2. Thread Safety
- Any operations involving file scanning, crawling, or network requests must be performed asynchronously inside a background worker thread (`GTask` via thread-pool).
- Never block the GTK4 main thread.
- Collation and naturally sorting results should be performed inside the worker thread prior to sending results back to the main UI loop callback.

### 3. Libadwaita Paradigm
- Keep visual assets and colors unified around the celestial theme.
- Avoid modifying the Forced Dark Mode style manager parameters without coordinating with core design developers.
