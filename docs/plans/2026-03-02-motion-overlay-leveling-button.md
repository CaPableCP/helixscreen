# Motion Overlay Leveling Button Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add QGL / Z-Tilt button to the motion overlay, reusing ControlsPanel's existing leveling logic.

**Architecture:** MotionPanel's leveling callbacks delegate to `get_global_controls_panel().handle_qgl()` / `handle_z_tilt()` — zero code duplication. The existing `controls_operation_in_progress` subject (already globally registered) handles disable state across both panels. Layout rearranged: position display moves to a horizontal row above the jog pad, leveling button placed below position card in the right column.

**Tech Stack:** LVGL 9.5 XML, C++, lv_subject reactive bindings

---

### Task 1: Make ControlsPanel leveling handlers public

**Files:**
- Modify: `include/ui_panel_controls.h:362-363`

**Step 1: Move handle_qgl and handle_z_tilt from private to public**

In `include/ui_panel_controls.h`, move these two declarations from the private section to a new public section:

```cpp
  public:
    // === Leveling Commands (shared with MotionPanel) ===
    void handle_qgl();
    void handle_z_tilt();

  private:
```

Place this just before the existing `private:` block containing `handle_quick_actions_clicked()` (line ~344). Remove the original declarations from lines 362-363.

**Step 2: Build to verify no regressions**

Run: `make -j`
Expected: Clean build, no errors

**Step 3: Commit**

```
git add include/ui_panel_controls.h
git commit -m "refactor(controls): make handle_qgl/handle_z_tilt public for reuse"
```

---

### Task 2: Rearrange motion panel XML layout

**Files:**
- Modify: `ui_xml/motion_panel.xml`

**Step 1: Restructure the layout**

Replace the entire `overlay_content` section (lines 24-106) with this new layout. The key changes:
- Position card moves to a horizontal row across the top of `overlay_content`
- X/Y/Z displayed side-by-side in a single row
- Below that, a two-column row: jog pad left, Z controls right
- Leveling button(s) added below position card in the right column

