# Frontend

The frontend is everything outside flight, such as the concourse and its
rooms, the briefings, the menus and the movies. This page describes the
files of `frontend/` and the game text lists that go with them. The images
themselves use the atlas format of [the images page](images.md).

```text
frontend/
  frontend.yaml       design size, paint settings and screen layouts
  frontres/           menu images and videos that replace them
  animations.yaml     room animations
  animations/         the videos and sounds of room animations
  concourse/
    layout.yaml       positions of the doors and other items
    starfield_mask.png
  awards.yaml         medals, Kalidor crescents, ranks and mementos
  movies/             movies, their subtitles and movies.yaml
  fonts/              menu fonts, see the images page
```

## frontend.yaml

```yaml
design: [2560, 1440]
paint:
  fill_alpha: 0.7
  line_width: pixel
  grid_colours: [0xFF0000, 0x800000]
layouts:
  default: wide
  datapad: fill
  loading: fill
```

| Key | Meaning |
|-|-|
| `design` | `[width, height]`, the screen size that your menu art is made for. The positions and sizes in design pixels of every `frontend/` and `resdata/` file use it. The default is `[640, 480]`. |
| `paint` | How the game draws its fills, lines and the briefing grid. See below. |
| `layouts` | How each screen fits on a wide screen. See below. |
| `family_missions` | `false` makes the tour's first seven missions use the concourse. See below. |
| `side_panels` | The screens that don't show the side panels. See below. |

When `design` has a wrong value, the log says so, and the design size is
640 × 480.

### paint

| Key | Meaning |
|-|-|
| `fill_alpha` | The opacity of the game's translucent fills, such as the boxes behind some texts. From 0 to 1. The default is 0.5. |
| `line_width` | `unit` draws lines, outlines and the briefing grid one unit of the original 640 × 480 screen thick. `pixel` draws them one screen pixel thick. The default is `unit`. |
| `grid_colours` | `[major, minor]`, the colours of the major and minor lines of the briefing map grid, as hex numbers `0xRRGGBB`. Without it, the grid keeps its colours. |

A wrong value is logged, and that key keeps the original look.

### layouts

The original screens are 640 × 480. `layouts` sets how each screen is shown
on a screen of another shape:

| Layout | Effect |
|-|-|
| `classic` | The original 4:3 screen in the middle, with black bars at the sides. |
| `wide` | The screen is stretched across the whole width. Use it with art made for a wide `design` size. |
| `fill` | The items stay in the middle 4:3 area, as with `classic`, but a background image with `fit: screen_height` fills the whole screen. |

The keys are screen names, and `default`. [Frontend
screens](reference/frontend_screens.md) lists the names.

- A screen that the table doesn't list uses `default`.
- A view, such as `family_room.rank`, that the table doesn't list uses the
  layout of its screen.
- A dialog, such as `datapad`, that the table doesn't list uses the layout
  of the screen below it.
- Without `default`, or without the table, screens are `classic`.

When a name or a layout is wrong, the log says so, and the game ignores the
whole table, so every screen is `classic`.

### Background layouts

A background image can also set the layout of its screen. Add `layout` to
the image's atlas file or video file:

```yaml
layout: wide
```

While the screen shows that image as its background, the screen uses this
layout. It replaces the layout that `layouts` gives the screen. This helps
in a view that shows different images. For example, the memento view of
the family room shows the picture of the memento: a 16:9 picture can be
`wide`, and a 4:3 picture can be `fill`.

Only the main image behind the screen counts, not the images that the game
draws over it, such as doors and buttons. When a dialog such as the datapad
is open, its own background counts. An image whose file has no `layout`
keeps the layout of its screen.

### family_missions

The original game plays the first seven missions of the tour from the
family transport, so a new pilot starts in the family transport room.
`family_missions: false` makes these missions use the concourse, for a
campaign that starts on a big ship. The pilot statistics page then always
shows "On leave". The default is `true`.

