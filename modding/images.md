# Images

The game's 2D images are the menu art, the HUD, explosions, sparks,
planets, suns and the other flat images of flight. A mod replaces them with
KTX2 image files and an atlas. The atlas is a YAML file that says where each
image is in the KTX2 files and how the game uses it.

This page describes the atlas format, the folders that hold atlases, the
KTX2 files, the `xwa_image_cook` tool that makes them from PNG files, and
the fonts. [The frontend page](frontend.md) describes the other files of
the menus, such as the videos and the screen layouts.

## Where images go

| Path | What it replaces |
|-|-|
| `resdata/atlas/group_<number>.yaml` | an image group of the game's `RESDATA` files, in the menus and in flight |
| `resdata/mission_art/<name>/group_<number>.yaml` | an image group, only while a mission that lists `<name>` plays |
| `frontend/frontres/<dir>/atlas/<base>.yaml` | the file `FRONTRES/<dir>/<base>.*` of the game folder |

The KTX2 files of an atlas are next to it, with the same name. When the mod
doesn't have an atlas for an image, the game uses the original image.

## The atlas file

An atlas lists the frames of an image. A frame is one picture, such as one
image of a group or one frame of an animation. Each frame gives its
rectangle in a KTX2 file and its size on screen.

```yaml
game_geometry: true
atlas: {w: 1440, h: 736}
bounds: [0, 0, 704, 357]
frames:
  - {x: 8, y: 8, w: 704, h: 357, size: [704, 357]}
  - {x: 728, y: 8, w: 704, h: 357, size: [704, 357]}
  - {page: 1, x: 8, y: 8, w: 704, h: 357, size: [704, 357]}
```

This atlas replaces a menu image with three frames. The first two frames
are in the first KTX2 file, and the third is in the second file.

### Keys of the file

