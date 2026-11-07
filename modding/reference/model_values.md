<!-- Made by xwa_mod_tables. Change the tool, not this page. -->

# Model values

Names that models and their settings files use for game values.

## Hardpoint types

Values for the `type` of a hardpoint in a model. The keys of `hardpoint_kinds` in the weapon settings are the same names.

The weapons of a craft come from its craft record, in `craft_record.lasers` and `craft_record.warheads`. A weapon hardpoint only gives a position from which a weapon fires. When the craft record has a free group, a weapon hardpoint also adds its weapon to that group.

| Number | Name | Meaning |
|---:|---|---|
| 0 | `none` | Not used, except on object types with the `float_hardpoints` flag, where it is a turret position. |
| 1 | `rebel_laser` | A firing position for `rebel_laser`. |
| 2 | `turbo_rebel_laser` | A firing position for `rebel_laser_turbo`. |
| 3 | `empire_laser` | A firing position for `imperial_laser`. |
| 4 | `turbo_empire_laser` | A firing position for `imperial_laser_turbo`. |
| 5 | `ion_cannon` | A firing position for `ion_cannon`. |
| 6 | `turbo_ion_cannon` | A firing position for `ion_cannon_turbo`. |
| 7 | `torpedo` | A firing position for `torpedo`. |
| 8 | `missile` | A firing position for `missile`. |
| 9 | `super_rebel_laser` | A firing position for `turbolaser_red`. |
| 10 | `super_empire_laser` | A firing position for `turbolaser_green`. |
| 11 | `super_ion_cannon` | A firing position for `turbolaser_ion`. |
| 12 | `super_torpedo` | A firing position for `advanced_torpedo`. |
| 13 | `super_missile` | A firing position for `advanced_missile`. |
| 14 | `dumb_bomb` | A firing position for `space_bomb`. |
| 15 | `fired_bomb` | A firing position for `heavy_rocket`. |
| 16 | `mag_pulse` | A firing position for `mag_pulse`. |
| 17 | `turbo_mag_pulse` | A firing position for `ion_pulse`. |
| 18 | `super_mag_pulse` | A firing position for `turbolaser_red_light`. |
| 19 | `gunner` | Not a gunner seat. The game also tests the craft's collisions from this point. |
| 20 | `cockpit_sparks` | In a cockpit model: where sparks appear when the player's craft is hit. Up to 16. |
| 21 | `docking_point` | Where another craft parks when it is ordered to dock with this one. Up to 9. |
| 22 | `towing` | The front link point. In a chain of linked objects, the next object attaches its back link point here. |
| 23 | `acceleration_start` | The same as `towing`. When a model has both, the last one counts. |
| 24 | `acceleration_end` | The back link point. The game also tests the craft's collisions from this point. |
| 25 | `inside_hangar` | The hangar: where craft launch from and where they land. The hangar's tractor beam pulls the player in near it. |
| 26 | `outside_hangar` | The hangar entrance. Craft fly to it before they land, and head toward it when they launch. |
| 27 | `dock_from_big` | On a craft that another craft carries: the height at which it is held, when both craft are freighters or larger. |
| 28 | `dock_from_small` | On a craft that another craft carries: the height at which it is held, when one of the craft is smaller than a freighter. |
| 29 | `dock_to_big` | On a carrier: the height at which it holds a craft that is a freighter or larger. |
| 30 | `dock_to_small` | On a carrier: the height at which it holds a craft that is smaller than a freighter. |
| 31 | `pilot_eye` | The pilot's eye: where the cockpit view is. |
| 32 | `engine_glow` | Not used. Engine glows are separate nodes of the model. |
| 33 | `custom_1` | Not used, unless a weapon setting makes it a firing position. |
| 34 | `custom_2` | Not used, unless a weapon setting makes it a firing position. |
| 35 | `custom_3` | Not used, unless a weapon setting makes it a firing position. |
| 36 | `custom_4` | Not used, unless a weapon setting makes it a firing position. |
| 37 | `custom_5` | Not used, unless a weapon setting makes it a firing position. |
| 38 | `custom_6` | Not used, unless a weapon setting makes it a firing position. |
| 39 | `jamming_point` | An enemy craft with working systems jams the player's weapons near this point. Up to 8. |

## Mesh types

Values for the `meshType` of a part in a model. `component_strength` in the model settings uses the same names as keys.

