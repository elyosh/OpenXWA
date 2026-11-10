# Hangars

`flight/hangars.yaml` sets up the hangars. It chooses the hangar model of
each command ship, and places the shuttle, the droids, the cranes, the
parked objects, the player's craft and the camera views. A mission can change these
settings in the `hangar` section of its mission file.

The game uses `hangars.yaml` only while model replacements are on, which is
the default. Without the file, the hangars keep the original game's layout,
and the `hangar` section of the missions is ignored. An empty file keeps
the original layout too, because every key's default is the original
game's value.

Lengths are in metres and angles in degrees. Objects are named by their
object type, such as `hangar_droid`, and models by their model name, such
as `frigate2`. [Object types](reference/object_types.md) lists both.

## Example

```yaml
default:
  shuttle:
    markings: 4
    path: {exit_side: right, straight_m: 48.8}
  droids:
    - {markings: 2}
  swaps:
    hangardroid: "hangardroidr2q5"
ships:
  frigate2:
    hangar_model: frigate2hangar
    player:
      offset_m: [12.2, 0, 0]
    by_iff:
      2:
        shuttle: {enabled: false}
crafts:
  xwing:
    rest_height_m: {normal: 1.2, inverted: 1.2}
    player_camera_m:
      hangar: [-6.1, 5.6, 1.2]
```

In every hangar, the shuttle has markings 4 and flies 48.8 m farther when it
comes back, and the first droid uses markings 2 and the model
`hangardroidr2q5`. In the hangar of a `frigate2`, the hangar model is
`flight/models/frigate2hangar.glb` and the player's craft stands 12.2 m to
the side. When that frigate's IFF is 2, its hangar has no shuttle. The
X-wing stands 1.2 m above the floor, and it has its own camera view.

## The file

| Key | Meaning |
|-|-|
| `default` | The settings of every hangar. |
| `ships` | The settings of the hangar of each command ship, by the ship's model. |
| `crafts` | Settings of each player craft, by the craft's model. |

The command ship is the ship whose hangar the player is in. It is the ship
that the player docks with, or the mothership of the player's flight group.

A wrong value is ignored with a warning in the log, and the setting keeps
its value from the next layer. An unknown key is also ignored with a
warning.

## Layers

The settings come in layers. For the hangar of a command ship, the game
looks in this order:

1. the mission's `hangar` section;
2. `ships.<model>.by_iff.<iff>`, for the command ship's IFF;
3. `ships.<model>`;
4. `default`.

Each setting comes from the first layer that sets it, so different
settings can come from different layers. Each droid's settings are looked
up on their own.

There are three exceptions:

- `crane_arm` and `parked` come together from the first layer that has
  either of them. A layer with only `parked` has no crane arm. The family
  base's `crane_arm` and `parked` work the same way.
- Each camera key comes from the first layer that sets it. A key that no
  layer sets keeps the original view.
- When several layers swap the same model, the first layer wins.

The proving ground uses only `default`.

## Default

`default` and each layer can have these keys.

| Key | Meaning |
|-|-|
| `hangar` | The hangar itself. |
| `shuttle` | The shuttle. |
| `droids` | The two droids. |
| `roof_crane` | The crane that moves under the roof. |
| `player` | Where the player's craft stands and how it flies in and out. |
| `swaps` | Models that hangar objects use in place of their own. |
| `crane_arm` | The crane arm on the floor. |
| `parked` | The objects parked in the hangar. |
| `cameras` | The camera views. |
| `family_base` | The crane arm, parked objects and cameras of the family base. |

Positions are measured from the origin of the hangar model, not from the
floor:

- +y points out of the hangar's opening, the way craft fly out;
- +x points to the right when you look out of the opening;
- +z points up. The floor is usually below the origin, so heights such as
  the roof crane's can be negative.

Objects with only x and y stand on the floor. Headings are in degrees: 0
faces out of the opening, and 90 faces +x, so a positive heading turns an
object clockwise seen from above. A positive pitch lowers the nose.

### hangar

| Key | Meaning |
|-|-|
| `floor` | `normal` or `inverted`. The floor that objects stand on when they don't choose one. The default is `normal`. |
| `inverted_floor_height_m` | The height of the inverted floor above the normal floor. Craft on the inverted floor hang below it, like TIE fighters in their racks. The default is 0. |
| `markings` | The markings that the hangar model shows: `command_ship` for the command ship's markings, or a number from 0 to 255. The default is `command_ship`. |

### shuttle

| Key | Meaning |
|-|-|
| `enabled` | `false` leaves the shuttle out. The default is `true`. |
| `type` | The shuttle's object type, or `default` for the original shuttle. The default is `default`. |
| `markings` | The shuttle's markings, 0 to 255. The default is 0. |
| `profile` | An object profile of the shuttle's model. See [Models](models.md). The default is `Default`. |
| `position_m` | `[x, y]`, where the shuttle stands. The default is `[27.515, 23.413]`. |
| `height_m` | Raises the shuttle above its floor. The default is 0. |
| `heading_degrees` | The shuttle's heading, -360 to 360. The default is 236.953. |
| `floor` | `normal` or `inverted`. The default is `normal`. |
| `path` | How the shuttle flies in and out. |