| Key | Meaning |
|-|-|
| `atlas` | Required. `{w: <width>, h: <height>}`, the size in pixels of the first KTX2 file. |
| `frames` | Required. The list of frames, in order. The frame keys are below. |
| `units` | The units of `size`, `offset`, `bounds`, `draw_adjust` and the `rect` of `regions`: `design` or `game`. The default is `design`. See [Units](#units). |
| `game_geometry` | `true` makes the game take the image's geometry from the atlas, and not from the original image. The default is `false`. See [Game geometry](#game-geometry). |
| `scale` | Only with `units: design`. A frame without `size` is drawn at its pixel size divided by `scale`. Must be above 0. The default is 1. |
| `classic_frames` | The number of frames that the game sees, from 1 to the number of frames. The default is the number of frames. |
| `fps` | Atlases in `frontend/frontres` only. The frames change at this rate, in frames per second, and don't follow the game's frame. Must be above 0. |
| `tint` | Which tints the game puts on the image when it draws it as a tinted icon in the menus: `all`, `none` or `highlights`. The default is `all`. |
| `draw_adjust` | `[x, y, width, height]`, added to the rectangle where the image is drawn in the menus. The game's own geometry doesn't change. |
| `bounds` | Atlases in `frontend/frontres` only. `[left, top, right, bottom]`, the bounds of the whole image. The default is the union of the frames' bounds. |
| `regions` | A list of `{type: <number>, rect: [left, top, right, bottom]}`. Only used by the map icon sheet, see [Regions](#regions). |
| `frame_sets` | Groups in `resdata/atlas` only. Other sets of images that the group can draw in flight. See [Frame sets](#frame-sets). |

Most keys only work with `game_geometry: true`. Without it, the game only
uses `atlas`, `units`, `frame_sets`, and the `x`, `y`, `w`, `h`, `page`,
`id`, `size` and `light` of each frame. When a value is wrong, the log says
so, and the game uses the atlas as if it had no `game_geometry`.

### Keys of a frame

| Key | Meaning |
|-|-|
| `x`, `y`, `w`, `h` | Required. The frame's rectangle in its KTX2 file, in pixels. The rectangle must fit inside the size that `atlas` gives. |
| `page` | The KTX2 file that holds the frame. See [Atlas pages](#atlas-pages). The default is 0. |
| `id` | The image's number in its group. Required for a frame of a group. Menu images don't use it. |
| `size` | `[width, height]`, the size the frame is drawn at. Without it, the size is `w` and `h` (divided by `scale`). |
| `offset` | `[x, y]`, added to the position where the game draws the frame. For a group image, this is the image's anchor. The default is `[0, 0]`. |
| `bounds` | Atlases in `frontend/frontres` only. `[left, top, right, bottom]`: the part of the frame that the game sees as its visible pixels. `right` and `bottom` are the last column and row. The default is the whole frame. |
| `keep_aspect` | In the menus, `height` draws the frame at its height and works out the width from the shape of its pixels. `width` keeps the width that the game gives and works out the height. `width` only applies to tinted icons. |
| `fit` | `screen_height` draws the frame in the menus as tall as the screen, centred left to right, with its own shape. Use it for full-screen backgrounds. The game still uses the frame's normal position and size. |
| `light` | `[r, g, b]`, from 0 to 1 in sRGB. The colour of a sun's glow and lens flares, for the suns in groups 9001 to 9010. Without it, the colour is white. |

Always give `size`. The pixels of a KTX2 file can have any resolution, and
`size` says how large the frame is in the game.

### Units

With `units: design`, the default, sizes and positions are in design
pixels, the pixels of the screen size that `design` sets in
`frontend/frontend.yaml`. The game converts them to its own units. See
[the mod folder](mod_folder.md#units-and-conventions).

With `units: game`, the values are the game's own units, with no
conversion. In the menus, these are pixels of the original 640 × 480
screen. In flight, they are the pixels of the original images. Use
`units: game` for the groups that flight uses. `scale` can't be used with
`units: game`.

### Game geometry

Without `game_geometry`, the game reads the original image as usual and
uses its frame count, its sizes and its positions. The atlas only gives the
pictures. Each frame of the atlas is drawn in place of the original frame
at the same position in the list, or with the same `id` in a group.

With `game_geometry: true`, the game doesn't read the original image. It
takes the number of frames, each frame's size and offset, and the bounds
from the atlas. Use it when your image has another size, shape or number of
frames than the original. The original image isn't used as a fallback. If
the KTX2 files are missing, the image isn't drawn, and the log says why.

In a group with `game_geometry: true`, every frame needs an `id`. When two
frames have the same `id`, the game keeps the first one. For a group whose
images are the frames of an animation, such as an explosion, list the
frames in the order of their `id`.

### Frames that change over time

The game picks the frame to draw. A door that opens, for example, goes
through its frames as the game asks. With `fps`, an image of
`frontend/frontres` plays all its frames in a loop at that rate, whatever
frame the game asks for.

`classic_frames` sets how many frames the game itself counts. For example,
an atlas with 60 frames, `fps: 24` and `classic_frames: 5` plays a smooth
60-frame animation, while the game still counts the 5 frames of the
original door.

### Tints

The game draws some menu images as tinted icons. It colours the whole
image with one colour, such as light blue for a highlighted button. Art
that has its own colours can turn this off:

| `tint` | Effect |
|-|-|
| `all` | The game's tint is used. |
| `none` | The image is drawn with its own colours. |
| `highlights` | The tint is only used for light blue, yellow and green, the colours of highlights. |

The craft icons of the briefing map always keep their tint.

### Regions

The map icon sheet, `frontend/frontres/mapicons/atlas/licons.yaml`, can
give the rectangle of each craft's icon in the sheet. `type` is the craft
type number from [Craft types](reference/craft_types.md), from 1 to 254,
and `rect` is the icon's rectangle. A craft without a region keeps the
rectangle that the game's ship list gives. All the colours of the sheet use
the rectangles of `licons`.

## Atlas pages

The frames of an atlas can be spread over several KTX2 files, called pages.
The files are next to the atlas file:

| Page | File |
|-|-|
| 0 | `<base>.ktx2` |
| 1 | `<base>_p1.ktx2` |
| 2 | `<base>_p2.ktx2` |

`<base>` is the atlas's name without `.yaml`, such as `group_15000` or
`tourdoor`. Every page from 0 to the highest `page` must exist. Pages can
have different sizes, but every frame must fit inside the size that `atlas`
gives.

## Image groups

`resdata/atlas/group_<number>.yaml` replaces the image group with that
number, in the menus and in flight. Its pages are
`resdata/atlas/group_<number>.ktx2`, then `group_<number>_p1.ktx2` and so
on. [Image groups](reference/image_groups.md) lists what each group holds.

Every group can be replaced, except group 16000, the font of the flight
text. The fonts in flight come from `flight/fonts/` instead. See
[Fonts](#fonts).

This atlas replaces the four images of a group used in flight:

```yaml
game_geometry: true
units: game
atlas: {w: 1056, h: 1056}
frames:
  - {id: 0, x: 8, y: 8, w: 512, h: 512, size: [128, 128]}
  - {id: 1, x: 536, y: 8, w: 512, h: 512, size: [128, 128]}
  - {id: 2, x: 8, y: 536, w: 512, h: 512, size: [128, 128]}
  - {id: 3, x: 536, y: 536, w: 512, h: 512, size: [128, 128]}
```

Some files of the mod pick images from a group by `id`, such as the HUD
icons of `flight/hud.yaml`. See [Flight](flight.md).

### Frame sets

A group used in flight can have other sets of images and change between
them while the game plays. The explosions use this, so that not every
explosion looks the same.

```yaml
game_geometry: true
units: game
atlas: {w: 1056, h: 1056}
frame_sets:
  shuffle: explosions
  reroll_seconds: 5
  sets:
    - {atlas: group_2000_blue}
    - {atlas: group_2000_ds2, missions: [52]}
frames:
  - {id: 0, x: 8, y: 8, w: 512, h: 512, size: [512, 512]}
  - {id: 1, x: 536, y: 8, w: 512, h: 512, size: [512, 512]}
```

| Key | Meaning |
|-|-|
| `sets` | Required. A list of 1 to 8 sets. |
| `reroll_seconds` | Required. How often the group picks a set again, in seconds. Above 0, and at most 3600. |
| `shuffle` | A name of up to 31 characters. Groups with the same name pick at the same time. |

Each set has these keys:

| Key | Meaning |
|-|-|
| `atlas` | Required. The name of another atlas in `resdata/atlas`, without `.yaml`. Only letters, digits, `_` and `-`. |
| `missions` | A list of up to 8 missions. In these missions, the group always draws this set. A mission is given by its position among the missions of its `MISSION.LST`, counted from 0, not by its mission number. |

A set is an ordinary atlas, such as `resdata/atlas/group_2000_blue.yaml`
with its pages `group_2000_blue.ktx2` and so on. It doesn't need
`game_geometry` or `id`. The set's first frame replaces the group's first
frame, and so on. Each frame is drawn at the size of the group's own frame.
When the set has fewer frames than the group, the missing frames aren't
drawn.

The group picks its own images or one of its sets, each with the same
chance. It picks again every `reroll_seconds`, but only when none of the
groups with the same `shuffle` name was on screen in the last frame, so
that an explosion never changes while it plays.

Frame sets only work in `resdata/atlas`, not in mission art. A set that
can't be read isn't used, and the log says why.

## Mission art

A mission can have its own image groups. They are in
`resdata/mission_art/<name>/`, with the same file names as in
`resdata/atlas`, such as `group_<number>.yaml` and `group_<number>.ktx2`.

The mission uses the art when its file lists the name in its `resdata`
list:

```yaml
resdata: ["red_suns"]
```

While this mission plays, each group of `resdata/mission_art/red_suns/`
replaces the group of `resdata/atlas` with the same number. The other
groups come from `resdata/atlas`, or from the original game. When several
listed folders have the same group, the first folder in the list wins.

| Rule | Limit |
|-|-|
| Folder names | lower-case letters, digits, `_` and `-`, up to 31 characters |
| Folders per mission | 8 |
| Groups in all the folders of a mission | 64 |

A name in the list without a folder in `resdata/mission_art` is ignored.
[Missions](missions.md) describes the mission file.

## KTX2 files

The images are KTX2 files. The game accepts these formats:

| Format | Notes |
|-|-|
| BC7, sRGB or UNORM | The best choice for colour art. `xwa_image_cook` writes BC7 sRGB. |
| BC1 and BC3, sRGB or UNORM | |
| R8G8B8A8, sRGB or UNORM | Not compressed, so the files are large. |
| R16G16B16A16 SFLOAT | |

Use the sRGB formats for normal colour art.

- **Alpha.** The 2D images are drawn with premultiplied alpha. The colour
  of each pixel is already multiplied by its alpha. A file in a format with
  alpha must say so with the `KHR_DF_FLAG_ALPHA_PREMULTIPLIED` flag of its
  data format descriptor. The game doesn't load a file with alpha that
  doesn't have the flag, and writes an error in the log.
- **Mipmaps.** The file can hold a full chain of mipmaps, a few, or only
  the first image. The game doesn't make mipmaps itself. Images that are
  drawn smaller than their pixels look better with mipmaps.
- **Compression.** The file can be stored as is or with Zstandard
  supercompression.
- **Resolution.** Any size works. The frame's `size` sets how large it is
  drawn, not its pixels.

## Making images with xwa_image_cook

`xwa_image_cook` makes an atlas and its KTX2 pages from PNG files. You put
the PNG files and an `image.yaml` file in a folder, and the tool writes the
atlas into the mod folder. It packs the frames into pages of up to
8192 × 8192 pixels and writes BC7 KTX2 files with mipmaps and premultiplied
alpha. The PNG files have normal alpha.

```sh
xwa_image_cook art/tourdoor --mod "/path/to/my-mod"
```

| Option | Meaning |
|-|-|
| `<folder>` | Required. The folder with `image.yaml` and the PNG files. |
| `--mod <folder>` | Required. The mod folder to write to. |
| `--quality fast\|med\|uber` | The quality of the BC7 compression. `uber` is the best and the slowest. The default is `med`. |

[Tools](tools.md) says where to find the tool.

### image.yaml

`image.yaml` names the image to replace and gives every key of the atlas
except the pixel rectangles and `atlas`, which the tool works out.

```yaml
frontres: concourse/tourdoor
game_geometry: true
tint: highlights
bounds: [0, 0, 704, 357]
frames:
  - {image: door_00.png, size: [704, 357]}
  - {image: door_01.png, size: [704, 357]}
  - {image: door_02.png, size: [704, 357]}
```

| Key | Meaning |
|-|-|
| `frontres` | The menu image to replace, as `<dir>/<base>` in lower case, such as `concourse/tourdoor`. The tool writes `frontend/frontres/<dir>/atlas/<base>.yaml`. |
| `group` | The image group to replace, from 1 to 65535. The tool writes `resdata/atlas/group_<number>.yaml`. |
| `units` | `design` or `game`, as in the atlas. The default is `design`. |
| `game_geometry` | `true` or `false`, as in the atlas. |
| `classic_frames` | A whole number from 1 to 65535. It must not be more than the number of frames. |
| `fps` | A whole number from 1 to 1000. |
| `tint` | `all`, `none` or `highlights`. |
| `draw_adjust` | `[x, y, width, height]`. |
| `bounds` | `[left, top, right, bottom]`. |
| `regions` | A list of 1 to 255 regions, each `{type: <1 to 254>, rect: [left, top, right, bottom]}`. |
| `frames` | Required. A list of 1 to 4096 frames. |

The file needs either `frontres` or `group`, not both. Each frame has these
keys:

| Key | Meaning |
|-|-|
| `image` | Required. The name of a PNG file in the folder. |
| `id` | From 0 to 65535. Required for a group. |
| `size` | `[width, height]`. Without it, the size is the PNG's size in pixels. |
| `offset` | `[x, y]`. |
| `bounds` | `[left, top, right, bottom]`. |
| `keep_aspect` | `height` or `width`. |
| `fit` | `screen_height`. |
| `light` | `[r, g, b]`. |

The keys mean the same as in the atlas. The tool stops with an error for
an unknown key or a wrong value.

The tool doesn't write mission art or frame sets. To make them, cook the
group, then move or rename its files. The atlas file and its pages need the
same new name, such as `group_2000_blue.yaml`, `group_2000_blue.ktx2` and
`group_2000_blue_p1.ktx2`. Then add `frame_sets` to the group's atlas by
hand.

## Fonts

The menus and the flight text can use a TrueType or OpenType font in place
of the game's bitmap fonts. Two files set the fonts:

| File | Fonts for | Font files in |
|-|-|-|
| `frontend/fonts/fonts.yaml` | the menus | `frontend/fonts/` |
| `flight/fonts/fonts.yaml` | the text in flight | `flight/fonts/` |

```yaml
default:
  file: MyFont-Bold.ttf
  metrics: font
  spacing: 0.05
sizes:
  20: {scale: 0.95}
  12: {file: MyFont-Regular.ttf}
```

The game draws its text at a few font sizes. `default` sets the font for
every size, and `sizes` changes the settings of single sizes. A size takes
the keys of `default` and replaces the ones it gives.

| Key | Meaning |
|-|-|
| `default` | The settings for every font size. |
| `sizes` | A map of font sizes, from 1 to 255, to their settings. |

The settings:

| Key | Meaning |
|-|-|
| `file` | The font file, in the same folder as `fonts.yaml`. Every size that the file sets needs a `file`, of its own or from `default`. |
| `metrics` | `game` places each character in the space of the original character, so the text keeps the original layout. `font` uses the widths and line height of the font. The default is `game`. |
| `scale` | Multiplies the font size. From 0.1 to 10. The default is 1. |
| `spacing` | Added to the width of each character, in em, with `metrics: font` only. From -1 to 1. The default is 0. |
| `baseline` | Moves the characters down, in em. A negative value moves them up. From -2 to 2. The default is 0. |

The font size is the height of the font's em. In the menus, it is in units
of the original 640 × 480 screen. The menus use the sizes 10, 12, 15 and
20. In flight, it is in pixels of the flight screen, and flight uses the
sizes 10, 12 and 16. In flight, `metrics: font` keeps the line height of
the original font, and the widths of the characters are cut to whole
pixels.

The game's text uses the Windows-1252 character set. The font needs the
characters of that set that your text uses. The log warns about a character
that the font doesn't have.

A file can use up to 8 font files, of up to 64 MB each. Without
`fonts.yaml`, the game uses its own fonts. When the file has an error, the
log gives its line, and the game ignores the whole file.

### Bitmap fonts

The game's own fonts are bitmap fonts. A mod can repaint them at a higher
resolution, in the same folders:

| Files | Font |
|-|-|
| `frontend/fonts/font<size>.fnt` and `font<size>.png` | The menu font of size 10, 12, 15 or 20, at 4 times the original size. |
| `flight/fonts/font_tier_<n>.fnt` and `font_tier_<n>.ktx2` | The flight font: tier 0 for lines 16 pixels high, tier 1 for 12 and tier 2 for 10. |

The `.fnt` file gives the place of each character in the image. A bitmap
font only changes how the characters look: the game keeps the widths and
the layout of the original fonts. When `fonts.yaml` gives a TrueType font
for a size, the game uses it instead of the bitmap font.

The tools that come with the game don't make bitmap fonts. To change a
bitmap font, start from the files of an imported mod, which has the flight
fonts, repaint the image at the same size and keep the `.fnt` file. For the
menus, a TrueType font in `fonts.yaml` is the simpler way.

## Testing images

- The log gives the file and line of each problem in an atlas.
- An atlas with `game_geometry: true` whose KTX2 files are missing makes
  the image disappear, and the log says so.
- The player can set `assets.prefer_original_2d: true` in `config.yaml`.
  The game then draws the original images first, and only uses the mod's
  images that have `game_geometry: true` or that have no original. The
  TrueType fonts aren't used either. Set it to `false` while you test a
  mod.