| Number | Name |
|---:|---|
| 0 | `default` |
| 1 | `main_hull` |
| 2 | `wing` |
| 3 | `fuselage` |
| 4 | `gun_turret` |
| 5 | `small_gun` |
| 6 | `engine` |
| 7 | `bridge` |
| 8 | `shield_generator` |
| 9 | `energy_generator` |
| 10 | `launcher` |
| 11 | `communication_system` |
| 12 | `beam_system` |
| 13 | `command_system` |
| 14 | `docking_platform` |
| 15 | `landing_platform` |
| 16 | `hangar` |
| 17 | `cargo_pod` |
| 18 | `misc_hull` |
| 19 | `antenna` |
| 20 | `rotary_wing` |
| 21 | `rotary_gun_turret` |
| 22 | `rotary_launcher` |
| 23 | `rotary_communication_system` |
| 24 | `rotary_beam_system` |
| 25 | `rotary_command_system` |
| 26 | `hatch` |
| 27 | `custom` |
| 28 | `weapon_system_1` |
| 29 | `weapon_system_2` |
| 30 | `power_regenerator` |
| 31 | `reactor` |

## Explosion flags

Values for the `explosion` list of a part.

| Name | Meaning |
|---|---|
| `hit_points` | The part has its own hit points. |
| `detachable` | The part can be damaged and break off. |

## Cockpit screen sources

Values for the `source` of a cockpit screen rectangle in a model. Each name is a cockpit display. The rectangle shows that display as the game draws it.

| Name |
|---|
| `left_sensor` |
| `right_sensor` |
| `laser_recharge` |
| `shield_recharge` |
| `engine_power` |
| `beam_recharge` |
| `shields` |
| `beam_level` |
| `targeting_computer` |
| `quad_lasers_left` |
| `quad_lasers_right` |
| `left_text_box` |
| `right_text_box` |
| `speed_throttle` |
| `missiles` |
| `name_time` |
| `ship_count` |
| `quad_lasers` |
| `dual_lasers_left` |
| `dual_lasers_right` |
| `dual_lasers` |
| `b_wing_lasers` |
| `six_lasers` |
| `six_lasers_left` |
| `six_lasers_right` |
| `shields_front` |
| `shields_back` |
| `text_radio` |
| `text_system` |
| `text_command` |
| `target_name` |
| `target_shields` |
| `target_hull` |
| `target_cargo` |
| `target_systems` |
| `target_distance` |
| `target_subcomponent` |
| `eight_lasers` |
| `throttle_bar` |
| `autosize_missiles_left` |
| `autosize_missiles_right` |
| `autosize_speed` |
| `autosize_chaff` |
| `autosize_name` |
| `autosize_time` |
| `autosize_missiles_both` |
| `autosize_missiles_trimmed` |
| `dual_laser_energy_left` |
| `dual_laser_energy_right` |
| `quad_laser_energy_left_up` |
| `quad_laser_energy_left_down` |
| `quad_laser_energy_right_up` |
| `quad_laser_energy_right_down` |
| `all_laser_energy` |

## Cockpit screen layers

Values for the `layers` list of a cockpit screen rectangle: the parts of the display that the rectangle shows.

| Name | Meaning |
|---|---|
| `graphics` | The display's images. |
| `text` | The display's text. |
| `bar` | A fill bar in a plain colour, given by the rectangle's `bar`. |

## HUD areas

Values for the `hide` list of a cockpit screen. The game doesn't draw these areas of the 2D HUD while the screen is visible.

| Name |
|---|
| `left_sensor` |
| `right_sensor` |
| `shields` |
| `beam` |
| `target_and_lasers` |
| `left_text_box` |
| `right_text_box` |
| `top_left` |
| `top_right` |
| `text_radio_system` |
| `text_command` |

## Reticle slots

The parts of the reticle that the `hud.reticle` group of a model settings file can change. Each value is the number of an image of image group 12000, counted from 1. The threat lights are written as nested maps, such as `threats: {laser_mode: {turret: 135}}`.

| Name | Meaning |
|---|---|
| `laser` | The laser reticle. |
| `laser_in_range` | Drawn over the laser reticle when the target is in range. |
| `warhead` | The warhead reticle. |
| `warhead_lock` | Drawn over the warhead reticle while the player gets a lock, and when the lock is done. |
| `cannon_ready` | Drawn over the mark of a cannon when that cannon fires next. |
| `cannon` | The mark of each cannon around the reticle. |
| `threats.laser_mode.laser` | The light that shows a laser threat, with the laser reticle. |
| `threats.laser_mode.turret` | The light that shows a turret threat, with the laser reticle. |
| `threats.laser_mode.beam` | The light that shows a beam threat, with the laser reticle. |
| `threats.laser_mode.warhead` | The light that shows a warhead threat, with the laser reticle. |
| `threats.warhead_mode.laser` | The light that shows a laser threat, with the warhead reticle. |
| `threats.warhead_mode.turret` | The light that shows a turret threat, with the warhead reticle. |
| `threats.warhead_mode.beam` | The light that shows a beam threat, with the warhead reticle. |
| `threats.warhead_mode.warhead` | The light that shows a warhead threat, with the warhead reticle. |

