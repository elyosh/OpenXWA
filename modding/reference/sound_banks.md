<!-- Made by xwa_mod_tables. Change the tool, not this page. -->

# Sound banks

The banks of `flight/sounds.yaml`. Most banks have one sound for each object type. **Entries** gives the keys of a bank's entries. An object type that a bank doesn't list plays the sound of the original game, unless the bank sets `unlisted: silent`.

For the cockpit, fly-by, engine wash, take-off, hyperspace and weapon sounds, a model's `sound` settings take priority over the banks.

| Bank | Entries | When it plays |
|---|---|---|
| `canopy_opening` | object type | When the player's craft stops in the hangar after it lands. |
| `engine_slowing` | object type | When the player's craft has turned around in the hangar after it lands, and when the hangar shuttle lands. |
| `engine_wash` | object type | While the player's craft is in the engine wash of another craft. The craft that makes the wash picks the sound. |
| `fly_by` | object type | When a craft passes close to the player's craft. The passing craft picks the sound. Also when the player's craft launches from the hangar. |
| `hangar_ambient` | object type, then a list of up to 10 sounds | Now and then in the hangar, chosen at random. The command ship of the hangar picks the list. |
| `hyper_abort` | object type | When the player's craft leaves hyperspace in a region that the mission marks as interdicted. |
| `hyper_end` | object type | When the player's craft leaves hyperspace. |
| `hyper_start` | object type | When the player's craft starts a hyperspace jump. |
| `hyper_zoom` | object type | A moment after `hyper_start`, when the player's craft enters hyperspace. |
| `interior` | object type | The engine sound in the player's cockpit. |
| `s_foil` | object type | When the S-foils or the landing gear of the player's craft open or close. |
| `shuttle_blast_off` | object type | When the hangar shuttle speeds out of the hangar. |
| `shuttle_shut_down` | object type | When the hangar shuttle lands and stops. |
| `shuttle_take_off` | object type | When the hangar shuttle starts to leave. |
| `sounds` | sound name | Single sounds, listed below. |
| `starship_ambient` | object type | While the player's craft is close to another craft. Each craft plays the sound of its own object type. Needs the `starship_ambient_sounds` mission setting. |
| `take_off` | object type | When the player's craft launches from the hangar, and when the hangar shuttle starts to leave. |
| `weapon` | object type, then weapon sound group | When a craft fires. The firing craft picks the entry, then the weapon sound group of the shot. |
| `weapon_explosion` | object type, then a list of up to 8 sounds | When a shot hits or an object explodes. The craft that fired the shot, or the object that explodes, picks the list. |
| `weapon_range` | names that the mod chooses | When a craft fires a weapon that its model's `sound.weapon_ranges` setting lists. Each shot plays one of the listed sounds, chosen at random. |

## Sounds of the `sounds` bank

The `sounds` bank has these entries, by name.

| Name | When it plays |
|---|---|
| `player_targeted` | When a warhead is fired at the player's craft. |
| `map_enter` | When the player opens the map and the map selects a target. |
| `map_exit` | When the view comes back to the player's craft, such as when the player closes the map. |
| `turret_switch` | When the player changes gunner seat. |
| `target_select` | When the player selects a new target. |

