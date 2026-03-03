# Motion Overlay Leveling Button Design

**Date:** 2026-03-02
**Status:** Approved

## Goal

Add a bed leveling button (QGL or Z-Tilt Adjust) to the motion controls overlay, reusing existing logic from the controls panel. Only shown when the printer has `quad_gantry_level` or `z_tilt` configured.

## Behavior

- **Fire-and-forget:** Button sends `QUAD_GANTRY_LEVEL` or `Z_TILT_ADJUST` gcode command
- **Toast notifications:** NOTIFY_INFO on start, NOTIFY_SUCCESS/ERROR/WARNING on completion
- **Adaptive label:** Shows "QGL" when `printer_has_qgl`, "Z-Tilt" when `printer_has_z_tilt`
- **Disabled states:** During active print (`print_active`) and during operation (`controls_operation_in_progress`)
- **Hidden:** When neither QGL nor Z-Tilt is configured

## DRY: Shared Leveling Command Logic

Extract the common QGL/Z-Tilt execution pattern from `ControlsPanel` into a shared free function:

```cpp
// leveling_commands.h / leveling_commands.cpp
void run_leveling_command(const std::string& command,
                          const std::string& display_name,
                          OperationTimeoutGuard& guard,
                          MoonrakerAdvancedAPI* api);
```

Both `ControlsPanel::handle_qgl()`/`handle_z_tilt()` and `MotionPanel` call this shared function. The function handles:
- Guard check (already active → warning)
- Guard begin with timeout
- NOTIFY_INFO on start
- Success/error/timeout callbacks with guard cleanup via `helix::ui::async_call()`

## Layout Changes to Motion Overlay

**Current layout:**
- Left column (65%): Jog pad
- Right column (35%): Position display (X/Y/Z) on top, Z buttons below

**New layout:**
- Top row: Position display (X/Y/Z) laid out **horizontally**
- Below-left: Jog pad
- Below-right: Z buttons + leveling button underneath

## Visibility Subjects

Reuse existing subjects — no new subjects needed:
- `printer_has_qgl` (from PrinterCapabilitiesState)
- `printer_has_z_tilt` (from PrinterCapabilitiesState)
- `print_active` (from PrinterState)
- `controls_operation_in_progress` (from ControlsPanel — shared across panels)