### side_panels

Some screens have panels that slide in from the bottom corners, with
buttons on them. They are the images `leftbar1` to `leftbar5` and
`rightbar1` to `rightbar4` of group 15000. If your background already shows
the buttons, you can hide the panels:

```yaml
side_panels:
  simulator: false
  film_room: false
```

| Key | Screens |
|-|-|
| `simulator` | Mission setup, multiplayer setup, and the briefing and debriefing of a mission outside the campaign. |
| `tour_briefing` | The briefing of a campaign mission. |
| `tour_debriefing` | The debriefing of a campaign mission. |
| `family_monitor` | The monitor of the family room. |
| `film_room` | The film room. |

On a screen set to `false`, the game doesn't draw the panels or play their
sound. The buttons work as usual and appear when the panels would have
finished sliding in. The default for each screen is `true`.

## Menu images

The menu art of the game is in the files of its `FRONTRES` folder, such as
`FRONTRES/CONCOURSE/TOURDOOR.CBM`. A mod replaces such a file with an
atlas:

```text
frontend/frontres/<dir>/atlas/<base>.yaml
frontend/frontres/<dir>/atlas/<base>.ktx2
```

`<dir>` is the folder in `FRONTRES` and `<base>` is the file name without
its extension. For `FRONTRES/CONCOURSE/TOURDOOR.CBM`, the atlas is
`frontend/frontres/concourse/atlas/tourdoor.yaml`. [The images
page](images.md) describes the atlas format.

Many other menu images are in the image groups 15000 to 15230 and 20001
to 20230. They are replaced in `resdata/atlas`, see
[Image groups](reference/image_groups.md).

Two simpler forms also replace a `FRONTRES` file. They keep the original
image's geometry:

| Path | Use |
|-|-|
| `frontend/frontres/<dir>/sprites/<base>.ktx2` | one KTX2 file for an image with one frame |
| `frontend/frontres/<dir>/atlas/<base>/frame_00.ktx2`, `frame_01.ktx2`, ... | one KTX2 file per frame |

Use only one form for each image.

## Videos in place of images

A looping video can replace a `FRONTRES` image, such as a door that opens
or a screen in the background:

```text
frontend/frontres/<dir>/video/<base>.yaml
```

```yaml
video: techdoor.webm
size: [388, 333]
offset: [0, 0]
classic_frames: 60
fps: 24
```

