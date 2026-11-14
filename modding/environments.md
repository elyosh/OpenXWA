# Environments

An environment is a named set of lighting for a region of a mission. It
can have a sky, lighting cubes for the 3D models, an ambient light, up to
four suns and an overlay drawn over the sky. The file
`flight/environments/environments.yaml` lists the environments and says
which mission regions use them.

Without the file, every region uses the game's own sky, with the
starfield, the backdrop images and the lights that the backdrops give.

When the file has an error, OpenXWA doesn't start. It shows an error, and
the log gives the file, the line and the problem.

This page also describes the `suns` section of `flight/flight.yaml`, which
changes how the backdrop suns and the lens flare look.

## A first environment

This file gives every region of every mission one sky with one sun, except
region 1 of one mission, which keeps the game's own sky:

```yaml
environments:
  red_nebula:
    intensity: 59.2
    radiance: "flight/environments/red_nebula/sky.ktx2"
    diffuse: "flight/environments/red_nebula/diffuse.ktx2"
    diffuse_strength: 1
    specular: "flight/environments/red_nebula/specular.ktx2"
    specular_strength: 1
    ambient_fill: [1, 1, 1]
    directional_lights:
      - name: sun
        direction_cube: [0.5, 0.3, -0.8]
        color_linear: [1, 0.9, 0.8]
        intensity: 186
        primary: true
        casts_shadows: true
        angular_radius_degrees: 0.27
        lens_flare: true
        visual:
          kind: disc
          radiance_scale: 1
          glow_radius_degrees: 3
          glow_strength: 0.5
xwa:
  default: red_nebula
  missions:
    "missions/1b0m1fw.tie":
      regions:
        "1": none
```