```xml
    <!-- Content Area: Position bar on top, then two columns -->
    <lv_obj name="overlay_content"
            width="100%" flex_grow="1" style_pad_all="#space_lg" scrollable="false" style_border_opa="0" flex_flow="column"
            style_pad_gap="#space_md">
      <!-- Disable content when printer not connected or klippy not ready -->
      <bind_state_if_not_eq subject="nav_buttons_enabled" state="disabled" ref_value="1"/>
      <!-- Position Display - horizontal bar across the top -->
      <ui_card name="position_card"
               width="100%" height="content" style_pad_all="#space_sm" flex_flow="row" style_pad_gap="#space_lg"
               style_flex_main_place="space_evenly" style_flex_cross_place="center">
        <lv_obj style_pad_all="0" flex_flow="row" style_pad_gap="#space_xs" style_flex_cross_place="center">
          <text_small text="X:" translation_tag="X:"/>
          <text_small name="pos_x" bind_text="motion_pos_x"/>
          <lv_obj width="8" height="8" style_radius="9999">
            <bind_style name="indicator_homed" subject="motion_x_homed" ref_value="1"/>
            <bind_style name="indicator_unhomed" subject="motion_x_homed" ref_value="0"/>
          </lv_obj>
        </lv_obj>
        <lv_obj style_pad_all="0" flex_flow="row" style_pad_gap="#space_xs" style_flex_cross_place="center">
          <text_small text="Y:" translation_tag="Y:"/>
          <text_small name="pos_y" bind_text="motion_pos_y"/>
          <lv_obj width="8" height="8" style_radius="9999">
            <bind_style name="indicator_homed" subject="motion_y_homed" ref_value="1"/>
            <bind_style name="indicator_unhomed" subject="motion_y_homed" ref_value="0"/>
          </lv_obj>
        </lv_obj>
        <lv_obj style_pad_all="0" flex_flow="row" style_pad_gap="#space_xs" style_flex_cross_place="center">
          <text_small text="Z:"/>
          <text_small name="pos_z" bind_text="motion_pos_z"/>
          <lv_obj width="8" height="8" style_radius="9999">
            <bind_style name="indicator_homed" subject="motion_z_homed" ref_value="1"/>
            <bind_style name="indicator_unhomed" subject="motion_z_homed" ref_value="0"/>
          </lv_obj>
        </lv_obj>
      </ui_card>
      <!-- Two Columns: Jog Pad (left) + Z Controls (right) -->
      <lv_obj name="columns_row"
              width="100%" flex_grow="1" style_pad_all="0" scrollable="false" style_border_opa="0" flex_flow="row"
              style_flex_main_place="center">
        <!-- Left Column (65%): XY Jog Pad -->
        <lv_obj name="left_column" width="65%" height="100%" style_pad_all="0" scrollable="false" style_border_opa="0">
          <lv_obj name="jog_pad_container"
                  width="content" height="content" style_pad_all="0" style_border_opa="0" scrollable="false"
                  clickable="true" align="center">
          </lv_obj>
        </lv_obj>
        <!-- Right Column (35%): Z-Axis Controls + Leveling -->
        <lv_obj name="right_column"
                width="35%" height="100%" style_pad_all="0" scrollable="false" style_border_opa="0" flex_flow="column"
                style_flex_main_place="start" style_flex_cross_place="center" style_pad_gap="#space_sm">
          <!-- Z-Axis Button Group -->
          <lv_obj name="z_controls"
                  width="100%" flex_grow="1" style_pad_all="0" style_border_opa="0" scrollable="false" flex_flow="column"
                  style_flex_main_place="space_evenly" style_flex_cross_place="center" style_pad_gap="#space_sm">
            <ui_button name="z_up_10"
                       width="100%" flex_grow="1" style_max_height="#button_height_lg" icon="arrow_up"
                       bind_icon="motion_z_up_icon" text="10mm" translation_tag="10mm">
              <event_cb trigger="clicked" callback="on_motion_z_button" user_data="z_up_10"/>
            </ui_button>
            <ui_button name="z_up_1"
                       width="100%" flex_grow="1" style_max_height="#button_height_lg" icon="arrow_up"
                       bind_icon="motion_z_up_icon" text="1mm" translation_tag="1mm">
              <event_cb trigger="clicked" callback="on_motion_z_button" user_data="z_up_1"/>
            </ui_button>
            <!-- Z-Axis label - shows "Bed" or "Print Head" based on kinematics -->
            <text_body name="z_axis_label" width="100%" bind_text="motion_z_axis_label" style_text_align="center"/>
            <ui_button name="z_down_1"
                       width="100%" flex_grow="1" style_max_height="#button_height_lg" icon="arrow_down"
                       bind_icon="motion_z_down_icon" text="1mm" translation_tag="1mm">
              <event_cb trigger="clicked" callback="on_motion_z_button" user_data="z_down_1"/>
            </ui_button>
            <ui_button name="z_down_10"
                       width="100%" flex_grow="1" style_max_height="#button_height_lg" icon="arrow_down"
                       bind_icon="motion_z_down_icon" text="10mm" translation_tag="10mm">
              <event_cb trigger="clicked" callback="on_motion_z_button" user_data="z_down_10"/>
            </ui_button>
          </lv_obj>
          <!-- Leveling Buttons: QGL and/or Z-Tilt (conditionally visible) -->
          <lv_obj name="leveling_buttons"
                  width="100%" height="content" style_pad_all="0" scrollable="false" flex_flow="row"
                  style_pad_gap="#space_xs">
            <ui_button name="btn_qgl"
                       width="0" height="#button_height_sm" flex_grow="1" text="QGL"
                       translation_tag="QGL" variant="secondary" hidden="true"
                       style_text_font="#font_xs">
              <bind_flag_if_eq subject="printer_has_qgl" flag="hidden" ref_value="0"/>
              <bind_state_if_eq subject="print_active" state="disabled" ref_value="1"/>
              <bind_state_if_eq subject="controls_operation_in_progress" state="disabled" ref_value="1"/>
              <event_cb trigger="clicked" callback="on_motion_qgl"/>
            </ui_button>
            <ui_button name="btn_z_tilt"
                       width="0" height="#button_height_sm" flex_grow="1" text="Z-Tilt"
                       translation_tag="Z-Tilt" variant="secondary" hidden="true"
                       style_text_font="#font_xs">
              <bind_flag_if_eq subject="printer_has_z_tilt" flag="hidden" ref_value="0"/>
              <bind_state_if_eq subject="print_active" state="disabled" ref_value="1"/>
              <bind_state_if_eq subject="controls_operation_in_progress" state="disabled" ref_value="1"/>
              <event_cb trigger="clicked" callback="on_motion_z_tilt"/>
            </ui_button>
          </lv_obj>
        </lv_obj>
      </lv_obj>
    </lv_obj>
```

**Step 2: Verify visually**

Applying [L031]: XML files are loaded at runtime — no rebuild needed. Just relaunch:

