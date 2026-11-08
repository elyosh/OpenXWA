# Missions

A mission is a `.TIE` file in the `MISSIONS`, `MELEE` or `COMBAT` folder of
the game. A mod changes missions in two ways:

- It adds a mission file, `flight/missions/.../<mission>.yaml`, with the
  things that a `.TIE` file can't hold: models, skins, the hangar, weapon
  and stats profiles, sounds and rules.
- It replaces or adds `.TIE` files, mission lists and mission voices in its
  `game/` folder.

This page describes both, and the text lists that go with missions, such as
the e-mails of the concourse.

## Mission files

A mission file has the path of its `.TIE` file inside `flight/missions/`,
in lower case, with `.yaml` in place of `.TIE`:

| Mission | Mission file |
|-|-|
| `MISSIONS/1B1M5FG.TIE` | `flight/missions/missions/1b1m5fg.yaml` |
| `MELEE/8YARDA.TIE` | `flight/missions/melee/8yarda.yaml` |
| `COMBAT/<name>.TIE` | `flight/missions/combat/<name>.yaml` |

The folder `missions` appears twice in the path of a campaign mission. The
first one is the folder of all mission files, the second one is the game's
`MISSIONS` folder.

A mission file is a map of sections. Every section is optional:

| Section | What it changes |
|-|-|
| `settings` | rules for this mission |
| `flight_groups` | markings, IFF, pilot voice, warheads and weapon profiles of flight groups |
| `object_profiles` | the parts of a model that a flight group shows |
| `articulations` | the moving parts of a flight group's craft |
| `skins` | the materials that each markings value shows |
| `model_swaps` | models loaded in place of others |
| `stats_profiles` | the stats profiles that craft use |
| `craft_names` | the names of craft |
| `headlights` | turns the player's headlights on |
| `object_limits` | the number of objects in each region |
| `resdata` | the mission's own images |
| `concourse` | the planet in the concourse window |
| `sounds` | sounds loaded in place of others |
| `hangar` | the hangar |

```yaml
settings:
  red_alert: true
flight_groups:
  1: { name: "Red", craft: xwing, markings: 4 }
model_swaps:
  superstardestroyer: superstardestroyerrecentered
headlights: true
```

Without a mission file, the mission plays as its `.TIE` file says. An empty
file changes nothing. When a section has the wrong form, the game logs a
warning and ignores that section.

The game reads the file each time it loads the mission. The briefing also
reads it: the order of battle uses `settings` and `craft_names`, and the
craft previews use `object_profiles`, `skins` and `model_swaps`.

The game doesn't convert the texts of a mission file, such as craft names
and order of battle lines, to its own character set. Write them in ASCII
only.

### Flight group entries

The sections `flight_groups`, `object_profiles` and `articulations` have one
entry for each flight group that they change. The key of an entry is the
flight group's number, which is its position in the `.TIE` file, from 0
to 191.

An entry can also check that it has the right flight group:

| Key | Meaning |
|-|-|
| `name` | The flight group's name in the `.TIE` file, exactly, case included. |
| `craft` | The object type of the flight group's craft, such as `xwing`. See [Object types](reference/object_types.md). |

When the mission has no flight group with that number, or a check fails,
the game logs a warning and ignores the entry. The checks catch entries
that no longer match after a change to the `.TIE` file. When the mod's
object type files rename an object type, `craft` must use the new name.

### Model names

The sections `skins`, `model_swaps`, `stats_profiles` and `craft_names` use
model names. A model name is the name of the original model file, without
its folder and extension, in lower case: `xwing` for `XWING.OPT`. It is
also the name of the mod's model, `flight/models/xwing.glb`. The **Model**
column of [Object types](reference/object_types.md) gives the model name of
each object type. Model names must be written in lower case.

## settings

