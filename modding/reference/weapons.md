<!-- Made by xwa_mod_tables. Change the tool, not this page. -->

# Weapons

## Weapon types

A weapon type is the object type of a shot, such as a laser bolt or a torpedo. Weapon types are object types 280 to 307, with the same names. Laser groups, weapon settings and weapon colours use them.

| Number | Name |
|---:|---|
| 280 | `rebel_laser` |
| 281 | `rebel_laser_turbo` |
| 282 | `imperial_laser` |
| 283 | `imperial_laser_turbo` |
| 284 | `ion_cannon` |
| 285 | `ion_cannon_turbo` |
| 286 | `torpedo` |
| 287 | `missile` |
| 288 | `turbolaser_red` |
| 289 | `turbolaser_green` |
| 290 | `turbolaser_ion` |
| 291 | `advanced_torpedo` |
| 292 | `advanced_missile` |
| 293 | `space_bomb` |
| 294 | `heavy_rocket` |
| 295 | `mag_pulse` |
| 296 | `ion_pulse` |
| 297 | `turbolaser_red_light` |
| 298 | `flare` |
| 299 | `spark_projectile` |
| 300 | `chaff` |
| 301 | `turbolaser_red_medium` |
| 302 | `turbolaser_red_heavy` |
| 303 | `turbolaser_green_light` |
| 304 | `turbolaser_green_medium` |
| 305 | `turbolaser_green_heavy` |
| 306 | `death_star_laser` |
| 307 | `custom_weapon` |

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