Run: `./build/bin/helix-screen --test -vv -p motion_panel 2>&1 | head -50`

Expected: App launches, motion overlay shows position bar across top, jog pad left, Z buttons right with leveling buttons below.

**Step 3: Commit**

```
git add ui_xml/motion_panel.xml
git commit -m "feat(motion): rearrange layout with horizontal position bar and leveling buttons"
```

---

### Task 3: Wire up leveling button callbacks in MotionPanel C++

**Files:**
- Modify: `src/ui/ui_panel_motion.cpp`

**Step 1: Add include for ControlsPanel**

At the top of `src/ui/ui_panel_motion.cpp`, after the existing includes (around line 19), add:

```cpp
#include "ui_panel_controls.h"
```

**Step 2: Add static callback functions**

After the existing `on_motion_z_button` forward declaration (line 33), add:

```cpp
static void on_motion_qgl(lv_event_t* e);
static void on_motion_z_tilt(lv_event_t* e);
```

**Step 3: Register the callbacks in register_callbacks()**

In `MotionPanel::register_callbacks()` (line 154), after the existing `on_motion_z_button` registration (line 163), add:

```cpp
    lv_xml_register_event_cb(nullptr, "on_motion_qgl", on_motion_qgl);
    lv_xml_register_event_cb(nullptr, "on_motion_z_tilt", on_motion_z_tilt);
```

**Step 4: Implement the static callbacks (DRY delegation)**

At the bottom of the file, after the existing `on_motion_z_button` implementation (line 583-590), add:

```cpp
static void on_motion_qgl(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[MotionPanel] on_motion_qgl");
    (void)e;
    get_global_controls_panel().handle_qgl();
    LVGL_SAFE_EVENT_CB_END();
}

static void on_motion_z_tilt(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[MotionPanel] on_motion_z_tilt");
    (void)e;
    get_global_controls_panel().handle_z_tilt();
    LVGL_SAFE_EVENT_CB_END();
}
```

**Step 5: Build and verify**

Run: `make -j`
Expected: Clean build, no errors

**Step 6: Commit**

```
git add src/ui/ui_panel_motion.cpp
git commit -m "feat(motion): add QGL/Z-Tilt leveling buttons delegating to controls panel"
```

---

### Task 4: Adjust jog pad sizing for new layout

**Files:**
- Modify: `src/ui/ui_panel_motion.cpp:226-238`

The jog pad size calculation needs updating since the position card is now above the columns row, eating into available height.

**Step 1: Update the height calculation in setup_jog_pad()**

In `MotionPanel::setup_jog_pad()`, update the size calculation (lines 226-238). The available height now excludes the header AND the position card row:

```cpp
    // Get header height (varies by screen size: 50-70px)
    lv_obj_t* header = lv_obj_find_by_name(overlay_root_, "overlay_header");
    lv_coord_t header_height = header ? lv_obj_get_height(header) : 60;

    // Get position card height (horizontal bar at top)
    lv_obj_t* position_card = lv_obj_find_by_name(overlay_root_, "position_card");
    lv_coord_t pos_card_height = position_card ? lv_obj_get_height(position_card) : 40;

    // Available height = screen height - header - position card - padding - gap
    lv_coord_t available_height = screen_height - header_height - pos_card_height - 60;

    // Jog pad = 85% of available height (compact to leave room for leveling buttons)
    lv_coord_t jog_size = (lv_coord_t)(available_height * 0.85f);
```

**Step 2: Build and verify**

Run: `make -j`
Expected: Clean build

**Step 3: Commit**

```
git add src/ui/ui_panel_motion.cpp
git commit -m "fix(motion): adjust jog pad sizing for horizontal position bar layout"
```

---

### Task 5: Visual verification

**Step 1: Launch the app**

Run: `./build/bin/helix-screen --test -vv 2>&1 | tee /tmp/motion-test.log`

(Use `run_in_background: true`)

**Step 2: Tell user to verify**

Ask user to:
1. Open the motion overlay
2. Verify position display is horizontal across the top (X | Y | Z)
3. Verify jog pad fills left area properly
4. Verify Z buttons are on the right
5. Verify QGL or Z-Tilt button appears below Z controls (if printer has those configured)
6. Click the leveling button and verify toast notifications appear

**Step 3: Read logs**

Read `/tmp/motion-test.log` and check for:
- `[MotionPanel] on_motion_qgl` or `on_motion_z_tilt` log lines on click
- No errors or warnings

**Step 4: Final commit (if any adjustments needed)**
