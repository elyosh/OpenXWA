# Craft

The three kinds of files work together:

- An **object type** is a kind of object in flight, such as the X-wing, a
  laser bolt or a mine. It has a model, a genus, a family, flags and a
  **craft record**, which holds its flight data: speed, hull, shields,
  weapons and more. Object type files are in `flight/object_types/`.
- A **craft type** is the number that a mission gives a flight group. Each
  craft type is also a line of the ship list and an entry of the tech room,
  and it points to an object type. Craft type files are in
  `flight/craft_types/`.
- A **model** is a `.glb` file in `flight/models/`. Its settings file has the
  same name with `.yaml`, and holds the settings that belong to the model:
  HUD, weapon colours and stats, sounds, gunner seats and more. The
  [models page](models.md) describes the `.glb` itself.

All three only apply when model replacements are on, which is the default.
Without these files, every craft is the craft of the original game.

## Units

Lengths and ranges are in metres. Most other values use the game's own
units, so that they match the values that the community tools show.

| Unit | Meaning |
|-|-|
| metres | Lengths and ranges. The game measures them in game units, 40.96 to the metre, and rounds a value in metres to the nearest game unit. |
| ticks | Times. A tick is 4 ms. The game counts its own seconds, such as the mission clock, as 236 ticks, which is 0.944 real seconds. |
| speed units | Speeds of craft and shots. One MGLT is 2.25 speed units, and a craft at 1 MGLT moves about 1 metre in one game second. A top speed of 225 is 100 MGLT. |
| angle units | Turn rates. 65536 is a full turn, so 16384 is 90 degrees. |
| points | Hull, shields and the damage of shots all use the same points. |

"Per second" below means per game second of 236 ticks.

Keys that use metres, degrees, MGLT or percent say so in their name, such
as `position_m` or `min_speed_mglt`.

## Object type files

`flight/object_types/<name>.yaml` changes one object type, or fills a free
slot with a new one. The file name only appears in the log. You can give
each file the name of its object type.

This file changes the A-wing's acceleration and hull, and gives it a new
name:

```yaml
object_type: awing
name: a_wing_interceptor
craft_record:
  acceleration: 47
  hull: 1470
names: {long: 'A-Wing', plural: 'A-Wings', gender: m}
beam_allowed: true
```

