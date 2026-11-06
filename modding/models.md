# Models

The 3D models of a mod are glTF files that the model cooker turns into
`.glb` files for the game. A model holds the meshes and materials of an
object, and the data that the game needs to fly it: its parts, hardpoints,
engine glows, lights, moving parts and collision meshes.

This page explains that data for authors. The materials of a model, with
skins, animations and cockpit screens, are on the
[materials page](materials.md). The settings of a model that aren't part of
the glTF file, such as its reticle or the speed at which its engine glows
show, go in `flight/models/<key>.yaml` and are on the
[craft page](craft.md). The full format of the glTF extension is in
[AERON_flight_model](../aeron/tools/extensions/AERON_flight_model/README.md).

Most authors don't write this data by hand. The Blender add-on edits all of
it, and imports original models as a starting point. See the
[tools page](tools.md).

## Files

The game loads the model of an object from `flight/models/<key>.glb` in
the mod. The key is the name of the original model file, without its
folder and extension, in lower case. For example, `flight/models/xwing.glb`
replaces `FLIGHTMODELS/XWING.OPT`. The **Model** column of the
[object types](reference/object_types.md) page gives the key of each object
type. An object type file can give an object type another model with
`model` (see the [craft page](craft.md)).

| File | What the game uses it for |
|-|-|
| `<key>.glb` | The model of each object type whose model is `<key>`. |
| `<name>cockpit.glb` | The cockpit of the player's craft. |
| `<name>exterior.glb` | The player's own craft, seen from outside. |
| `tiecockpit.glb` | The cockpit of a TIE craft that has no cockpit of its own. |
| `combatsimcockpit.glb` | The cockpit of any other craft that has no cockpit of its own. |

Other settings can name a model that no object type uses, such as a
hangar. The game then loads `flight/models/<name>.glb` for it. The
[hangars page](hangars.md) describes them.

Only cooked `.glb` files work. The game doesn't read `.gltf` files, and it
can't load a `.glb` that the cooker didn't make. See
[cooked files](#cooked-files).

When the mod has no `.glb` for a key, the game uses the original model.

### Cockpit and exterior models

`<name>` is the cockpit name of the player's craft. It is
`craft_record.cockpit_name` in the craft's object type file (see the
[craft page](craft.md)). In the original game, the cockpit name is often
the model key, but not always. When you aren't sure of a craft's cockpit
name, set `cockpit_name` in its object type file.

The cockpit model is drawn in the cockpit view. The exterior model
replaces the craft's model when the player looks at their own craft from
outside, so that it can have more detail. When the mod has
`<name>.glb` but not `<name>exterior.glb`, the game no longer uses the
original exterior model, and it draws the craft's model instead.

The cockpit of a gunner seat is the model of the seat's object type. The
[craft page](craft.md) describes gunner seats.

