# Image groups

The game's 2D images are in numbered image groups. A group holds one image
or a series of images, such as the frames of an explosion. The original
groups are in the `.DAT` files of the game's `RESDATA` folder.

A mod replaces a group with an atlas, `resdata/atlas/group_<number>.yaml`,
and its images. A mission can also have its own groups in
`resdata/mission_art/<name>/`. The images page describes the atlas format.

Every group can be replaced, except group 16000, the font of the flight
text. Fonts in flight come from `flight/fonts/` instead.

## Flight

| Group | What it holds |
|-|-|
| 1000 | Lens flares, engine glows, and the white flash when a craft enters hyperspace. The 3D craft in the menus also use its engine glows. |
| 2000 to 2005 | Explosions of craft and of hits. The game picks one of four at random. |
| 2006 | A large explosion cloud, made of many images. |
| 2007 | Electric sparks over the hull of a damaged craft. |
| 2008 | Electric sparks over the hull of a craft hit by ion weapons. |
| 3000 to 3003 | Sparks and small explosions when lasers hit. |
| 3004 | Spark particles. |
| 3005 | Flashes on the hull of a craft that is hit. |
| 3006 | A particle image. |
| 3050 | Blast marks on hulls. |
| 3051 | The hyperspace tunnel. |
| 3055 | The tractor beam on its target. |
| 3100 | Shield hits by ion weapons. |
| 3200 | Shield hits by rebel lasers. |
| 3300 | Shield hits by imperial lasers. |
| 3400 | Shield hits by mag pulses and flares. |
| 3500 | Shield hits by missiles, rockets and other weapons. |
| 4000 to 4003 | Debris. |
| 5000 | Chaff. |
| 6010 to 6114 | Planets in the backdrop of missions. `backdrops.extra_planets` in `flight/flight.yaml` can add more. |
| 6250 | The emitter of the Death Star superlaser. |
| 6251 | The beam of the Death Star superlaser. |
| 7001 to 7015 | Nebulae in the backdrop of missions. |
| 8001 to 8010 | Galaxies in the backdrop of missions. |
| 9001 to 9010 | Suns in the backdrop of missions. They give the lens flares. |
| 9100 | A backdrop of missions. |
| 10000 | The 2D HUD: sensors, displays, shield and laser bars, power indicators, the beam gauge and the command menu. |
| 10100 | Versions of the 2D HUD images for the original game's software renderer. |
| 12000 | The reticle, the threat lights and other parts of the HUD. A model's `hud.reticle` setting picks images from it. |
| 13000 | The arrow that points to a target off the screen. |
| 14610 to 14650 | The map icons in each IFF colour, for the original game's software renderer. |
| 14800 | The icons of craft on the map. `map_icons` in `flight/hud.yaml` picks images from it. |
| 16000 | The font of the flight text. It can't be replaced with an atlas. |
| 17002 | The shockwave when the second Death Star explodes. |
| 18000, 19000 to 19400 | Bands of images that wrap around the sky in some backdrops. |
| 21000 to 21025 | Smoke trails behind warheads. |
| 22000, 22003, 22007 | Particle images, such as the particles of impacts and collisions. |
| 25000 to 25030 | The hull icons in each damage colour, for the original game's software renderer. |
| 26000 | The craft outline in the shield and hull display. `hull_icons` in `flight/hud.yaml` picks images from it. |

## Frontend

| Group | What it holds |
|-|-|
| 15000 | Menu images: the side bars, the tech room pictures, the settings panels and their buttons. |
| 15001 | The concourse background. |
| 15010 to 15114 | The planet in the concourse window. A mission picks it with `concourse.front_planet`. |
| 15210 | The mission and battle pictures of the mission selection screens. |
| 15220 | The mouse cursors, and a few parts of the settings panels. |
| 15230 | The connection pictures of the multiplayer screens. |
| 15900 | The pilot's awards: medals, ranks, ratings, Kalidor crescents, and the medal case. |
| 20001 to 20230 | The picture of the chosen craft in the mission setup and combat screens. The group is 20000 plus the craft type. |

### Mission and battle pictures

Group 15210 holds three series of pictures:

| Image ids | Pictures |
|-|-|
| 1 to 999 | The large picture of each mission. |
| 1000 to 1999 | The small picture of each mission. |
| 2000 to 2099 | The picture of each battle: 2000 plus the battle number in the mission file names, such as 8 in `1b8m1.tie`. |

The original game has 53 mission pictures and 8 battle pictures. An atlas
can have more. The game uses every id up to the highest id that the atlas
has in each series.

## Groups that the game doesn't use

The game files also have groups 14000, 14100, 14500, 17000, 17001 and
22005. The game doesn't use them.