| Key | Meaning |
|-|-|
| `object_type` | Required. The original name of the object type, such as `awing` or `slot_35`. The [object types](reference/object_types.md) page lists them. |
| `name` | A new name for the object type. See [Renaming an object type](#renaming-an-object-type). |
| `model` | The model that the object type loads: `flight/models/<model>.glb`. |
| `list` | The model list that the model goes in: `spacecraft`, the default, or `equipment`. |
| `genus` | The genus, such as `fighter` or `starship`. |
| `family` | The family, such as `craft` or `weapon`. |
| `flags` | The list of flags that are on. |
| `craft_record` | Changes to the craft record. See [The craft record](#the-craft-record). |
| `names` | The names that the game shows for the craft. See [Craft names](#craft-names). |
| `sounds_like` | Another object type whose stock engine and launch sounds this one uses. |
| `beam_allowed` | `true` or `false`: whether the craft keeps the beam that its flight group gives it. |

Every key except `object_type` is optional. A missing key keeps the game's
value. A key that the file can't have is ignored, and the log says so.

When two files change the same object type, the game ignores both files.

The game reads the object type files once, when it starts, before the other
files of the mod.

### Renaming an object type

Each object type has a name, such as `xwing`, or `slot_33` for a free slot.
`name` gives the object type a new name. Use it when you put another craft
in a slot, so that the name says what the craft is.

After a rename, the rest of the mod must use the new name: other object
type files, craft type files, mission files, `sounds.yaml`, `hud.yaml` and
the model settings files. The original name no longer works there. Only the
`object_type` key of the object type file itself always uses the original
name.

A name can have lower-case letters, digits and `_`, and at most 63
characters. Names must be unique. When another object type already has the
name, or two files use the same name, the game ignores that rename, and the
log says so.

### Models and lists

`model` gives the object type a model of its own. The value is the name of a
`.glb` file in `flight/models/`, without the extension. It can have letters,
digits, `_` and `-`, and the game turns it into lower case. The `.glb` must
exist, or the game ignores the whole file.

Without `model`, the object type keeps the model of the original game. The
**Model** column of the [object types](reference/object_types.md) page gives
it. Several object types can share one original model, such as `xwing` and
`xwing_30`. When one of them gets a `model`, the others keep the original
model.

`list` only matters with `model`. Use the list that the **List** column
gives for object types of the same kind: `equipment` for weapon types, mines,
buoys, pilots and the other object types in that list, and `spacecraft` for
the others.

### Filling a free slot

The object types named `slot_<number>` are free slots. They load nothing in
the original game. To add a craft, fill a free slot:

- give it a `model`, which a free slot needs, or the game ignores the file;
- give it a `name`;
- set its `craft_record`, `genus` and `family`;
- add a craft type file that points to it, so that missions, the ship list
  and the tech room can use it.

The free fighter slots, `slot_33` to `slot_42`, start with flight data
like the X-wing's, but with no lasers, no warheads and no cockpit name.

The game also uses free slots for the cockpits of gunner seats. Each cockpit
model that a `gunner_seats` group names takes one free slot. Don't fill every
free slot when your mod has gunner seats.

### Genus, family and flags

`genus` and `family` take the names of the
[genus](reference/object_types.md#genus) and
[family](reference/object_types.md#family) tables.

`flags` is the list of [flags](reference/object_types.md#flags) that are on.
The flags that the list leaves out are off. Without `flags`, the object type
keeps the flags of the original game. A file can only set
`player_targetable`, `ai_targetable`, `nav_buoy`, `static_object`,
`float_hardpoints`, `spins` and `collision_face_groups`. When the list has
another name, the game ignores the whole `flags` key.

```yaml
flags: [player_targetable, ai_targetable, float_hardpoints]
```

### The craft record

`craft_record` changes the flight data of the object type. Each key is
optional. A key that the map leaves out keeps the game's value.

```yaml
craft_record:
  speed: 198
  acceleration: 25
  deceleration: 29
  roll_rate: 8448
  pitch_rate: 3328
  hull: 6930
  shields: 5000
  lasers:
    - {type: rebel_laser, count: 2, kind: laser_cannon}
    - {type: ion_cannon, count: 2, kind: ion_cannon}
  warheads:
    - {type: torpedo, count: 2, capacity: 6}
  cockpit_name: 'UWing'
```

Some object types share one craft record. In the original game,
`mod_strike_cruiser` and `golan_two` share one. When a file changes the
`craft_record` or the `names` of such an object type, the game gives it a
craft record of its own, which starts as a copy of the shared one. The other
object types keep the shared record. The log says when this happens. An
object type without a craft record, such as a weapon type, can't have
`craft_record` or `names`: the game ignores the file.

These keys take one value. The X-wing column gives the values of the
original X-wing, to compare with.

| Key | Values | X-wing | Meaning |
|-|-|-|-|
| `speed` | 0 to 65535 | 225 | The top speed, in speed units. 225 is 100 MGLT. |
| `acceleration` | 0 to 65535 | 36 | How fast the craft speeds up, in speed units per second. The craft gets a quarter of it at zero throttle and all of it at full throttle. |
| `deceleration` | 0 to 65535 | 60 | How fast the craft slows down, in speed units per second. The craft gets all of it at zero throttle and a quarter of it at full throttle. |
| `yaw_rate` | -32768 to 32767 | 10240 | How fast an AI craft turns left and right, in angle units per second. |
| `pitch_rate` | -32768 to 32767 | 5120 | How fast the craft turns up and down, in angle units per second. |
| `roll_rate` | -32768 to 32767 | 9216 | How fast the craft rolls, in angle units per second. When a player flies the craft, it also sets how fast it turns left and right. |
| `auto_bank` | 0 to 65535 | 49152 | How much the craft rolls when it turns left or right. 65536 would be as much as it turns. |
| `max_tumble_angle` | 0 to 65535 | 24575 | Limits how fast the craft spins when it breaks apart. |
| `max_push_rate` | 0 to 65535 | 600 | How fast an AI craft can slide sideways, for example when it dodges. |
| `hull` | whole number | 2100 | The hull points. The craft is destroyed when the damage reaches them. |
| `shields` | whole number | 2500 | The shield points of each side, front and back. A side can charge up to twice this value. |
| `has_shields` | `true` or `false` | `true` | Whether the craft has shields. |
| `has_hyperdrive` | `true` or `false` | `true` | Whether the craft has a hyperdrive. |
| `system_strength` | 0 to 65535 | 10 | How much system damage, such as ion damage, the craft takes before its systems fail. |
| `system_damage_hull_threshold` | whole number | 700 | Once the hull damage reaches this value, hits can knock out the craft's systems. |
| `component_max_hp` | 0 to 255 | 255 | The full health of each engine. |
| `rating_weight` | 0 to 65535 | 10 | How much a kill of this craft counts for the pilot rating. A kill gives more points when this value is high and the weight of the player's craft is low. With 0, a kill gives no rating points. |
| `laser_converge_mode` | 0 to 255 | 2 | What the laser convergence key does. 0: nothing, and the lasers don't converge. 1: it steps through off and three levels, and the craft starts at the third level. 2: it turns convergence on and off, and the craft starts with it on. |
| `countermeasures` | 0 to 255 | 10 | The number of countermeasures that the craft carries. |
| `gunsight_offset_y` | whole number | -76 | Moves the centre of the cockpit view, and the reticle with it, up or down, in pixels of the game's flight view. A negative value moves it up. |
| `ai_hits_to_evade` | 0 to 255 | 4 | How many hits an AI craft takes during an attack before it reacts to the craft that hits it. It reacts sooner when its shields are low. |

A value out of range, or of the wrong kind, is ignored, and the log says so.

#### Lasers

`lasers` is a list of up to 3 laser groups. Each group is a map:

| Key | Meaning |
|-|-|
| `type` | The [weapon type](reference/weapons.md#weapon-types) that the group fires, such as `rebel_laser`, or `none` for an empty group. |
| `count` | The number of cannons in the group, from 0 to 255. |
| `kind` | The [laser group kind](reference/weapons.md#laser-group-kinds): `laser_cannon`, `ion_cannon`, `turret` or `long_range_turret`. |
| `range_m` | The range of the group's turrets, in metres. 0 uses the default: 2000 m, or twice that for `long_range_turret`. |
| `cooldown` | The time between two shots of the group's turrets, in ticks. 0 uses the default: 118 for `turret`, and 236 for the other kinds. AI craft wait two to four times longer, by skill. |

`lasers` replaces every group. A group that the list leaves out is emptied,
and a key that a group leaves out is 0. So list every group the craft has,
in order.

When a group is empty, the model's weapon hardpoints can add their weapon
to it. The [model values](reference/model_values.md#hardpoint-types) page
explains how.

#### Warheads

`warheads` is a list of up to 2 warhead groups. Each group is a map:

| Key | Meaning |
|-|-|
| `type` | The weapon type that the launchers fire, such as `torpedo` or `missile`, or `none` for an empty group. |
| `count` | The number of launchers, from 0 to 255. |
| `capacity` | The most warheads that each launcher holds, from 0 to 255. A mission loads a percent of it, which depends on the warhead that the flight group carries. |

As with `lasers`, the list replaces every group, and a missing key is 0.

#### Turrets

`turrets` is a list of up to 2 gunner seats. Each seat takes the keys of a
gunner seat, as described in [gunner_seats](#gunner_seats), except `cockpit`
and `inverted`. Instead of `cockpit`, `object_type` names the object type of
the seat's cockpit, such as `corellian_transport_gunner`.

```yaml
craft_record:
  turrets:
    - {position_m: [0.464, 0, 6.104], mount_pitch_degrees: 90,
       mount_yaw_degrees: -90, aim_limit_pitch_degrees: 66.138,
       aim_limit_yaw_degrees: 95.076,
       object_type: corellian_transport_gunner}
```

A seat that the list leaves out is emptied. The `gunner_seats` group of the
model settings file replaces these seats, and it can use any cockpit model,
so it is usually the better place for seats.

#### The cockpit

`cockpit_eye_m` is the position of the pilot's eye, as `[right, forward,
up]` in metres. When the model has a `pilot_eye` hardpoint, the hardpoint
gives the eye's position instead. The
[models page](models.md#the-pilots-eye) describes it.

`cockpit_name` names the cockpit models of the craft. The game loads
`flight/models/<name>cockpit.glb` as the cockpit, and
`flight/models/<name>exterior.glb` as the craft's model in the exterior view
of the player's craft, when the mod has it. The name has no folder and no
extension, and the game ignores case. For example, `cockpit_name: 'UWing'`
loads `uwingcockpit.glb`. When the cockpit model is missing, the game uses a
cockpit of the original game.

`mesh_attach` is a list of 11 whole numbers that the importer copies from
the original craft table. They hold points such as the dock points. When the
game loads the model, the model's docking hardpoints replace them. You don't
need this key for a new model.

### Craft names

`names` sets the names that the game shows for the craft in flight:

```yaml
names: {short: 'U-W', long: 'U-Wing', plural: 'U-Wings', gender: m}
```

| Key | Meaning |
|-|-|
| `short` | The short name, such as `U-W`. |
| `long` | The full name. |
| `plural` | The plural, used by the mission goals. |
| `gender` | The grammatical gender of the name, for the mission goals: `m`, `f` or `n`. |

Each key is optional. Write the names in quotes. A name can have at most
255 characters, and only characters of Latin-1, the game's character set.

`f` and `n` need goal texts for that gender in the game's text. The English
game has none, so the game ignores them there, and the log says so.

Only the craft records 0 to 217 have names of their own. When a split
record is above 217, the game ignores `names`, and the log says so.

### Sounds and the beam

`sounds_like` names another object type. When neither the model's `sound`
group nor the sound banks choose a sound, the game plays the stock engine,
fly-by and launch sounds of that object type. That object type must use its
own sounds. A chain, where it also has a `sounds_like`, is ignored.

```yaml
sounds_like: tie_fighter
```

`beam_allowed: false` takes away the beam that a mission gives the craft's
flight group. Without the key, the X-wing, Y-wing, A-wing, B-wing, Z-95 and
TIE Fighter lose their beam, and the other object types keep it.

## Craft type files

`flight/craft_types/<name>.yaml` changes one craft type: its object type,
its line in the ship list and its entry in the tech room. The file name only
appears in the log.

```yaml
craft_type: 186
object_type: u_wing_starfighter
ship_list: {name: 'U-Wing Starfighter', category: light_transport,
            flyable: true, known: true, skirmish: true, hidden: false,
            icon: [39, 448, 66, 470]}
tech_room: {name: 'UT-60D U-Wing Starfighter', manufacturer: 'Incom Corporation',
            side: 'Rebel Alliance', crew: '2, Passengers: 8',
            description: 'A light gunship and troop transport.'}
```

| Key | Meaning |
|-|-|
| `craft_type` | Required. The number of the craft type, from 1 to 231. The [craft types](reference/craft_types.md) page lists them. |
| `object_type` | The object type that the craft type uses. Without it, the craft type keeps its object type. |
| `ship_list` | Changes to the ship list line. |
| `tech_room` | The texts of the tech room entry. |

Craft types have numbers, not names. The ship list name is the only name a
craft type has.

The object type must load a model. A free slot loads one only when an
object type file fills it. When the object type loads nothing, the game
ignores the file. This also applies to the craft types whose object type is
`none`: give them an `object_type`.

When two files change the same craft type, the game ignores both files.

### The ship list

`ship_list` changes the craft type's line in the ship list. A key that the
map leaves out keeps the value of the game's line.

| Key | Meaning |
|-|-|
| `name` | The name in the ship list, in quotes. It can't have a comma. |
| `category` | The [ship list category](reference/craft_types.md#ship-list-categories), such as `fighter` or `starship`. |
| `flyable` | `true` when a player can fly the craft in skirmish missions, `false` when not, or `gunner`, which also gives the craft a gunner slot in multiplayer skirmish missions. |
| `known` | `true`: a new pilot already knows the craft, so the tech room shows it. Pilots made before the change don't. |
| `skirmish` | `true`: the craft is in the list of craft for skirmish missions. |
| `hidden` | `true`: the craft isn't in the ship list at all. Missions can still use it. |
| `icon` | The craft's icon in the map icon images of the frontend, as `[left, top, right, bottom]` in pixels, from 0 to 4096. When those images have an atlas with a region for this craft type, the region is used instead. See [images](images.md). |

Many unused craft types have the line `*DontUseMe`, with the category
Planet/asteroid. The `*` hides the line, and the game skips that category.
To use such a craft type, set `name`, `category` and `hidden: false`.

The whole line must fit in 255 characters.

### The tech room

`tech_room` sets the texts of the craft's tech room entry. Each key is
optional, and a key that the map leaves out keeps the game's text.

| Key | Most characters | Meaning |
|-|-|-|
| `name` | 63 | The full name of the craft. |
| `manufacturer` | 63 | Who builds it. |
| `side` | 63 | Who uses it. |
| `description` | 255 | The description. |
| `crew` | 63 | The crew. |

Write the texts in quotes, with only Latin-1 characters.

The ship list line and the tech room texts replace the texts of the same
craft type in `text/strings.yaml`. The log says when this happens.

## Model settings files

`flight/models/<model>.yaml` holds the settings of one model. `<model>` is
the model's name, in lower case, as in `flight/models/<model>.glb`.

```yaml
hud:
  colors: {inner: 0xff66667f, border: 0xff19197f}
engine_glows:
  0: {min_speed_percent: 1}
  1: {min_speed_percent: 1}
weapons:
  colors:
    impact: {rebel_laser: 0xff0000, ion_cannon: 0x3333ff}
cockpit_view: {wide_vertical_fov: 58.95}
```

The file has one group for each subject. Every group is optional, and every
key in a group is optional. Without the file, or without a group, the craft
keeps the game's values for that subject.

| Group | What it sets |
|-|-|
| [`hud`](#hud) | the reticle and the HUD colours |
| [`engine_glows`](#engine_glows) | engine glows that show by speed |
| [`surfaces`](#surfaces) | tagged materials and lights that show by the craft's state |
| [`weapons`](#weapons) | weapon colours, laser rates, weapon stats and other weapon values |
| [`stats_profiles`](#stats_profiles) | speeds, rates, hull and shields, by mission, difficulty and player |
| [`shield`](#shield) | shield recharge |
| [`sound`](#sound) | the craft's sounds |
| [`component_strength`](#component_strength) | the strength of the model's parts, by mesh type |
| [`gunner_seats`](#gunner_seats) | gunner seats |
| [`warhead_lock`](#warhead_lock) | warhead lock ranges and times |
| [`jamming`](#jamming) | where the craft jams the player's weapons |
| [`docking`](#docking) | the approach height when the player docks |
| [`cockpit_view`](#cockpit_view) | the wide field of view of the cockpit |

The settings belong to the model, and every object type that loads the
model in a mission uses them. When a mission swaps a model for another, the
craft uses the settings of the model that it loads. The file applies even
when the mod doesn't replace the model itself, so you can change the
settings of an original model.

The game reads the settings when a mission loads. A value that the game
can't use is ignored, and the log says so.

### hud

`hud` sets the craft's reticle and HUD colours.

```yaml
hud:
  reticle:
    laser: 135
    laser_in_range: 136
    warhead: 137
    warhead_lock: 68
    cannon_ready: 139
    cannon: 140
    threats:
      laser_mode: {laser: 141, turret: 142, beam: 143, warhead: 144}
      warhead_mode: {laser: 145, turret: 146, beam: 147, warhead: 148}
  colors: {inner: 0xff00ff00, border: 0xff008000}
```

| Key | Meaning |
|-|-|
| `reticle` | The image that each part of the reticle shows, as an image number of image group 12000, counted from 1. The [reticle slots](reference/model_values.md#reticle-slots) page lists the parts. A part that the map leaves out keeps the game's image. |
| `colors.inner` | The inner colour of the HUD, as an ARGB hex number. |
| `colors.border` | The border colour of the HUD, as an ARGB hex number. |

The hull and map icons of each object type are in `flight/hud.yaml`. The
[flight page](flight.md) describes it.

### engine_glows

`engine_glows` hides engine glows while the craft is slow. Each key is the
number of a glow in the model, counted from 0, in the order of the model's
engine glows.

```yaml
engine_glows:
  0: {min_speed_percent: 1}     # hidden when the craft stops
  6: {min_speed_percent: 100}   # shown only at top speed
  7: {min_speed_mglt: 40}
```

| Key | Meaning |
|-|-|
| `min_speed_percent` | The glow is hidden while the speed is below this percent of the craft's top speed. 1 or more. |
| `min_speed_mglt` | The glow is hidden while the speed is below this many MGLT. 1 or more. |

A glow without a key always shows. A craft with a top speed of 0 shows all
its glows. This group only changes what you see.

### surfaces

`surfaces` hides the materials and lights of the model that have a tag,
while the craft is disabled or slow. Each key is a tag. The
[models page](models.md) and the [materials page](materials.md) describe how
to tag a light or a material. See also
[material and light tags](reference/states_and_tags.md#material-and-light-tags).

```yaml
surfaces:
  engine_glow: {hidden_when_disabled: true, min_throttle_percent: 1}
  forcefield: {hidden_when_disabled: true}
```

| Key | Meaning |
|-|-|
| `hidden_when_disabled` | `true`: hidden while none of the craft's systems work. |
| `min_speed_percent` | Hidden while the speed is below this percent of the top speed. |
| `min_speed_mglt` | Hidden while the speed is below this many MGLT. |
| `min_throttle_percent` | Hidden while the throttle is below this percent, from 1 to 100. |
| `min_ordered_speed_mglt` | Hidden while the speed that the craft's orders give is below this many MGLT. |

A tag is hidden when any of its keys hides it. A tag can have lower-case
letters, digits, `.` and `_`, and at most 63 characters. A model can have at
most 32 tags in this group. Objects without a craft record show all their
surfaces. This group only changes what you see.

### weapons

`weapons` holds several groups. The `weapons` section of
`flight/flight.yaml` has the same groups, except `switch_by_iff`, `colors`
and `by_markings`, and sets their values for every craft. The
[flight page](flight.md) describes it. For each value, the game uses the
model's value first, then the value in `flight/flight.yaml`, then the value
of the original game.

```yaml
weapons:
  switch_by_iff: none
  colors:
    variant: {default: 0, ion_cannon: 1}
    impact: {rebel_laser: 0xff0000, ion_cannon: 0x3333ff}
    energy_bars: {laser_high: 0xffff0000, laser_low: 0xff9b1e00}
  by_markings:
    1: {variant: {default: 2}, impact: {rebel_laser: 0x3333ff}}
  rates: {decharge: 4, recharge: 2, cooldown: 47}
  energy_transfer: {rate: 4, penalty: 0, laser_limit: 100, shield_limit: 800}
  ai_torpedoes: {per_pass: 6, per_target: 16}
  impact_spin: {enabled: true, speed_percent: 100, angle_percent: 100}
  system_damage: {max: 1000, ion_cannon: 1, ion_cannon_turbo: 2}
  stats:
    default:
      rebel_laser: {power: 350, fire_rate: 47}
  hardpoint_kinds: {custom_1: laser}
```

Weapon types are written as names, such as `rebel_laser`. The
[weapons](reference/weapons.md#weapon-types) page lists them.

#### Weapon colours

| Key | Meaning |
|-|-|
| `switch_by_iff` | Which shots of the craft use the weapon of its side, so that an Imperial craft fires Imperial lasers instead of Rebel lasers. `turrets`: only shots from turrets, as in the original game. `none`: no shots. `all`: all shots. Without the key, the `rules.weapon_iff_swap` value of `flight/flight.yaml` applies. |
| `colors.variant` | The markings of the craft's bolts, by weapon type, from 0 to 255. `default` sets the value for the weapon types that the map leaves out. The bolt's markings pick the variant of the bolt's model. |
| `colors.impact` | The colour of the marks that the craft's bolts leave on what they hit, by weapon type, as an RGB hex number. 0 keeps the white mark of the original game. |
| `colors.energy_bars` | The colours of the player's charge bars, as ARGB hex numbers: `laser_high`, `laser_low`, `ion_high` and `ion_low`. |
| `by_markings` | Changes for craft with one markings value. Each key is a markings value from 0 to 255, and each value has the keys of `colors`. |

A craft's markings come from its flight group. The
[materials page](materials.md#skins-and-markings) describes markings.

The bolt's markings come from the first of these that is set:
`by_markings` for the weapon type, `by_markings` `default`, `colors` for the
weapon type, `colors` `default`, then 0. The impact colour comes from
`by_markings`, then `colors`, then `weapons.impact` in `flight/flight.yaml`.
The charge bar colours come from `by_markings`, then `colors`, then the
colours of the original game.

The colours only change what you see.

#### Laser rates

`rates` sets how much charge a player's shot uses, how much comes back, and
the time between shots.

| Key | Meaning |
|-|-|
| `decharge` | The charge that a player's shot uses. The original game uses 3 for the TIE Fighter, the TIE Bomber and every craft that isn't a fighter, and 4 for the other fighters. |
| `recharge` | The charge that comes back at each power update. The original game uses 3 for the TIE Fighter and the TIE Bomber, and 2 for the others. |
| `cooldown` | The time between two laser shots, in ticks. 47 in the original game. |
| `decharge_percent` | Scales `decharge`. 100 by default. |
| `recharge_percent` | Scales `recharge`. 100 by default. |

All values are whole numbers of 0 or more. The decharge rate only applies
to the player's shots of laser and ion cannons. AI shots use 1 charge, as in the
original game. The cooldown applies to every craft. The rates are only used
when the model has `rates`, or when `flight/flight.yaml` has `rates` or a
global percent. Otherwise the original rules apply.

A weapon profile can set these rates for each weapon type. See
[Weapon profiles](#weapon-profiles).

#### Other weapon values

| Group | Key | Meaning |
|-|-|-|
| `energy_transfer` | `rate` | The shield energy that one unit of laser charge is worth when the player moves energy between lasers and shields. The original game uses 32 for the Missile Boat and 4 for the others. |
| | `penalty` | Taken from `rate` when the lasers charge the shields. 0 by default. |
| | `laser_limit` | The most laser charge that one key press moves. 100 by default. |
| | `shield_limit` | The most shield energy that one key press moves. 800 by default. |
| `ai_torpedoes` | `per_pass` | The most warheads that an AI craft fires at a big target, such as a starship, in one attack run. 6 by default. |
| | `per_target` | The most warheads of the craft that can home on one big target at once. 16 by default. |
| `impact_spin` | `enabled` | `true` or `false`: whether a warhead hit spins the craft. The original game uses the game option. |
| | `speed_percent` | Scales the speed of that spin. 100 by default. |
| | `angle_percent` | Scales the angle of that spin. 100 by default. |
| `system_damage` | `max` | The most system damage that the craft can take. 1000 by default. |
| | `ion_cannon` | The system damage of one hit of `ion_cannon`. 1 by default. |
| | `ion_cannon_turbo` | The same for `ion_cannon_turbo`. 2 by default. |
| | `turbolaser_ion` | The same for `turbolaser_ion`. 4 by default. |
| | `ion_pulse` | The same for `ion_pulse`. 30 by default. |

All values except `enabled` are whole numbers of 0 or more.

`impact_spin` and `system_damage.max` come from the model of the craft that
is hit. The damage of each ion weapon comes from the model of the craft that
fired. When the mod renames one of these weapon types, its key in
`system_damage` is the new name.

#### Weapon profiles

A profile is a set of weapon values. A mission can pick a profile for each
flight group. Four groups hold profiles, and each group has its own:

| Group | What a profile sets | Mission key |
|-|-|-|
| `stats` | the stats of each weapon type | `weapon_profile` |
| `warhead_capacity_percent` | how many warheads the craft loads | `warhead_profile` |
| `energy_speed_q16` | how much speed an energy step to the engines gives | `energy_profile` |
| `linking` | the laser link mode and the linking rules | `linking_profile` |

Each key of the group is a profile name, of up to 47 characters. The game
ignores case. The profile `default` is used when the mission doesn't pick
one. The [missions page](missions.md) describes how a mission picks a
profile.

When a flight group uses a profile, only that profile counts. A key that the
profile leaves out gets the value of the original game, not the `default`
value. When neither the model file nor `flight/flight.yaml` has the profile,
the flight group uses `default`.

`stats` maps weapon types to their stats:

```yaml
weapons:
  stats:
    default:
      rebel_laser: {range_m: 1600, fire_ratio: 236, precise: false, spread_mask: 63}
      imperial_laser:
        fire_rate: 47
        player_easy: {fire_rate: 39, power: 350}
    devastator:
      turbolaser_green_light: {power: 900, range_m: 5000}
```

[Flight settings](flight.md#stats) describes each stat, and
[Weapons](reference/weapons.md#weapon-types) gives the values of the
original game.

Stats are whole numbers, except `precise`. A weapon type can also have stats
for the player, for one difficulty, or for both, in the maps `player`,
`easy`, `medium`, `hard`, `player_easy`, `player_medium` and `player_hard`.
For each stat, the game tries these maps first, then the plain value. Each
time, it tries the model's value first, then the value in
`flight/flight.yaml`.

A laser uses the rates of its weapon type. Once its charge reaches
`high_power_charge`, it uses the rates of the next weapon type, its
high-power type.

The other profile groups:

| Group | Keys | Meaning |
|-|-|-|
| `warhead_capacity_percent` | [warhead](reference/weapons.md#warheads) names | The percent of each launcher's capacity that the craft loads with that warhead. The original percents go from 25 to 100. A craft always gets at least 1 warhead. |
| `energy_speed_q16` | `all`, `player` | How much speed the craft gains or loses for each step of energy that it moves to or from its engines, where 65536 is its top speed. `player` is for the player's craft and `all` for every craft. The original game uses 4096 for the TIE Bomber, 12288 for the TIE Fighter with spare engine power, and 8192 for the others. |
| `linking` | `laser_link` | The laser link mode that the craft always uses: 1 fires one cannon at a time, 2 fires half of the cannons together, and 3 fires all of them together. Other values make the craft fire wrongly. |
| | `prevent_ai_lasers_ion_when_disabling` | `true`: the AI doesn't fire lasers while it disables its target. |
| | `prevent_ai_ion_when_not_disabling` | `true`: the AI doesn't fire ion cannons when it doesn't disable its target. |
| | `prevent_player_linked_lasers_ion` | `true`: the player can't link the lasers and the ion cannons. |

```yaml
weapons:
  warhead_capacity_percent:
    default: {space_bomb: 50, heavy_rocket: 50}
  energy_speed_q16:
    default: {all: 8192, player: 7864}
  linking:
    default: {laser_link: 2, prevent_player_linked_lasers_ion: true}
```

Without a profile value, the linking rules come from
`weapons.linking_rules` in `flight/flight.yaml`.

#### Hardpoint kinds

`hardpoint_kinds` sets what the model's hardpoints fire, by
[hardpoint type](reference/model_values.md#hardpoint-types). Each value is a
[hardpoint kind](reference/weapons.md#hardpoint-kinds): `laser`, `warhead`
or `none`.

```yaml
weapons:
  hardpoint_kinds: {custom_1: laser, custom_2: warhead}
```

A hardpoint type that the map leaves out uses the kind from
`flight/flight.yaml`, then the kind of the original game. This group isn't a
profile: it applies to every craft that loads the model.

### stats_profiles

`stats_profiles` holds profiles of flight data that replace the values of
the craft record. A mission picks a profile for each model, and the game
picks a variant for the difficulty and for players.

```yaml
stats_profiles:
  default: {speed: 86, acceleration: 9, deceleration: 20, hull: 3045, shields: 3000}
  default_easy: {speed: 86, hull: 4000, shields: 4000}
  default_player: {speed: 90, hull: 3500, shields: 3500}
  escort: {speed: 70, hull: 6000, shields: 5000}
```

Each key is a profile name, of up to 63 characters. The game ignores case.
These keys can be in a profile:

| Key | Meaning |
|-|-|
| `speed`, `acceleration`, `deceleration` | As in `craft_record`. |
| `pitch_rate`, `roll_rate`, `yaw_rate` | As in `craft_record`. |
| `hull`, `shields` | As in `craft_record`. |
| `system_strength`, `system_damage_hull_threshold` | As in `craft_record`. |
| `has_shields`, `has_hyperdrive` | `true` or `false`, as in `craft_record`. |
| `speed_increment` | The speed that the craft gains at each small step while it speeds up. 1 by default. |
| `speed_decrement` | The speed that the craft loses at each small step while it slows down. 1 by default. |
| `warhead_collision_damage` | `false`: the warheads that the craft carries don't add to the damage when it hits another craft. `true` by default. |

The values use the units of the craft record. A key that the profile leaves
out keeps the craft record's value. A profile is whole: a variant doesn't
take values from another profile.

The game picks a craft's profile this way:

1. The mission file gives a profile name for the model: one for players,
   one for each markings value, and one for all craft. Without one, the name
   is `default`. The [missions page](missions.md) describes this.
2. For a player's craft, the game looks for `<name>_player_<difficulty>`,
   then `<name>_player`.
3. Then it looks for `<name>_<difficulty>`, then `<name>`.

The difficulty is `easy`, `medium` or `hard`. When none of these profiles
exists, the craft has no profile and uses its craft record. A mission can
also scale some values of every craft, in percent.

The tech room's ratings use the `default` profile.

### shield

`shield` sets how fast the craft's shields recharge.

```yaml
shield: {per_generator_rate: 5, player_rate: 40, delay_seconds: 30}
```

| Key | Meaning |
|-|-|
| `rate` | The recharge rate of the craft. |
| `per_generator_rate` | The recharge rate for each shield generator part that is still intact. With this key, `rate` is ignored. |
| `player_rate` | The rate when a player flies the craft. Without it, a player's craft uses the model's rate, or 20. |
| `delay_seconds` | After the craft's shields fall to 0, its flight group doesn't recharge for this many seconds of mission time. |

All values are whole numbers of 0 or more.

This group only applies when `flight/flight.yaml` has a `shield_recharge`
section. That section decides which craft recharge, for example whether
starships do. The [flight page](flight.md) describes it.

Without the group, the rate is 20 for fighters, 5 for each intact shield
generator of the Star Destroyers, the Interdictor and the Super Star
Destroyer, 5 for the other starships, and 0 for the other craft.

### sound

`sound` picks sounds of the original game for the craft.

```yaml
sound:
  interior: awing
  fly_by: awing
  fly_by_mode: 2d
  engine_wash: starship
  take_off: falcon
  weapons: empire
  weapon_ranges:
    imperial_laser_turbo: [isd_turbolaser_near, isd_turbolaser_far]
  hyper: rebel
  ambient:
    offset_m: [0, 300, 0]
    distance_m: 1000
```

| Key | Values | Meaning |
|-|-|-|
| `interior` | `xwing`, `ywing`, `awing`, `tie`, `gunboat`, `corellian`, `falcon` | The engine sound in the cockpit. |
| `fly_by` | the same names, and `slave` | The sound when the craft passes close to the player. |
| `fly_by_mode` | `2d`, `3d` | Always plays that version of the fly-by sound. Without it, the game plays the 3D version when 3D sound is on. |
| `engine_wash` | `normal`, `starship` | The sound in the craft's engine wash. |
| `take_off` | `gear`, `falcon` | The sound when the craft launches from the hangar. |
| `weapons` | see below | Uses the game's own weapon sounds. |
| `weapon_ranges` | map | For each weapon type, a list of 1 to 16 sounds of the `weapon_range` bank. Each shot plays one of them, chosen at random. |
| `hyper` | `rebel`, `imperial`, `imperial_zoom` | The sounds when the player's craft jumps to hyperspace. `imperial_zoom` adds the zoom sound to the Imperial jump. |
| `ambient.offset_m` | `[right, up, forward]` | Moves the centre of the craft's ambient sound, in whole metres. |
| `ambient.distance_m` | a number, or `[right, up, forward]` | The size of the area where the player hears the ambient sound, in whole metres. The default is the craft's size. In the list, `null` keeps the default for that axis. |

For each sound, the game uses the model's value first. Without it, it uses
the sound bank of `flight/sounds.yaml` for the craft's object type. The
[flight page](flight.md) describes the banks, and the
[sound banks](reference/sound_banks.md) page lists them.

`weapons` makes the craft use the game's own weapon sounds, instead of the
banks and `weapon_ranges`. With `plain`, each weapon type plays its own
sound. The other values change the laser sounds:

| Value | Laser sounds |
|-|-|
| `plain` | each weapon type plays its own sound |
| `corellian_transport` | Rebel lasers play the Corellian transport's lasers |
| `tie_fighter` | every laser plays the TIE Fighter's laser |
| `tie_fighter_with_ion` | every laser except ion cannons plays the TIE Fighter's laser |
| `rebel`, `rebel_turbo` | Rebel lasers play the Rebel laser, or its turbo version |
| `empire`, `empire_turbo` | Imperial lasers play the Imperial laser, or its turbo version |
| `falcon`, `falcon_turbo` | every laser except ion cannons plays the Falcon's laser, or its turbo version |
| `ion`, `ion_turbo` | ion cannons play the ion cannon, or its turbo version |

The ambient sound is the `starship_ambient` bank entry of the craft's object
type. It needs the `starship_ambient_sounds` rule of `flight/flight.yaml`.
The sound plays while the player's craft is inside an ellipsoid around the
craft, and gets louder towards its centre.

### component_strength

`component_strength` sets the strength of the model's parts, by
[mesh type](reference/model_values.md#mesh-types), from 0 to 255.

```yaml
component_strength: {gun_turret: 18, small_gun: 18, shield_generator: 36}
```

A new craft's parts start with this strength, and the target display shows
a part's health as a percent of it. A mesh type that the map leaves out
keeps the game's strength. The shield generators of the Super Star
Destroyer keep their own strength.

### gunner_seats

`gunner_seats` gives the craft gunner seats, where a player can fire the
turrets. In the original game, only the Corellian transport, the Millennium
Falcon and the family transport have seats.

```yaml
gunner_seats:
  forward_fire: false
  seats:
    - {position_m: [0.464, 0, 6.104], mount_pitch_degrees: 90,
       mount_yaw_degrees: -90, aim_limit_pitch_degrees: 66.138,
       aim_limit_yaw_degrees: 95.076, cockpit: corelliantransportgunner}
    - {position_m: [0.464, 0, -6.104], mount_pitch_degrees: -90,
       mount_yaw_degrees: 90, aim_limit_pitch_degrees: 66.138,
       aim_limit_yaw_degrees: 95.076, cockpit: corelliantransportgunner,
       inverted: true}
```

| Key | Meaning |
|-|-|
| `seats` | Required. The list of seats, at most 2. `seats: []` means no seats. |
| `forward_fire` | `false`: the turret laser groups don't also fire forward when the pilot fires. `true` by default, as in the original game. |

The list replaces the seats of the craft record. Each seat is a map:

| Key | Meaning |
|-|-|
| `position_m` | The gunner's eye in the craft's model, as `[right, forward, up]` in metres. |
| `mount_pitch_degrees` | Turns the view up or down, from -360 to 360. Positive looks up, so a seat under the craft has -90. |
| `mount_yaw_degrees` | Turns the view left or right, before the pitch, from -360 to 360. |
| `aim_limit_pitch_degrees` | How far the gunner can aim up and down from the mount direction, from 0 to 180. |
| `aim_limit_yaw_degrees` | How far the gunner can aim left and right from the mount direction, from 0 to 180. |
| `cockpit` | Required. The model of the seat's cockpit, such as `corelliantransportgunner`. |
| `inverted` | `true` draws the cockpit upside down, as the original game does for its second seat. |

The keys other than `cockpit` default to 0 or `false`. When a seat can't be
used, the game ignores the whole group, and the craft keeps the seats of its
craft record.

The original cockpit is `corelliantransportgunner`. For another cockpit,
put its `.glb` in `flight/models/`. When the game starts, it gives each
cockpit model that a seat uses a hidden object type in a free slot.

The shots and the turret parts of the model follow the gunner's pitch, and
the beam system part follows the yaw.

### warhead_lock

`warhead_lock` sets the ranges and times of warhead locks.

```yaml
warhead_lock:
  lock_range_m: {small: 2500, big: 5965.14, transport: 2485.47}
  lock_ticks: 354
  ticks_per_homing_level: 118
  ai_lock_range_m: {small: 2500, big: 5965.14, big_top_tier: 2485.47}
  ai_lock_ticks_per_tier: 472
```

| Key | Default | Meaning |
|-|-|-|
| `lock_range_m.small` | 2485.47 | How close the player's target must be for a lock, in metres, for targets that aren't big or transports. |
| `lock_range_m.big` | 5965.14 | The same for starships, platforms, containers and freighters. |
| `lock_range_m.transport` | 3976.76 | The same for transports. |
| `lock_ticks` | 708, or 354 for the Missile Boat | How long the player's lock takes, in ticks, from 200 to 32767. It only changes the HUD and the lock tone. |
| `ticks_per_homing_level` | 236 | A warhead's homing level is the craft's lock ticks divided by this value, up to 6. From 1 to 32767. It applies to players and to the AI. |
| `ai_lock_range_m.small` | 2485.47 | How close the AI's target must be for the AI to fire a warhead, in metres, for targets that aren't big. |
| `ai_lock_range_m.big` | 4970.95 | The same for starships, platforms, freighters, containers, weapon emplacements and, in newer missions, transports. |
| `ai_lock_range_m.big_top_tier` | 5965.14 | Replaces `big` for the AI of the top skill tier. |
| `ai_lock_ticks_per_tier` | 472 | The AI needs this many lock ticks for each skill tier, plus one, before it fires. From 0 to 65535. |

The ranges are 0 or more.

### jamming

`jamming` sets where the craft jams the weapons of an enemy player who is
near it.

```yaml
jamming:
  enabled: true
  direction: down
  range_m: none
  rule: hangar_and_jamming_points
```

| Key | Meaning |
|-|-|
| `enabled` | `false` turns jamming off for the model. |
| `range_m` | The player is jammed within this distance of the craft's centre, in metres, whatever the other keys say. `none` turns this off, so that `direction` and `rule` apply. |
| `direction` | `all`, `up` or `down`: jams everywhere, or only above or below the craft. |
| `rule` | Where the craft jams. `none`: nowhere. `hangar_mesh_center`: near the centre of a hangar part, but not near the hangar point. `hangar_point`: near the hangar point, and not too far above or below it. `hangar_and_jamming_points`: near the hangar point or the jamming points of the model. |

A key that the group leaves out keeps the rule of the object type in the
original game:

- the Imperial Research Center doesn't jam;
- the Factory jams within 100 m;
- the Imperial Star Destroyer, the Victory Star Destroyer and the
  Interdictor use `hangar_and_jamming_points`, and jam only below;
- the Super Star Destroyer uses `hangar_point`, but in the original game it
  never jams. It only jams when its model has the group, or with the
  `rules.super_star_destroyer_jamming` rule of `flight/flight.yaml`;
- the Repair Yard uses `hangar_mesh_center`;
- the other object types use `hangar_and_jamming_points`.

Jamming points are hardpoints of the model. The
[model values](reference/model_values.md#hardpoint-types) page lists them.

### docking

`docking` sets how high the player's craft approaches when it docks with
this craft.

```yaml
docking: {approach_height_m: 0}
```

| Key | Meaning |
|-|-|
| `approach_height_m` | The highest approach, in metres. Without it, 22.75 m, or 0 for `space_colony2`. |

The approach height is the smaller of this value and the gap between the
target's dock point and the player's dock point. This group only applies
with the `docking.player_approach_height` rule of `flight/flight.yaml`.

### cockpit_view

`cockpit_view` gives the cockpit a wide view.

```yaml
cockpit_view: {wide_vertical_fov: 61.6}
```

| Key | Meaning |
|-|-|
| `wide_vertical_fov` | The vertical field of view of the wide view, in degrees, above 0 and below 180. |

The `presentation.cockpit_fov` setting in the player's `config.yaml` picks
the view:

- `cockpit`, the default: the view of the camera on the model's
  `pilot_eye` hardpoint;
- `wide`: the wide view, or the camera's view when the craft has no wide
  view;
- `off`: the view of the original game.

A craft without the view that the setting needs keeps the view of the
original game. The [models page](models.md#the-pilots-eye) describes the
cockpit camera.
This group only changes what you see.

## Adding a new ship

This walk-through adds a V-wing fighter to the game. It fills the free slot
`slot_36`, uses the unused craft type 190, and gives the model its settings.

### 1. Make the model

Make `vwing.glb` and put it in `flight/models/`. For a cockpit, also make
`vwingcockpit.glb`. The [models page](models.md) and the
[tools page](tools.md) describe how. Put a `pilot_eye` hardpoint in the
craft's model, at the pilot's eye.

### 2. Fill a free slot

Create `flight/object_types/v_wing.yaml`:

```yaml
object_type: slot_36
name: v_wing
model: vwing
genus: fighter
family: craft
craft_record:
  speed: 270
  acceleration: 45
  deceleration: 60
  yaw_rate: 12288
  pitch_rate: 6656
  roll_rate: 11776
  hull: 1500
  shields: 2000
  has_shields: true
  has_hyperdrive: true
  countermeasures: 8
  lasers:
    - {type: imperial_laser, count: 2, kind: laser_cannon}
  warheads:
    - {type: missile, count: 2, capacity: 4}
  cockpit_name: 'VWing'
names: {short: 'V-W', long: 'V-wing', plural: 'V-wings'}
```

`model` gives the slot its model. `name` gives the object type the name that
the rest of the mod uses. The free slot starts with the X-wing's flight
data, so set every value that should be different. It has no lasers, no
warheads and no cockpit, so `lasers`, `warheads` and `cockpit_name` are
needed. `cockpit_name: 'VWing'` loads `vwingcockpit.glb`.

### 3. Add the craft type

Create `flight/craft_types/v_wing.yaml`:

```yaml
craft_type: 190
object_type: v_wing
ship_list: {name: 'V-wing', category: fighter, flyable: true,
            known: true, skirmish: true, hidden: false}
tech_room: {name: 'Alpha-3 Nimbus V-wing', manufacturer: 'Kuat Systems Engineering',
            side: 'Galactic Empire', crew: '1 Pilot',
            description: 'A fast escort fighter of the late Clone Wars.'}
```

Craft type 190 is unused. Its line is hidden, and its category is one that
the game skips. The file sets `name`, `category` and `hidden: false`, so the
craft is in the ship list, the skirmish list and the tech room. Add `icon`
when the map icon images have an icon for the V-wing.

### 4. Add the model settings

Create `flight/models/vwing.yaml`:

```yaml
hud:
  colors: {inner: 0xff00ff00, border: 0xff008000}
engine_glows:
  0: {min_speed_percent: 1}
  1: {min_speed_percent: 1}
weapons:
  colors:
    impact: {imperial_laser: 0x55ff00}
stats_profiles:
  default_easy: {hull: 2000, shields: 2600}
sound:
  interior: tie
  fly_by: tie
  weapons: tie_fighter
```

The engine glows go out when the craft stops. On easy, the craft has more
hull and shields. On the other difficulties, no profile matches, so the
craft uses its craft record.

### 5. Test the ship

1. Start OpenXWA with `--mod` and the mod's folder.
2. Read the log. It says how many object type and craft type files apply,
   and why a file is ignored.
3. Make a new pilot, open the tech room and find the V-wing.
4. Set up a skirmish mission with the V-wing and fly it.

To use the V-wing in a mission, give a flight group craft type 190 in the
mission's `.TIE` file with a mission editor, and put the mission in
`game/MISSIONS/`. The [mod folder](mod_folder.md) page lists the files that
`game/` can replace. The [flight page](flight.md) describes how to give the
craft its hull and map icons in `flight/hud.yaml`, and its sounds in
`flight/sounds.yaml`. Those files use the new name, `v_wing`.
