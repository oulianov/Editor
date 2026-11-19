# Feedback

User feedback to look at.

## 2026-11-18 (a tester coming from the Maniacs editor, Linux AppImage)

### Pictures

- [ ] Picture effects: Rotation and Waver refuse negative values. The Maniacs editor takes
  them (check RPG Maker 2003 and the Player); a variable works around it for now.
- [ ] Values out of range, at the user's own risk: typed by hand, a number field takes any
  value (negative rotation or waver, the negative indices the Maniac Patch uses for self
  switches and variables...); the sliders and arrows stay within the range. A value out of
  range gets a badge beside it, as the M/E/P ones, saying the engine may not run it and
  things may break.

### More EasyRPG-only database options

- [ ] Edit more of the options only EasyRPG Player reads, which today need lcf2xml: the
  tester names the number of save files (System easyrpg_max_savefiles) and fixed enemy
  facing directions in RPG Maker 2003 battles (BattleCommands
  easyrpg_fixed_enemy_facing_direction). Fields of liblcf's fields_easyrpg.csv that no
  QML page or model uses yet:
  - System: easyrpg_battle_options, easyrpg_max_savefiles, easyrpg_variable_min_value,
    easyrpg_variable_max_value, easyrpg_default_actorai, easyrpg_default_enemyai
  - BattleCommands: easyrpg_default_atb_mode, easyrpg_enable_battle_row_command,
    easyrpg_sequential_order, easyrpg_disable_row_feature,
    easyrpg_fixed_actor_facing_direction, easyrpg_fixed_enemy_facing_direction
  - Actor: easyrpg_actorai, easyrpg_unarmed_state_chance
  - Enemy: easyrpg_enemyai, easyrpg_hit, easyrpg_state_chance, easyrpg_super_guard
  - Skill: easyrpg_battle2k3_message, easyrpg_ignore_reflect, easyrpg_state_hit,
    easyrpg_attribute_hit, easyrpg_ignore_restrict_skill, easyrpg_ignore_restrict_magic,
    easyrpg_enable_stat_absorbing, easyrpg_affected_by_evade_all_physical_attacks,
    easyrpg_critical_hit_chance, easyrpg_affected_by_row_modifiers, easyrpg_hp_type,
    easyrpg_hp_percent, easyrpg_hp_cost
  - Item: easyrpg_using_message, easyrpg_max_count
  - Terrain: easyrpg_damage_in_percent, easyrpg_damage_can_kill

### Resources

- [ ] Subfolders of the resource folders: let the file pickers choose files in subfolders
  (`CharSet/heroes/x.png`, `Sound/Variated/cardplace1`). EasyRPG Player plays them, RPG
  Maker 2009 Ultimate offered it (so RPG_RT.exe reads them too, to check). The Maniacs
  editor: the picture picker has a folder tree on the left (the project's folders with
  their file count, "Project [0]", "BattleCharSet [64]", subfolders expandable, "Show RTP"
  and "Open folder" below), the file list (RTP files marked `<RTP>`) and the preview; the
  sound picker lists the subfolders' files by path in the same list ("Variated\cardplace1").

## 2026-11-19 (Zoe On The Loose, tester)

### Pictures

- [ ] Show Picture: the preview shows a spritesheet's frame; it could also play the
  spritesheet's animation (animation mode, its speed).

### New Project

- [ ] What is the tile size setting (Advanced tab) for? The tester sees no way to use it,
  and every RPG Maker 2000/2003 version is fixed to 16 px tiles: hide it, or explain it
  (a future plan?).

### Map

- [ ] Ctrl + mouse wheel zooms in, but zooms out only down to 1/2; 1/4 and 1/8 need the
  buttons. The wheel should reach every zoom level.
- [ ] What are the tiles at the top of the tile selection (chipset) window? Explain them
  (help, tooltip) or check they should be there.
- [ ] A double right-click switches between the map layers, and choosing a tile switches to
  tile editing; there is no quick way back to the event layer but its button. Idea: a
  double middle-click switches to the event layer.

### Database

- [ ] Chipset tab: a lot of unused space. Let the user zoom into the tileset there; bigger
  tiles would also make the passability markers easier to tell apart.
- [ ] Chipset tab, Linux: every passability symbol (counter, arrows, star...) looks broken.
  The markers are pixel-exact icons scaled by a non-integer factor with nearest-neighbour
  interpolation, so their few pixels stretch unevenly. Scale them by an integer factor (or
  draw them as vectors) so they stay crisp.

### Call Command (Maniac Patch)

- [ ] Load a command into Call Command. The Maniacs editor's Call Command window has "Load
  Command Data from Clipboard": copy a command (as for copy and paste) and the button fills
  Call Command with its code, its string and its number arguments. Under Wine it does not
  work (no access to the clipboard there). The use: make a command with many parameters
  (Show Picture has more than 20, Show String Picture...) in its own editor, load it into
  Call Command, then replace some values with variables or pointers the command's own
  editor does not offer. Typing them all by hand is tedious and error-prone. In the
  Editor: load the command copied in the Editor's own event commands (the Maniacs editor's
  clipboard format may differ), or a "Convert to Call Command" on a command of the list.
