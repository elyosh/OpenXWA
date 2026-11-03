<!-- Made by xwa_mod_tables. Change the tool, not this page. -->

# Weapons

## Weapon types

A weapon type is the object type of a shot, such as a laser bolt or a torpedo. Weapon types are object types 280 to 307, with the same names. Laser groups, weapon settings and weapon colours use them.

The other columns give the stats of the original game, which the `stats` weapon profiles of `flight/flight.yaml` and of the model settings can change. **Speed** is in speed units: 2.25 is 1 MGLT, and a shot of a fighter adds the fighter's speed. **Power** is the damage of a shot, in the points of hull and shields. **Life** is how long a shot lives, in game seconds of 236 ticks. **Score** is the score of a warhead hit.

| Number | Name | Speed | Power | Life | Score |
|---:|---|---:|---:|---:|---:|
| 280 | `rebel_laser` | 2000 | 250 | 1 | 0 |
| 281 | `rebel_laser_turbo` | 2000 | 500 | 1.5 | 0 |
| 282 | `imperial_laser` | 1800 | 200 | 1 | 0 |
| 283 | `imperial_laser_turbo` | 1800 | 400 | 1.5 | 0 |
| 284 | `ion_cannon` | 1400 | 200 | 1.5 | 0 |
| 285 | `ion_cannon_turbo` | 1600 | 400 | 2 | 0 |
| 286 | `torpedo` | 250 | 10000 | 40 | 9 |
| 287 | `missile` | 500 | 3000 | 25 | 6 |
| 288 | `turbolaser_red` | 1000 | 1500 | 3 | 0 |
| 289 | `turbolaser_green` | 900 | 1600 | 3 | 0 |
| 290 | `turbolaser_ion` | 400 | 800 | 5 | 0 |
| 291 | `advanced_torpedo` | 300 | 15000 | 45 | 15 |
| 292 | `advanced_missile` | 600 | 6000 | 25 | 15 |
| 293 | `space_bomb` | 50 | 65000 | 120 | 45 |
| 294 | `heavy_rocket` | 100 | 35000 | 90 | 24 |
| 295 | `mag_pulse` | 600 | 3000 | 25 | 9 |
| 296 | `ion_pulse` | 300 | 6000 | 40 | 9 |
| 297 | `turbolaser_red_light` | 1400 | 600 | 3 | 0 |
| 298 | `flare` | 500 | 500 | 5 | 6 |
| 299 | `spark_projectile` | 225 | 2000 | 10 | 0 |
| 300 | `chaff` | 0 | 0 | 0 | 0 |
| 301 | `turbolaser_red_medium` | 1200 | 750 | 3 | 0 |
| 302 | `turbolaser_red_heavy` | 1000 | 1000 | 4.5 | 0 |
| 303 | `turbolaser_green_light` | 1250 | 750 | 3 | 0 |
| 304 | `turbolaser_green_medium` | 1100 | 1000 | 3 | 0 |
| 305 | `turbolaser_green_heavy` | 900 | 1200 | 4.5 | 0 |
| 306 | `death_star_laser` | 0 | 1000000 | 10 | 0 |
| 307 | `custom_weapon` |  |  |  |  |

## Warheads

The warheads that a mission can give a flight group. Mission files and `warhead_capacity_percent` use these names. The numbers are the values in the `.TIE` mission files.

| Number | Name |
|---:|---|
| 1 | `space_bomb` |
| 2 | `heavy_rocket` |
| 3 | `missile` |
| 4 | `torpedo` |
| 5 | `advanced_missile` |
| 6 | `advanced_torpedo` |
| 7 | `mag_pulse` |
| 8 | `ion_pulse` |
| 9 | `advanced_mag_pulse` |
| 10 | `cluster_bomb` |

## Laser group kinds

Values for the `kind` of a laser group in `craft_record.lasers`.

| Name | Meaning |
|---|---|
| `none` | An empty group. |
| `laser_cannon` | Cannons that the pilot fires. They use the laser charge. |
| `ion_cannon` | Ion cannons that the pilot fires. They are the second set of cannons. |
| `turret` | Turrets that aim by themselves. They also shoot at incoming warheads. A gunner can fire them. |
| `long_range_turret` | Turrets with twice the default range. They don't fire at close targets or at warheads. |

## Hardpoint kinds

What a hardpoint type fires. The values of `hardpoint_kinds` in the weapon settings.

| Name | Meaning |
|---|---|
| `none` | Fires nothing. |
| `laser` | Fires the craft's lasers. |
| `warhead` | Fires the craft's warheads. |

## Weapon sound groups

The keys under each object type in the `weapon` sound bank of `flight/sounds.yaml`. Each group plays for the weapon types in the second column. The other weapon types, such as warheads, have no group in this bank.

| Name | Weapon types |
|---|---|
| `rebel_laser` | `rebel_laser` |
| `rebel_laser_turbo` | `rebel_laser_turbo` |
| `imperial_laser` | `imperial_laser` |
| `imperial_laser_turbo` | `imperial_laser_turbo` |
| `ion_cannon` | `ion_cannon` |
| `ion_cannon_turbo` | `ion_cannon_turbo` |
| `turbolaser_red` | `turbolaser_red`, `turbolaser_red_light`, `turbolaser_red_medium` and `turbolaser_red_heavy` |
| `turbolaser_green` | `turbolaser_green`, `turbolaser_green_light`, `turbolaser_green_medium` and `turbolaser_green_heavy` |
| `turbolaser_ion` | `turbolaser_ion` |
| `custom_weapon` | `custom_weapon` |

## Ranks

The keys of `weapons.fire_ratio_by_rank` in `flight/flight.yaml`, from the lowest rank to the highest.

| Name |
|---|
| `novice` |
| `officer` |
| `veteran` |
| `ace` |
| `top_ace` |
| `super_ace` |

## Difficulties

The keys of `weapons.damage_by_difficulty` in `flight/flight.yaml`.

| Name |
|---|
| `easy` |
| `medium` |
| `hard` |