`settings` changes rules that the original game hard-codes for some
missions. The same keys can go in `mission_settings` in `flight/flight.yaml`
to set a default for every mission. A key in a mission file replaces the
same key of `mission_settings` for that mission. Without either, the game
keeps the original game's behaviour.
[Flight settings](flight.md#mission_settings) lists what the original game
does.

```yaml
settings:
  hide_friendly_arrival_messages: true
  skip_arrival_messages_iff: 1
  force_player_in_turret: "0:00:08"
  damage_through_shields_on_hard: true
  hyperspace_regions:
    2: {look: interdiction, arrival_sound: abort}
```

| Key | Meaning |
|-|-|
| `red_alert` | `true` puts the hangar on red alert. `false` keeps it off. |
| `hide_hyperspace_exit_messages` | `true` hides the message shown when a fighter leaves by hyperspace. |
| `hide_friendly_arrival_messages` | `true` hides the "entering area" messages shown when a friendly flight group arrives in the player's region. The default is `false`. |
| `skip_arrival_messages_iff` | Skips every arrival message of the flight groups with this IFF. The value is an IFF from 0 to 254, `all` or `none`. |
| `force_player_in_turret` | A mission time in quotes, such as `"0:00:08"`. At that time, the player moves to the turret, as with the G key. While the key is set, the map key does nothing. `false` never moves the player. |
| `disable_player_lasers` | `true` stops the player from firing lasers. The default is `false`. |
| `disable_player_warheads` | `true` stops the player from firing warheads. The default is `false`. |
| `warhead_collision_damage` | `false` stops a craft's warheads from adding to the damage when it hits another craft. The default is `true`. |
| `damage_through_shields_on_hard` | `true` lets a craft's parts take damage while its shields are up, on hard difficulty too. The original game only does this on easy and medium. The default is `false`. |
| `target_key` | Makes the O key target craft of chosen flight groups. See below. |
| `proximity_triggers_check_craft_only` | `true` makes proximity triggers ignore every object that isn't a craft. The default is `false`. |
| `order_of_battle` | Changes the order of battle in the briefing. See below. |
| `use_hyperspace_speeds` | `false` keeps the original game's hyperspace arrivals and departures of AI craft in this mission, even when the mod sets hyperspace speeds. The default is `true`. See [Flight settings](flight.md#hyperspace). |
| `hyperspace_regions` | What happens when the player jumps to a region. See below. |

Regions are numbered from 0, as the game counts them. An unknown key is
logged and ignored.

### hyperspace_regions

`hyperspace_regions` maps a region, from 0 to 3, to what happens when the
player jumps to it by hyperspace.

```yaml
settings:
  hyperspace_regions:
    1: {look: interdiction, arrival_sound: abort}
    3: {arrival_sound: abort}
```

| Key | Meaning |
|-|-|
| `look` | The look of the jump, in modern graphics. Without it, the jump has the normal look. |
| `arrival_sound` | `abort` plays the abort sound instead of the arrival sound when the player arrives. `normal` keeps the arrival sound, which is the default. |

The game has one look:

| Look | What it shows |
|-|-|
| `interdiction` | A jump that a gravity well pulls out of hyperspace. The tunnel turns red, with yellow streaks, and shakes more and more. The streaks of the exit are red. The jump's start looks normal. |

The regions that the key doesn't list keep the normal jump. A
`hyperspace_regions` key in a mission replaces the whole list of
`mission_settings`.

### target_key

In the original game, the O key targets the nearest objective craft. With
`target_key`, it targets a craft of the listed flight groups, in the
player's region, instead.

```yaml
settings:
  target_key: { flight_groups: [5, 6], method: cycle, only_not_inspected: true }
```

| Key | Meaning |
|-|-|
| `flight_groups` | Required. The flight group numbers. |
| `method` | `nearest` targets the nearest craft. `cycle` targets the next craft each time the key is pressed. The default is `nearest`. |
| `only_not_inspected` | `true` skips the craft that the player's team has already inspected. The default is `false`. |

The O key keeps its own behaviour in the proving grounds.

### order_of_battle

`order_of_battle` changes the order of battle page of the briefing.

```yaml
settings:
  order_of_battle:
    flight_groups: [3, 4, 12]
    names: { 12: "Home One" }
```

```yaml
settings:
  order_of_battle:
    lines: ["Fleet Assignment: Admiral Thrawn", "", "Command Ship: Chimaera"]
    top: 140
```

| Key | Meaning |
|-|-|
| `flight_groups` | The flight groups to show, whatever their team and arrival. The listed fighters are counted by craft type. The other listed craft are shown one by one. Without the key, the game picks the flight groups as the original game does. |
| `names` | Maps flight group numbers to the text shown for them, in place of the craft name and flight group name. It only applies to listed flight groups that aren't fighters. Up to 32 names. |
| `lines` | Text lines shown under the page title, in place of the order of battle. Up to 32 lines. |
| `top` | The top of the title and of `lines`, from 0 to 479, in the units of the original 640 × 480 screen. |

Each name and line has up to 127 characters.

## flight_groups

`flight_groups` changes values that the `.TIE` file gives each flight
group. The game applies them right after it reads the `.TIE` file, so the
whole mission sees the new values.

```yaml
flight_groups:
  0: { name: "Alpha", craft: tie_fighter, warheads: { missile: 80 } }
  1: { name: "Red", craft: xwing, markings: 4 }
  5: { name: "Gold", craft: ywing, iff: 2, pilot_voice: "RP3" }
  13: { name: "Devastator", weapon_profile: heavy_guns }
  26: { name: "Red", craft: xwing, craft_markings: { 0: 0, 1: 1 } }
```

| Key | Meaning |
|-|-|
| `name`, `craft` | The checks described in [Flight group entries](#flight-group-entries). |
| `markings` | The flight group's markings, from 0 to 255. |
| `craft_markings` | Markings for single craft of the flight group. See below. |
| `iff` | The flight group's IFF, from 0 to 255. |
| `pilot_voice` | The flight group's pilot voice, up to 19 characters. See below. |
| `warheads` | The warheads that the flight group has for the whole mission. See below. |
| `weapon_profile` | The `stats` weapon profile of the flight group. |
| `warhead_profile` | The `warhead_capacity_percent` profile of the flight group. |
| `energy_profile` | The `energy_speed_percent` profile of the flight group. |
| `linking_profile` | The `linking` profile of the flight group. |

A key that is missing keeps the value of the `.TIE` file. A value out of
range is logged and ignored.

### craft_markings

`craft_markings` gives some craft of the flight group their own markings.
Its keys are the craft's number in its wave, from 0. Its values are
markings from 0 to 255. The same craft of every wave gets them. Craft
without a value use the flight group's markings.

[Materials](materials.md) describes what markings show on a model.

### pilot_voice

`pilot_voice` replaces the pilot voice of the `.TIE` file. It sets the
voice of the flight group's leader. The other craft of the group get voices
at random. The game knows these voices, in any case:

| Voice | Sound list |
|-|-|
| `RP1` to `RP12` | `Wave/RSPXWA1.LST` to `Wave/RSPXWA12.LST` |
| `OLIN` | `Wave/RSPXWA10.LST`, the same voice as `RP10` |
| `AERON` | `Wave/AERON.LST` |
| `EMON` | `Wave/EMON.LST` |
| `EMKAY` | `Wave/EMKAY.LST` |

With another value, the game picks a voice itself. Pilot voices only play
for the flight groups in the same global unit as the player's flight group,
and only when that global unit isn't 0. The `.TIE` file sets the global
units. The player's options must have pilot voices on.

### warheads

`warheads` sets a pool of warheads for the flight group, by warhead type.
The keys are warhead names, such as `missile` or `torpedo`. See
[Weapons](reference/weapons.md#warheads). The values are numbers of 0 or
more. A warhead type without a value has no limit.

- When the flight group has the double warheads or half warheads status,
  the game doubles or halves the values.
- The warheads that a craft starts with don't come out of the pool, but
  the pool limits them. Each rack of a launcher gets at most what is left
  divided by the number of racks of the launcher. This applies to
  every craft of the flight group, AI craft included.
- In the hangar, every warhead that goes into a player's rack comes out of
  the pool, both when the player loads warheads from the menu and when the
  service adds warheads after landing. The hangar menu shows what is left.
- The pool only goes down in single-player flights.
- The pools are full again at each mission load, so a restart or a film
  starts with full pools.

### Weapon profiles

`weapon_profile`, `warhead_profile`, `energy_profile` and `linking_profile`
pick weapon profiles by name. The profiles are in the `weapons` group of
the craft's model settings file, and in the `weapons` section of
`flight/flight.yaml`. Names have up to 47 characters and can be written in
any case. When neither file has the profile, the flight group gets the
original game's values, and the log says so. [Flight settings](flight.md#weapon-profiles) and
[Craft](craft.md) describe the profiles.

## object_profiles

An object profile hides some parts of a model, such as an astromech droid
or a cargo pod. The model lists the parts that profile `<name>` hides in its
part set `hidden_by_profile.<name>`. See [Models](models.md#profiles).

```yaml
object_profiles:
  flight_groups:
    1: { profile: "R2", name: "Red", craft: xwing }
    2: { profile: "R5", name: "Red", craft: xwing }
```

| Key | Meaning |
|-|-|
| `profile` | Required. The profile name, in any case. |
| `name`, `craft` | The checks described in [Flight group entries](#flight-group-entries). |

The game picks the profile of a craft in this order:

1. the profile of its flight group in the mission file;
2. `Skirmish`, in a quick start or a skirmish, when the model has it;
3. `Default_<markings>`, such as `Default_2` for markings 2.

A profile that hides nothing, or that the model doesn't have, uses
`Default`. Without `Default` either, the craft shows every part. Profiles
only apply to the mod's models.

`weapons` sets the profile of laser bolts and other objects that aren't
craft, based on the type of craft that fires them. The first key is the
object type of the craft that fires. The second key is the object type of
what it fires:

```yaml
object_profiles:
  weapons:
    imperial_star_destroyer2:
      turbolaser_green_medium: "ShowBeam"
```

In this example, the `turbolaser_green_medium` shots fired by
`imperial_star_destroyer2` craft use the `ShowBeam` profile.

When `weapons` doesn't set a profile, these objects use `Skirmish` in a
quick start or a skirmish if the model has it, and `Default` otherwise. If
the profile hides nothing, the object shows all its parts. Unlike craft, it
doesn't fall back to `Default`.

## articulations

`articulations` changes the moving parts of a flight group's craft:
S-foils, landing gear and the other parts that
[Models](models.md#moving-parts) describes.

```yaml
articulations:
  flight_groups:
    3: { name: "Blue", craft: xwing, start: [sfoils, landing_gear] }
    4: { variant: closed }
```

| Key | Meaning |
|-|-|
| `start` | A list of groups, `sfoils` and `landing_gear`. These groups start at the angle that the model gives them, as when a craft leaves a hangar. |
| `variant` | A variant name. The craft use the model's articulations of group `variant.<name>.<group>` in place of the model's own for the same group and part, and gain the others. Lower-case letters, digits and `_`, up to 48 characters. |
| `name`, `craft` | The checks described in [Flight group entries](#flight-group-entries). |

An entry with another key, a group other than `sfoils` and `landing_gear`,
or an invalid variant name is logged and ignored. When the model has no
articulations for the variant, the game logs it and uses the model's own.

## skins

`skins` chooses the materials that each markings value shows on a model.
A model's file lists the material variant of each markings value. A
mission can choose others. See [Materials](materials.md).

```yaml
skins:
  awing:
    0: "default_3 + jake"
    1: "default_3 + damaged"
  xwing:
    2: "red3"
    5: null
```

The keys are model names. Under each model, the keys are markings values
from 0 to 255, and the values are names of the model's material variants.
`null` shows the model's own materials, without a variant. A name that the
model doesn't have is logged, and the model's list is used. A mission can
have up to 256 choices.

## model_swaps

`model_swaps` loads another model in place of a model, for every object
that uses it in this mission. The keys are the model names to replace, and
the values are the model names to load.

```yaml
model_swaps:
  superstardestroyer: superstardestroyerrecentered
  millenniumfalcon2cockpit: minifalconcockpit
```

The model to load must be a `.glb` file of the mod,
`flight/models/<name>.glb`. It can be a model that no object type uses.
The shape, collision, hardpoints, skins and settings all come from the
loaded model. When the file is missing, the game logs it and keeps
the original model. A mission can have up to 64 swaps. The tech room always
shows the models without swaps.

## stats_profiles

A stats profile replaces the speed, hull, shields and other values of a
craft. A model settings file holds the model's profiles. See
[Craft](craft.md). A mission file picks the profiles that its craft use,
and can scale some values for every craft.

```yaml
stats_profiles:
  models:
    escorttransport: { player: x1, markings: { 2: etrmod }, all: rebel }
  percent: { hull: 120 }
  player_percent: { hull: 400 }
```

| Key | Meaning |
|-|-|
| `models` | Maps model names to the profile names that their craft use. |
| `percent` | Scales values of every craft, in percent. |
| `player_percent` | Scales values of the player's craft, in percent. A value here replaces the same value of `percent`. |

Under `models`, each model can have:

| Key | Meaning |
|-|-|
| `player` | The profile name for a craft that a player flies. |
| `markings` | Maps markings values, from 0 to 255, to profile names. |
| `all` | The profile name for the other craft. |

Profile names have up to 63 characters and can be written in any case.

The model is the one that the craft loads in the mission. When the mission
swaps the model, use the name of the model that it loads.

The game finds the profile name of a craft in this order: `player` when a
player flies it, the name for its markings, `all`, then `default`. Then it
looks in the model's profiles for `<name>_player_<difficulty>` and
`<name>_player` for a player's craft, then `<name>_<difficulty>`, then
`<name>`. The difficulty is `easy`, `medium` or `hard`. When the model has
none of these, the craft has no profile, even if the model has a `default`
profile.

`percent` and `player_percent` take these keys: `speed`, `acceleration`,
`deceleration`, `pitch_rate`, `roll_rate`, `yaw_rate`,
`system_damage_hull_threshold`, `hull`, `system_strength` and `shields`.
The game applies them after the profile, and also to craft without a
profile.

## craft_names

`craft_names` renames the craft of a model for the length of the mission.
The keys are model names.

```yaml
model_swaps:
  containerbrick: imperialstardestroyer2lod
craft_names:
  containerbrick:
    name: "Imperial II Star Destroyer"
    short_name: "ISD"
    plural: "Imperial II Star Destroyers"
    ship_list: "Imperial II Star Destroyer"
```

| Key | Meaning |
|-|-|
| `name` | The craft's name, which the HUD and the messages show. |
| `short_name` | The craft's short name. |
| `plural` | The plural of the craft's name. |
| `ship_list` | The ship list name of the craft type. The order of battle shows it. |

Every key is optional. Each text has up to 255 characters. The model is the
one that the object type loads without swaps. In the example above, the
key is `containerbrick`, not the model that replaces it. When no object
type with a craft record uses the model, the game logs it and ignores the
names. A mission can rename up to 64 models.

The game sets the names when the mission starts and puts the original
names back after the debriefing. The briefing only uses `ship_list`, in the
order of battle.

## headlights

```yaml
headlights: true
```

`true` turns the headlights of the player's craft on for the whole mission,
except in the hangar. The light itself is set in the `headlights` section
of `flight/flight.yaml`. Without that section, there are no headlights.
Without the key, or with `false`, the headlights follow the automatic rule
of `flight.yaml`. See [Flight settings](flight.md#headlights).

## object_limits

`object_limits` sets the number of craft, projectiles, explosions and radar
blips for this mission. It takes the same keys as `object_limits` in
`flight/flight.yaml`, and its keys replace the ones of `flight.yaml`.

```yaml
object_limits:
  craft_per_region: 192
  ai_projectiles_per_region: 256
```

The keys are `craft_per_region`, `player_projectiles`,
`ai_projectiles_per_region`, `explosions_per_region` and `radar_blips`.
[Flight settings](flight.md#object_limits) describes them.

## resdata

A mission can have images of its own, which replace the images of some
image groups while the mission plays. They are in
`resdata/mission_art/<name>/`, one atlas for each group, next to the shared
atlases in `resdata/atlas/`. [Images](images.md) describes the atlas
format, and [Image groups](reference/image_groups.md) lists the groups.

`resdata` lists the names of the image files that the mission uses, in
order:

```yaml
resdata: ["planet2", "planetcustom"]
```

A name with a folder in `resdata/mission_art/` gives its groups to the
mission. A name without a folder is skipped, because its images are
already in the shared atlases. When two folders have the same group, the
first one in the list wins. The other groups come from the shared
atlases.

Names have only lower-case letters, digits, `_` and `-`, and fewer than 32
characters. A mission can use up to 8 folders and 64 groups.

## concourse

`concourse.front_planet` sets the planet that the concourse window shows
while this mission is the next one to fly.

```yaml
concourse:
  front_planet: { position: [1276, 234], image: 15010 }
```

| Key | Meaning |
|-|-|
| `image` | The image group of the planet, such as 15010. Groups 15010 to 15114 hold the planets. Without it, the game picks the planet of the mission's backdrops, or one at random. |
| `position` | The centre of the planet, `[x, y]` in design pixels. Without it, the position is random. |

[The mod folder](mod_folder.md#units-and-conventions) explains design
pixels.

## sounds

`sounds` lists sound files to load in place of others during the mission.
Each entry is a pair: the path that the game loads, then the file to load
instead.

```yaml
sounds:
  - ['Wave/Sfx_HangarAmbient/FamilyHangar0.wav', 'Wave/Custom/Evacuate.wav']
  - ['Wave/Sfx_HangarAmbient/RebelHangar1.wav', 'Wave/FrontEnd/T01PA33.wav']
```

- Paths start at the game folder. Case doesn't matter, and `/` and `\` are
  the same.
- The new file keeps the sound name of the file it replaces.
- New sound files go in the mod's `game/Wave/` folder. See
  [The mod folder](mod_folder.md#replacing-original-game-files).
- When the new file doesn't exist, the game loads the original file and
  logs a warning.
- When two entries have the same first path, the game uses the first one.

## hangar

`hangar` changes the hangar for this mission: the objects in it, the
shuttle, the droids and the cameras. It is one more layer of hangar
settings, with the keys of a layer of `flight/hangars.yaml`, and it comes
before the other layers. It can also make a command ship use the settings
of another model, with `use_ship_settings`. See
[Hangars](hangars.md#a-missions-hangar-section).

## Skies

A mission's skies aren't in its mission file. The file
`flight/environments/environments.yaml` gives each mission and region its
environment. See [Environments](environments.md#choosing-environments).

## TIE missions

The game plays `.TIE` files in the X-Wing Alliance mission format. A mod
puts its `.TIE` files in `game/`, in the folder that the game uses:

| Folder | Missions |
|-|-|
| `game/MISSIONS/` | the campaign |
| `game/MELEE/` | the melee missions |
| `game/COMBAT/` | the combat missions |

A file with the name of an original mission replaces it. A file with a new
name adds a mission, which the mission list of its folder must name. The
mission file of a new mission follows the same naming rule as the others.

The original game has no mission editor. The community has written mission
editors that make and change `.TIE` files, and has documented the format.
The texts inside a `.TIE` file, such as the briefing and the in-flight
messages, can only be changed there.

### Mission lists

`MISSION.LST` in each folder lists the missions that the game shows. A
mod's copy, such as `game/MISSIONS/MISSION.LST`, replaces the whole list.

```text
//
!BATTLE_1_HEADER![Battle 1: Clearing the Way]
//
9
* 1B1M1G.tie
!MISSION_9_DESC!Convoy Attack
10
* 1B1M2FG.tie
!MISSION_10_DESC!Rescue Uncle Antan
```

- A line that starts with `//` is a comment.
- A line whose text starts with `[` and ends with `]` starts a new section,
  such as a battle. The text between the brackets is its name.
- Each mission has three lines: its number, its file name and its name in
  the list.
- `* ` before the file name locks the mission until the mission before it
  is completed. `& ` before it locks the mission.

The texts use the `!ID!` format described in
[Text lists](#e-mails-and-other-text-lists).

## Mission voices

The voices of a mission's in-flight messages are listed in
`Wave/MISSIONVOICE/<mission>.LST`, where `<mission>` is the name of the
`.TIE` file without its folder. For example, `MISSIONS/1B0M1FW.TIE` uses
`Wave/MISSIONVOICE/1B0M1FW.LST`. A mod puts its copy in
`game/Wave/MISSIONVOICE/`.

```text
MissionVoice\B0m1\R000101.wav
MissionVoice\B0m1\R000102.wav
MissionVoice\B0m1\R000103.wav
```

The list has one line for each message of the mission, in the order of the
messages in the `.TIE` file: line 1 is the sound of message 1, and so on.
A mission has up to 64 messages. The paths start at the `Wave` folder. The
sound files go in `game/Wave/`, at the same paths. Empty lines are skipped
and don't count as a message. To leave a message without a voice, write a
file name that doesn't exist.

Voices play at the player's voice volume. The game only loads and plays
them when the voice volume is above 0 and the player's options have special
messages on. The `sounds` section of the mission file also applies to the
voices.

## E-mails and other text lists

These copies of game files hold texts that go with missions:

| File | What it holds |
|-|-|
| `MISSIONS/MISSION.LST` and the other mission lists | the battle and mission names |
| `MISSIONS/EMAIL.TXT` | the e-mails in the concourse |
| `FRONTRES/CUTSCENE/CUTSCENE.TXT` | the cutscenes that follow missions |
| `FRONTRES/MEDALS/FAMILYAWARDS.TXT` | the memento texts |

A mod puts its copies in `game/`, at the same path. The files are in the
game's encoding, Windows-1252. `EMAIL.TXT`, `CUTSCENE.TXT` and
`FAMILYAWARDS.TXT` explain their layout in comment lines at their top.

Each line of text starts with an ID between two `!`, followed by the text:

```text
!EMAIL_0_FROM!Alliance Net
```

The game shows the text after the second `!`. `text/strings.yaml` can
replace the text of an ID without a copy of the file. See [Text](text.md).
A line without an ID is shown as it is written, so new lines don't need
one.

### EMAIL.TXT

Each e-mail is a block that ends with a line holding `*`:

```text
3
!EMAIL_3_FROM!Olin Garn
!EMAIL_3_HEADER!Pass it on..
!EMAIL_3_MSG1!What's up, Ace? I want you to keep an eye on her for me.
!EMAIL_3_MSG2!
!EMAIL_3_MSG3!Olin
*
```

| Line | Meaning |
|-|-|
| 1 | The number of the mission, as in `MISSION.LST`, from which the e-mail is in the inbox. |
| 2 | The sender, up to 63 characters. |
| 3 | The subject, up to 127 characters. |
| Next lines | The message, one line of text for each line of the file, up to 1023 characters in all. |

Keep the e-mails in the order of their mission numbers. The game stops
reading at the first e-mail for a mission that the player hasn't reached.
