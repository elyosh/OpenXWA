# XWAU and TFTC support

X-Wing Alliance Upgrade (XWAU) and the TIE Fighter Total Conversion (TFTC)
add many features to the original game. This page lists them and says how
far OpenXWA supports each one.

OpenXWA doesn't read XWAU's files as they are. The [importer](importer.md)
converts them into the files that the other pages describe. So a feature
that is implemented works in a mod that the importer made, and you can also
use it in your own mods with OpenXWA's files. The last column links to the
page that describes it.

| Status | Meaning |
|-|-|
| Implemented | Works with the data of XWAU and TFTC. |
| Partial | Some of it works. The notes say what is missing. |
| Not implemented | Doesn't work in OpenXWA. The importer leaves it out. |

The end of `import.log` lists the features of an installation that the
import left out. See [Importer](importer.md#when-an-import-stops).

## Models

| Feature | Status | Notes | See |
|-|-|-|-|
| Larger models (`hook_opt_limit`) | Implemented | Up to 255 parts, 255 engine glows and 120 weapon hardpoints. | [Models](models.md) |
| 32-bit OPT textures | Implemented | | [Models](models.md) |
| Levels of detail | Partial | The importer keeps only the most detailed level of an OPT. | [Models](models.md) |
| Object profiles | Implemented | Including the choice by flight group in missions. | [Models](models.md#profiles), [Missions](missions.md#object_profiles) |
| Skins | Implemented | Including skins by markings, and layers with opacity. | [Materials](materials.md#skins-and-markings) |
| Moving parts | Implemented | S-foils, landing gears, hatches, hangar doors, cargo clamps, pilot heads and rotating parts, with their start states in missions. | [Models](models.md#moving-parts) |
| `[MeshesCollisions]` and `TargetableMeshes` | Implemented | | [Models](models.md#collision-meshes) |
| `[EngineGlows]` by speed | Implemented | | [Craft](craft.md#engine_glows) |

## Materials and effects

| Feature | Status | Notes | See |
|-|-|-|-|
| Material files (`.mat`) | Implemented | Metallic, glossiness, specular, normal maps, shadeless surfaces and glass. | [Materials](materials.md#pbr-materials) |
| Animated textures and light maps | Implemented | Hull damage, shield and energy effects, warning lights, screens and blast marks. | [Materials](materials.md#flipbooks) |
| Lights in `.mat` files | Implemented | They become lights of the model. | [Models](models.md#lights) |
| Greebles | Implemented | They become detail maps. The UV displacement modes are left out. | [Materials](materials.md#detail-maps) |
| Lava | Implemented | The importer makes animated textures that look like XWAU's lava. | [Materials](materials.md#flipbooks) |
| `SkipWhenDisabled` and `DisplayIf` | Implemented | | [Craft](craft.md#surfaces) |
| Bloom | Implemented | OpenXWA has its own bloom, which follows the brightness of the image. `NoBloom` isn't needed: lower the emission of a surface that shouldn't glow. | [Materials](materials.md#pbr-materials) |
| `EffectBloom` | Not implemented | Used by two Death Star models. | |
| `AlphaToBloom` and `NoColorAlpha` | Partial | The surface becomes fully unlit. | |
| Alternative explosions (`alt_frames_N`) | Implemented | | [Images](images.md#frame-sets) |
| Procedural explosions | Not implemented | XWAU doesn't use them. | |

## Cockpits

| Feature | Status | Notes | See |
|-|-|-|-|
| Dynamic Cockpit | Implemented | Including the holograms. | [Materials](materials.md#cockpit-screens) |
| Active Cockpit | Not implemented | It needs VR. XWAU and TFTC turn it off. | |
| Moving stick, throttle and hyperspace levers | Implemented | Levers that slide or turn on a free axis are left out. No installation has them. | [Models](models.md#cockpit-controls) |
| Cockpit field of view | Implemented | | [Craft](craft.md#cockpit_view) |
| Target display edges (`enable_wireframe_CMD`) | Implemented | | [Flight](flight.md#target-display) |
| Cockpit shadows | Implemented | OpenXWA casts shadows from the cockpit model itself. The `ShadowMapping` files aren't needed. | |

## Craft and weapons

| Feature | Status | Notes | See |
|-|-|-|-|
| New craft types and changed craft values (`[ExeCraftValues]`) | Implemented | | [Craft](craft.md) |
| Stats profiles | Partial | `[StatsModifiers]` isn't converted. | [Craft](craft.md#stats_profiles), [Missions](missions.md#stats_profiles) |
| Shield recharge | Implemented | | [Craft](craft.md#shield) |
| Component strength (`[SubcomponentsStrength]`) | Implemented | | [Craft](craft.md#component_strength) |
| Gunner seats (`[Turrets]`) | Implemented | | [Craft](craft.md#gunner_seats) |
| Tech room ratings (`[SpecRci]`) | Implemented | OpenXWA computes them from the craft's stats. | [Craft](craft.md#the-tech-room) |
| Weapon colours | Implemented | By weapon, flight group and side, with the impact and light colours. | [Craft](craft.md#weapon-colours) |
| Laser rates and energy transfer (`[WeaponRate]`) | Partial | The rates that a mission sets for a flight group aren't converted. | [Craft](craft.md#laser-rates) |
| Weapon, warhead, energy and linking stats | Implemented | Including the mission's weapon profiles. | [Craft](craft.md#weapon-profiles) |
| Warhead lock (`[WarheadLock]`) | Partial | The global section and the keys for object profiles aren't converted. | [Craft](craft.md#warhead_lock) |
| Jamming | Implemented | The section of `default.ini` isn't converted. | [Craft](craft.md#jamming) |
| Two warhead launchers (`hook_weapon_type`) | Implemented | | [Craft](craft.md#warheads) |
| Docking | Implemented | | [Craft](craft.md#docking), [Flight](flight.md#docking) |
| More craft, projectiles and explosions (`hook_crafts_count`) | Implemented | | [Flight](flight.md#object_limits) |
| Craft sounds | Implemented | The hyperspace sound banks aren't converted. | [Craft](craft.md#sound), [Flight](flight.md#soundsyaml) |

## HUD and flight art

| Feature | Status | Notes | See |
|-|-|-|-|
| HD HUD, planets, backdrops and effects | Implemented | The importer converts the 32-bit DAT images. | [Images](images.md) |
| Backdrop scales (`BackdropScales.txt`) | Implemented | A mission's own scales aren't converted. | |
| Reticles and HUD colours by craft | Implemented | | [Craft](craft.md#hud) |
| Hull and map icons | Implemented | | [Flight](flight.md#hull-and-map-icons) |
| Flight text font | Implemented | | [Images](images.md#fonts) |
| Enhanced HUD | Not implemented | The bars of the target's shields, hull and systems next to the target brackets. | |

## Skies, lighting and effects

| Feature | Status | Notes | See |
|-|-|-|-|
| Skyboxes (`[SkyBoxes]`) | Implemented | Including overlays, the lighting values and the rotations. | [Environments](environments.md) |
| `DefaultStarfield.dds` | Not implemented | OpenXWA draws its own starfield. | |
| Procedural suns and lens flares | Implemented | The bloom strengths of `Bloom.cfg` for suns and flares aren't converted. | [Environments](environments.md#suns), [Flight](flight.md#suns) |
| Shadows | Implemented | OpenXWA has its own shadows, which players set in the video options. XWAU's ray-traced shadows aren't copied. | |
| Ambient occlusion (SSAO) | Implemented | OpenXWA has its own, which players set in the video options. | |
| Laser lights | Implemented | | |
| Hyperspace effect | Not implemented | OpenXWA draws its own hyperspace tunnel, and doesn't show XWAU's hyperspace images. | |
| Speed effect | Not implemented | The particles that show the craft's speed. | |

## Missions

| Feature | Status | Notes | See |
|-|-|-|-|
| The mod's own missions and mission lists | Implemented | | [Missions](missions.md#tie-missions) |
| `[mission_tie]` | Implemented | Markings, side, pilot voice, values of single craft, craft names and mission settings. | [Missions](missions.md) |
| Model swaps (`[Objects]`) | Partial | A swap of a model that has `[ExeCraftValues]` isn't converted. | [Missions](missions.md#model_swaps) |
| `[Resdata]` | Implemented | | [Missions](missions.md#resdata) |
| `[Sounds]` | Implemented | | [Missions](missions.md#sounds) |
| `[WarheadTypeCount]` | Implemented | | [Missions](missions.md#warheads) |
| `[WeaponProfiles]` | Implemented | | [Missions](missions.md#weapon-profiles) |
| `[StatsProfiles]` | Implemented | | [Missions](missions.md#stats_profiles) |
| `[concourse]` planet | Implemented | | [Missions](missions.md#concourse) |
| `[Interdiction]` | Partial | The abort sound plays when the player arrives in the listed regions. XWAU's hyperspace tunnel for an interdicted jump isn't implemented. | [Missions](missions.md#settings) |
| `[Headlights]` | Implemented | | [Missions](missions.md#headlights) |
| `[CraftsCount]` | Implemented | | [Missions](missions.md#object_limits) |

## Hangars

| Feature | Status | Notes | See |
|-|-|-|-|
| Custom hangars | Implemented | Hangar models, objects, maps, cameras, family hangars, and the choice by ship and side. | [Hangars](hangars.md) |
| A mission's hangar settings | Partial | The settings by ship and a mission's own hangar model aren't converted. | [Hangars](hangars.md#a-missions-hangar-section) |
| Hangar ambient sounds | Implemented | | [Flight](flight.md#soundsyaml) |
| Hangar lights and shadows | Implemented | OpenXWA lights the hangars in its own way. XWAU's light settings aren't needed. | [Hangars](hangars.md#hangar-lighting) |

## Menus and movies

| Feature | Status | Notes | See |
|-|-|-|-|
| HD concourse and wide rooms | Implemented | | [Frontend](frontend.md) |
| Menu font | Implemented | OpenXWA can't ship Consolas, so the importer uses Inconsolata Bold. | [Images](images.md#fonts) |
| Concourse layout (`ConcourseDoors.txt`) | Implemented | | [Frontend](frontend.md#concourse-layout) |
| Room animations (`ConcourseAnimations.txt`) | Partial | Animations that exist only in DAT files aren't played. TFTC has three. | [Frontend](frontend.md#room-animations) |
| Videos in place of images | Implemented | The tech room door, the Death Star briefing and the squadron logos. | [Frontend](frontend.md#videos-in-place-of-images) |
| HD movies | Implemented | | [Frontend](frontend.md#movies) |
| Movies for one mission | Implemented | The briefing room and the award ceremony. | [Frontend](frontend.md#movies-for-one-mission) |
| Film room and pod movies | Implemented | | [Frontend](frontend.md#room-movies) |
| Movies on points, rank and rating | Not implemented | TFTC uses them for its points and rank movies. | |
| Medals, Kalidor Crescent levels, mementos, rank and rating thresholds | Implemented | `MedalDetailListOrder.txt` isn't converted. No installation has it. | [Frontend](frontend.md#awards) |
| New battle images (`FrontEnd2_Battles.dat`) | Not implemented | | |
| Craft pictures for new craft types | Not implemented | | |
| 3D craft preview in skirmish (`SkirmishOpt*`, `[ViewConfiguration]`) | Not implemented | | |
| Film room background and hidden side panels | Not implemented | | |
| Tech room holograms | Not implemented | The tech room shows the craft as a model. | |

## Camera and controls

| Feature | Status | Notes | See |
|-|-|-|-|
| Gimbal lock fix | Implemented | It is on by default, and mods can't change it. | |
| Camera lag, tilt and cockpit sway (`CockpitLook`) | Implemented | Players can turn it off in the video options. The camera doesn't move back with speed. | [Flight](flight.md#camera) |
| HUD in the exterior view | Partial | The HUD and MFD keys don't work in the exterior view. | [Flight](flight.md#camera) |
| VR | Not implemented | Including the head positions of `POVOffsets.cfg` and `[CockpitPOVOffset]`. | |
| Head tracking | Not implemented | | |
| Telemetry | Not implemented | | |

## Game changes without mod data

Some XWAU hooks change the game without any mod data. OpenXWA doesn't
have these changes:

- the animation speed of explosions and debris, and the AI update rate
  (`hook_time`);
- countermeasures on reload (`hook_countermeasures`);
- the Victory Star Destroyer's craft values (`hook_patcher`);
- the joystick thresholds (`hook_joystick_ff`);
- the preset power keys (`hook_userdata`).