- [ ] Call Command: show the name of the command its code stands for, and choose a command
  by its name ("By Command Name" in the Maniacs editor). Today the Editor has a number field
  only (Command code).
- [ ] The other way: turn a Call Command into the command it calls. The Maniacs editor's
  "Generate Command instead of Call (@raw)" writes the regular command (its own editor, its
  own line in the list) instead of the Call Command. It refuses when the values are not all
  fixed or all variables, so the tester rarely finds it useful as is; but it shows which
  value does what in the resulting command. In the Editor: convert when every value is
  fixed, and otherwise show, beside each parameter of Call Command, what it is in the
  command called (its field's name in that command's editor).
- Reference, the Maniacs editor's window: Command Code (Fixed, Variable, Variable ID; By
  Command Name), String Argument (Fixed, Variable, Variable ID), Number Arguments in tabs
  (Fixed: comma-separated numbers, `v[number]`, `v[v[number]]`; Expression; Variable Array),
  "Align elements", "Generate Command instead of Call (@raw)", "Load Command Data from
  Clipboard".

## Addressed

### 2026-11-09 (0.2.14)

- [x] You can't navigate with the arrow keys after typing an event name. Event Commands:
  Tab (or Down) goes from the search to the results, the arrows go through them.
- [x] In RPG Maker 2003 the game's own files appear before the RTP ones; here they are
  sorted by name only. The file pickers, the Resource Manager and the Jukebox list the
  game's files first.
- [x] Maybe add a details header to every source, clickable to change how the files are
  sorted. Resource Manager and Jukebox: a click on a column's title sorts by it.

### Variables, switches and values (feedback of 2026-11-18)

- [x] The tester asks whether the drop-down and the "..." of a variable or switch do the
  same thing (both pick an entry; the "..." window also renames the entries): say the
  difference in their tooltips, or keep one of them. The "..." says it opens the window of
  the switches (variables...) to choose one or to rename them.
- [x] Show the names of switches, variables and string variables everywhere they appear,
  not only their numbers (the Maniacs editor does; the tester misses it most).
- [x] Let the user type a variable or switch number straight into its field, instead of
  always picking it from the pop-up window (RefComboBox). The Maniacs editor (Control
  Variables, Target "Single"): a spin box with the number (typed in, its arrows, or
  scrolled), then a field with the number and the name ("0008:x") and a "..." button for
  the window; no window to open just to change the number. Typing a number in the box
  chooses that entry; the command lines show the names.

Fixed in 0.2.21 (feedback of 2026-11-18).

### Variables, switches and values

- [x] Picking a variable or switch: the "file icon" button of the box takes a while to open,
  the "..." does not (perhaps because of the number of variables of the game: measure it on
  a game with many). The file icon is the box's drop-down arrow (RefComboBox: the searchable
  list under the box), drawn on Linux as a blank page, the placeholder of a missing icon
  (screenshot from the tester: "..." then a grey page). The icon theme is likely missing in
  the AppImage, as for the spin boxes' arrows: fix the arrow icons there.
- [x] Bug, the entry picker window ("..."; Linux): with "0008" typed in its search, the list
  shows 0008: x then 0080... 0805. Clicking 0008: x selects another line (0086: panel slot
  6), and double-clicking 0008 still chooses 0086. The click lands on the wrong line of
  the searched list (as the database lists' search once did, fixed in 0.2.12). In 0.2.22,
  fixed for every database list (their "index" role renamed "row").
- [x] Spin boxes: a value typed in (a variable operand...) is not taken by the + and -
  buttons, which put the previous value back.
- [x] Spin boxes: the + and - buttons show no arrow in the Linux AppImage, so it is hard to
  tell what they do (check the icons there). The tester is not used to drag-scrubbing values
  either (a preference: the buttons stay side by side, with drag-scrub).

### Pictures

- [x] Spritesheets of pictures are not recognized, though the changelog says they are
  (Show Picture, Advanced tab: RPG Maker 2003 1.12's spritesheet, PictureEditor.qml).

### Battle system

- [x] In an RPG Maker 2003 game with the EasyRPG Extensions, the skills' battle messages stay
  greyed after switching to RPG Maker 2000's battle system. Check that the Skills page follows
  Database > System > EasyRPG Player > Battle system (0.2.17), in the same database window too
  (SkillPage.qml, has2kMessages). The other way round too: RPG Maker 2003's messages
  (Skill easyrpg_battle2k3_message) in a 2000 battle system.

### Event Commands window (0.2.23)

- [x] Control Switches and Control Variables are on the Messages & Party tab: the tester
  looked for them on Flow & System (and is not used to the tabs' layout yet). Editor
  Settings › Display › Event Commands window: "By topic" moves them, with Timer Operations,
  to Flow & System; RPG Maker 2003's order (1.0.8) stays the default.

### Database (0.2.23)

- [x] Skills, Using Message › Failure showed garbled text in bismillahdventure: the game's
  Vocabulary still has RPG Maker's Japanese battle messages, which the Vocabulary page hid
  in an RPG Maker 2003 game. It now shows RPG Maker 2000's battle terms when the game uses
  RPG Maker 2000's battle system (EasyRPG Player's option).
