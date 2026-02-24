# Multi-Printer Support Plan

## Branch: `feature/multi-printer`
## Worktree: `.worktrees/multi-printer/`

## Phase 1: Config v3 Schema - COMPLETE
**Commit:** `6eca88f6` feat(config): add multi-printer config v3 schema with dynamic routing

- Config restructured from `/printer/` to `/printers/{id}/` object map
- `df()` dynamically routes to active printer
- v2->v3 migration, printer CRUD methods, slugify()
- StaticSubjectRegistry/StaticPanelRegistry: added clear() for soft restart
- 13 new test cases, all ~70 existing call sites updated

**Known issue:** 11 LED config test failures from path changes (test_led_config.cpp needs updating)

## Phase 2: Soft Restart Machinery - COMPLETE
**Commit:** `e2d21607` feat(app): add soft restart machinery for runtime printer switching

Files modified:
- `include/application.h` - Added switch_printer(), tear_down_printer_state(), init_printer_state()
- `src/application/application.cpp` - ~150 lines implementing the 3 methods + 'P' key shortcut
- `src/ui/ui_nav_manager.cpp` - Reset shutting_down_ in deinit_subjects()
- `tests/unit/test_config.cpp` - 2 registry cycle tests (5 assertions, all pass)

Key design decisions:
- tear_down_printer_state() follows shutdown() ordering exactly (steps 1-15)
- init_printer_state() follows run() phases 9-15 (skips display/theme/assets)
- DO NOT call lv_deinit() during soft restart - display stays alive
- 'P' key cycles printers in test mode; moved action prompt test to 'A' key

## Phase 3: UI Integration - NOT STARTED
TODO:
- Printer manager overlay/modal (list printers, add/remove, switch)
- Printer name display in navbar or home panel
- Wizard integration for adding new printers
- Connection status per-printer

## Phase 4: Discovery & Auto-Add - NOT STARTED
TODO:
- mDNS/Zeroconf printer discovery
- Auto-add discovered printers
- Scan network for Moonraker instances

## Testing Notes
- Build: `cd .worktrees/multi-printer && make -j`
- Tests: `./build/bin/helix-tests "[core][config]"` (63 assertions, all pass)
- Registry: `./build/bin/helix-tests "[core][registry]"` (5 assertions, all pass)
- Manual: `./build/bin/helix-screen --test -vv` with 2+ printers in config, press 'P' to switch