The three `.ktx2` files are cube images.
[Making the images](#making-the-images) shows how to make them with
`environment_bake`.

## The file

| Key | Meaning |
|-|-|
| `environments` | Required. The environments, by name. Use `{}` for none. |
| `xwa` | Required. Which environment each mission region uses. Use `{}` to give no region an environment. |

Other keys are an error.

Texture paths in the file are relative to the mod folder, such as
`flight/environments/red_nebula/sky.ktx2`. The textures must be in the
mod.

## Environments

Each key of `environments` is the name of an environment. A name has 1 to
63 characters: lower-case letters, digits, `.` and `_`. A file can have up
to 256 environments.

| Key | Meaning |
|-|-|
| `intensity` | Required. Multiplies the sky, the overlay and the two lighting cubes. 0 to 1000. See [Light units](#light-units). |
| `radiance` | The sky: a cube image that the game draws behind everything. Without it, the region keeps the game's starfield and backdrops. |
| `diffuse` | The cube that gives the soft light that the sky casts on models. |
| `diffuse_strength` | Multiplies the light of `diffuse`. 0 to 4. Give it with `diffuse`, and only with it. |
| `specular` | The cube that models reflect. It needs `diffuse`. |
| `specular_strength` | Multiplies the reflections of `specular`. 0 to 4. Give it with `specular`, and only with it. |
| `ambient_fill` | The least soft light that models get, `[r, g, b]` radiance in W/m²/sr, 0 to 1000 each. Where the `diffuse` cube gives less light than this, the game adds some of the fill until the light is as bright as the fill. Where the cube is brighter, the fill does nothing. `intensity` doesn't scale it. The default is `[0, 0, 0]`. |
| `rotation_degrees` | Turns the sky and its suns, `[yaw, pitch, roll]`. The default is `[0, 0, 0]`. See [Directions](#directions). |
| `directional_lights` | The suns, up to four. See [Suns](#suns). It is required with `radiance`. Use `[]` for a sky without suns. |
| `overlay` | A cube with transparency, drawn over the sky. See [Overlays](#overlays). It needs `radiance`. |

Any other key is an error.

### What an environment replaces

While a region uses an environment:

- The environment's `ambient_fill` replaces the game's ambient light. The
  region has no other ambient light, so set a fill unless the cubes give
  enough light everywhere. Without a `diffuse` cube, the fill is all of the
  ambient light.
- The `diffuse` and `specular` cubes light the models.
- With `radiance`, the sky replaces the starfield, the backdrop images
  (planets, nebulae, suns), the lights that the backdrops give and their
  lens flares. The environment's suns take their place.
  [Backdrops](#backdrops) shows how to keep the backdrops over the sky.
- With `directional_lights`, the environment's suns replace the lights of
  the backdrops, even in an environment without a sky. An empty list
  removes them. Without the key, the region keeps the game's lights.

An environment without `radiance` changes only the lighting. For example,
this environment keeps the starfield, the backdrops and their lights, and
sets the ambient light:

```yaml
environments:
  dim_station:
    intensity: 1
    ambient_fill: [0.3, 0.3, 0.35]
```

### Directions

Environments use the cube's own directions: +X is right, +Y is up and -Z
is forward. In flight, the cube's +X is the world's +X axis, its +Y (up)
is the world's +Z axis, and its -Z (forward) is the world's +Y axis.

In the coordinates of a mission's `.TIE` file, the world's +Y is the
file's −Y, and the other axes are the same. So the sky's forward face is
toward the file's −Y, its right face toward +X and its top face toward +Z.
A flight group with a yaw of 0 in the file faces the sky's forward face.

`rotation_degrees` is `[yaw, pitch, roll]`. The game applies the roll,
then the pitch, then the yaw:

- A positive yaw turns the sky to the left around the up axis: what was
  ahead moves to the left.
- A positive pitch moves what was ahead upward.
- A positive roll turns the sky counter-clockwise as you look forward.

The rotation turns the sky, the lighting cubes and the suns together. It
doesn't turn the overlay, which has its own rotation.

### Overlays

An overlay is a cube with transparency, such as an asteroid field or a
nebula in front of the stars. The game draws it over the sky and over the
backdrops that it keeps, and under everything else. It doesn't light
anything. `intensity` scales it like the sky.

```yaml
environments:
  rocky_nebula:
    intensity: 59.2
    radiance: "flight/environments/nebula/sky.ktx2"
    directional_lights: []
    overlay:
      cube: "flight/environments/asteroids/overlay.ktx2"
      rotation_degrees: [10, -40, 0]
```

| Key | Meaning |
|-|-|
| `cube` | Required. The overlay's cube image, a BC7 sRGB cube made with `environment_bake --overlay-faces`. |
| `rotation_degrees` | Turns the overlay, `[yaw, pitch, roll]`, as for the sky. The default is `[0, 0, 0]`. The environment's rotation and a region's rotation don't turn it. |

## Suns

`directional_lights` is a list of up to four suns. Each sun lights the
models from one direction, like a light that is very far away. In an
environment with a sky, the game also draws each sun in the sky.

| Key | Meaning |
|-|-|
| `name` | Required. The sun's name, unique in the environment. Same rules as an environment name. |
| `direction_cube` | Required. `[x, y, z]`, the direction from the scene toward the sun, in the cube's directions. The length doesn't matter, but it can't be zero. |
| `color_linear` | Required. The sun's colour, `[r, g, b]` linear, 0 to 16 each. |
| `intensity` | Required. How much light the sun gives, 0 to 2000. See [Light units](#light-units). |
| `primary` | Required. `true` for the main sun. At most one sun can be primary. |
| `casts_shadows` | Required. `true` if the sun casts shadows. Only the primary sun can cast shadows. |
| `angular_radius_degrees` | Required. The size of the sun's disc as seen from the scene, as a radius, 0.01 to 5. The real sun is about 0.27. A larger sun gives softer shadows. |
| `lens_flare` | Required. `true` if the sun makes a lens flare. |
| `visual` | How the game draws the sun in the sky: a disc or a sprite. Required in an environment with `radiance`. Without a sky, the sun is never drawn and `visual` can be left out. |

The primary sun is the main light. It is the only sun that can cast
shadows, and the only one that can make the procedural lens flare. With no
primary sun, no sun casts shadows.

### Disc

A disc is a round sun drawn by the game, with an optional glow around it.

```yaml
visual:
  kind: disc
  radiance_scale: 1
  glow_radius_degrees: 3
  glow_strength: 0.5
```

| Key | Meaning |
|-|-|
| `kind` | `disc`. |
| `radiance_scale` | Required. How bright the disc is, 0 to 16. At 1, the visible disc gives exactly the light that the sun casts. |
| `glow_radius_degrees` | The radius of the glow around the disc, 0 to 30. It must be at least `angular_radius_degrees` when `glow_strength` isn't 0. |
| `glow_strength` | The glow's light, as a multiple of the disc's light, 0 to 16. The default is 0: no glow. |

A real sun is far brighter than the screen can show. The game draws a disc
no brighter than a fixed limit, so the glow gives the halo. The limit
doesn't change the light that the sun casts.

### Sprite

A sprite is an image of the sun, drawn as a square in the sky.

```yaml
visual:
  kind: sprite
  sprite: "flight/environments/red_nebula/sun.ktx2"
  angular_size_degrees: 4
  sprite_strength: 1
```

| Key | Meaning |
|-|-|
| `kind` | `sprite`. |
| `sprite` | Required. The image, a KTX2 file made with `environment_bake --sprite`. |
| `angular_size_degrees` | Required. The width of the square in the sky, 0.01 to 60. |
| `sprite_strength` | Required. Multiplies the image's values, 0 to 1000. `intensity` doesn't scale sprites. |

The game adds the sprite to the sky, so its background must be black.

### Lens flares

When the `suns` section of `flight/flight.yaml` turns on the procedural
flare, only the primary sun makes a flare, if its `lens_flare` is `true`.
Otherwise, in an environment with a sky, each sun with `lens_flare: true`
makes the game's classic flare, made of the images of group 1000 (see
[Image groups](reference/image_groups.md)).

## Choosing environments

The `xwa` section says which environment each region of each mission uses.

```yaml
xwa:
  default: deep_space
  missions:
    "missions/1b0m1fw.tie":
      regions:
        "0": red_nebula
        "1":
          environment: red_nebula
          rotation_degrees: [90, 0, 0]
        "2": none
```

| Key | Meaning |
|-|-|
| `default` | The environment of every region that has no entry. Without it, these regions keep the game's own sky. It can't be `none`. |
| `missions` | The regions of each mission, by mission path. |

The key of a mission is its path in the game folder, such as
`missions/1b0m1fw.tie`. It can be written in any case, with `/` or `\`.
Each mission has `regions`, a map from a region number to an environment.
Regions are numbered `"0"` to `"3"`, and 0 is the mission's first region.

A region's value is one of:

- the name of an environment;
- `none`, to keep the game's own sky in this region, even when there is a
  `default`;
- a map, with the keys below.

| Key | Meaning |
|-|-|
| `environment` | Required. The name of the environment. |
| `rotation_degrees` | Replaces the environment's `rotation_degrees` in this region, so one environment can face other ways in other places. It doesn't turn the overlay. |
| `backdrops` | `replace` or `keep`. See [Backdrops](#backdrops). The default is `replace`. |
| `hide_backdrops` | `all`, or a list of backdrop images to hide. Needs `backdrops: keep`. |
| `show_backdrops` | `all`, or a list of backdrop images to show, even when `hide_backdrops` hides them. Needs `backdrops: keep`. |
| `hidden_backdrop_lights` | `keep` or `remove`, for the lights of hidden backdrops. Needs `backdrops: keep`. The default is `keep`. |
| `backdrop_light_scale` | Multiplies the light of the backdrops that aren't suns, 0 to 4. Needs `backdrops: keep`. The default is 1. |

`xwa.default` can also be a map with these keys.

### Backdrops

By default, a sky replaces the mission's backdrops. With `backdrops: keep`,
the game draws the sky, then the mission's backdrops (planets, nebulae and
suns) over it. Use it when the sky only replaces the stars, and the
mission's planets must stay.

With `keep`:

- The environment's suns aren't drawn, and its `directional_lights` aren't
  used. The backdrops keep their lights and their lens flares.
- The game makes the diffuse and specular lighting from the sky and the
  backdrops that it shows, so that planets and nebulae light the ships.
  The environment's `diffuse_strength` and `specular_strength` scale it.
  When the region shows no backdrop other than suns, the game uses the
  environment's own cubes.
- `hide_backdrops` hides backdrop images, and `show_backdrops` shows them
  again. An image is hidden when `hide_backdrops` lists it or is `all`,
  unless `show_backdrops` lists it or is `all`.
- A hidden backdrop still gives its light. With
  `hidden_backdrop_lights: remove`, the hidden backdrops that aren't suns
  give no light. Suns always keep their light.
- `backdrop_light_scale` multiplies the light of every backdrop that isn't
  a sun, hidden or not. Use it when the missions' planets and nebulae light
  the ships too much.

The backdrop keys are an error without `backdrops: keep`. They have no
effect in an environment without a sky, because it keeps the backdrops
anyway.

A backdrop image is written as `"group-image"`, with the image group and
the number of the image in the game's data, such as `"6104-0"`. A list
holds up to 64 images. To find the images of a region, set `backdrops: keep`, play
the region, and read the log. It has a line like this one, with
`(hidden)` after each hidden image:

```text
environment: region 0 keeps its backdrops: 6104-0 6104-5 (hidden) 9001-1100
```

In this region, the game shows one sun and one planet over the sky, and
hides the other backdrops:

```yaml
"0":
  environment: blue_stars
  backdrops: keep
  backdrop_light_scale: 0.25
  hide_backdrops: all
  show_backdrops: ["9001-1100", "6104-0"]
```

The groups of planets, nebulae, galaxies and suns are in
[Image groups](reference/image_groups.md).

### Special cases

- A Death Star region uses its own entry, like any other region. Without
  one, there is no sky, an even fill replaces the light of the mission's
  backdrop, and the headlights are on. `default` doesn't apply there.
- No environment is used during a hyperspace jump.
- In a hangar, the game uses the environment of the region where the
  player entered the hangar, but only its sky. The hangar has its own
  lighting. See [Hangars](hangars.md#hangar-lighting).
- When an environment's textures can't be loaded, the region keeps the
  game's own sky, and the log says why.

## Cube images

The game reads environment images as KTX2 files. `environment_bake` makes
them all.

| Image | Used by | Format | Default size |
|-|-|-|-|
| Sky | `radiance` | BC6H HDR cube, with mip levels | 2048 pixels per face |
| Diffuse | `diffuse` | BC6H HDR cube | 32 pixels per face |
| Specular | `specular` | BC6H HDR cube, one mip level per roughness | 512 pixels per face |
| Overlay | `overlay.cube` | BC7 sRGB cube, colour multiplied by alpha | 2048 pixels per face |
| Sun sprite | `visual.sprite` | BC6H HDR 2D image, with mip levels | 256 pixels |

The game also accepts RGBA16F in place of BC6H. Each image must be the
right kind: a cube for the sky, the lighting cubes and the overlay, and a
2D image for a sprite.

- The **sky** is what the player sees. It is an HDR image, so a bright
  nebula can be brighter than white.
- The **diffuse** cube is the sky blurred over a half sphere. It gives the
  soft light that falls on a surface from the sky.
- The **specular** cube is the sky blurred at several roughnesses. Shiny
  surfaces reflect its sharp levels, and rough surfaces its blurred ones.
- The **overlay** has transparency, so it is an 8-bit image with alpha.

The sky must not show the suns. The game draws each sun from its
`visual`, and a sun left in the image would light the models twice.

## Making the images

`environment_bake` makes the cube images and writes the environment entry
for them. [Tools](tools.md) says where to find it. It takes one of these
inputs:

- an equirectangular panorama, `--input`;
- six cube faces, `--faces`;
- six overlay faces, `--overlay-faces`;
- one sun image, `--sprite`;
- a capture of a region by the game, `--capture`.

The output folders must exist before you run it.

### From a panorama

The panorama is a Radiance `.hdr` image, twice as wide as it is high. The
centre of the image is forward (-Z), and the top is up (+Y). For a sharp
sky, make it at least π times as wide as the sky's face size. For the
default size of 2048, that is 6434 pixels.

```sh
environment_bake --input red_nebula.hdr --name red_nebula \
  --radiance-out my-mod/flight/environments/red_nebula/sky.ktx2 \
  --diffuse-out my-mod/flight/environments/red_nebula/diffuse.ktx2 \
  --specular-out my-mod/flight/environments/red_nebula/specular.ktx2 \
  --lights-input red_nebula_lights.yaml \
  --manifest-root my-mod --environment-out red_nebula.yaml
```

| Option | Meaning |
|-|-|
| `--name` | Required. The environment's name in the written entry. |
| `--radiance-out` | The sky cube. Without it, see [Lighting only](#lighting-only). |
| `--diffuse-out` | Required. The diffuse cube. |
| `--specular-out` | Required. The specular cube. |
| `--lights-input` | A file with the suns. See [Suns in a bake](#suns-in-a-bake). |
| `--environment-out` | Writes the environment entry to this file. Without it, the entry goes to the console. |
| `--manifest-root` | The mod folder. The entry's texture paths are written relative to it, and the outputs must be inside it. Without it, the paths are written as you give them. |
| `--radiance-size` | The sky's face size, a power of two from 16 to 4096. The default is 2048. |
| `--diffuse-size` | The diffuse cube's face size, a power of two from 4 to 256. The default is 32. |
| `--specular-size` | The specular cube's face size, a power of two from 16 to 2048, and no larger than the sky. The default is 512. |
| `--specular-samples` | Samples per pixel for the specular cube, 16 to 65536. More is smoother and slower. The default is 1024. |
| `--jobs` | The number of threads. The default is 8. The result doesn't depend on it. |

The written entry is a complete `environments:` file with one environment.
Copy the environment into `environments.yaml`. It starts with
`intensity: 1`, both strengths at 1, no fill and no rotation. Set them for
your sky.

The baker warns when the sky is bright around a sun's direction. That
usually means that the sun was left in the image.

### From six faces

The faces are files named `<prefix>_px`, `_nx`, `_py`, `_ny`, `_pz` and
`_nz`, each `.hdr`, `.png` or `.jpg`. They must all be HDR, or all PNG and
JPEG. They are square, all the same size, and at least as large as the sky:
with 1024-pixel faces, give `--radiance-size 1024`.

The faces use the standard layout of KTX2 cube maps, in the cube's
directions: `_nz` is the forward face. To check the layout, see
[Checking the orientation](#checking-the-orientation).

```sh
environment_bake --faces faces/red_nebula --name red_nebula \
  --radiance-out my-mod/flight/environments/red_nebula/sky.ktx2 \
  --diffuse-out my-mod/flight/environments/red_nebula/diffuse.ktx2 \
  --specular-out my-mod/flight/environments/red_nebula/specular.ktx2 \
  --radiance-size 1024 \
  --manifest-root my-mod --environment-out red_nebula.yaml
```

This reads `faces/red_nebula_px.png` and the five other faces. It takes
every option of `--input`, and `--ldr-intensity`.

HDR faces are linear. PNG and JPEG faces are sRGB, and white is 1.
`--ldr-intensity` multiplies them. You can leave it at 1 and set the
environment's `intensity` instead: `intensity: 59.2` makes white as bright
as a white pixel of the game's backdrop images.

### Suns in a bake

`--lights-input` reads a YAML file with only a `directional_lights` list,
with the keys of [Suns](#suns). The baker copies the suns into the
environment entry. With a sky, give each sun a `visual`, or the entry isn't
valid.

```yaml
directional_lights:
  - name: sun
    direction_cube: [0.5, 0.3, -0.8]
    color_linear: [1, 0.9, 0.8]
    intensity: 186
    primary: true
    casts_shadows: true
    angular_radius_degrees: 0.27
    lens_flare: true
    visual:
      kind: sprite
      sprite: "flight/environments/red_nebula/sun.ktx2"
      angular_size_degrees: 4
      sprite_strength: 1
```

The sprite path is copied as written, so write it relative to the mod
folder.

### Lighting only

Without `--radiance-out`, the baker makes only the diffuse and specular
cubes. Use it for an environment without a sky:

```sh
environment_bake --input station_interior.hdr --name station_light \
  --diffuse-out my-mod/flight/environments/station/diffuse.ktx2 \
  --specular-out my-mod/flight/environments/station/specular.ktx2 \
  --manifest-root my-mod --environment-out station_light.yaml
```

The entry lists suns only when you give `--lights-input`. Without suns, the
region keeps the game's lights.

### Overlay

The overlay faces are PNG files with alpha, named like the sky faces, and
at least as large as `--radiance-size`.

```sh
environment_bake --overlay-faces faces/asteroids \
  --overlay-out my-mod/flight/environments/asteroids/overlay.ktx2 \
  --radiance-size 2048
```

The baker writes no entry for an overlay. Add `overlay` to an environment
by hand.

### Sun sprite

The input is a square Radiance `.hdr` image of the sun on a black
background. A background that isn't black shows as a square in the sky,
and the baker warns about it.

```sh
environment_bake --sprite sun.hdr \
  --sprite-out my-mod/flight/environments/red_nebula/sun.ktx2
```

| Option | Meaning |
|-|-|
| `--sprite-size` | The sprite's size, a power of two from 16 to 1024. The default is 256. |
| `--sprite-format` | `bc6h` or `rgba16f`. The default is `bc6h`. |

### Checking the orientation

```sh
environment_bake --diagnostic-faces my-mod/flight/environments/test/sky.ktx2
```

This writes a sky with a label on each face. Use it as the `radiance` of an
environment, and look around in flight to see which face is where.

### Capturing a region

The debug packages of OpenXWA can capture the sky of a region as six faces,
with an image of each backdrop sun and a list of the suns. Open the debug
overlay, then the **Environment** tool, and press **Capture region** in
flight. The capture is saved in the user folder, under
`captures/environments/<mission>/region_<n>/`.

```sh
environment_bake --capture "<user folder>/captures/environments/<mission>/region_0" \
  --bake-root my-mod
```

The baker writes the cubes, the sun sprites, `environment.yaml` and
`mapping.yaml` into
`my-mod/flight/environments/captured/<mission>/region_<n>/`, where
`<mission>` is the mission path with `_` in place of `/`. Copy the
environment into `environments.yaml`, and the mapping under
`xwa.missions`.

## Light units

The game measures light in physical units, so that the suns, the sky and
the game's own lights match.

| Value | Unit | 1 means |
|-|-|-|
| A sun's `intensity` | the light that falls on a surface facing the sun, in W/m² | 683 lux |
| An environment's `intensity`, `sprite_strength` | multiplies the image's values to give the light that the sky sends, in W/m²/sr | 683 cd/m² for a pixel of 1 |
| `ambient_fill` | the least light that comes from every direction, in W/m²/sr | 683 cd/m² |

Use these values to start:

- A sun of 186 is as bright as the sun at the Earth's distance, about
  127,000 lux. It is also the light of a white sun in the original
  missions. The maximum, 2000, is about ten times that.
- For a sky made from 8-bit images, `intensity: 59.2` makes a white pixel
  as bright as a white pixel of the game's backdrop images.
- For a sky made from HDR images in physical units, use `intensity: 1`.
- An ambient fill of 1 gives a surface about 3 W/m², under 2% of the light
  of a sun of 186.

`color_linear` sets the colour of a sun, and `intensity` its brightness.
Keep the brightest channel of the colour at 1, so that `intensity` alone
says how bright the sun is.

## Suns in flight.yaml

The `suns` section of `flight/flight.yaml` changes how the mission's
backdrop suns and the lens flare look. It applies with or without
environments.

```yaml
suns:
  procedural_sun: true
  procedural_flare: true
  flare_intensity: 3
```

| Key | Meaning |
|-|-|
| `procedural_sun` | `true` draws each backdrop sun as a disc and a glow made by the game, in place of its image. The default is `false`. |
| `procedural_flare` | `true` draws a lens flare made by the game, from the sun that casts the shadows, in place of the classic flare images. The default is `false`. |
| `flare_intensity` | The strength of the procedural flare, 0 to 10. The default is 3. |
| `brightness` | The brightness of a procedural sun, as a multiple of a white pixel of the game's images, 0 to 100. The default is 1.2. |
| `core_brightness` | Extra light in the centre of a procedural sun, as a multiple of the disc's brightness, 0 to 1000. It makes the sun glow through the bloom. The default is 1. |
| `flare_elements` | The shapes of the procedural flare. |

A missing key keeps its default. A wrong value is ignored with a warning in
the log. The player's settings can override `procedural_sun`,
`procedural_flare` and `flare_intensity`.

When the region uses the suns of its environment, the procedural flare
comes from the primary sun, and only if its `lens_flare` is `true`.
Otherwise, it comes from the backdrop sun that gives the main light. The
flare fades when something hides the sun and when the sun leaves the
screen.

### Flare elements

`flare_elements` is a list of up to 32 shapes. A mod's list replaces the
whole default list. When the list has an error, the game warns and uses the
default list.

```yaml
suns:
  procedural_flare: true
  flare_elements:
    - { shape: halo, position: -0.8, size: 32, color: [0.25, 0, 0] }
    - { shape: disc, position: -0.4, stretch: -0.5, size: 0.15, power: 2.4, color: [0.06, 0.05, 0.03] }
    - { shape: hexagon, position: -0.5, size: 0.1, rotation: 0.72, color: [0.008, 0.008, 0.007] }
    - { shape: streak, position: 1, size: 0.001, power: 16, color: [0.04, 0.04, 0.04], tinted: true }
```

Flare positions use the screen. The centre of the screen is 0, half the
screen's shorter side is 1, and y points down.

| Key | Meaning |
|-|-|
| `shape` | Required. `halo`, `disc`, `hexagon` or `streak`. |
| `color` | Required. `[r, g, b]`, 0 or more each. The shape's brightness, before `flare_intensity`. |
| `size` | Required. Above 0, or 0 or more for a halo. Its meaning depends on the shape. |
| `position` | Where the shape is on the line from the centre of the screen to the sun. 1 is on the sun, 0 is the centre, and -1 is the opposite side. The default is 0. |
| `offset` | `[x, y]`, moves the shape from its place on the line. The default is `[0, 0]`. |
| `stretch` | Bends the shape more and more toward the screen's edges. 0 doesn't bend it. The default is 0. |
| `power` | For a disc, how sharp its edge is, above 0. For a streak, how fast it fades across its length, 0 or more. The default is 1. |
| `rotation` | Turns a hexagon or a streak, in radians. The default is 0. |
| `tinted` | `true` multiplies the colour by the sun's colour. The default is `false`. |

| Shape | What `size` does |
|-|-|
| `halo` | A soft glow that fades with distance. A larger size makes it smaller. 0 makes it fill the screen. |
| `disc` | A round shape. `size` is its radius. |
| `hexagon` | A flat hexagon. `size` is the distance from its centre to its sides. |
| `streak` | A thin horizontal line. A smaller size makes its centre brighter. |

The game adds up the shapes, multiplies the sum by `flare_intensity`, and
shows it at most at full white.

## Testing

The log has one line each time the environment of the player's region
changes, with the mission, the region and the environment. When a region
can't use its environment, the line says why, for example
`fallback: environment not ready` when a texture can't be loaded. Earlier
lines name the texture and the reason.

In the debug packages, the **Environment** tool of the debug overlay shows
the region's environment. **Reload** reads `environments.yaml` and the
images again without a restart. The tool also has sliders for the values
of the environment, and **Copy YAML** copies the changed environment, ready
to paste into the file.