`path` has these keys:

| Key | Meaning |
|-|-|
| `exit_side` | Which way the shuttle moves off its place when it leaves: `right` moves it sideways, `top` moves it up while it rolls, and `bottom` moves it down while it rolls. The default is `right`. |
| `straight_m` | How much farther away the shuttle starts when it flies back in. The default is 0. |
| `rise_m` | How much higher the shuttle rises before it flies out. The default is 0. |

### droids

A list of one or two droids. The first entry is the first droid, the second
entry the second droid. A missing entry or key keeps its value from the
next layer. The droids keep their original places.

| Key | Meaning |
|-|-|
| `enabled` | `false` leaves the droid out. The default is `true`. |
| `type` | The droid's object type. The defaults are `hangar_droid` and `hangar_droid2`. |
| `markings` | The droid's markings, 0 to 255. The default is 0. |
| `profile` | An object profile of the droid's model. The default is `Default`. |
| `height_m` | Raises the droid above its floor. The default is 0. |
| `moves` | `false` keeps the droid still. The default is `true`. |
| `floor` | `normal` or `inverted`. The default is `normal`. |

```yaml
droids:
  - {markings: 1}
  - {enabled: false}
```

As in the original game, the first droid nods part 3 of its model, and the
second droid swings part 1 and spins part 2, whatever its `type`. With
another type, the wrong parts can move. The
`hangar_droid_parts_from_model` rule of [flight.yaml](flight.md#rules)
makes each droid move the parts that its model lists instead. See
[hangar droids](models.md#hangar-droids).

### roof_crane

| Key | Meaning |
|-|-|
| `position_m` | `[x, y, z]`, where the crane starts. The default is `[-34.18, 19.189, -6.885]`. |
| `axis` | `x`, `y` or `z`, the axis that the crane moves along. The default is `x`. |
| `travel_m` | `[low, high]`, how far the crane can move along its axis, from `position_m`. The default is `[0, 68.359]`. |

To remove the roof crane, swap its model `hangarroofcrane` for an empty
model.

### player

| Key | Meaning |
|-|-|
| `offset_m` | `[x, y, z]`, moves the player's craft from its original place. The default is `[0, 0, 0]`. |
| `inverted_offset_m` | The same, for a craft on the inverted floor. The default is `offset_m`. |
| `rise_m` | How much higher the craft rises before it flies out, and how high it comes back in. The default is 0. |
| `inverted_rise_m` | The same, for a craft on the inverted floor. The default is `rise_m`. |
| `straight_m` | How much farther away the craft starts when it flies back in. It flies straight over that distance before it slows down and lands. The default is 0. |
| `floor` | `normal` or `inverted`, the floor of every player craft. The default is `normal`. |
| `inverted_floor_crafts` | A list of object types that hang from the inverted floor, such as `[tie_fighter, tie_interceptor]`. |
| `craft_offsets_m` | A map from object types to `[x, y, z]`. A craft in the map uses its offset in place of `offset_m` and `inverted_offset_m`. |

The lists hold up to 64 craft.

### swaps

A map from a model to another model. While the player is in the hangar,
the hangar's objects that use the first model use the second one. The
other objects of the mission keep their models.

```yaml
swaps:
  hangardroid: "hangardroidr2q5"
  hangarroofcrane: "blank"
```

The new model must be a model of the mod, in
`flight/models/<model>.glb`. A swap to a model that doesn't exist is
ignored with a warning. The map holds up to 64 swaps.

### crane_arm and parked

`crane_arm` is one object, and `parked` is a list of objects. Both have the
same keys.

| Key | Meaning |
|-|-|
| `type` | Required. The object type. |
| `position_m` | Required. `[x, y]` stands the object on its floor. `[x, y, z]` puts it at that height from the hangar's origin. |
| `markings` | The object's markings, 0 to 255. The default is 0. |
| `heading_degrees` | The object's heading, -360 to 360. The default is 0. |
| `pitch_degrees` | The object's pitch, -360 to 360. The default is 0. |
| `profile` | An object profile of the object's model. The default is `Default`. |
| `floor` | `normal` or `inverted`. The default is the floor of `hangar.floor`. |

An entry with a wrong value is left out, with a warning.

```yaml
crane_arm: {type: hangar_crane, position_m: [17.7, -10.156], heading_degrees: 129.089}
parked:
  - {type: hangar_monitor, position_m: [-25.854, 32.056], heading_degrees: 212.036}
  - {type: hangar_generator, position_m: [-65.9, -120.9], heading_degrees: 349}
  - {type: escort_shuttle, markings: 4, position_m: [-36.62, -104.98, -23.93],
     profile: NoPilot}
```

When no layer has `crane_arm` or `parked`, the hangar has its original
layout. A layer that sets them replaces the whole original layout.

- The crane arm swings in time with the first droid. Without a crane arm,
  there is none.
- The droids walk to the first `hangar_monitor` and the first
  `hangar_generator` of the list.
- The parked objects share the hangar's object slots with the droids, the
  shuttle and the player's craft. When there are too many, the last ones
  aren't placed, and the log says how many.

### cameras

`cameras` places the camera views that the number keys choose in the
hangar. Each key is `key_<number>`, with `[x, y, z]` from the hangar's
origin. The other keys
follow an object, such as a droid or the player's craft, and can't be set.

| Key | Default in the hangar | Default in the family base |
|-|-|-|
| `key_1` | `[27.588, -56.641, -7.324]` | `[19.043, -157.983, -121.509]` |
| `key_2` | `[30.273, -8.057, -17.09]` | `[-48.096, -215.088, -114.917]` |
| `key_3` | `[-27.344, 33.203, -19.287]` | `[61.279, -131.616, -123.706]` |
| `key_6` | `[-29.297, -37.354, -20.752]` | `[42.48, -206.567, -123.218]` |
| `key_7` | can't be set | `[77.637, 64.185, -92.212]` |
| `key_8` | can't be set | `[201.221, 161.133, 0.244]` |
| `key_9` | `[26.123, 113.281, -3.174]` | `[-326.172, 854.956, -159.595]` |

The view of the player's craft is `player_camera_m` in
[crafts](#crafts).

### family_base

The family base has its own `crane_arm`, `parked` and `cameras`, with the
same keys as in the hangar. Its cameras can set `key_1` to `key_3` and
`key_6` to `key_9`.

```yaml
family_base:
  crane_arm:
    type: hangar_crane
    markings: 1
    position_m: [21.362, -152.588]
    heading_degrees: 4
  cameras:
    key_7: [77.6, 64.2, -92.2]
```

## Ships

Each key of `ships` is the model of a command ship, such as `frigate2`. Its
value has every key of `default`, and two more.

| Key | Meaning |
|-|-|
| `hangar_model` | The model of this ship's hangar, a model of the mod in `flight/models/<model>.glb`. Without it, the ship uses the hangar model `hangar`. |
| `by_iff` | Settings for this ship when it has a given IFF, by IFF number from 0 to 255. Each value has every key of `default`. |

```yaml
ships:
  imperialstardestroyer2:
    hangar_model: isdhangar
    hangar:
      inverted_floor_height_m: 30
    player:
      inverted_floor_crafts: [tie_fighter, tie_interceptor, tie_bomber]
    by_iff:
      1:
        shuttle: {type: escort_shuttle}
```

## Crafts

Each key of `crafts` is a model, such as `xwing`.

| Key | Meaning |
|-|-|
| `rest_height_m` | `{normal: ..., inverted: ...}`. How high the model stands above the normal floor, and how far it hangs below the inverted floor. |
| `player_camera_m` | The view of the player's craft, as `[x, y, z]` from the craft, along the hangar's axes, for each of `hangar`, `hangar_inverted`, `family_base` and `family_base_inverted`. The `_inverted` views are for a craft on the inverted floor. |

`rest_height_m` applies to every object that stands on a floor, such as
the player's craft, the shuttle, the droids and the parked objects.
Without it, the B-wing stands 1.22 m high, the shuttle 3.37 m, and other
models half their height.

The view of the player's craft is key 7 in the hangar and key 5 in the
family base. Without `player_camera_m`, the craft uses its original view.
The view comes from the craft's own model, never from a swapped model.

Where the player's craft stands in the hangar is set in
[player](#player), with `offset_m`, `craft_offsets_m` and
`inverted_floor_crafts`.

## A mission's hangar section

A mission can change the hangar in the `hangar` section of its mission
file. [Missions](missions.md) describes the mission files. The section is
the first layer, so its settings come before the settings of
`hangars.yaml`. It has every key of `default`, and one more.

| Key | Meaning |
|-|-|
| `use_ship_settings` | A map from a command ship's model to the name of an entry of `ships`. The command ship uses that entry's settings and hangar model in place of its own. |

```yaml
hangar:
  droids:
    - {markings: 1}
  swaps:
    hangardroid: "hangardroidr2q5"
  use_ship_settings:
    frigate2: frigate2carrier
```

In this mission, a `frigate2` command ship uses the settings of
`ships.frigate2carrier`, and its hangar model.

The section is only read when the mod has `flight/hangars.yaml`. The
proving ground ignores it.

## Hangar lighting

`hangars.yaml` has no lighting keys. The game lights the hangar with its
own lights and with the reflection probes of the hangar model. Bake the
probes into the hangar model to light it well. [Models](models.md)
describes reflection probes.

The environment of the region where the player entered the hangar gives
only the sky seen through the hangar's opening. Its suns, cubes and fill
don't light the hangar. See [Environments](environments.md).

## Testing

When the game sets up the hangar of a command ship, the log names the
ship, its IFF, the hangar model, and the number of layers, parked objects
and swaps:

```text
hangar: ship 'frigate2' IFF 1 (settings), model 'frigate2hangar', 3 layers, 24 parked objects, 2 swaps
```

`(default settings)` means that `ships` has no entry for this ship.