The cockpit and exterior models use the part state of the player's craft,
part by part. Part 5 of the cockpit turns and hides with part 5 of the
craft. So the [moving parts](#moving-parts) and [part sets](#part-sets) of
the craft model can name parts of the cockpit or exterior model, even past
the craft model's own parts.

### When the game rejects a model

The game checks each model when it loads it. When a check fails, the game
writes `replacement '<key>' rejected` and the reason to the log, and the
model isn't loaded. A model is rejected when:

- it has no parts, or more than 255;
- it has more than 255 engine glows;
- it has more than 8 hardpoints of the types `gunner` and
  `acceleration_end` together;
- it has more than one `pilot_eye` hardpoint;
- its object type has the `float_hardpoints` flag and the model has no
  laser hardpoint (`none`, `rebel_laser`, `turbo_rebel_laser`,
  `empire_laser`, `turbo_empire_laser`, `ion_cannon`, `turbo_ion_cannon`,
  `super_rebel_laser`, `super_empire_laser`, `super_ion_cannon` or
  `super_mag_pulse`), or too many of them. A laser hardpoint more than
  6.25 m from the centre line counts twice, and the limit is 255;
- a position or vector isn't a finite number.

The game also writes a warning, but loads the model, when:

- a cockpit model has a `pilot_eye` hardpoint. The pilot eye belongs on the
  craft model;
- an object type with the `float_hardpoints` flag has no `main_hull` part.
  It then has no hull debris;
- a cockpit model has more than 5000 faces, or a part with very short edges
  and no collision mesh. The game checks the burn marks of hits on the
  cockpit against its faces;
- a part tag that must mark one part marks none, or several (see
  [part tags](reference/states_and_tags.md#part-tags));
- a moving part uses an unknown group, behaviour or setting.

## glTF conventions

- **Axes.** +Y is up and the nose points along +Z. The craft's right side
  is along −X. These are the glTF axes.
- **Units.** Lengths are in metres.
- **Meshes** are made of triangles. Give them normals. A mesh needs
  tangents when its material has a normal map.
- **Texture coordinates.** The game uses `TEXCOORD_0` only.
- **Textures** are PNG files next to the `.gltf`. The
  [materials page](materials.md#textures) describes them.
- **Transforms.** A node uses translation, rotation and scale, not a
  matrix. A missing translation, rotation or scale takes the glTF default.
  Each kind of node has its own rules, given in its section below.
- **No glTF animations or skinned meshes.** The game moves parts itself.
  See [moving parts](#moving-parts) and [cockpit controls](#cockpit-controls).

The game reads these glTF extensions:

| Extension | Use |
|-|-|
| `AERON_flight_model` | The game data of nodes and materials. List it in `extensionsUsed`, not in `extensionsRequired`, so that any glTF viewer can show the model. |
| `KHR_lights_punctual` | [Lights](#lights). |
| `KHR_materials_variants` | The looks of a model, for skins and markings. |
| `KHR_materials_emissive_strength` | Bright emissive surfaces. |
| `KHR_materials_specular` | Specular strength and colour, without textures. |
| `KHR_materials_transmission` | Glass. |

The [materials page](materials.md) describes the material extensions.

## The model tree

The scene has one root node, the model root. Its children, in order, are
the parts of the model. A part's children are its hardpoints, engine
glows, lights and controls, and at most one collision mesh.

```text
model root
  part 0
    hardpoints, engine glows, lights, controls, collision mesh
  part 1
    ...
```

Each node says what it is with `role` in its `AERON_flight_model`
extension. Every node of the tree needs a role. The model root has no
mesh, and each part has one.

Numbers come from the order of the children, starting at 0:

- parts are numbered in the order of the root's children;
- hardpoints are numbered in each part, in the order of its children;
- engine glows are numbered across the whole model, part by part;
- controls come in the order of the parts, then of their children.

Lights and collision meshes have no number. A model has at most 255 parts
and controls together.

The model root can have a translation, a rotation and a positive scale
that is the same on all axes. Its `AERON_flight_model` object is:

```json
{
  "name": "mycraft",
  "children": [1, 2, 3],
  "extensions": {
    "AERON_flight_model": {
      "role": "model",
      "energy": ["shields", "tractor", "jamming"]
    }
  }
}
```

| Key | Meaning |
|-|-|
| `role` | Required. `model`. |
| `energy` | The energy effects of the materials that don't give their own. See the [materials page](materials.md#energy-effects). Without it, those materials show none. |
| `articulations`, `articulationSettings` | The [moving parts](#moving-parts). |
| `componentSets` | The [part sets](#part-sets). |
| `markings`, `skinVariants` | The looks of the model. See the [materials page](materials.md#skins-and-markings). |
| `reflectionProbes`, `lightGrid`, `reflectionPlane` | [Probes](#probes-light-grid-and-reflection-plane). |

## Parts

A part is a mesh node under the model root. The game simulates each part
on its own. A part can be hit, damaged, destroyed, turned or hidden. It
can have a translation, a rotation and a positive scale that is the same on
all axes.

```json
{
  "name": "wing_top_left",
  "mesh": 3,
  "children": [40, 41],
  "extensions": {
    "AERON_flight_model": {
      "role": "component",
      "meshType": "rotary_wing",
      "explosion": ["hit_points", "detachable"],
      "rotation": {
        "pivot": [0, 0, -3.66],
        "rotationAxis": [0, 0, 1],
        "directionAxis": [-1, 0, 0],
        "upAxis": [0, 1, 0]
      }
    }
  }
}
```

| Key | Meaning |
|-|-|
| `role` | Required. `component`. |
| `meshType` | Required. The kind of part, such as `main_hull`, `engine` or `gun_turret`. The [mesh types](reference/model_values.md#mesh-types) page lists them. |
| `explosion` | A list of [explosion flags](reference/model_values.md#explosion-flags): `hit_points` gives the part its own hit points, and `detachable` lets it be damaged and break off. The default is none. |
| `targetId` | Parts with the same `targetId` and the same mesh type form one target. 0, the default, means none. |
| `target` | The point that the game aims at on this target, `[x, y, z]` in the part's space. Required when `targetId` isn't 0. |
| `rotation` | The part's hinge. See below. |
| `span`, `center`, `boundsMin`, `boundsMax` | The part's size, centre and bounds, `[x, y, z]` each. Give all four or none. Without them, the game computes them from the collision mesh, or else from the mesh. |
| `tags` | Names that let the game find the part. See below. |

The mesh type matters to the game. For example, `rotary_gun_turret` parts
turn on their hinge to aim, and the `component_strength` setting of the
[craft page](craft.md) gives hit points by mesh type.

### The hinge

`rotation` gives the frame that the part turns in, in the part's space:

| Key | Meaning |
|-|-|
| `pivot` | The point that the part turns about. |
| `rotationAxis` | The axis that the part turns about. |
| `directionAxis` | The part's forward direction. |
| `upAxis` | The part's up direction. |

All four are needed. The game uses the vectors as you write them, and
doesn't scale them to length 1. The length of `rotationAxis` scales the
turn, so an axis of length 2 turns the part about twice as far. Use
vectors of length 1, unless you want this. A part that never turns can
leave out `rotation`.

[Moving parts](#moving-parts) and turrets turn about this hinge.

### Part tags

A few models of the game have parts that the game moves, hides or reacts
to, such as the doors of the compactor. The game finds these parts by tag.
The [part tags](reference/states_and_tags.md#part-tags) page lists them.
When no part has the tag, the game uses the part with the same number as
in the original model.

A tag is 1 to 63 characters: lower-case letters, digits, `.` and `_`. The
game ignores the tags that it doesn't know.

## Hardpoints

A hardpoint is an empty node under a part. It marks a place on the model,
such as a laser's muzzle, the hangar or the pilot's eye. A hardpoint has a
translation only.

```json
{
  "name": "laser_top_left",
  "translation": [4.42, 1.70, 3.93],
  "extensions": {
    "AERON_flight_model": { "role": "hardpoint", "type": "rebel_laser" }
  }
}
```

| Key | Meaning |
|-|-|
| `role` | Required. `hardpoint`. |
| `type` | Required. The hardpoint type. The [hardpoint types](reference/model_values.md#hardpoint-types) page lists them and says what each one does. |

A weapon hardpoint is a place that a weapon fires from. The weapons
themselves come from the craft record (see the [craft page](craft.md)).
The craft record has up to 3 laser groups and 2 warhead groups. Each group
fires from the hardpoints of its weapon type, in the order of the parts.
A model can have up to 120 such firing positions. On a `gun_turret` or
`rotary_gun_turret` part, every second hardpoint of a type is the other
barrel of the one before it, and doesn't count. The game drops the
hardpoints past the limit without a warning, and a group that gets no
position past the limit loses its weapon. Warhead groups come last, so
they are the first to lose it.

### The pilot's eye

A `pilot_eye` hardpoint on the craft model sets where the pilot's eye is in
the cockpit view. A model can have one at most. Put it on the craft model.
The game ignores a `pilot_eye` in a cockpit model.

The `pilot_eye` node can also have a glTF perspective camera. Its `yfov`,
in radians, sets the vertical field of view of the cockpit view. The
camera's direction is ignored. Without a camera, the craft uses the
game's own field of view. The player's settings choose between this view,
a wider one and the game's own. The wide view is `cockpit_view` in the
model settings (see the [craft page](craft.md)).

```json
"cameras": [
  { "type": "perspective", "perspective": { "yfov": 0.8793, "znear": 0.01 } }
],
"nodes": [
  {
    "name": "pilot_eye",
    "translation": [0, 0.78, 0.81],
    "camera": 0,
    "extensions": {
      "AERON_flight_model": { "role": "hardpoint", "type": "pilot_eye" }
    }
  }
]
```

## Engine glows

An engine glow is an empty node under a part. The game draws a glow there
that grows with the throttle.

```json
{
  "name": "engine_glow_0",
  "translation": [-1.087, 1.10, -5.133],
  "rotation": [0, 1, 0, 0],
  "scale": [0.78, 0.78, 0.1],
  "extensions": {
    "AERON_flight_model": {
      "role": "engineGlow",
      "coreColor": [1, 1, 1, 1],
      "outerColor": [1, 0, 1, 0.5]
    }
  }
}
```

The node's frame sets the glow's direction and size:

- local +Z is the direction that the glow faces, +X is right and +Y is up.
  In the example, the rotation turns +Z toward the back of the craft;
- the X and Y scale are the full width and height of the glow. They must be
  positive;
- the Z scale is the glow's depth. It can be negative.

| Key | Meaning |
|-|-|
| `role` | Required. `engineGlow`. |
| `coreColor` | Required. The colour at the centre, `[r, g, b, a]`: 8-bit sRGB values divided by 255. The alpha is used too. |
| `outerColor` | Required. The colour at the edge, in the same form. |
| `enabled` | `false` doesn't draw the glow, but it keeps its number. The default is `true`. |

The `engine_glows` setting of the [craft page](craft.md) can hide a glow
below a speed. It names glows by number.

## Lights

A light is an empty node under a part, with a `KHR_lights_punctual` point
or spot light. The light gives the colour, the intensity in candela and
the range in metres. Without a range, the light ends where it becomes too
faint to see. The game ignores directional lights, and writes a warning.

A light shines while its part is shown, and moves with the part. A light
on a destroyed part is dark.

```json
"extensionsUsed": ["AERON_flight_model", "KHR_lights_punctual"],
"extensions": {
  "KHR_lights_punctual": {
    "lights": [
      { "type": "point", "color": [1.0, 0.9, 0.7], "intensity": 40, "range": 30 }
    ]
  }
},
"nodes": [
  {
    "name": "bay_light",
    "translation": [0, 3.5, -12],
    "extensions": {
      "KHR_lights_punctual": { "light": 0 },
      "AERON_flight_model": { "role": "light", "shape": "tube", "length": 3.2 }
    }
  }
]
```

| Key | Meaning |
|-|-|
| `role` | Required. `light`. |
| `shape` | `point` (the default), `tube` or `panel`. A spot light must be a `point`. |
| `radius` | For a point or a tube: the radius of the glowing ball, or the thickness of the tube, in metres. The default is 0.1. |
| `length` | Required for a tube: its length in metres, along the node's +X. |
| `size` | Required for a panel: `[x, y]` in metres, along the node's X and Y. |
| `oneSided` | For a tube: `true` makes it shine only toward the node's −Z. The default is `false`. A panel always shines only toward −Z. |
| `variants` | The looks that the light shines with. See below. |
| `baked` | `true` when the light is baked into the model's probes. See [probes](#probes-light-grid-and-reflection-plane). |
| `tags` | Names that the `surfaces` setting uses. See below. |

A key that doesn't apply to the shape is an error. For example, a `point`
can't have `length`.

The node has a translation, a rotation and a positive scale that is the
same on all axes. Lengths take the node's scale and its parents'. A spot
light points along the node's −Z. Its cone is the glTF cone: full light
within `innerConeAngle`, none past `outerConeAngle`.

The intensity is the light straight ahead for a one-sided light. A tube
gives out as much light as a point light of the same intensity, spread
along its length.

**Variants.** `variants` lists the names of the model's looks that the
light shines with, and `null` for the model as modelled. For example, a
bolt with one look per colour has one light per colour, each listed with
its look. Without `variants`, the light shines with every look. The
[materials page](materials.md#skins-and-markings) describes looks.

**Tags.** The `surfaces` setting of the [craft page](craft.md) can hide
the lights and materials with a tag while the craft is disabled or slow.
See [material and light tags](reference/states_and_tags.md#material-and-light-tags).
A model's materials and lights use at most 32 different tags.

## Cockpit controls

A control is a part of a model that the game moves, such as the flight
stick or the throttle lever of a cockpit. Its node is a child of a part or
of another control. Its origin is the hinge. It can have a mesh, and it
can have controls as children.

At value v, a control turns by v times its angle, or slides by v times its
offset. The value comes from its first tag that the game knows. The
[cockpit control tags](reference/states_and_tags.md#cockpit-control-tags)
page lists the tags and their values. A control without such a tag stays
as modelled. Only the controls of the player's cockpit move.

A stick that tilts both ways is two controls: an empty control that tilts
forward and back, which holds the stick mesh as a control that tilts left
and right. In this example, `stick_roll` is node 56:

```json
"nodes": [
  {
    "name": "stick_pitch",
    "translation": [0, 0.007, 0.78],
    "children": [56],
    "extensions": {
      "AERON_flight_model": {
        "role": "control",
        "rotate": { "axis": [1, 0, 0], "degrees": -8 },
        "tags": ["cockpit.stick.y"]
      }
    }
  },
  {
    "name": "stick_roll",
    "mesh": 53,
    "extensions": {
      "AERON_flight_model": {
        "role": "control",
        "rotate": { "axis": [0, 0, 1], "degrees": 10 },
        "tags": ["cockpit.stick.x"]
      }
    }
  }
]
```

| Key | Meaning |
|-|-|
| `role` | Required. `control`. |
| `rotate` | `{ "axis": [x, y, z], "degrees": d }`: at value v, the control turns by v × d degrees about `axis`, in the node's own frame. The axis can't be zero. |
| `translate` | `[x, y, z]`: at value v, the control slides by v times this offset, in metres, in the node's own frame. |
| `tags` | Up to 4 tags. The game uses the first one that it knows. |

A control has `rotate` or `translate`, not both. Its node has a
translation, a rotation and a positive scale that is the same on all axes.
A control moves with its parent, and shows and hides with its part.

## Collision meshes

A part collides with its own mesh. A collision mesh replaces that mesh for
collisions and hits. Use one when the mesh is very detailed, or when its
shape is a poor fit for hits.

A collision mesh is a mesh node under a part, at most one per part:

```json
{
  "name": "hull_collision",
  "mesh": 12,
  "extensions": {
    "AERON_flight_model": { "role": "collision" }
  }
}
```

The game never draws it, and ignores its materials. Its mesh is made of
triangles, and the node has no children. The node has a translation, a
rotation and a positive scale that is the same on all axes.

A part whose collision triangles all have zero area has no collision faces.
Nothing collides with it. To turn off the collisions of a part without
changing its meshes, use the `collision.off` [part set](#part-sets).

## Moving parts

Moving parts turn about their [hinge](#the-hinge) when the game asks for
it: S-foils, landing gear, hatches, hangar doors, a warheads bay, cargo
clamps and crew. The model root lists them in `articulations`. Each entry
turns one part:

```json
"articulations": [
  { "component": 3, "group": "sfoils", "angleDegrees": 16.875, "speedDegrees": 1.40625 },
  { "component": 4, "group": "sfoils", "angleDegrees": -16.875, "speedDegrees": 1.40625 },
  { "component": 9, "group": "landing_gear", "angleDegrees": 90,
    "speedDegrees": 5.625, "speedBackDegrees": 2.8125 }
],
"articulationSettings": { "sfoils.close_in_hyperspace": 1 }
```

| Key | Meaning |
|-|-|
| `component` | Required. The number of the part, 0 to 254. It can be past the model's own parts, to move a part of the cockpit or exterior model. |
| `group` | Required. The group that the part moves with. See below. |
| `angleDegrees` | Required. The angle that the part turns to, in degrees, from −180 to 358.594. The model as built is at 0. |
| `speedDegrees` | Required. How far the part turns toward the angle at each step, in degrees, from 0 to 358.594. |
| `speedBackDegrees` | How far the part turns back toward 0 at each step. The default is `speedDegrees`. |
| `behavior` | For `crew` only. See [crew](#crew). |

The game turns parts in steps of 1/256 of a turn, 1.40625 degrees. It
rounds angles and speeds to the nearest step. A step lasts `step_ticks`
game ticks, 15 by default. For example, the S-foils above turn 16.875
degrees in 12 steps.

A part can be in several groups. The game ignores the groups that it
doesn't know, and writes a warning.

In a model with moving parts, when a part of mesh type `bridge` turns, the
game turns the rest of the model the other way around the model's origin,
so the bridge stays level, as on the B-wing. Only give the `bridge` type to
a moving part when you want this.

### Groups

| Group | When the parts move |
|-|-|
| `sfoils` | The S-foils. At 0 they are open, and at their angle they are closed. The S-foil key opens and closes them. The craft can't fire while they move, or while they are closed, unless `sfoils.allow_fire_closed` is on. A craft leaving the hangar starts with them closed. |
| `landing_gear` | The landing gear. At 0 it is up, and at its angle it is down. The landing gear key raises and lowers it. The craft can't fire while it is down. A craft in the hangar has it down. When the model also has `hatches`, the gear goes up as the craft climbs out of the hangar. Otherwise it goes up after the craft leaves the hangar. |
| `hatches` | The hatches. At 0 they are closed, and at their angle they are open. They are open while the craft is parked in the hangar at the start of a mission, close while it lifts off and flies out, and open again when it lands back in the hangar. The hangar shuttle also uses them. Hatches don't move in multiplayer games. |
| `hangar_doors` | The hangar doors of a ship of genus `starship`, `freighter` or `platform`. They open while a craft of its team (genus `fighter`, `utility`, `transport` or `pilot_droid`) is nearer than `hangar_doors.distance`, and no craft of another team is. Otherwise they close. |
| `warheads_bay` | The warheads bay of the player's craft. It opens while the player has warheads selected. |
| `cargo_clamps.open` | The cargo clamps, open. The clamps turn to this angle while the craft moves in to pick something up, and while it releases it. They turn back to 0 once the load is gone. |
| `cargo_clamps.close` | The cargo clamps, holding a load. |
| `cargo_clamps.<model>` | The cargo clamps, holding the model `<model>`. The game uses this group instead of `cargo_clamps.close` when the model has it. |
| `crew` | Heads and limbs of the crew. See [crew](#crew). |

The S-foils and the landing gear get in each other's way. When a model has
both, the landing gear only goes down while the S-foils are closed, and the
S-foils only open while the landing gear is up. Each key does nothing
while the other group moves.

In `cargo_clamps.<model>`, `<model>` is the key of the carried model, with
each character other than `a`–`z`, `0`–`9` and `_` written as `.` and two
hex digits. For example, `z-95` becomes `cargo_clamps.z.2d95`. The clamp
groups turn each part to its own angle, from either side. A part in
`cargo_clamps.open` and `cargo_clamps.close` can have a different angle in
each.

A mission can start a flight group with its S-foils closed or its landing
gear down. See the [missions page](missions.md).

### Settings

`articulationSettings` holds whole numbers. A setting that is 0 or missing
is off.

| Setting | Meaning |
|-|-|
| `step_ticks` | The length of a step, in game ticks. The default is 15. It applies to every group except `crew`. |
| `sfoils.close_in_hyperspace` | 1 closes the S-foils when the craft enters hyperspace, and opens them again when it arrives. AI craft then arrive with closed S-foils and open them soon after. |
| `sfoils.allow_fire_closed` | 1 lets the craft fire while its S-foils are closed. It still can't fire while they move. |
| `sfoils.park_closed` | 1 makes an AI craft close its S-foils when it is ordered to park. When parking starts, its S-foils close and its landing gear goes down at once. When parking ends, its S-foils open. |
| `hangar_doors.distance` | How near a craft must be to open the hangar doors, in game units (40.96 per metre). The default is 16384, which is 400 m. |
| `crew.always_animate` | 1 makes the crew move on every object that uses the model, in every view. |

### Crew

`crew` entries move the heads and limbs of a crew. Each part swings from
−angle to +angle around the model as built, at its speed. The `behavior`
of the entry says how:

| Behaviour | Movement |
|-|-|
| `swing` | Back and forth from one end to the other. This is the default. |
| `random` | Back and forth, but it can turn around at any time. |
| `random_pause` | Like `random`, but it stops for a while halfway to each end. It only stops when its steps land exactly on half the angle, so use a `speedDegrees` that divides half of `angleDegrees`. |

Without `crew.always_animate`, the crew only moves on the player's craft,
in an external view, in a single-player game. This is what the original
game does.

```json
{ "component": 23, "group": "crew", "angleDegrees": 11.25,
  "speedDegrees": 2.8125, "behavior": "random" }
```

### Flight group variants

A model can have other angles and speeds for some flight groups. An entry
whose group is `variant.<name>.<group>` belongs to the variant `<name>`.
`<name>` uses `a`–`z`, `0`–`9` and `_`. A mission chooses the variant of
each flight group (see the [missions page](missions.md)).

For an object of a flight group that uses variant `<name>`, the variant's
entries replace the model's entries with the same group and part. The
variant can also add parts to a group. Objects of the other flight groups
ignore variant entries. `crew` can't have variants.

```json
{ "component": 3, "group": "variant.wide.sfoils", "angleDegrees": 22.5,
  "speedDegrees": 1.40625 }
```

## Part sets

The model root can list named sets of parts in `componentSets`. Each set
is a list of part numbers, 0 to 254. Like moving parts, a set can name
parts past the model's own, to reach the cockpit or exterior model.

```json
"componentSets": {
  "hidden_by_profile.default": [25, 26],
  "hidden_by_profile.nopilot": [23, 25, 26],
  "collision.off": [40],
  "targetable": [0, 4, 5, 6, 7]
}
```

| Set | Meaning |
|-|-|
| `hidden_by_profile.<profile>` | The parts that profile `<profile>` hides. |
| `collision.off` | Nothing collides with these parts, such as the force field across a hangar opening. |
| `targetable` | The only parts that the player can target. Without the set, every part can be targeted. |

The game ignores the sets that it doesn't know.

### Profiles

A profile is a version of a model with some parts hidden, such as a craft
without its astromech droid. The game hides the parts of a profile when the
object appears, as if they were destroyed.

`<profile>` is the profile's name in lower case, with each character other
than `a`–`z`, `0`–`9` and `_` written as `.` and two hex digits. For
example, profile `Default` is the set `hidden_by_profile.default`.

The game picks the profile of each object in this order:

1. the profile that the mission gives the object's flight group (see the
   [missions page](missions.md));
2. in a quick start or a skirmish, `Skirmish`, when the model has it;
3. `Default_<markings>`, where `<markings>` is the object's markings
   value, such as `Default_2`;
4. when that profile hides nothing, `Default`.

Laser bolts, warheads and other objects that aren't craft can use profiles
too. The game picks their profile in this order:

1. the profile that the mission sets for this object when a given type of
   craft fires it (see `object_profiles.weapons` on the
   [missions page](missions.md#object_profiles));
2. in a quick start or a skirmish, `Skirmish`, if the model has it;
3. `Default`.

For example, a bolt model can contain both a short bolt and a long beam, and
its `Default` profile hides the beam. A mission can then show the beam only
on the shots of one type of craft.

## Probes, light grid and reflection plane

Probes give the inside of a model its own light and reflections: a hangar,
or the bays of a capital ship that craft fly into. The game lights the
model and every object inside the probes with them. Outside the probes,
the sky lights these objects. Add probes to models whose insides the
player sees up close. Other models don't need them.

The model root holds three things:

- **Reflection probes**, `reflectionProbes`: up to 42. Each one records
  the reflections around it.
- **The light grid**, `lightGrid`: a box of points that record the light
  inside the model.
- **The reflection plane**, `reflectionPlane`: a floor, such as a hangar's,
  that reflects the craft standing on it. The probes can't show these
  craft, because they are baked without them. The game uses the plane only
  in a model that has probes.

```json
"reflectionProbes": [
  { "position": [0.0, 1.2, -4.5], "size": [20.0, 8.0, 30.0], "shape": "box",
    "falloff": 0.2, "roomParallax": true, "clipStart": 0.8, "clipEnd": 40.0 }
],
"lightGrid": { "position": [0.0, 4.0, -10.0], "size": [40.0, 12.0, 60.0],
               "spacing": 4.0, "resolution": [9, 2, 14], "keyLightShadow": 0.7 },
"reflectionPlane": { "position": [0.0, 0.0, -10.0], "size": [40.0, 60.0], "distance": 0.1 }
```

Positions and sizes are in metres, in the model's frame. Boxes are aligned
with the model's axes. The
[extension page](../aeron/tools/extensions/AERON_flight_model/README.md#probes)
describes each key.

The probes and the light grid must be baked before the game can use them.
The Blender add-on bakes them, and the cooker puts the bake into the
`.glb`. See the [tools page](tools.md). A cooked file holds the bake, not
`reflectionProbes` and `lightGrid`. The reflection plane needs no bake.

The lights inside the light grid's box are baked into the probes. The
Blender add-on sets `baked` on them. The game doesn't use these lights
while the probes light the scene, so their light isn't counted twice.

## Cooked files

The cooker, `aeron_gltf_cook`, turns a `.gltf` and its PNG textures into
one `.glb` for the game:

```sh
aeron_gltf_cook mycraft/mycraft.gltf --out my-mod/flight/models/mycraft.glb \
    --effects-dir my-mod/flight/effects
```

It checks the model, packs and compresses the textures, adds the data that
the game computes from the meshes, and loads the result as the game does.
When the model has a problem, the cooker stops and says why.

The cooker writes its own data into the `.glb`, in the
`AERON_flight_model_cooked` extension. Don't edit cooked files. Change the
`.gltf` and cook it again. Keep the `.gltf` and its textures, because
Blender can't read the textures of a cooked `.glb`.

The [tools page](tools.md) describes the cooker's options and how to cook a
whole project at once.
