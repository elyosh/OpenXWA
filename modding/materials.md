# Materials

The materials of a model are glTF PBR materials. On top of them, a
material can play animations, show hull damage and energy effects, carry
skins, and show the cockpit displays live. This page explains these, and
the effect files that the animations take their images from.

The model itself, with its parts and hardpoints, is on the
[models page](models.md). The full format of the glTF extension is in
[AERON_flight_model](../aeron/tools/extensions/AERON_flight_model/README.md).
The Blender add-on edits everything on this page. See the
[tools page](tools.md).

## PBR materials

The game draws glTF metallic-roughness materials. It reads:

| Material value | Meaning |
|-|-|
| `baseColorFactor`, `baseColorTexture` | The base colour. The texture is multiplied by the factor. |
| `metallicFactor`, `roughnessFactor`, `metallicRoughnessTexture` | Metallic and roughness. In the texture, G is the roughness and B the metallic. |
| `normalTexture` | A tangent-space normal map. The mesh needs tangents. |
| `emissiveFactor`, `emissiveTexture` | The light that the surface gives out. See below. |
| `alphaMode`, `alphaCutoff` | How the alpha is used. See [alpha](#alpha). |
| `doubleSided` | `true` draws the back of the faces too. |

These glTF extensions add to it:

| Extension | Meaning |
|-|-|
| `KHR_materials_emissive_strength` | `emissiveStrength` multiplies the emissive, so that a surface can be brighter than its texture allows. |
| `KHR_materials_specular` | `specularFactor` (0 to 1) and `specularColorFactor` set the strength and colour of the reflections. |
| `KHR_materials_transmission` | Glass. See below. |
| `KHR_materials_variants` | Other looks of the model. See [skins and markings](#skins-and-markings). |

The cooker refuses a material with an `occlusionTexture`, a
`specularTexture` or a `specularColorTexture`. The game ignores other
material extensions.

**Emissive.** The emitted light is the emissive texture times
`emissiveFactor`, times `emissiveStrength`. The game also scales all
emissive light by a setting of its own. Paint the emissive texture as the
emitted colour at full brightness, and raise `emissiveStrength` to make it
brighter than white.

**Glass.** A material with a `transmissionFactor` above 0 is thin glass:
light passes through it without bending. What shows through is darkened by
the brightness of the base colour, but its hue doesn't tint it. A
`transmissionTexture` must use the same texture coordinates as
the base colour texture, or the cooker ignores it and uses the factor. The
glass is always drawn as `BLEND`, even when its `alphaMode` is another
mode, and the alpha of `baseColorFactor` isn't used.

**Texture coordinates.** The game uses `TEXCOORD_0` for every texture.
The cooker replaces each texture's own `texCoord` and
`KHR_texture_transform` with its own, so a transform in the source file is
lost. Put the layout that you want in the UVs themselves. Coordinates
outside 0 to 1 repeat the texture.

## Alpha

| `alphaMode` | How the game draws it |
|-|-|
| `OPAQUE` | Solid. The alpha is ignored. This is the default. |
| `MASK` | Solid, with holes where the base colour's alpha is below `alphaCutoff` (0.5 by default). Use it for grilles and other cut-outs. |
| `BLEND` | Mixed over what is behind it by its alpha: the base colour texture's alpha times the alpha of `baseColorFactor`. Use it only for see-through surfaces, such as glass and holograms. |

Glass, [hologram screens and transparent screens](#cockpit-screens) are
always drawn as `BLEND`.

A primitive keeps its alpha mode in every look. When a variant gives a
primitive another material, that material must have the same alpha mode,
or the game can't load the model.

## The game data of a material

The `AERON_flight_model` extension of a material holds the game data:

| Key | Meaning |
|-|-|
| `flipbooks` | [Animations](#flipbooks) played on game states. |
| `damage` | [Hull damage](#hull-damage) images, shown as the hull weakens. |
| `details` | Up to two [detail maps](#detail-maps). |
| `energy` | The [energy effects](#energy-effects) that the material shows. |
| `screen` | [Cockpit displays](#cockpit-screens) drawn live on the material. |
| `skins` | The [skin layers](#skin-layers) of the material. |
| `tags` | Names that the `surfaces` setting uses. See [tags](#tags). |

All the keys are optional. The cooker stops with an error when one of them
is malformed or has an unknown key. Animations, damage and details take
their images from [effect libraries](#effect-libraries).

## Skins and markings

A model can have several looks, such as the colours of each squadron. The
game shows one look for each object. A look is a glTF variant, from
`KHR_materials_variants`. There are two ways to make one:

- **Replaced materials.** A `KHR_materials_variants` variant gives some
  primitives another material, as in any glTF file.
- **Skins.** A skin variant paints images over the materials. The cooker
  turns each skin variant into a `KHR_materials_variants` variant.

A look can do both.

### Skin layers

A skin is a set of images painted over materials, such as squadron
stripes. Each material that the skin covers has a layer of it, in its
`skins`, keyed by the skin's name:

```json
"AERON_flight_model": {
  "skins": {
    "red": { "baseColorTexture": { "index": 7 } },
    "dirt": {
      "baseColorTexture": { "index": 8 },
      "metallicRoughnessTexture": { "index": 9 }
    }
  }
}
```

A skin name is 1 to 63 characters: lower-case letters, digits, `.`, `_`
and `-`. A layer has:

| Key | Meaning |
|-|-|
| `baseColorTexture` | Required. The layer's base colour. Its alpha is where the layer covers the material, for every image of the layer. |
| `emissiveTexture` | The layer's emissive. Without it, the layer leaves the emissive as it is. |
| `metallicRoughnessTexture` | The layer's metallic (B) and roughness (G). Without it, the layer leaves them as they are. |
| `emissiveStrength` | The strength of the layer's emissive. The default is the material's `emissiveStrength`, or 1. |

The textures are ordinary textures of the file. They use the texture
coordinates of the material's base colour texture. Each must have the size
of the material's own texture of the same kind. When the material has no
texture of that kind, all layers of that kind must have the same size.

### Skin variants

The model root lists the looks made of skins in `skinVariants`:

```json
"skinVariants": [
  { "name": "Red", "layers": [{ "skin": "red" }] },
  { "name": "Red, worn", "layers": [{ "skin": "red" }, { "skin": "dirt", "opacity": 60 }] }
]
```

| Key | Meaning |
|-|-|
| `name` | Required. The look's name. It must be unique among the skin variants. When it is the name of a `KHR_materials_variants` variant, the layers are added to that variant. |
| `base` | Another `KHR_materials_variants` variant that this look starts from. |
| `layers` | The skins, painted in order. Each has `skin`, its name, and `opacity`, 0 to 100, which scales its cover. The default `opacity` is 100. |

Every skin in `layers` must be on at least one material. `layers` can only
be empty or missing when the look has a `base`, or the name of a
`KHR_materials_variants` variant.

On a material that has some of the look's skins, the layers are painted
over the material's own images, in order. The result keeps the material's
alpha. On any other primitive, the look shows the material that the
`KHR_materials_variants` variant of the same name gives it, else the
`base` variant's material, else the primitive's own material. Layers always
paint the primitive's own material, not a variant's.

### Markings

The game gives each object a markings value, from 0 to 255, usually the
one of its flight group. The `markings` list of the model root says which
look each value shows:

```json
"markings": [null, "Red", "Blue"]
```

Entry m is the name of the look for markings value m, or `null` for the
model as modelled. Here, markings 0 shows the model as modelled, 1 shows
`Red`, and 2 and above show `Blue`, because a value past the end of the
list shows the last entry. Without the list, every value shows the model as modelled.
The list has at most 256 entries, and every name must be a look of the
model, or the game can't load it.

The game only loads the textures of the looks that the objects of the
mission show.

### How missions pick a look

A mission can show another look for a markings value of a model, in the
`skins` section of its mission file:

```yaml
skins:
  awing:
    0: "Red"
    3: null
```

The keys are model keys, then markings values. `null` shows the model as
modelled. When the model has no look with that name, the game writes a
warning and uses the model's `markings` list. The
[missions page](missions.md) describes mission files.

## Flipbooks

A flipbook plays a series of images on a material while a game state is
on, such as cockpit lights that blink while the player has a target.

```json
"AERON_flight_model": {
  "flipbooks": [
    { "state": "target", "layer": "baseColor", "mode": "loop", "library": "mycockpit",
      "frames": [["screen_a", 0.5], ["screen_b", 0.5]] },
    { "state": "always", "layer": "emissive", "mode": "loop", "library": "mycockpit",
      "emissiveStrength": 2, "frames": [["lights_0", 0.5], ["lights_1", 0.5, 1.5]] }
  ]
}
```

| Key | Meaning |
|-|-|
| `state` | Required. The state that plays the flipbook: `always`, or a [flipbook state](reference/states_and_tags.md#flipbook-states). |
| `layer` | Required. `baseColor`: the frames are painted over the base colour by their alpha. `emissive`: the frames replace the material's emissive. |
| `mode` | Required. `loop` plays the frames again and again. `once` stops on the last frame. |
| `library` | The [effect library](#effect-libraries) of the frames. Required when there are frames. |
| `frames` | Required. The frames in order, each `[image, seconds]`. An emissive frame can have a third number, `[image, seconds, strength]`, that multiplies `emissiveStrength` for that frame. The default is 1. |
| `emissiveStrength` | The brightness of emissive frames. The default is 1. |
| `unlit` | For `baseColor` flipbooks: the part of the frames drawn as light instead of lit, from 0 to 1. The default is 0. Use 1 for screens that glow. |
| `blendFrames` | `true` fades each frame into the next one. The default is `false`. Use it for slow animations, such as flowing lava. With `once`, the last frame doesn't fade. |

The images of a `baseColor` flipbook must be of kind `baseColor`, and those
of an `emissive` flipbook of kind `emissive`. See
[image kinds](#image-kinds).

The game plays the first flipbook in the list whose state is on, for each
layer. So a `baseColor` and an `emissive` flipbook can play at the same
time. Put `always` last, as the flipbook to play when no other state is on.
When no flipbook plays, the material shows its own textures. Make them the
look at rest, which any glTF viewer also shows.

A flipbook starts from its first frame when its state comes on. `always`
flipbooks play on every object that uses the model. The other states are
those of the player's craft, so they only play on the player's craft and
cockpit.

Give each animated surface its own material, laid out on its own part of
the texture. A frame covers the material's whole texture.

## Hull damage

A material can show damage as the hull weakens: images that darken the
base colour at 75%, 50% and 25% hull.

```json
"damage": {
  "library": "mydamage",
  "choice": "wings",
  "alternatives": [
    { "stages": ["burn_75", "burn_50", "burn_25"], "scale": [0.2, 0.5], "offset": "inside" },
    { "stages": [null, "scorch_50", "scorch_25"] }
  ]
}
```

| Key | Meaning |
|-|-|
| `library` | Required. The effect library of the images. |
| `choice` | Required. A name. Each object picks one of the alternatives at random, and the materials with the same `choice` name pick the same one. |
| `placement` | A name. The materials with the same `placement` name share the random size and position. The default is the `choice` name. |
| `alternatives` | Required. At least one alternative. |

An alternative has:

| Key | Meaning |
|-|-|
| `stages` | Required. Three images of kind `damage`, shown when the hull is at 75%, 50% and 25%. `null` shows nothing at that stage. |
| `scale` | `[min, max]`. The image covers a square whose side is a random size between them, as a fraction of the texture. Without it, the image covers the whole texture. |
| `offset` | Moves the square. `"inside"` puts it anywhere inside the texture. `{ "min": [x, y], "max": [x, y] }` moves it by a random amount between them on each axis, as a fraction of the texture. Without it, the square stays in the centre. |

Each object draws its random values once. Outside its square, a stage
shows nothing. A model can use up to 4096 different `choice` and
`placement` names.

## Detail maps

A detail map is an image repeated many times over a material. It adds fine
detail to large surfaces, such as rock or hull plating. A material can have
two.

```json
"details": [
  { "library": "rock", "image": "rock_normal", "blend": "normal",
    "strength": 0.4, "uvScale": 2, "fadeDistance": 1000 },
  { "library": "rock", "image": "rock_grain", "blend": "overlay",
    "strength": 0.6, "uvScale": 3, "fadeDistance": 1000 }
]
```

| Key | Meaning |
|-|-|
| `library` | Required. The effect library of the image. |
| `image` | Required. The image: of kind `detailNormal` for the `normal` blend, and `detailColor` for the others. |
| `blend` | Required. How the image changes the material. See below. |
| `strength` | How much the image is mixed in, times its alpha. The default is 1. `replace` ignores it. |
| `uvScale` | How many times the image repeats over the texture coordinates. The default is 1. |
| `fadeDistance` | The distance from the camera, in metres, at which the image has faded out. Without it, the image doesn't fade. |

The blends:

| `blend` | Result, for the material's colour c and the image's colour g |
|-|-|
| `multiply` | c × g. |
| `overlay` | c × g ÷ mid-grey. Mid-grey keeps the colour, lighter areas brighten it, and darker areas darken it. |
| `screen` | c + g − c × g. |
| `replace` | g. |
| `normal` | The image is a tangent-space normal map. Its slope bends the material's normal. The mesh needs tangents. |

The colour blends work in linear light. Details apply first, and flipbook
frames and hull damage paint over them. Paint detail images so that they
tile.

## Energy effects

Energy effects play over a craft's surfaces: `shields` when its shields go
down, `tractor` while a tractor beam holds it, and `jamming` while a
jamming beam holds it. [effects.yaml](#effectsyaml) sets their images.

A material lists the effects that it shows in `energy`. `[]` shows none.
A material without `energy` uses the `energy` of the model root. Without
either, it shows none. Most models list all three on the root, and give
`[]` to the few materials that shouldn't show them, such as cockpit
screens.

```json
"energy": ["shields", "tractor", "jamming"]
```

## Cockpit screens

A screen material shows the displays of the cockpit live, such as the
sensors or the targeting computer. Each display goes in a rectangle of the
material's texture coordinates.

```json
"AERON_flight_model": {
  "screen": {
    "hide": ["shields"],
    "rects": [
      { "source": "shields", "dst": [0, 0, 1, 1], "bg": [0, 0, 0],
        "highlight": [0, 0, 0], "lock": [0, 0, 0], "gain": 1,
        "layers": ["graphics", "text"] }
    ]
  },
  "energy": []
}
```

| Key | Meaning |
|-|-|
| `rects` | Required. Up to 16 rectangles. |
| `hide` | The areas of the 2D HUD that the game doesn't draw while the screen is visible, so the display isn't shown twice. The [HUD areas](reference/model_values.md#hud-areas) page lists them. A model can hide up to 32 areas. |
| `cover` | `true`: the screen shows through the material where its base colour's alpha is below 1, such as through the window of a panel. |
| `transparent` | `true`: the screen is drawn as light with no surface, its alpha taken from the display's brightness. |
| `alwaysVisible` | `true`: a transparent screen stays visible when the player uses the classic HUD. |
| `hologram` | `true`: the screen is drawn as a hologram. The player can hide holograms. |
| `noisy` | `true`: a hologram with scanlines. |

The flags are `false` by default. A rectangle has these keys, all required
except `bar`:

| Key | Meaning |
|-|-|
| `source` | The display that the rectangle shows. The [cockpit screen sources](reference/model_values.md#cockpit-screen-sources) page lists them. |
| `dst` | `[u0, v0, u1, v1]`: the rectangle, in the material's texture coordinates. (u0, v0) gets the display's top left corner, and (u1, v1) its bottom right. `[0, 0, 1, 1]` fills the whole texture, upright. Swapping two values mirrors the display. |
| `bg` | The background colour, linear `[r, g, b]`. |
| `highlight` | The background while the reticle is highlighted. |
| `lock` | The background while a warhead lock is solid. |
| `gain` | The brightness of the display, 0 or more. |
| `layers` | What the rectangle shows: `graphics`, `text`, or `bar`. See [cockpit screen layers](reference/model_values.md#cockpit-screen-layers). |
| `bar` | The colour of the fill bar, linear `[r, g, b]`. Required with the `bar` layer. |

Outside all rectangles, the screen shows the first rectangle's background.

Without `cover` or `transparent`, the display is added as light over the
whole lit material. Give such a material a dark base colour, so that the
display reads well.

A `baseColor` flipbook on a screen covers the display by its alpha where
the cover lets the screen through. On a screen without a cover, it
replaces the display.

The game checks the names when it loads the model. A rectangle whose
`source` it doesn't know shows nothing, and an unknown `hide` area is
ignored. Both write a warning. A malformed screen is ignored, and the
material draws as an ordinary one.

Screens show the displays only in the cockpit of the player's craft, and
only while the player has cockpit screens on in the options.

The `bar` layer shows the throttle as a bar that fills from the bottom of
the rectangle, in the `bar` colour over a dark grey. It ignores the
`source`. It only shows while the classic HUD option is off, and never on a
hologram.

## Effect libraries

An effect library is a set of images that models share: the frames of
flipbooks, the stages of hull damage, detail maps, and the images of
`effects.yaml`. Models name their images by library and key.

You write a library as a YAML file with its PNG images, and the cooker
builds it into the files that the game reads.

### The library file

```yaml
library: mycockpit
images:
  lights_0: { kind: emissive, emissive: frames/lights_0.png }
  lights_1: { kind: emissive, emissive: frames/lights_1.png }
  screen_a: { kind: baseColor, baseColor: frames/screen_a.png }
  dmg_75:   { kind: damage, factor: frames/dmg_75.png }
```

| Key | Meaning |
|-|-|
| `library` | Required. The library's name: lower-case letters, digits, `_` and `-`. The built files take this name. |
| `images` | Required. The images, keyed by name. A key is any name, unique in the library. |

An image has:

| Key | Meaning |
|-|-|
| `kind` | Required. What the image is for. See below. |
| the inputs of its kind | The PNG files of the image, as paths relative to the library file. |
| `maxSize` | The largest size of the image's longer side, in pixels. The default and the largest value are 1024, and 2048 for detail images. Larger images are scaled down. |
| `crop` | `[x0, y0, x1, y1]`, for blast marks: the part of the image that the original blast mark covers. The image spreads around it. |

### Image kinds

| Kind | Inputs | What it does |
|-|-|-|
| `baseColor` | `baseColor` (RGBA) | Painted over the material's base colour. The alpha is the cover. |
| `emissive` | `emissive` (RGB), and optionally `baseColor` (RGBA) | Replaces the material's emissive. With `baseColor`, it also paints over the base colour. |
| `damage` | `factor` (RGBA) | Hull damage. The RGB multiplies the base colour. Alpha below 1 makes the surface duller. |
| `mark` | `baseColor` (RGBA) | A blast mark, painted over the hull. See [effects.yaml](#effectsyaml). |
| `energy` | `emissive` (RGBA) | An energy effect, added as light. The alpha is its strength. See [effects.yaml](#effectsyaml). |
| `detailColor` | `baseColor` (RGBA) | A detail map that changes the base colour. The alpha is its strength. |
| `detailNormal` | `normal` (RGB) | A detail map that bends the normal. A tangent-space normal map. |

All inputs are PNG files, and their colours are sRGB. Paint flipbook
frames and damage images in the texture layout of the material that shows
them. The inputs of one image must have the same size.

For an emissive frame, black means no light. Paint the emitted colour at
full brightness where the light is brightest, and raise the flipbook's
`emissiveStrength` to make it brighter.

### Building a library

Build each library into the mod's `flight/effects` folder:

```sh
aeron_gltf_cook --library effects/mycockpit/mycockpit.yaml --out my-mod/flight/effects
```

The build writes the cooked library: `<library>.yaml`, which lists the
images, and `<library>.layer.ktx2` and `<library>.emissive.ktx2`, which
hold them. Don't edit these files, and don't keep your own library file in
`flight/effects`, because the build writes a file with the same name
there. A
project folder of the Blender add-on keeps each library in
`effects/<library>/<library>.yaml`.

The cooker skips a library whose inputs haven't changed. When a rebuild
removes an image or changes its kind, the cooker lists it, because the
models that use it lose that image.

Build the libraries before the models. With `--effects-dir`, the cooker
checks that each image that a model names is in its library, with the
right kind. The [tools page](tools.md) describes the cooker and how to
build a whole project at once.

## effects.yaml

`flight/effects/effects.yaml` sets the images of the game's own effects:
the energy effects and the animated blast marks. Its images come from the
libraries in the same folder.

```yaml
energy:
  shields: { library: genericdamage, mode: once, strength: 60, tile: 0.3,
             frames: [["shield_0", 0.025], ["shield_1", 0.025], ["shield_2", 0.025]] }
  tractor: { library: genericdamage, mode: loop, strength: 60, tile: 0.3,
             frames: [["tractor_0", 0.05], ["tractor_1", 0.05]] }
images:
  blast_mark.1: { library: blastmarks, kind: mark, mode: loop,
                  frames: [["mark_a", 0.0667], ["mark_b", 0.0667]] }
```

`energy` can have `shields`, `tractor` and `jamming`:

| Key | Meaning |
|-|-|
| `library` | Required. The effect library of the frames. |
| `mode` | Required. `loop` or `once`. |
| `frames` | Required. The frames, each `[image, seconds]`. The images are of kind `energy`. |
| `strength` | Required. The brightness of the effect, 0 or more. |
| `tile` | Required. The size of the image on the surface, as a fraction of the model's largest size, above 0. |

`shields` starts when a craft's shields go down. A craft that has no
shields shows it once when it appears. `tractor` and `jamming` play while
the beam holds the craft. A craft shows one effect at a time: `jamming`
first, then `tractor`, then `shields`.

`images` holds the animated blast marks. `blast_mark.<n>` animates image
`<n>` of [image group](reference/image_groups.md) 3050, counted from 1.
Each entry has `library`, `mode` and `frames` as above, and
`kind: mark`. Its frames are images of kind `mark`. Each mark on a hull
starts at a random point of its frames. A blast mark without an entry shows
the image of group 3050 as it is.

Without `effects.yaml`, there are no energy effects, and blast marks show
the images of group 3050. When an entry is malformed, the game writes an
error and ignores the whole file.

A project folder of the Blender add-on keeps this file as
`effects/effects.yaml`, and the cooker copies it into the mod.

## Textures

The textures of a model are PNG files, which the `.gltf` names by path.
The cooker doesn't read other image formats, or images stored inside the
file.

The cooker packs all the textures of a model into a few large pages for
each kind of texture, compresses them, and makes their smaller sizes
(mipmaps):

| Texture | Compression |
|-|-|
| Base colour | BC7, sRGB. |
| Emissive | BC7, sRGB. |
| Metallic-roughness | BC7, linear. |
| Normal | BC5. Only X and Y are kept, and the game rebuilds Z. |

A texture can have any size. The cooker scales down a texture whose longer
side is above 4096 pixels. These options of the cooker change the result:

| Option | Meaning |
|-|-|
| `--max-texture <N>` | Scale down textures whose longer side is above N pixels. The default is 4096. |
| `--atlas-page <N>` | The size of a page: a power of 2, 128 or more. The default is 2048. It grows to fit the largest texture. |
| `--quality fast\|med\|uber` | How hard the compression works. `med` is the default, `fast` is quicker and `uber` is slower. |
| `--zstd-level <N>` | How hard the extra file compression works, 1 to 22. The default is 9. |
| `--no-zstd` | Leave out the extra file compression. |

The images of effect libraries are also PNG files. They are scaled down to
1024 pixels on their longer side, or 2048 for detail maps, or `maxSize`.

## Tags

A material can have `tags`, like lights. The `surfaces` setting of the
[craft page](craft.md) uses them to hide the materials and lights with a
tag while the craft is disabled or slow, such as engine surfaces that go
dark when the craft stops. See
[material and light tags](reference/states_and_tags.md#material-and-light-tags).

A tag is 1 to 63 characters: lower-case letters, digits, `.` and `_`. A
model's materials and lights use at most 32 different tags.

```json
"AERON_flight_model": { "tags": ["engine_glow"] }
```
