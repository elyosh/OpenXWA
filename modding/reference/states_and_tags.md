<!-- Made by xwa_mod_tables. Change the tool, not this page. -->

# States and tags

## Flipbook states

Values for the `state` of a flipbook in a model's material. The states are those of the player's craft, so flipbooks only play on the player's craft and cockpit.

A material plays the first flipbook in its list whose state is on. When no state is on, it shows its normal texture. A flipbook starts again from its first frame each time its state comes on.

`broken.cmd`, `broken.laser_ion`, `broken.throttle`, `broken.sensors`, `broken.laser_recharge` and `broken.engine_power` are also on for a craft that doesn't have the system.

| Name | On when |
|---|---|
| `threat.laser` | A craft is firing lasers at the player. |
| `threat.turret` | An enemy turret is aimed at the player. |
| `threat.beam` | An enemy craft aims its beam at the player. |
| `threat.missile_locking` | An enemy is getting a warhead lock on the player. The state blinks, so the flipbook starts again at each blink. |
| `threat.missile_locked` | An enemy has a warhead lock on the player. |
| `cannon.1` | Cannon 1 fires next. |
| `cannon.2` | Cannon 2 fires next. |
| `cannon.3` | Cannon 3 fires next. |
| `cannon.4` | Cannon 4 fires next. |
| `cannon.5` | Cannon 5 fires next. |
| `cannon.6` | Cannon 6 fires next. |
| `cannon.7` | Cannon 7 fires next. |
| `cannon.8` | Cannon 8 fires next. |
| `broken.cmd` | The targeting computer is damaged. |
| `broken.laser_ion` | The lasers or ion cannons are damaged. |
| `broken.beam_weapon` | The beam weapon is damaged. |
| `broken.shields` | The shields are damaged. |
| `broken.throttle` | The throttle is damaged. |
| `broken.sensors` | A sensor is damaged. |
| `broken.laser_recharge` | The laser recharge is damaged. |
| `broken.engine_power` | The engine power is damaged. |
| `broken.shield_recharge` | The shield recharge is damaged. |
| `broken.beam_recharge` | The beam recharge is damaged. |
| `hull.25` | The hull is at 25% or less. |
| `hull.50` | The hull is at 50% or less, and above 25%. |
| `hull.75` | The hull is at 75% or less, and above 50%. |
| `laser_lock` | The target is in laser range. |
| `warhead.locking` | The player is getting a warhead lock. |
| `warhead.locked` | The player has a warhead lock. |
| `hangar` | The player is in a hangar. |
| `target` | The player has a target. |
| `always` | Always on. Put it last, as the flipbook to play when no other state is on. |

## Part tags

In a few models, the game moves or hides some parts, or reacts when they are destroyed. A model that replaces one of these models marks these parts with the tags below, in the `tags` of the part. **Parts** says how many parts can have the tag.

When no part has a tag that marks one part, the game uses the part with the same number as in the original model. Parts are numbered in the order of the model's children.

| Tag | Models | Parts | Meaning |
|---|---|---|---|
| `platform.hidden_by_beam` | `platform1`, `platform2`, `platform3`, `platform4`, `platform5` | any number | Hidden when the flight group has a beam weapon. |
| `platform.hidden_by_jamming_decoy` | `platform1`, `platform2`, `platform3`, `platform4`, `platform5` | any number | Hidden when the flight group has a jamming or decoy beam. |
| `hangarcrane.arm` | `hangarcrane` | one | The arm that the hangar scene moves. |
| `hangardroid.nod` | `hangardroid` | one | The part that nods in the hangar scene. |
| `hangardroid2.swing` | `hangardroid2` | one | The part that swings in the hangar scene. |
| `hangardroid2.spin` | `hangardroid2` | one | The part that spins in the hangar scene. |
| `workdroid1.spin` | `workdroid1` | one | The part that spins in the hangar scene. |
| `workdroid1.turn` | `workdroid1` | one | The part that turns in the hangar scene. |
| `workdroid1.nod.1` | `workdroid1` | one | The first part that nods in the hangar scene. |
| `workdroid1.nod.2` | `workdroid1` | one | The second part that nods in the hangar scene. |
| `accelring.core` | `accelring2`, `accelring3` | one | The core that the player destroys in the proving grounds. |
| `compactor.door.1` | `compactor` | one | The first door. |
| `compactor.door.2` | `compactor` | one | The second door. It decides when the compactor is fully open or closed. |
| `compactor.door.3` | `compactor` | one | The third door. |
| `compactor.door.4` | `compactor` | one | The fourth door. |
| `centrifuge.spin.1` | `centrifuge` | one | The spinning part of the first mechanism. |
| `centrifuge.spin.2` | `centrifuge` | one | The spinning part of the second mechanism. |
| `centrifuge.spin.3` | `centrifuge` | one | The spinning part of the third mechanism. |
| `centrifuge.door.1` | `centrifuge` | one | The door of the first mechanism. |
| `centrifuge.door.2` | `centrifuge` | one | The door of the second mechanism. |
| `centrifuge.door.3` | `centrifuge` | one | The door of the third mechanism. |
| `smeltingroom.turret.1` | `smeltingroom` | one | The turret that fires first, from weapon hardpoint 1. |
| `smeltingroom.turret.2` | `smeltingroom` | one | The turret that fires second, from weapon hardpoint 2. |
| `smeltingroom.turret.3` | `smeltingroom` | one | The turret that fires third, from weapon hardpoint 3. |
| `smeltingroom.turret.4` | `smeltingroom` | one | The turret that fires fourth, from weapon hardpoint 4. |
| `smeltingroom.turret.5` | `smeltingroom` | one | The turret that fires fifth, from weapon hardpoint 5. |
| `smeltingroom.turret.6` | `smeltingroom` | one | The turret that fires sixth, from weapon hardpoint 6. |
| `smeltingroom.power.1` | `smeltingroom` | one | Destroying it stops turrets 1 to 3. |
| `smeltingroom.power.2` | `smeltingroom` | one | Destroying it stops turrets 4 to 6. |
| `smeltingroom.target` | `smeltingroom` | one | The only part that the proving grounds turbolaser can hit. |
| `dsreactorcore.regenerator` | `dsreactorcore` | one | Must be destroyed before the reactor can be damaged. |
| `dsreactorcore.reactor` | `dsreactorcore` | one | Destroying it starts the destruction of the Death Star. |

## Cockpit control tags

A control is a part of a cockpit model that the game moves, such as the flight stick. Its `tags` say which value moves it. The game uses the first tag of the control that it knows. A control without one of these tags doesn't move. Only the controls of the player's cockpit move.

| Tag | Value |
|---|---|
| `cockpit.stick.x` | The stick from left to right, -1 to 1. |
| `cockpit.stick.y` | The stick from forward to back, -1 to 1. |
| `cockpit.stick.roll` | The roll, -1 to 1. |
| `cockpit.throttle` | The throttle, 0 to 1. |
| `cockpit.hyperspace` | Goes from 0 to 1 when the craft enters hyperspace, stays at 1 in hyperspace, and goes back to 0 when it leaves. |

## Material and light tags

Material and light tags have no fixed names. The `surfaces` group of the model settings file names them, and hides the materials and lights with a tag while the craft is disabled or slow. For example, a tag `engine_glow` on the engine materials can hide them when the craft stops. A model can use up to 32 different material and light tags.

