# Changelog

All notable changes to the **Nyx Media Player** project will be documented in this file.

---

## [0.1.0] - 2026-05-18

### Added
- **Fork Creation**: Cloned Rafostar's open-source Nyx player and established the **Nyx** codebase.
- **Brand Identity**: Complete deep renaming of GObject types, schema identifiers, desktop configuration files, metainfo records, DBus registries, resource paths, and logs to `Nyx`/`nyx`/`NYX` branding.
- **Celestial Dark Styling**: Force Libadwaita dark mode with modern night-themed color palette `#1a1f3a`.
- **Custom Application Icon**: Hand-crafted symbolic and full-color SVG crescent moon brand logos.
- **Background Traverser**: Implemented a responsive asynchronous GTask folder crawling daemon (`MAX_RECURSION_DEPTH = 32`) keeping GUI thread execution perfectly fluid.
- **Natural Comparator**: Developed a C90-compliant zero-allocation natural numeric string collator ensuring sequential media items sort correctly.
- **Queue Merger Dialogue**: Integrates an `AdwAlertDialog` when loading directories on non-empty queues to offer "Replace" and "Append" enqueuing strategies.
- **Hotkeys**: Standardized keyboard shortcuts `<Ctrl><Shift>O` and `<Ctrl>d` to trigger directory playlist expansion, complete with updated layout shortcut overlays and initial state action pills.
