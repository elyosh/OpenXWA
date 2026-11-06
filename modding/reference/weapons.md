<!-- Made by xwa_mod_tables. Change the tool, not this page. -->

# Weapons

## Weapon types

A weapon type is the object type of a shot, such as a laser bolt or a torpedo. Weapon types are object types 280 to 307, with the same names. Laser groups, weapon settings and weapon colours use them.

The other columns give the stats of the original game, which the `stats` weapon profiles of `flight/flight.yaml` and of the model settings can change. **Speed** is in MGLT, and a shot of a fighter adds the fighter's speed. **Power** is the damage of a shot, in the points of hull and shields. **Life** is how long a shot lives, in milliseconds. **Score** is the score of a warhead hit.

| Number | Name | Speed | Power | Life | Score |
|---:|---|---:|---:|---:|---:|
| 280 | `rebel_laser` | 888.89 | 250 | 944 | 0 |
| 281 | `rebel_laser_turbo` | 888.89 | 500 | 1416 | 0 |
| 282 | `imperial_laser` | 800 | 200 | 944 | 0 |
| 283 | `imperial_laser_turbo` | 800 | 400 | 1416 | 0 |
| 284 | `ion_cannon` | 622.22 | 200 | 1416 | 0 |
| 285 | `ion_cannon_turbo` | 711.11 | 400 | 1888 | 0 |
| 286 | `torpedo` | 111.11 | 10000 | 37760 | 9 |
| 287 | `missile` | 222.22 | 3000 | 23600 | 6 |
| 288 | `turbolaser_red` | 444.44 | 1500 | 2832 | 0 |
| 289 | `turbolaser_green` | 400 | 1600 | 2832 | 0 |
| 290 | `turbolaser_ion` | 177.78 | 800 | 4720 | 0 |
| 291 | `advanced_torpedo` | 133.33 | 15000 | 42480 | 15 |
| 292 | `advanced_missile` | 266.67 | 6000 | 23600 | 15 |
| 293 | `space_bomb` | 22.22 | 65000 | 113280 | 45 |
| 294 | `heavy_rocket` | 44.44 | 35000 | 84960 | 24 |
| 295 | `mag_pulse` | 266.67 | 3000 | 23600 | 9 |
| 296 | `ion_pulse` | 133.33 | 6000 | 37760 | 9 |
| 297 | `turbolaser_red_light` | 622.22 | 600 | 2832 | 0 |
| 298 | `flare` | 222.22 | 500 | 4720 | 6 |
| 299 | `spark_projectile` | 100 | 2000 | 9440 | 0 |
| 300 | `chaff` | 0 | 0 | 0 | 0 |
| 301 | `turbolaser_red_medium` | 533.33 | 750 | 2832 | 0 |
| 302 | `turbolaser_red_heavy` | 444.44 | 1000 | 4248 | 0 |
| 303 | `turbolaser_green_light` | 555.56 | 750 | 2832 | 0 |
| 304 | `turbolaser_green_medium` | 488.89 | 1000 | 2832 | 0 |
| 305 | `turbolaser_green_heavy` | 400 | 1200 | 4248 | 0 |
| 306 | `death_star_laser` | 0 | 1000000 | 9440 | 0 |
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
| `slow_turret` | Turrets that fire half as often as `turret` and don't shoot at warheads. Unlike `long_range_turret`, their default range is 2000 m, and they also fire at close targets. |

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

