# Flight settings

Three files in the mod's `flight/` folder change how the game plays in
flight:

| File | What it changes |
|-|-|
| `flight/flight.yaml` | game rules, weapons, shields, docking, lights, object counts, the camera, extra backdrop planets and the procedural suns |
| `flight/hud.yaml` | the hull and map icons of each object type, and the target display |
| `flight/sounds.yaml` | the sounds of each object type: engines, weapons, explosions, hyperspace, hangar and a few interface sounds |

The game reads the three files once, when it starts. Restart OpenXWA after
you change one of them.

Each file is optional. Without it, the game behaves as the original game.
Inside a file, every section and every key is optional, and a missing key
keeps the original game's behaviour. When the game finds a problem in a
value, it writes a warning or an error in the log. Usually it ignores only
that value. The sections below say when it ignores more. See
[Testing a mod](mod_folder.md#testing-a-mod).

Object types, weapon types and other game values are written as names. The
[reference pages](README.md#reference) list them. If an object type file
of your mod gives an object type a new name, use the new name here too.

## flight.yaml

`flight/flight.yaml` holds the settings for the whole game. It has these
sections:

| Section | What it changes |
|-|-|
| `rules` | rules that fix or change the original game's behaviour |
| `mission_settings` | the default mission settings of every mission |
| `weapons` | weapon colours, rates, stats, damage and linking |
| `shield_recharge` | how fast shields come back |
| `docking` | how craft dock and carry each other |
| `headlights` | a light on the nose of the player's craft |
| `object_limits` | how many craft, shots, explosions and radar blips a mission can have |
| `camera` | the lag and tilt of the exterior camera, the cockpit sway and the HUD in the exterior view |
| `cockpit_alert_lights` | the blinking alert lights in the cockpit |
| `particle_lights` | the lights of sparks, smoke and other effects on craft |
| `backdrops` | extra planet backdrops for missions |
| `suns` | the procedural suns and their lens flare |

If the file can't be read, for example because of a YAML error, the game
writes an error in the log and uses none of its sections.

Check the spelling of section and key names. The game ignores a section
that it doesn't know, and several sections, such as `rules`, `weapons`,
`docking` and `shield_recharge`, ignore unknown keys without a warning.

Some of these settings can also be set for one mission or for one model:

- A mission file can have its own `settings`, `object_limits` and
  `headlights`, and can pick weapon profiles for its flight groups. See
  [Missions](missions.md).
- A model settings file, `flight/models/<model>.yaml`, can set weapon
  colours, rates, stats and hardpoint kinds, shield rates, a docking
  height and HUD settings for the craft that use the model. See
  [Craft](craft.md).

This page describes the game-wide form of each setting.

### rules

Each rule changes one behaviour of the original game. Most rules fix a
problem or lift a limit. Turn on only the rules that your mod needs.

```yaml
rules:
  hangar_red_alert_any_mothership: true
  no_fire_from_destroyed_hardpoints: true
  starship_ambient_sounds: true
  weapon_iff_swap: none
```

All rules are `true` or `false` and are off by default, except
`weapon_iff_swap`, which takes a name, and `player_weapon_jamming`, which
is on by default.

| Key | Meaning |
|-|-|
| `reset_combat_music_at_mission_load` | Each mission starts with a fresh combat music state, instead of the state that the previous mission left. |
| `hangar_red_alert_any_mothership` | A departure mothership of any type can set off the hangar's red alert. In the original game, only three types of mothership can. |
| `classic_hide_destroyed_engine_glows` | Classic graphics stop drawing the engine glows and lights of a destroyed or hidden part, as modern graphics already do. This rule only changes drawing. |
| `weapon_iff_swap` | Which shots use the weapon of the firing craft's side, for example Imperial lasers instead of Rebel lasers for an Imperial craft. `turrets` (the default, as in the original game) swaps only shots from turrets. `none` swaps no shots. `all` swaps all shots. A model's `weapons.switch_by_iff` setting replaces this rule for that model. |
| `ai_attack_uses_max_speed_cache` | When an AI craft is attacked, it compares its top speed with the attacker's using the speeds that the craft's stats profiles set. The original game uses the speeds of the craft records. |
| `fire_needs_decharge_rate` | A craft fires `rebel_laser`, `imperial_laser` or `ion_cannon` only when the cannon's charge is at least the craft's decharge rate. In the original game, any charge above 0 is enough. |
| `no_starship_fire_from_destroyed_racks` | A freighter, starship, platform or satellite doesn't fire lasers or warheads from a weapon whose part is destroyed. |
| `starship_ambient_sounds` | Craft play their `starship_ambient` sound when the player's craft is near. See [sounds.yaml](#soundsyaml). |
| `no_player_warhead_slot_cap` | Each warhead launcher of a player's craft can hold more than 9 warheads, at the start and when the craft reloads in the hangar. The original game only allows this for the Missile Boat. |
| `gunner_weapon_select_in_seat` | In a gunner seat, the weapon key works, and changing seats keeps the selected weapon. In the original game, the key does nothing in a seat, and changing seats selects the lasers again. |
| `gunner_fires_selected_group` | A gunner seat fires the first turret laser group, or the first ion group when the player selected ions, if the gunner cockpit has a hardpoint for it. The original game fires all turret laser groups. |
| `gunner_warheads_need_cockpit_hardpoint` | A gunner seat fires warheads only when the gunner cockpit has a warhead hardpoint. The original game always allows it. |
| `no_fire_from_destroyed_hardpoints` | A craft doesn't fire lasers or warheads from a hardpoint on a destroyed part. |
| `tie_fighter_energy_step_always_high` | The TIE Fighter always gets its larger speed change for each step of energy moved to or from its engines. In the original game, it gets it only with spare engine power. |
| `player_turret_racks_keep_charge` | A player's shot doesn't drain the craft's turrets. In the original game, a starfighter's shot drains every weapon that fires. |
| `rearm_follows_both_statuses` | When a supply craft rearms a craft, it reads both warhead statuses of the craft's flight group. The original game only reads the first one. |
| `warhead_collision_bonus_any_target` | When a craft hits another craft, its warheads always add to the damage. In the original game, they add to it only when the target is a starship or a platform. |
| `turbolaser_hardpoint_types` | Hardpoint types `custom_1` to `custom_6` fire new weapon types. See [Hardpoint kinds](#hardpoint-kinds). |
| `gun_turret_hardpoints` | Each turret position of a starship belongs to the gun turret part that holds it, or to the nearest one. A gun turret part doesn't fire at targets behind it, and its hardpoints aren't copied to the other side of the model. Gun turret parts are parts of type `gun_turret`, `rotary_gun_turret`, `small_gun`, `weapon_system_1` or `weapon_system_2`. |
| `player_weapon_jamming` | `false` turns off the jamming of the player's weapons near enemy starships and platforms. The default is `true`. |
| `super_star_destroyer_jamming` | The Super Star Destroyer jams the player's weapons near its hangar. In the original game, it never jams. |
| `two_launcher_fixes` | Fixes for craft with two warhead launchers. The hangar menu shows the second launcher of any such craft and lists its warheads. Loading one launcher keeps both launchers counted. Each launcher gets the warhead count of the warhead it loads. A supply craft reloads the warhead that each launcher holds. The weapon key skips an empty launcher. |
| `starship_missile_defense_any_warhead` | A starship's missile defense can fire any warhead. The original game fires only missiles and advanced missiles. |
| `weapon_rack_count_from_record` | A craft's weapon count includes all laser groups and warhead launchers of its craft record. The original game counts the laser groups, plus the turrets of craft that fire from points over their hull. |
| `overlapping_fly_by_sounds` | The fly-by and engine wash sounds of several craft can play at the same time. In the original game, one of these sounds doesn't start while it is already playing. |

### mission_settings

Mission settings change things that the original game hard-codes for some
missions. A mission file sets them in its `settings` section, and
`mission_settings` in `flight.yaml` sets the default for every mission. Both
take the same keys. A key in a mission file replaces the same key of
`mission_settings` for that mission.

```yaml
mission_settings:
  hide_friendly_arrival_messages: true
  warhead_collision_damage: false
```

| Key | Meaning |
|-|-|
| `red_alert` | `true` puts the hangar on red alert. `false` keeps it off. |
| `hide_hyperspace_exit_messages` | `true` hides the message shown when a fighter leaves by hyperspace. |
| `hide_friendly_arrival_messages` | `true` hides the "entering area" messages of friendly flight groups. |
| `skip_arrival_messages_iff` | The IFF (0 to 254) whose arrival messages are skipped, or `all` or `none`. |
| `force_player_in_turret` | A mission time such as `"0:00:08"`, at which the player is put in the turret, or `false`. |
| `disable_player_lasers` | `true` stops the player from firing lasers. |
| `disable_player_warheads` | `true` stops the player from firing warheads. |
| `warhead_collision_damage` | `false` stops a craft's warheads from adding to the damage when it hits another craft. The default is `true`. |
| `damage_through_shields_on_hard` | `true` lets craft parts take damage while the shields are up, on hard difficulty too. The original game only does this on easy and medium. |
| `target_key` | Makes the target key (O) target craft of chosen flight groups. |
| `proximity_triggers_check_craft_only` | `true` makes proximity triggers ignore every object that isn't a craft. |
| `order_of_battle` | The flight groups, names and text of the order of battle in the briefing. |
| `hyperspace_abort_sound_regions` | A list of regions (0 to 31). A hyperspace arrival in one of them plays the abort sound. |

Without a value, the game keeps the original game's behaviour. The
original game puts the hangar on red alert in mission 20, hides the
hyperspace exit message in mission 49, skips the arrival messages of IFF 1
in missions 49 to 52, and puts the player in the turret at 0:00:08 in
mission 1. [Missions](missions.md) describes each key in full.

### weapons

The `weapons` section changes weapons for every craft. A model settings
file has the same `weapons` group, for the craft that use the model. For
each value, the game uses the model's value first, then the value of
`flight.yaml`, then the original game's value.

```yaml
weapons:
  impact: {rebel_laser: 0xff0000, imperial_laser: 0x00ff00}
  rates: {decharge: 4, recharge: 2, cooldown_ms: 188}
  ai_torpedoes: {per_pass: 6, per_target: 16}
  fire_ratio_by_rank: {novice: 4, officer: 3, veteran: 2, ace: 2, top_ace: 1, super_ace: 1}
  damage_by_difficulty: {easy: 0.5, medium: 0.25, hard: 1}
  hardpoint_kinds: {custom_1: laser}
```

| Key | Meaning |
|-|-|
| `impact` | The colour of the marks that each weapon type leaves on hulls. |
| `rates` | Laser charge rates and the time between shots. |
| `decharge_percent`, `recharge_percent` | Scale the laser charge rates of every craft. |
| `energy_transfer` | How the player moves energy between lasers and shields. |
| `ai_torpedoes` | How many warheads the AI fires at a big target. |
| `impact_spin` | Whether a warhead hit spins the craft that it hits. |
| `system_damage` | The system damage that ion weapons do. |
| `fire_ratio_by_rank` | How fast AI turrets and launchers fire, by rank. |
| `damage_by_difficulty` | The damage that parts take through shields, by difficulty. |
| `limit_linked_warhead_fire_rate` | Slows down linked warhead fire. |
| `linking_rules` | When the AI and the player can fire lasers and ions together. |
| `stats`, `warhead_capacity_percent`, `energy_speed_percent`, `linking` | Weapon profiles, which missions can pick for each flight group. |
| `hardpoint_kinds` | Whether each hardpoint type fires lasers, warheads or nothing. |

Weapon types are written as names, such as `rebel_laser`. See
[Weapons](reference/weapons.md) for the names.

#### Impact colours

`impact` maps weapon types to colours. A laser that hits a hull leaves a
mark of its weapon type's colour. The colour is a hex RGB number such as
`0xff0000`. Without a colour, or with `0`, the mark stays white, as in the
original game.

A model's `weapons.colors.impact` and `weapons.by_markings` settings take
priority for the shots of its craft. The model settings also set the bolt
variants and the colours of the charge bars. See [Craft](craft.md).

#### Laser rates

```yaml
weapons:
  rates: {decharge: 4, recharge: 2, cooldown_ms: 188}
  recharge_percent: 150
```

| Key | Meaning |
|-|-|
| `rates.decharge` | The charge that one shot of the player's lasers uses. The original values are 3 for the TIE Fighter and the TIE Bomber, 4 for other starfighters and 3 for other craft. |
| `rates.recharge` | The charge that comes back to each laser each game second. The original values are 3 for the TIE Fighter and the TIE Bomber, and 2 for other craft. |
| `rates.cooldown_ms` | The time that each laser fired adds before the group's next shot, in milliseconds. The original value is 188. |
| `rates.decharge_percent` | Scales the decharge rate, in percent. The default is 100. |
| `rates.recharge_percent` | Scales the recharge rate, in percent. The default is 100. |
| `decharge_percent` | Scales the decharge rate of every craft, after `rates.decharge_percent`. The default is 100. Only in `flight.yaml`. |
| `recharge_percent` | Scales the recharge rate of every craft, after `rates.recharge_percent`. The default is 100. Only in `flight.yaml`. |

All values are 0 or more. The decharge and recharge values are whole
numbers.

The game uses these rates in place of the original ones when `flight.yaml`
has `rates`, `decharge_percent` or `recharge_percent`, when the craft's
model has `rates`, or when the craft's weapon profile sets `decharge_rate`
or `recharge_rate`. Otherwise the original rules apply. Even a value of 100
in `decharge_percent` turns the rates on.

The decharge rate only applies to the player's laser shots. The racks of
the player's turrets then keep their charge, where the original game drains
them too. AI shots still use 1 charge, as in the original game. The
cooldown applies to every craft.

A weapon profile can also set the decharge rate, the recharge rate and the
cooldown of each weapon type, with `decharge_rate`, `recharge_rate` and
`fire_rate_ms`. See [Weapon profiles](#weapon-profiles). The percents still
scale the profile's values.

#### Energy transfer

`energy_transfer` changes how the player moves energy between the lasers
and the shields.

```yaml
weapons:
  energy_transfer: {rate: 4, penalty: 0, laser_limit: 100, shield_limit: 800}
```

| Key | Meaning |
|-|-|
| `rate` | The shield energy that one unit of laser charge is worth. The original value is 32 for the Missile Boat and 4 for other craft. |
| `penalty` | When the lasers charge the shields, each unit of laser charge is worth `rate` minus `penalty`, and at least 1. The default is 0. |
| `laser_limit` | The most laser charge that one key press moves. The default is 100. |
| `shield_limit` | The most shield energy that one key press moves. The default is 800. |

#### AI warheads

`ai_torpedoes` limits the warheads that an AI craft fires at a starship, a
freighter, a platform, a container, a weapon emplacement or, in newer
missions, a transport.

```yaml
weapons:
  ai_torpedoes: {per_pass: 6, per_target: 16}
```

| Key | Meaning |
|-|-|
| `per_pass` | The most warheads fired in one attack run. The default is 6. |
| `per_target` | The most warheads homing on the target at once. The default is 16. |

#### Impact spin

`impact_spin` decides whether a warhead hit spins the craft that it hits.

```yaml
weapons:
  impact_spin: {enabled: true, speed_percent: 50, angle_percent: 50}
```

| Key | Meaning |
|-|-|
| `enabled` | `true` or `false`. Without it, the game option decides, as in the original game. |
| `speed_percent` | Scales the speed of the spin, in percent. The default is 100. |
| `angle_percent` | Scales the angle of the spin, in percent. The default is 100. |

The values come from the model of the craft that is hit.

#### System damage

`system_damage` changes the damage that ion weapons do to a craft's
systems.

```yaml
weapons:
  system_damage: {max: 1000, ion_cannon: 1, ion_cannon_turbo: 2, turbolaser_ion: 4, ion_pulse: 30}
```

| Key | Meaning |
|-|-|
| `max` | The most system damage that a craft can take. The default is 1000. This value comes from the model of the craft that is hit. |
| `ion_cannon`, `ion_cannon_turbo`, `turbolaser_ion`, `ion_pulse` | The system damage of one hit of that weapon type. The defaults are 1, 2, 4 and 30. These values come from the model of the craft that fired. |

If your mod gives one of these weapon types a new name, use the new name
as the key.

With `system_damage`, or with the laser rates on, system damage stops at
`max`. In the original game, the damage stops growing once it reaches
1000, but one hit can take it past 1000.

#### Rank and difficulty

These keys change rules of the original game. They are only read from
`flight.yaml`.

| Key | Meaning |
|-|-|
| `fire_ratio_by_rank` | A factor for each rank. The time between two shots of an AI turret or launcher is multiplied by the factor of its flight group's rank. The map needs all six ranks. In the original game, the factors are 4, 3, 1, 2, 2 and 2, from `novice` to `super_ace`. |
| `damage_by_difficulty` | A factor for each difficulty. The damage that a craft's parts take while its shields are up is multiplied by the factor of the difficulty. The map needs all three difficulties. In the original game, the damage is halved on easy and divided by 4 on medium. |
| `limit_linked_warhead_fire_rate` | `true` multiplies the time between two warhead shots by the number of warheads fired together. |

The factors can have decimals. A map that misses a rank or a difficulty is
ignored. [Weapons](reference/weapons.md#ranks) lists the rank and
difficulty names.

#### Linking rules

```yaml
weapons:
  linking_rules:
    prevent_ai_lasers_ion_when_disabling: true
    prevent_ai_ion_when_not_disabling: true
    prevent_player_linked_lasers_ion: true
```

| Key | Meaning |
|-|-|
| `prevent_ai_lasers_ion_when_disabling` | The AI doesn't fire lasers while it disables its target. |
| `prevent_ai_ion_when_not_disabling` | The AI doesn't fire ions when it isn't disabling its target. |
| `prevent_player_linked_lasers_ion` | The player can't link the lasers and the ions. |

Each rule is `true` or `false`, and off by default. A `linking` weapon
profile can set the same rules for some craft.

#### Weapon profiles

A weapon profile is a named set of values. A mission picks a profile for a
flight group, so the same craft can have different weapons in different
missions or flight groups. There are four kinds of profile, each in its own
map:

| Map | What a profile sets | Key that picks it in a mission |
|-|-|-|
| `stats` | the stats of each weapon type | `weapon_profile` |
| `warhead_capacity_percent` | how many warheads a craft loads | `warhead_profile` |
| `energy_speed_percent` | the speed change for each step of engine energy | `energy_profile` |
| `linking` | the laser link mode and the linking rules | `linking_profile` |

```yaml
weapons:
  stats:
    default:
      rebel_laser: {range_m: 1600, fire_ratio_ms: 944, precise: false, spread_mask: 63}
      imperial_laser:
        fire_rate_ms: 188
        player_easy: {fire_rate_ms: 156, power: 350}
    heavy_guns:
      turbolaser_green_light: {power: 900, range_m: 5000}
  warhead_capacity_percent:
    default: {space_bomb: 50, heavy_rocket: 50}
  energy_speed_percent:
    default: {all: 12.5, player: 12}
  linking:
    default: {laser_link: 2, prevent_player_linked_lasers_ion: true}
```

Profile names have up to 47 characters, and can be written in any case.
The profile named `default` applies to flight groups that don't pick a
profile. A flight group picks a profile in its entry of the mission file.
See [Missions](missions.md).

The game looks for the picked profile in the model settings file of the
craft, then in `flight.yaml`. If neither has it, the flight group uses
`default`. A flight group that uses a profile only gets that profile's
values. A key that the profile doesn't set gets the original game's value,
not the `default` profile's value.

##### stats

Each `stats` profile maps weapon types to stats. Stats with a unit in their
name can have decimals. The other stats are whole numbers, except
`precise`, which is `true` or `false`. The units are described in
[Craft](craft.md#units). [Weapons](reference/weapons.md#weapon-types)
gives the speed, power, life and score of each weapon type in the original
game.

| Stat | Meaning |
|-|-|
| `speed_mglt` | The speed of a shot, in MGLT. A shot of a fighter or of a player's craft also gets the speed of the craft that fires it. |
| `power` | The damage of a shot, in the points of hull and shields. The shield of the side that is hit takes the damage first, and only the rest goes on to the parts and the hull. A starship or a platform takes 1/16 of the damage, and a freighter or a container 1/4. |
| `power_speed_percent` | The percent of the firing craft's speed added to the power of its shots. The original value is 100. Turret shots get no speed bonus. |
| `duration_ms` | How long a shot lives, in milliseconds, from 0 to 262140. A shot that a mine fires lives twice as long. |
| `duration_offset_ms` | How long a turret shot lives after the time it needs to reach its target, in milliseconds. The original value is 236. |
| `score` | The score that a warhead hit gives the player and the player's team. The score goes back down when the warhead runs out of time without a hit. |
| `fire_rate_ms` | Lasers: the wait after a volley, in milliseconds, for each cannon that fired, so that linked cannons wait longer. Warheads: the wait after each warhead. The original values are 188 for lasers and 1888 for warheads. |
| `fire_ratio_ms` | The wait between two shots of a turret, in milliseconds. By default, the `cooldown_ms` of the laser group in the craft record. AI turrets wait 2 to 4 times longer, by skill, or by `fire_ratio_by_rank`. |
| `range_m` | The range of a turret, in metres. By default, the `range_m` of the laser group. |
| `precise` | `false` makes the shots of AI turrets spread. In the original game, AI turrets aim exactly. A player's turret shots always spread. |
| `spread_mask` | How far turret shots spread, in angle units where 65536 is a full turn. Use a value of the form 2ⁿ − 1, such as 255, about 1.4 degrees. The original value is 255. |
| `decharge_rate` | The charge that a player's shot takes from its cannon. It replaces `rates.decharge`. A cannon holds up to 127 charge. |
| `recharge_rate` | The charge that comes back to a cannon each game second, for each step of the laser recharge setting above the middle. Below the middle, the cannons lose charge at the same rate. It replaces `rates.recharge`. |
| `high_power_charge` | When a cannon's charge reaches this value, `rebel_laser`, `imperial_laser` and `ion_cannon` fire their turbo version. The original value is 64. |

A weapon type can also have stats for the player, for one difficulty, or
for both, in the maps `player`, `easy`, `medium`, `hard`, `player_easy`,
`player_medium` and `player_hard`. For each stat, the game tries the
player and difficulty form first, then the player form, then the
difficulty form, then the plain value. For each of these, it tries the
model's profile first, then the profile in `flight.yaml`. If none sets the
stat, the original value stays.

When a laser's charge reaches `high_power_charge`, it fires the next weapon
type, its high-power type, such as `rebel_laser_turbo` after `rebel_laser`.
The laser then uses the rates of that weapon type.

##### warhead_capacity_percent

Each profile maps warhead names to the percent of a launcher's capacity
that a craft loads with that warhead. The original percents go from 25 to
100. A craft always gets at least 1 warhead. The warhead names are in
[Weapons](reference/weapons.md#warheads).

##### energy_speed_percent

Each profile sets how much speed a craft gains or loses for each step of
energy moved to or from its engines. The value is a percent of the craft's
top speed, from 0 to 100.

| Key | Meaning |
|-|-|
| `all` | The value for every craft. |
| `player` | The value for the player's craft. Without it, the player's craft uses `all`. |

The original value is 6.25 for the TIE Bomber, 18.75 for the TIE Fighter
with spare engine power, and 12.5 for other craft.

##### linking

| Key | Meaning |
|-|-|
| `laser_link` | The laser link mode of the craft: `1` fires one cannon at a time, `2` fires half of the cannons together, and `3` fires all of them together. It replaces the mode that the player picks with the link key, and the mode of AI craft. Other values make the craft fire wrongly. |
| `prevent_ai_lasers_ion_when_disabling`, `prevent_ai_ion_when_not_disabling`, `prevent_player_linked_lasers_ion` | The linking rules, as in `linking_rules`. A rule that the profile sets replaces the one of `linking_rules`. |

#### Hardpoint kinds

`hardpoint_kinds` maps hardpoint type names to the kind of weapon that
hardpoints of that type fire, `laser`, `warhead` or `none`. A model's
own `hardpoint_kinds` takes priority for its craft. A hardpoint type that
neither map lists keeps its original kind. The hardpoint type names are in
[Model values](reference/model_values.md#hardpoint-types), and the kinds in
[Weapons](reference/weapons.md#hardpoint-kinds).

In the original game, hardpoint types `custom_1` to `custom_6` fire
nothing. To use them for new turbolasers, turn on the
`turbolaser_hardpoint_types` rule and give them a kind:

```yaml
rules:
  turbolaser_hardpoint_types: true
weapons:
  hardpoint_kinds: {custom_1: laser, custom_2: laser, custom_6: laser}
```

With the rule, these hardpoint types fire these weapon types:

| Hardpoint type | Weapon type |
|-|-|
| `custom_1` | `turbolaser_red_medium` |
| `custom_2` | `turbolaser_red_heavy` |
| `custom_3` | `turbolaser_green_light` |
| `custom_4` | `turbolaser_green_medium` |
| `custom_5` | `turbolaser_green_heavy` |
| `custom_6` | `custom_weapon` |

The other hardpoint types fire the same weapon types as in the original
game.

### shield_recharge

```yaml
shield_recharge:
  starships: true
  starships_on_easy: false
  double_starfighter_shields: false
  rate_percent: 100
```

Without the section, the original rules apply. With it, the game uses
these rules, and the shield rates that the model settings files give (see
[Craft](craft.md)):

| Key | Meaning |
|-|-|
| `starships` | Starships recharge their shields. The default is `true`. |
| `starships_on_easy` | Starships and other craft that aren't starfighters also recharge on easy difficulty. The default is `false`. |
| `double_starfighter_shields` | A starfighter's shields charge up to twice their usual maximum. The default is `false`. |
| `rate_percent` | Scales every recharge rate, in percent. The default is 100. |

With the section, the recharge rate of each craft is:

- for the player's craft, its model's player rate, else its model rate,
  else 20;
- for an AI starfighter, its model rate. The AI also turns on shield
  recharge whenever the shields are below their maximum;
- for an AI starship, its model rate when `starships` is on and the
  difficulty isn't easy or `starships_on_easy` is on, else 0;
- for other AI craft, their model rate. The AI only turns on their shield
  recharge when the difficulty isn't easy or `starships_on_easy` is on.

A model without shield settings has a rate of 20 for starfighters, 5 for
each live shield generator on the Star Destroyers, the Interdictor and the
Super Star Destroyer, 5 for other starships, and 0 for other craft.

### docking

```yaml
docking:
  dock_to_small_sets_big: true
  ai_offsets_own_dock_point: true
  carried_offsets_carrier_dock_point: true
  player_approach_height: true
  player_snaps_to_target_orientation: true
```

Each key is `true` or `false`, and `false` by default, which keeps the
original rule.

| Key | Meaning |
|-|-|
| `dock_to_small_sets_big` | A model's `dock_to_small` hardpoint also sets its `dock_to_big` point. A `dock_to_big` hardpoint that comes after it still sets its own point. |
| `ai_offsets_own_dock_point` | An AI craft that boards or picks up a target lines up its own dock point with the target's. In the original game, it only uses the target's dock point. |
| `carried_offsets_carrier_dock_point` | A carried craft sits at the carrier's dock point, less its own. In the original game, only its own dock point counts. |
| `player_approach_height` | The player's craft approaches its docking target at a height that the target's model limits. |
| `player_snaps_to_target_orientation` | Once docked, the player's craft turns to match the target's heading, pitch and roll. |

With `player_approach_height`, the approach height is the smaller of the
gap between the target's `dock_from_big` point and the player's
`dock_to_big` point, and a limit from the target's model. A
model sets the limit with `docking.approach_height_m` in its settings file
(see [Craft](craft.md)). Without it, the limit is 0 for `space_colony2`
and 22.75 m for other models. The original game approaches at twice the gap,
and not at all for `space_colony2`.

### headlights

Headlights are a light along the nose of the player's craft, for dark
places such as the Death Star.

```yaml
headlights:
  position_m: [0, 0, 20]
  color: [0.944, 0.944, 1]
  power_w: 1314756
  angle_degrees: 25
  auto_turn_on: true
```

| Key | Meaning |
|-|-|
| `position_m` | Where the light is, in metres from the pilot's eye: X right, Y up, Z forward. The default is `[0, 0, 20]`. |
| `color` | The light's colour, linear. The default is `[1, 1, 1]`. |
| `power_w` | Required. The light's power in watts, above 0, as for a Blender area light. |
| `angle_degrees` | Makes the light a spot light that ends at this angle from the nose, above 0 and at most 90. Without it, the light shines forward over a half sphere and fades towards the sides. |
| `auto_turn_on` | `true` (the default) turns the headlights on in missions that have no directional light. |

The light always points along the nose. Without the section, there are no
headlights. If a value is wrong, the game writes an error in the log and
there are no headlights.

A mission file can turn the headlights on with `headlights: true`. They are
then on while the player isn't in the hangar, whatever `auto_turn_on` says.
See [Missions](missions.md).

### object_limits

`object_limits` raises the number of objects that a mission can have.

```yaml
object_limits:
  craft_per_region: 3072
  player_projectiles: 128
  ai_projectiles_per_region: 1024
  explosions_per_region: 256
  radar_blips: 512
```

| Key | Meaning | Default |
|-|-|-|
| `craft_per_region` | The craft slots of each region. | 96 |
| `player_projectiles` | The shot slots of each player. Warheads use the slots from the ninth one on. | 12 |
| `ai_projectiles_per_region` | The shot slots of the AI in each region. | 64 |
| `explosions_per_region` | The explosion slots of each region. Each one comes with a debris slot. | 32 |
| `radar_blips` | The blips on each radar scope, at most 1024. | 48 |

Each value is a whole number from 1 up. A mission file can have the same
section. Its keys replace those of `flight.yaml` for that mission.

All the slots must fit in the game's table of 32768 objects. When they
don't, the game lowers the counts and writes a warning in the log. It
lowers the craft first, down to 96, then the AI's shots, down to 64, then
the explosions, down to 32, then the player's shots, down to 12. Missions
with many regions or players have less room.

The proving grounds keep their 36 craft slots, unless the mod has hangar
settings. Then they use `craft_per_region` like the other missions.

### camera

The `camera` section sets the enhanced camera. With it, the exterior
camera trails the craft's turns and looks at it from above, the cockpit
view sways with turns and speed changes, and the HUD shows in the exterior
view.

```yaml
camera:
  exterior_tilt: 10.0
  cockpit_sway: false
```

| Key | Meaning | Range | Default |
|-|-|-|-|
| `enabled` | Turns the enhanced camera on. | `true` or `false` | `true` |
| `exterior_lag` | The exterior camera trails the craft's turns. | `true` or `false` | `true` |
| `exterior_lag_strength` | How far the exterior camera turns for each radian of lag, in radians. | 0 to 2 | 0.47 |
| `exterior_lag_max` | The most that the exterior camera turns, in degrees. | 0 to 15 | 4.05 |
| `exterior_tilt` | How far above the craft the exterior camera looks from, in degrees. | -30 to 30 | 10 |
| `cockpit_sway` | The cockpit view sways with turns and speed changes. | `true` or `false` | `true` |
| `cockpit_sway_strength_m` | How far the eye moves for each radian of lag, in metres. | 0 to 0.5 | 0.122 |
| `cockpit_speed_sway_m` | How far the eye moves back for each MGLT of speed change, in metres. | 0 to 0.05 | 0.0041 |
| `cockpit_sway_max_m` | The most that the eye moves on each axis, in metres. | 0 to 0.25 | 0.061 |
| `lag_time` | How fast the lag and the sway catch up, in seconds. | 0.05 to 2 | 0.32 |
| `exterior_hud` | Shows the HUD and the reticle in the exterior view of the player's craft, in modern graphics. | `true` or `false` | `true` |

Without the section, the enhanced camera is off unless the player turns on
**Enhanced Camera** in the options. With the section, it is on, unless the
section has `enabled: false`. A missing key keeps its default.

The player's choices win over the mod. If the player turns **Enhanced
Camera** on or off in the options, that choice counts.

### cockpit_alert_lights

The game blinks six coloured lights in the cockpit, at the pilot's eye.
This section replaces those lights in modern graphics. The game still
decides when each light is on and how it blinks.

```yaml
cockpit_alert_lights:
  hull_critical:
    position_m: [0, 0.3, 0.6]
    color: [1, 0, 0]
    power_w: 20
  warhead_lock:
    position_m: [0, 0.3, 0.6]
    power_w: 15
```

The lights are:

| Light | When it blinks |
|-|-|
| `hull_critical` | The shields are down and less than a third of the hull is left. |
| `warhead_lock` | An enemy locks a warhead on the craft. |
| `beam_warning` | A tractor beam or a jamming beam targets the craft. |
| `hyperspace_jump` | The jump to hyperspace starts. |
| `hyperspace_transfer` | The jump between regions. |
| `hyperspace_arrival` | The craft leaves hyperspace. |

Each light has these keys:

| Key | Meaning |
|-|-|
| `position_m` | Where the light is, in metres from the pilot's eye: X right, Y up, Z forward. The default is the eye. |
| `color` | The light's colour, linear, with no value below 0. The default is the game's colour. |
| `power_w` | Required. The light's power in watts, above 0, as for a Blender point light. |

A light that the section doesn't list keeps the game's light. A light with
a wrong value also keeps the game's light, and the game writes an error in
the log.

### particle_lights

The game gives the particle effects on objects, such as sparks and smoke,
a yellow-white light at their emitter. This section replaces those lights
in modern graphics, by group.

```yaml
particle_lights:
  cockpit_sparks:
    color: [1, 1, 0.522]
    power_w: 40
    fade: true
  craft_damage:
    power_w: 0
```

The groups are:

| Group | What it lights |
|-|-|
| `cockpit_sparks` | Sparks from a hit through the shields. |
| `cockpit_smoke` | Smoke from a hit through the shields. |
| `craft_damage` | The other effects on objects. |

Each group has these keys:

| Key | Meaning |
|-|-|
| `color` | The light's colour, linear, with no value below 0. The default is the game's colour. |
| `power_w` | Required. The light's power in watts, 0 or more, as for a Blender point light. 0 gives no light. |
| `fade` | `true` makes the light dim to nothing over the effect's life. Without it, the light stays at full strength until the effect ends, as in the original game. |

A group that the section doesn't list keeps the game's light. A group with
a wrong value also keeps the game's light, and the game writes an error in
the log.

### backdrops

A backdrop flight group in a mission has a backdrop number. The original
game knows numbers 0 to 103. `backdrops.extra_planets` makes numbers 104
to 255 draw planets, each with its own image group.

```yaml
backdrops:
  extra_planets:
    - { ids: [104, 255], image_groups: [6304, 6455] }
```

`extra_planets` is a list. Each entry has:

| Key | Meaning |
|-|-|
| `ids` | A range of backdrop numbers, `[first, last]`, from 104 to 255. |
| `image_groups` | A range of image groups, `[first, last]`, with as many groups as `ids` has numbers. The first number uses the first group, the second number the second group, and so on. |

Each extra planet is drawn like planet backdrop 1 of the original game,
with the images of its group. Make the images of each group with an atlas
file in `resdata/atlas/`. See [Images](images.md) and
[Image groups](reference/image_groups.md).

An entry with a wrong range is ignored, with an error in the log. So is an
entry whose numbers overlap an earlier entry. Without the section, backdrop
numbers above 103 show nothing, as in the original game.

### suns

The `suns` section sets the procedural suns and their lens flare:

```yaml
suns:
  procedural_sun: true
  procedural_flare: true
  flare_intensity: 3
```

| Key | Meaning |
|-|-|
| `procedural_sun` | Backdrop suns draw a procedural disc and corona in place of their image. |
| `procedural_flare` | The sun that casts the shadows draws a procedural lens flare in place of the classic flare images. |
| `flare_intensity` | The flare's strength, from 0 to 10. |
| `brightness` | The brightness of the sun's disc and rays, from 0 to 100. |
| `core_brightness` | The light added in the sun's core, from 0 to 1000. |
| `flare_elements` | The shapes of the procedural flare. A list here replaces the whole default list. |

[Environments](environments.md) describes these keys and the flare shapes.

## hud.yaml

`flight/hud.yaml` changes the icons of each object type and the target
display.

```yaml
hull_icons:
  xwing: 1
  ywing: none
map_icons:
  xwing: 1
target_display:
  style: shaded
```

| Key | Meaning |
|-|-|
| `hull_icons` | The craft outline in the shield and hull display, by object type. |
| `map_icons` | The icon of each object type on the map. |
| `target_display` | How the target display shows the target. |

If the file can't be read, the game writes an error in the log and ignores
the whole file.

The HUD settings of one craft, such as its reticle and its HUD colours, are
in its model settings file. See [Craft](craft.md).

### Hull and map icons

`hull_icons` and `map_icons` map object type names to image numbers. The
number counts from 1 and goes up to 655. Hull icons come from image group
26000 and map icons from image group 14800. `none` gives the object type no
icon. An object type that the map doesn't list keeps its original icon.

To add new icons, replace the image group with an atlas that has more
images. See [Images](images.md) and
[Image groups](reference/image_groups.md).

### Target display

`target_display` sets how the target display draws the target.

`style: shaded` shows the lit model. This is the default.

`style: edges` shows only the model's edges, in the colour of the target's
IFF:

```yaml
target_display:
  style: edges
  edges:
    luminance: [0.33, 0.5, 0.16]
    contrast: 3
    colors: iff
    iff: [[0.01, 0.45, 0.01], [0.21, 0.01, 0.01], [0.01, 0.01, 0.21],
          [0.07, 0.03, 0.01], [0.07, 0.01, 0.01], [0.21, 0.01, 0.21]]
    friend: [0.01, 0.32, 0.01]
    foe: [0.32, 0.01, 0.01]
    other: [0.01, 0.01, 0.21]
```

With `edges`, every key of `edges` is required:

| Key | Meaning |
|-|-|
| `luminance` | The weights of red, green and blue when the game turns the shaded display into brightness, in sRGB values. |
| `contrast` | Scales the strength of the edges. 0 or more. |
| `colors` | `iff` colours the edges by the target's IFF. `friend_foe` colours them as friend or foe. |
| `iff` | Six colours, for IFF 0 to 5. |
| `friend` | The colour of a friendly target, with `colors: friend_foe`. |
| `foe` | The colour of an enemy target, with `colors: friend_foe`. |
| `other` | The colour of a target that has no IFF colour. |

Colours are linear, three numbers of 0 or more. With `edges`, a 6-pixel
border of the display stays empty, and the display is empty while the
target is dying. If a key is missing or wrong, the game writes an error in
the log and the display stays shaded.

## sounds.yaml

`flight/sounds.yaml` gives object types their own sounds, such as engine,
weapon and explosion sounds. The sounds are grouped in banks. Each bank
plays at one moment of the game, such as when a craft fires or when the
player's craft enters hyperspace. [Sound banks](reference/sound_banks.md)
lists the banks and when each one plays.

```yaml
banks:
  interior:
    sounds:
      xwing: {file: 'Wave/Custom/XwingEngine.wav', volume: 76, distance_m: 200}
      awing: {silent: true}
  weapon:
    sounds:
      xwing:
        rebel_laser: {file: 'Wave/Custom/XwingLaser.wav', volume: 70}
        turbolaser_red: {file: 'Wave/Custom/RedTurbolaser.wav'}
  hangar_ambient:
    sounds:
      imperial_star_destroyer2:
        - {file: 'Wave/Custom/HangarHum1.wav'}
        - {file: 'Wave/Custom/HangarHum2.wav'}
  sounds:
    sounds:
      map_exit: {file: 'Wave/Custom/MapExit.wav', volume: 80}
  weapon_range:
    sounds:
      isd_turbolaser_near: {file: 'Wave/Custom/IsdNear.wav', distance_m: 50}
```

The sound files in these examples are new files in the mod's
`game/WAVE/Custom/` folder. See [Entries](#entries).

The file needs a `banks` map. Each key of `banks` is a bank name, and each
bank has these keys:

| Key | Meaning |
|-|-|
| `sounds` | The bank's entries. |
| `unlisted` | What plays for an entry that the bank doesn't list. `stock` (the default) plays the original game's sound. `silent` plays nothing. |

A bank that the file doesn't have plays the original game's sounds.

If the file can't be read, or has no `banks` map, the game writes an error
in the log and ignores the whole file.

### Entries

An entry is a sound or a silent entry:

```yaml
xwing: {file: 'Wave/Custom/XwingEngine.wav', volume: 76, distance_m: 200}
awing: {silent: true}
```

| Key | Meaning |
|-|-|
| `file` | The sound file, as a path in the game folder, such as `Wave/Custom/XwingLaser.wav`. |
| `volume` | From 0 to 255. The default, 127, plays the file at its own level, and lower values play it quieter. The game never plays a sound louder than its file. Values above 127 keep a sound at full level when the player's volume settings or the distance would make it quieter. |
| `distance_m` | How far the sound carries, in metres, from 0 to 1599.98. The default is 200. See below. |
| `silent` | `true` makes the entry play nothing. A silent entry has no `file`. |

The sounds of objects fade with the distance from the player. With 3D sound
off, which is the game's default, a sound fades to a quarter of its volume
at `distance_m`, plays at an eighth from twice `distance_m`, and is silent
from four times `distance_m`. With 3D sound on, it is silent from twice
`distance_m`.

To add a new sound, put it in the mod's `game/` folder under `WAVE/`,
such as `game/WAVE/Custom/Alarm.wav`, and name it `Wave/Custom/Alarm.wav`.
See [Replacing original game files](mod_folder.md#replacing-original-game-files).

A silent entry is different from an entry that the bank doesn't list. A
silent entry always plays nothing. An unlisted entry plays what `unlisted`
says.

The banks can hold 16384 entries in all. Silent entries count, and so does
each bank with `unlisted: silent`.

### How each bank is keyed

| Banks | Keys of `sounds` | Value |
|-|-|-|
| `canopy_opening`, `engine_slowing`, `engine_wash`, `fly_by`, `hyper_abort`, `hyper_end`, `hyper_start`, `hyper_zoom`, `interior`, `s_foil`, `shuttle_blast_off`, `shuttle_shut_down`, `shuttle_take_off`, `starship_ambient`, `take_off` | object type | an entry |
| `weapon` | object type of the craft that fires | a map from weapon sound group to entry |
| `hangar_ambient` | object type of the hangar's command ship | a list of 1 to 10 entries |
| `weapon_explosion` | object type of the craft that fired the weapon that explodes | a list of 1 to 8 entries |
| `sounds` | `player_targeted`, `map_enter`, `map_exit`, `turret_switch` or `target_select` | an entry |
| `weapon_range` | names that you choose | an entry |

In the `weapon` bank, the sound of each shot depends on the craft that
fires and on the weapon sound group of the shot. For example, the
`turbolaser_red` group also plays for the light, medium and heavy red
turbolasers. [Weapons](reference/weapons.md#weapon-sound-groups) lists the
groups.

In the list banks, the game picks one entry at random each time. In
`weapon_explosion`, a sound doesn't repeat until half of the list has
played. An entry with an error is left out of its list.

The `weapon_range` bank holds sounds with names that you choose. Names use
lower-case letters, digits and `_`. The bank doesn't use `unlisted`. A
model's `sound.weapon_ranges` setting lists some of these names for a
weapon type. Each shot of that weapon type from the model's craft then
plays one of them, chosen at random. See [Craft](craft.md).

For the cockpit, fly-by, engine wash, take-off, hyperspace and weapon
sounds, a model's `sound` settings take priority over the banks. See
[Craft](craft.md).

The `starship_ambient` bank only plays with the `starship_ambient_sounds`
rule in `flight.yaml`.