| Key | Meaning |
|-|-|
| `video` | Required. The video file, in the same folder as this file. |
| `size` | Required. `[width, height]` in design pixels, larger than 0. The size the video is drawn at. |
| `offset` | `[x, y]` in design pixels, added to the position where the game draws the image. The default is `[0, 0]`. |
| `classic_frames` | The number of frames that the game sees, from 1 to 4096. The default is 1. |
| `fps` | The frames per second to play at. The default is the video's own rate. |
| `layout` | `classic`, `wide` or `fill`: the layout of the screen while the video is its background. See [Background layouts](#background-layouts). |

The video plays in a loop, with no sound, for as long as the game draws the
image, whatever frame the game asks for. When the image disappears and
comes back, the video starts again. A video takes priority over an atlas
for the same image.

An unknown key or a wrong value is logged, and the game ignores the file.
At most 16 videos play at once in the menus, room animations included.
[Video formats](#video-formats) lists the formats.

## Room animations

`frontend/animations.yaml` plays videos over the art of a room, such as
ships that pass the concourse windows. The videos and sounds are in
`frontend/animations/`. Only the concourse has room animations.

```yaml
groups:
  - room: concourse
    repeat: random
    delay: [0, 14.5]
    animations:
      - video: window_xwing.webm
        at: [0, 0]
        size: [2560, 300]
        sound: flyby.wav
        missions: [7, 8]
      - video: window_shuttle.webm
        at: [0, 0]
        size: [2560, 300]
        missions: {except: [7, 8]}
```

The file has one key, `groups`, which is a list of groups. A group plays
one of its animations at a time.

| Group key | Meaning |
|-|-|
| `room` | Required. The room: `concourse`. |
| `animations` | Required. The list of animations. |
| `repeat` | `random` picks a new animation after each play. `same` plays the same one again. The default is `random`. |
| `delay` | The wait before each play, in seconds: a number, or `[shortest, longest]` for a random wait. The default is no wait. |

| Animation key | Meaning |
|-|-|
| `video` | Required. The video file in `frontend/animations/`. |
| `at` | Required. `[x, y]`, the position of the video's top left corner, in design pixels. |
| `size` | `[width, height]` in design pixels. Without it, the video is drawn at its pixel size divided by `scale`. |
| `scale` | The video's pixels per design pixel, when there is no `size`. Above 0. The default is 1. |
| `fps` | The frames per second to play at. The default is the video's own rate. |
| `sound` | A WAV file in `frontend/animations/`, played at the start of each play. The name can have up to 43 characters. |
| `missions` | The missions in which the animation can play: `all`, a list of mission numbers, or `{except: [...]}`. The default is `all`. |
| `delay` | The wait before each play of this animation, in the same form as the group's `delay`. It replaces the group's delay when the group picks this animation. `0` is no wait. |
| `layer` | A whole number. The animations are drawn from the lowest layer to the highest. Without it, the layer is the animation's place in the file: 1 for the first animation, 2 for the next one, and so on, across all the groups. |

The mission numbers are the numbers of the missions in
`MISSIONS/MISSION.LST`. The game checks them against the pilot's current
mission.

When the player enters the room, each group picks a random animation among
those allowed in the current mission. Before each play, it waits for the
`delay`. The videos play without their own sound. `sound` plays at the
volume of the datapad sounds, again each time a looping video starts over.
The game draws the animations in the order of their `layer`. Without
`layer`, this is the order of the file.

An unknown key or a wrong value is logged with its line, and the game
ignores the whole file.

## Concourse layout

A mod whose art moves the doors and other items of the rooms gives their
new positions in `frontend/concourse/layout.yaml`. The values are in design
pixels. Every key is optional; a missing key keeps the original position.

```yaml
starfield: false
planet: {left: 0, right: 2560, top: 165, bottom: 375}
doors:
  tour: [928, 384]
  marko: [470.8, 1002.9]
combat_room:
  single_player: {door: [1537.2, 804], selection: [1536, 756, 984, 516]}
family_room:
  statistics: [1400, 684, 120, 150]
medals:
  positions: [[944, 210], [1160, 240], [1464, 210]]
```

In the tables, `[x, y]` is a point and `[x, y, width, height]` a rectangle.
The original values are in units of the 640 × 480 screen, which are design
pixels with the default design size.

### Concourse

| Key | Value | Meaning | Original |
|-|-|-|-|
| `starfield` | `true` or `false` | `false` hides the stars in the window. | `true` |
| `planet.left`, `planet.right` | numbers | The range across for the random centre of the planet in the window. | 0, 640 |
| `planet.top`, `planet.bottom` | numbers | The range down for the planet's centre. | 55, 125 |
| `doors.tour` | point | The Tour of Duty door. Its click area moves with it. | `[231, 174]` |
| `doors.training` | point | The training door. | `[536, 174]` |
| `doors.combat` | point | The combat room door. | `[35, 174]` |
| `doors.film` | point | The film room door. | `[536, 286]` |
| `doors.tech` | point | The tech room door. | `[426, 219]` |
| `doors.marko` | point | Marko. The click area is 110 × 80 units from this point. | `[110, 358]` |

A mission can also set the planet and its centre, see
[Missions](missions.md).

### Rooms

| Key | Value | Meaning | Original |
|-|-|-|-|
| `combat_room.multiplayer.door` | point | The multiplayer door. | `[24, 140]` |
| `combat_room.multiplayer.selection` | rectangle | Its click area. | `[36, 148, 282, 108]` |
| `combat_room.single_player.door` | point | The single player door. | `[373, 313]` |
| `combat_room.single_player.selection` | rectangle | Its click area. | `[384, 252, 246, 172]` |
| `combat_room.back_door` | point | The door back to the concourse. Its click area moves with it. | `[299, 90]` |
| `family_room.back` | rectangle | The click area that leaves the room. | `[0, 150, 120, 137]` |
| `family_room.statistics` | rectangle | The click area of the pilot's statistics. | `[403, 228, 96, 79]` |
| `family_room.play_mission` | rectangle | The click area that goes to the briefing of the family mission. | `[103, 288, 131, 128]` |
| `family_room.marko_eye` | point | Marko's eye while Marko speaks. | `[160, 200]` |
| `family_room.monitor` | point | The e-mail monitor. | `[426, 243]` |
| `briefing.squad_logos` | map of logo numbers to points | The squadron logo in the briefing room, for each logo number that a mission uses. The title and the order of battle move with it. | |

### Other screens

| Key | Value | Meaning | Original |
|-|-|-|-|
| `loading_screen.item.left`, `loading_screen.item.right` | numbers | The left and right edges of the briefing lines on the loading screen. The titles move with them. | 64, 573 |
| `loading_screen.item.height` | number | The height of each line. | 41 |
| `loading_screen.progress_bar` | `even` or `original` | `even` makes the progress bar grow by the same amount at each step and stay within the lines. | `original` |
| `mission_selection.panel_x` | number | The left edge of the mission description panel in the Tour of Duty and melee mission lists. | 230 |
| `create_pilot.name_field` | rectangle | The field for the new pilot's name. | `[250, 245, 190, 20]` |
| `cutscene_viewer.scroll_bar_x` | number | The left edge of the cutscene viewer's scroll bar. Its width stays the same. | 31 |

### Medals

| Key | Value | Meaning | Original |
|-|-|-|-|
| `medals.positions` | list of points | The battle medals in the medal case, in the order of `MEDALS.TXT`. A medal without a point in the list is drawn at `[0, 0]`. | |
| `medals.kalidor` | point | The Kalidor crescent in the medal case. | `[224, 168]` |
| `award_ceremony.show_names` | `true` or `false` | `false` hides the names of the awards during the award ceremony. | `true` |
| `award_ceremony.battle.images` | two points | The centre of a battle medal in the two steps of the ceremony. | `[[320, 240], [152, 360]]` |
| `award_ceremony.battle.text_offsets` | two points | Moves the medal's name in each step. | `[[0, 0], [0, 0]]` |
| `award_ceremony.kalidor.images` | two points | The centre of a Kalidor crescent in the two steps. | `[[320, 240], [388, 120]]` |
| `award_ceremony.kalidor.text_offsets` | two points | Moves the crescent's name in each step. | `[[0, 0], [0, 0]]` |

In `images` and `text_offsets`, the first entry is for the view with the
award in the middle of the screen, and the second for the other view.
Moving an award doesn't move its name. Use `text_offsets` for that.

An unknown key or a wrong value is logged with its line, and the game
ignores the whole file.

## Starfield mask

The concourse window shows twinkling stars. In the original game, the stars
only appear where the background is black. When the concourse background
gets its geometry from an atlas (`game_geometry: true`), the game can't see
its pixels, so it uses the mask `frontend/concourse/starfield_mask.png`
instead.

| Colour | Meaning |
|-|-|
| White | The window. Stars can appear here. |
| Black | No stars. |

A pixel counts as white when its red value is 128 or more. The mask covers
the whole screen and can have any size; a 640 × 480 mask matches the
original screen. The stars are only placed in a band near the top of the
screen, rows 25 to 124 of a 640 × 480 screen.

Without the mask, or when it has no white pixel in that band, the stars
appear across the whole band, and the log says why. `starfield: false` in
the [concourse layout](#concourse-layout) hides the stars.

## Awards

`frontend/awards.yaml` changes the medals, the Kalidor crescent levels, the
promotion thresholds and the mementos in the family room. Every key is
optional, and a missing key keeps the original behaviour.

```yaml
battle_medals:
  - {name: 'Medal of Redemption'}
  - {name: 'Star of Alderaan'}
kalidor:
  - {points: 5000, name: 'First Initiate'}
  - {}
  - {}
  - {}
  - {}
  - {}
  - {points: 90000, name: 'Grand Master'}
rank_thresholds: [7500, 15000, 25000]
rating_thresholds: [100, 200, 350]
mementos:
  images: {9: 'frontres\medals\tf.bmp'}
  names: {9: 'TIE Fighter Model'}
  lady_blue: {mission: 23}
  cologne: {mission: 9}
```

| Key | Meaning |
|-|-|
| `battle_medals` | The names of the battle medals, in the order of `MEDALS.TXT`. Up to 20 entries, each `{name: '...'}`. `MEDALS.TXT` still sets how many medals there are and which missions give them. |
| `kalidor` | One entry per Kalidor crescent level, up to 100. Each entry can have `points` and `name`. |
| `rank_thresholds` | Up to 8 scores that promote the pilot to the next rank. They replace the original values in order; the rest stay original. |
| `rating_thresholds` | Up to 25 values that raise the pilot's rating. They replace the original values in order; the rest stay original. |
| `mementos` | The mementos in the family room. See below. |

The length of `kalidor` is the number of levels. `points` is the bonus
score, in whole points, that reaches the level. Must be above 0. A level
without `points` keeps its original threshold, so only the first 6 levels
can leave it out. The values of `rank_thresholds` and `rating_thresholds`
must be whole numbers above 0.

### Mementos

Each memento belongs to a Tour of Duty mission. The keys of `images` and
`names` are the mission numbers of `MISSIONS/MISSION.LST`, from 0 to 255.

| Key | Meaning |
|-|-|
| `mementos.images` | The detail image of each memento, as the path of a `FRONTRES` file. With this key, a mission that isn't listed has no detail image, and its memento can't be clicked. Up to 139 characters. |
| `mementos.names` | The name of each memento: its tooltip and the title of its detail view. Up to 167 characters. |
| `mementos.lady_blue.mission` | The Tour of Duty mission that unlocks the Lady Blue in the family room. |
| `mementos.cologne.mission` | The Tour of Duty mission that unlocks the cologne. |

For `lady_blue` and `cologne`, the missions are counted from 1, so the
value is the mission number of `MISSION.LST` plus 1. The originals are 23
and 9. A value of 0 or less, or 255 or more, unlocks the item from the
start. The item's detail view shows the text that `FAMILYAWARDS.TXT` gives
for that mission.

Names can only use the characters of the Latin-1 set. When the file
doesn't name an award, the game uses its original name, or no name for an
award that the original game doesn't have.

When a key or a value has an error, the log gives its line, and the game
ignores the whole file.

### Award images

The award images are in image group 15900. New awards need new images in
that group's atlas, with these ids:

| Image | Id |
|-|-|
| The memento of mission number `n` | `100 + n`, or `1900 + n` from mission 100 |
| Battle medal `k`, counted from 0 | `200 + k` |
| Kalidor crescent level `i`, counted from 0 | `600 + i` |

The medal case and the award ceremony also show large images of the
medals and crescents. `FRONTRES/MEDALS/MEDALS.LST` lists them, and a mod
can replace it with a copy in `game/FRONTRES/MEDALS/MEDALS.LST`. Its first
line is a header that the game skips. Each other line is
`<file> <name> <number>`: the image file in `FRONTRES/MEDALS`, the name
that the game asks for, and 1 to compress the image or 0. The game asks for
these names:

| Name | Image |
|-|-|
| `battlezoom<k>` | Battle medal `k`, large, in the medal case. |
| `lbattlezoom<k>` | Battle medal `k`, at the award ceremony. |
| `kalidorzoom<i>` | Kalidor crescent level `i`, large, in the medal case. |
| `lkalidorzoom<i>` | Kalidor crescent level `i`, at the award ceremony. |

`k` and `i` count from 0. For an image that the game folder doesn't have,
add its line to `MEDALS.LST` and give the image as an atlas, in
`frontend/frontres/medals/atlas/<base>.yaml` with `game_geometry: true`. See
[Menu images](#menu-images).

A new battle medal needs a line in [MEDALS.TXT](#medalstxt), an image
`200 + k` in group 15900, the two large images in `MEDALS.LST` and a name in
`awards.yaml`. The original medal case only has places for 7 medals. Give
the place of each other medal in `medals.positions` of the
[concourse layout](#concourse-layout). A new Kalidor crescent level needs
an entry in `kalidor` of `awards.yaml`, an image `600 + i` in group 15900
and the two large images in `MEDALS.LST`.

## Movies

A movie in `frontend/movies/<name>.mp4` replaces the original movie
`MOVIES/<name>.SNM`. The file must be an MP4 file with the `.mp4`
extension. For example, `frontend/movies/Battle1.mp4` replaces
`MOVIES/BATTLE1.SNM`. The movie is scaled to fit the screen and keeps its
shape.

A mod can also add movies with new names, for the room movies below and for
`CUTSCENE.TXT`.

### Movies for one mission

The game first looks for a mission's own version of three movies:

| Movie | When it plays |
|-|-|
| `ceremony` | the award ceremony |
| `marko` | entering the briefing room for a family mission |
| `briefroom` | entering the briefing room for other missions |

The mission's version is named `<mission>_<movie>`, where `<mission>` is
the mission's file name without its extension, such as
`frontend/movies/1B1M7G_ceremony.mp4`. Without it, the normal movie plays.

### Room movies

`frontend/movies/movies.yaml` picks the movie that a room plays when the
player enters it:

```yaml
film_room: Filmroom
single_player_pod: Pod
multiplayer_pod: Pod
```

| Key | Meaning |
|-|-|
| `film_room` | The movie of the film room. |
| `single_player_pod` | The movie of the single player pod. |
| `multiplayer_pod` | The movie of the multiplayer pod. |

The values are movie names without an extension, up to 63 characters. The
movie can be in the mod or in the game. Without a key, and when the movie
isn't found, the room plays `pod`, as in the original game. When a key or a
value has an error, the log says so, and the game ignores the whole file.

### Subtitles

A movie of the mod uses the subtitles in `frontend/movies/<name>.sub`. It
never uses the subtitles of the original movie, which are timed for the
original. The file has one block per subtitle:

```text
// x, y
// red, green, blue
// first frame, last frame
// font size
// text
-1,400
255,255,255
30,90
12
!MY_MOVIE_LINE1!We have to leave now.
*
```

| Line | Meaning |
|-|-|
| 1 | The position in units of the 640 × 480 screen. A negative value centres the text across or down. |
| 2 | The colour, from 0 to 255 per channel. |
| 3 | The first and last frame, counted at 15 frames per second of playback. |
| 4 | The font size: 10, 12, 15 or 20, the sizes of the menu fonts. Another size shows nothing. See [Fonts](images.md#fonts). |
| 5 | The text. It can start with `!NAME!`: the game then uses the text named `NAME` when the mod's text has one, and the rest of the line when it doesn't. See [Text](text.md). |
| 6 | `*`, the end of the block. |

Lines that start with `//` are skipped before each block. List the
subtitles in time order. A subtitle fades in and out. A centred subtitle
that is too wide for the screen is split into two lines.

### Original movies

When the player sets `assets.prefer_original_movies: true` in
`config.yaml`, the game plays the original movies. It doesn't use the
mod's movies for one mission or the mod's room movies. A movie of the mod
with a new name, such as one that `CUTSCENE.TXT` lists, still plays.

## Video formats

| Use | Format |
|-|-|
| Movies, `frontend/movies/<name>.mp4` | MP4 with H.264 video and AAC sound |
| Videos in place of images and room animations | WebM with VP9 video, with or without alpha, or MP4 with H.264 video |

Videos in place of images and room animations play without sound.
For transparency, use a WebM file with VP9 and an alpha channel, written
with normal (straight) alpha. Other formats and codecs aren't supported.

These FFmpeg command lines make files in these formats:

```sh
# A movie
ffmpeg -i movie.mov -c:v libx264 -pix_fmt yuv420p -c:a aac Battle1.mp4
# A video with alpha, from numbered PNG files
ffmpeg -framerate 24 -i door_%04d.png -c:v libvpx-vp9 -pix_fmt yuva420p -an techdoor.webm
```

## Cutscene and medal lists

Three text lists of the game go with the frontend. They aren't YAML. A mod
replaces them with a copy in `game/`, see [Replacing original game
files](mod_folder.md#replacing-original-game-files). They use the game's
encoding, Windows-1252. In each list, a mission is given by its number in
`MISSIONS/MISSION.LST`.

### CUTSCENE.TXT

`game/FRONTRES/CUTSCENE/CUTSCENE.TXT` lists the cutscenes that play between
Tour of Duty missions. Each cutscene is a block of six lines:

```text
// movie, picture, description, mission, 0 before or 1 after the debriefing
Battle1
cut2
!CUT_DESC_BATTLE1!Rebels escape into hyperspace.
15
0
*
```

| Line | Meaning |
|-|-|
| 1 | The movie's name, up to 127 characters. A movie of the mod with this name replaces the original, see [Movies](#movies). |
| 2 | The name of the cutscene's picture in the cutscene viewer, up to 31 characters. It must be a picture of the original cutscene viewer: a mod can't add new ones. A mod can change the image of a picture, as a menu image of `frontend/frontres/cutscene/`. See [Menu images](#menu-images). |
| 3 | The description, up to 127 characters. It can start with `!NAME!`, as in subtitles. |
| 4 | The mission number. |
| 5 | `0` plays the cutscene before the debriefing of that mission, `1` after it. |
| 6 | `*`, the end of the block. |

Lines that start with `//` are skipped. The game counts the cutscenes by
the lines that start with `*`, so every block needs one. The cutscene
viewer in the options shows a cutscene once the pilot has completed its
mission.

### MEDALS.TXT

`game/FRONTRES/MEDALS/MEDALS.TXT` lists the missions that give the battle
medals, with one mission number per line, up to 20 lines. Every line
counts, so the file has no comments. The first line is the first medal, and
so on.
The names of the medals can be set in [awards.yaml](#awards), and their
images in group 15900, see [Award images](#award-images).

### FAMILYAWARDS.TXT

`game/FRONTRES/MEDALS/FAMILYAWARDS.TXT` has the texts of the mementos'
detail views. Each memento is a block:

```text
1
!FAMAWARD_MEDAL0!Key to Harlequin Station
!FAMAWARD_MEDAL0_DESC!Presented to you by the inhabitants of the station.
*
```

| Line | Meaning |
|-|-|
| 1 | The mission number plus 1. |
| 2 | The memento's name. The game doesn't use this line: names come from `mementos.names` in [awards.yaml](#awards) or from the game's text. |
| 3 | The text of the detail view. It can start with `!NAME!`, as in subtitles. |
| 4 | `*`, the end of the block. |

Lines that start with `//` are skipped. A block can have more name and text
pairs before its `*`; the texts are joined. The file can have up to 100
blocks.
