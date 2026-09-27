# Changelog

All notable TAUREON V4 changes will be documented here.

## [Unreleased]

### MIDI engine package

- Separated the generic MIDI/SysEx/transfer engine from the Qt product host and
  added an installed CMake package with independent Debug/Release verification.
- Assigned the experimental engine package its own `0.1.0` version, independent
  of the product application version.
- Licensed the current repository source under MIT; the previously restricted
  real Summit dump was removed rather than included under that license.
- Replaced the restricted real Summit SysEx test fixture with an artificial
  frame generated only in the build directory; no `.syx` fixture remains tracked
  in the current source tree.

### Project reset

- Established TAUREON V4 as a clean native C++20 / Qt 6 rebuild.
- Separated the new V4 project from the legacy TAUREON repository.
- Reorganized project documentation into product, architecture, process, design, status, stage, and reference areas.
- Defined ChatGPT Classic as Project Manager/Supervisor, Codex as Implementation Lead, Claude Code as Architecture/Review Lead, and the User as Product Owner/Hardware Tester/final decision authority.
- Added the initial clickable GUI workflow mockup as a design-only artifact.
