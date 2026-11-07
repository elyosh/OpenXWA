<!-- Made by xwa_mod_tables. Change the tool, not this page. -->

# Craft types

A craft type is the type that a mission gives a flight group. It is also a line of the ship list and of the tech room. Each craft type points to an object type, which gives the model and the flight data.

Craft types have numbers, not names. A craft type file gives the number in `craft_type`. **Name** is the name in the original ship list.

| Number | Name | Object type | Category |
|---:|---|---|---|
| 1 | Xwing | `xwing` | `fighter` |
| 2 | Ywing | `ywing` | `fighter` |
| 3 | Awing | `awing` | `fighter` |
| 4 | Bwing | `bwing` | `fighter` |
| 5 | TIE Fighter | `tie_fighter` | `fighter` |
| 6 | TIE Interceptor | `tie_interceptor` | `fighter` |
| 7 | TIE Bomber | `tie_bomber` | `fighter` |
| 8 | TIE Advanced | `tie_advanced` | `fighter` |
| 9 | TIE Defender | `tie_defender` | `fighter` |
| 10 | Authority IRD | `ird_fighter` | `fighter` |
| 11 | Toscan Fighter | `toscan_fighter` | `fighter` |
| 12 | Missile Boat | `missile_boat` | `fighter` |
| 13 | Twing | `twing` | `fighter` |
| 14 | Z-95 | `z95` | `fighter` |
| 15 | R-41 | `r41` | `fighter` |
| 16 | Gunboat | `assault_gunboat` | `fighter` |
| 17 | Shuttle | `shuttle` | `light_transport` |
| 18 | Escort Shuttle | `escort_shuttle` | `light_transport` |
| 19 | Sys Patrol Craft | `system_patrol_craft` | `light_transport` |
| 20 | Scout Craft | `scout_craft` | `light_transport` |
| 21 | Stormtrpr Trans | `stormtrooper_transport` | `light_transport` |
| 22 | Assault Trans | `assault_transport` | `light_transport` |
| 23 | Escort Trans | `escort_transport` | `light_transport` |
| 24 | Tug | `tug` | `utility` |
| 25 | Combat Util Vehicle | `combat_utility_vehicle` | `utility` |
| 26 | CN/A Brick | `container_brick` | `container` |
| 27 | CN/B Hexbox | `container_hex_box` | `container` |
| 28 | CN/C Tube | `container_tube` | `container` |
| 29 | CN/D Pronged | `container_pronged` | `container` |
| 30 | Heavy Lifter | `heavy_lifter` | `utility` |
| 31 | Mole Miner | `mole_miner` | `utility` |
| 32 | Bulk Freighter | `bulk_freighter` | `heavy_transport` |
| 33 | Cargo Ferry | `cargo_ferry` | `heavy_transport` |
| 34 | Modular Conveyor | `modular_conveyor` | `heavy_transport` |
| 35 | Container Trans | `container_transport` | `heavy_transport` |
| 36 | Rebel Med Transport | `tuna_boat2` | `heavy_transport` |
| 37 | Muurian Transport | `muurian_transport` | `light_transport` |
| 38 | Corellian Trans | `corellian_transport2` | `light_transport` |
| 39 | Millenium Falcon | `millenium_falcon2` | `light_transport` |
| 40 | Corvette | `corvette2` | `starship` |
| 41 | Modified Corvette | `mod_corvette` | `starship` |
| 42 | Nebulon B Frigate | `frigate2` | `starship` |
| 43 | Mod Neb B Frigate | `mod_frigate` | `starship` |
| 44 | Passenger Liner | `passenger_liner` | `starship` |
| 45 | Carrack Cruiser | `carrack_cruiser` | `starship` |
| 46 | Strike Cruiser | `strike_cruiser` | `starship` |
| 47 | Escort Carrier | `escort_carrier` | `starship` |
| 48 | Dreadnaught | `dreadnaught2` | `starship` |
| 49 | Calamari Cruiser | `calamari_cruiser_new` | `starship` |
| 50 | Lt Calamari Cruiser | `light_calamari_cruiser` | `starship` |
| 51 | Interdictor | `interdictor2` | `starship` |
| 52 | Vict Star Dest | `victory_star_destroyer2` | `starship` |
| 53 | Imp Star Dest | `imperial_star_destroyer2` | `starship` |
| 54 | SSD | `super_star_destroyer` | `starship` |
| 55 | CN/E Hemisphere | `container_hemisphere` | `container` |
| 56 | CN/F Slotted | `container_slotted` | `container` |
| 57 | CN/G Hourglass | `container_hourglass` | `container` |
| 58 | CN/H Gem | `container_gem` | `container` |
| 59 | CN/I Y shaped | `container_yshaped` | `container` |
| 60 | Platform 1 | `platform1` | `station` |
| 61 | Platform 2 | `platform2` | `station` |
| 62 | Platform 3 | `platform3` | `station` |
| 63 | Platform 4 | `platform4` | `station` |
| 64 | Platform 5 | `platform5` | `station` |
| 65 | Platform 6 | `platform6` | `station` |
| 66 | Asteroid Hangar | `asteroid_base` | `station` |
| 67 | Ast Laser Bat | `asteroid_laser_battery` | `weapon_emplacement` |
| 68 | Ast Warhd Launcher | `asteroid_warhead_launcher` | `weapon_emplacement` |
| 69 | Factory | `factory` | `station` |
| 70 | Satellite1 | `comm_sat1` | `satellite_buoy` |
| 71 | Satellite2 | `comm_sat2` | `satellite_buoy` |
| 72 | Satellite3 | `comm_sat3` | `satellite_buoy` |
| 73 | *Satellite4 | `comm_sat3` | `satellite_buoy` |
| 74 | *Satellite5 | `comm_sat3` | `satellite_buoy` |
| 75 | Mine1 | `mine1` | `mine` |
| 76 | Mine2 | `mine2` | `mine` |
| 77 | Mine3 | `mine3` | `mine` |
| 78 | GunPlatform | `gun_platform` | `weapon_emplacement` |
| 79 | *Mine5 | `mine3` | `weapon_emplacement` |
| 80 | Probe1 | `probe` | `satellite_buoy` |
| 81 | Probe Capsule | `probe_capsule` | `satellite_buoy` |
| 82 | *Probe3 | `probe` | `satellite_buoy` |
| 83 | Buoy1 | `nav_buoy1` | `satellite_buoy` |
| 84 | Buoy2 | `nav_buoy2` | `satellite_buoy` |
| 85 | Hyper buoy | `hyper_buoy` | `satellite_buoy` |
| 86 | Asteroid | `asteroid_hr1` | Planet/asteroid |
| 87 | Planet | `asteroid_hr1` | Planet/asteroid |
| 88 | Rendezvous Buoy | `nav_buoy3` | `satellite_buoy` |
| 89 | Cargo Canister | `cargo_canister` | `container` |
| 90 | Shipyard | `ship_yard` | `station` |
| 91 | Repair Yard | `repair_yard` | `station` |
| 92 | *Mod Strike Cruiser | `mod_strike_cruiser` | `starship` |
| 93 | Lancer Frigate | `lancer_frigate` | `starship` |
| 94 | Bulk Cruiser | `bulk_cruiser` | `starship` |
| 95 | Assault Frigate | `assault_frigate` | `starship` |
| 96 | Corellian Gunship | `corellian_gunship` | `starship` |
| 97 | Imp Landing Craft | `imperial_landing_craft` | `light_transport` |
| 98 | Assault Shuttle | `assault_shuttle` | `light_transport` |
| 99 | Marauder Corvette | `marauder_corvette` | `starship` |
| 100 | Star Galleon | `star_galleon` | `heavy_transport` |
| 101 | Imp Research Ship | `imperial_research_ship` | `starship` |
| 102 | Luxury Yacht 3000 | `luxury_yacht` | `light_transport` |
| 103 | Ferryboat Liner | `ferryboat_liner` | `light_transport` |
| 104 | Mod Action Trans | `mod_action_transport` | `heavy_transport` |
| 105 | Mobquet Transport | `mobquet_transport` | `heavy_transport` |
| 106 | Xiytiar Transport | `xiytiar_transport` | `heavy_transport` |
| 107 | Freight Transport/C | `freighter_con_b` | `heavy_transport` |
| 108 | Freight Transport/H | `freighter_con_g` | `heavy_transport` |
| 109 | Freight Transport/K | `freighter_box` | `heavy_transport` |
| 110 | YT-2000 | `family_transport` | `light_transport` |
| 111 | YT-2400 | `outrider` | `light_transport` |
| 112 | Suprosa | `suprosa` | `heavy_transport` |
| 113 | Skipray Blastboat | `skipray_blast_boat` | `fighter` |
| 114 | TIE Experimental M1 | `tie_bizarro` | `fighter` |
| 115 | TIE Experimental M2 | `tie_big_gun` | `fighter` |
| 116 | TIE Experimental M3 | `tie_warheads` | `fighter` |
| 117 | TIE Experimental M4 | `tie_bomb` | `fighter` |
| 118 | TIE Experimental M5 | `tie_booster` | `fighter` |
| 119 | Cloakshape Fighter | `cloakshape_fighter` | `fighter` |
| 120 | Razor Fighter | `razor_fighter` | `fighter` |
| 121 | Planetary Fighter | `planetary_fighter` | `fighter` |
| 122 | Supa Fighter | `supa_fighter` | `fighter` |
| 123 | Pinook Fighter | `piggyback` | `fighter` |
| 124 | *Booster Pack | `booster` | `fighter` |
| 125 | Preybird Fighter | `preybird_fighter` | `fighter` |
| 126 | *StarViper | `xwing_30` | `fighter` |
| 127 | Firespray | `slave_one` | `fighter` |
| 128 | Pursuer | `slave_two` | `fighter` |
| 129 | Golan One | `golan_one` | `station` |
| 130 | Golan Two | `golan_two` | `station` |
| 131 | Golan Three | `golan_three` | `station` |
| 132 | Derilyn Platform | `derilyn_platform` | `station` |
| 133 | Sensor Array | `sensor_array` | `station` |
| 134 | Comm Relay | `comm_relay` | `station` |
| 135 | Space Colony 1 | `space_colony1` | `station` |
| 136 | Space Colony 2 | `space_colony2` | `station` |
| 137 | Space Colony 3 | `space_colony3` | `station` |
| 138 | Casino | `casino` | `station` |
| 139 | Cargo Facility 1 | `cargo_facility1` | `station` |
| 140 | Cargo Facility 2 | `cargo_facility2` | `station` |
| 141 | Ast Mining Unit | `asteroid_mining_unit` | `station` |
| 142 | Processing Plant | `processing_plant` | `station` |
| 143 | Rebel Platform | `rebel_platform` | `station` |
| 144 | Imp Research Center | `imperial_research_center` | `station` |
| 145 | Family Base | `family_base` | `station` |
| 146 | Family Repair Yard | `family_repair_yard` | `station` |
| 147 | Pirate Ship Yard | `pirate_shipyard` | `station` |
| 148 | Industrial Complex | `industrial_complex` | `station` |
| 149 | *Pirate Junkyd Base | `pirate_junkyard_base` | `station` |
| 150 | Escape Pod Type 1 | `escape_pod` | `utility` |
| 151 | Pressure Tank | `propane_tank` | `container` |
| 152 | CN/J Grande | `container_grande` | `container` |
| 153 | CN/K Box | `container_box` | `container` |
| 154 | CN/L Sphere | `container_sphere` | `container` |
| 155 | CN/Hangar | `container_hanger` | `container` |
| 156 | Lrg Gun Emplace | `gun_pad` | `weapon_emplacement` |
| 157 | Lrg Gun/Wrhd Empl | `gun_warhead_pad` | `weapon_emplacement` |
| 158 | Proximity Mine A | `proximity_mine_a` | `weapon_emplacement` |
| 159 | Proximity Mine B | `proximity_mine_b` | `weapon_emplacement` |
| 160 | *Homing Mine A | `homing_mine_a` | `weapon_emplacement` |
| 161 | Homing Mine B | `homing_mine_b` | `weapon_emplacement` |
| 162 | New Laser battery | `laser_bat` | `weapon_emplacement` |
| 163 | New Ion battery | `ion_bat` | `weapon_emplacement` |
| 164 | Cargo Freighter | `crew_cabin_front` | `heavy_transport` |
| 165 | *Cargo Freighter 2 | `connector_rod` | `heavy_transport` |
| 166 | *Cargo Freighter 3 | `engine_back` | `heavy_transport` |
| 167 | *Cargo Freighter 4 | `xwing_108` | `heavy_transport` |
| 168 | *Cargo Freighter 5 | `xwing_109` | `heavy_transport` |
| 169 | Cargo Tanker | `engine_front` | `heavy_transport` |
| 170 | *Cargo Tanker 2 | `crew_cabin_back` | `heavy_transport` |
| 171 | *Cargo Tanker 3 | `xwing_112` | `heavy_transport` |
| 172 | *Cargo Tanker 4 | `xwing_113` | `heavy_transport` |
| 173 | *Cargo Tanker 5 | `xwing_114` | `heavy_transport` |
| 174 | Escape Pod Type 2 | `escape_pod_a` | `utility` |
| 175 | *Rebel Pilot | `rebel_pilot` | `droid` |
| 176 | *Imp Pilot | `imperial_pilot` | `droid` |
| 177 | *Civ Pilot | `civilian_pilot` | `droid` |
| 178 | ZeroG Stormtrooper | `zero_g_stormtrooper` | `droid` |
| 179 | ZeroG Utility | `zero_g_utility` | `droid` |
| 180 | Emkay | `marko` | `droid` |
| 181 | Astromech Droid | `r2d2` | `droid` |
| 182 | Worker Droid | `r2d2` | `droid` |
| 183 | BACKDROP | `backdrop_group_9001_sprite_1100_263` | Planet/asteroid |
| 184 | *DontUseMe | `none` | Planet/asteroid |
| 185 | *DontUseMe | `none` | Planet/asteroid |
| 186 | *DontUseMe | `none` | Planet/asteroid |
| 187 | *DontUseMe | `none` | Planet/asteroid |
| 188 | *DontUseMe | `none` | Planet/asteroid |
| 189 | *DontUseMe | `none` | Planet/asteroid |
| 190 | *DontUseMe | `none` | Planet/asteroid |
| 191 | *DontUseMe | `none` | Planet/asteroid |
| 192 | *DontUseMe | `none` | Planet/asteroid |
| 193 | *DontUseMe | `none` | Planet/asteroid |
| 194 | *DontUseMe | `none` | Planet/asteroid |
| 195 | *DontUseMe | `none` | Planet/asteroid |
| 196 | *DontUseMe | `none` | Planet/asteroid |
| 197 | *DontUseMe | `none` | Planet/asteroid |
| 198 | *DontUseMe | `none` | Planet/asteroid |
| 199 | *DontUseMe | `none` | Planet/asteroid |
| 200 | *DontUseMe | `none` | Planet/asteroid |
| 201 | *DontUseMe | `none` | Planet/asteroid |
| 202 | *DontUseMe | `none` | Planet/asteroid |
| 203 | *DontUseMe | `none` | Planet/asteroid |
| 204 | *DontUseMe | `none` | Planet/asteroid |
| 205 | *DontUseMe | `none` | Planet/asteroid |
| 206 | *DontUseMe | `none` | Planet/asteroid |
| 207 | *DontUseMe | `none` | Planet/asteroid |
| 208 | *DontUseMe | `none` | Planet/asteroid |
| 209 | *DontUseMe | `none` | Planet/asteroid |
| 210 | *DontUseMe | `none` | Planet/asteroid |
| 211 | *DontUseMe | `none` | Planet/asteroid |
| 212 | *DontUseMe | `none` | Planet/asteroid |
| 213 | *DontUseMe | `none` | Planet/asteroid |
| 214 | *DontUseMe | `none` | Planet/asteroid |
| 215 | *DontUseMe | `none` | Planet/asteroid |
| 216 | *DontUseMe | `none` | Planet/asteroid |
| 217 | *DontUseMe | `none` | Planet/asteroid |
| 218 | *DontUseMe | `none` | Planet/asteroid |
| 219 | *DontUseMe | `none` | Planet/asteroid |
| 220 | *DontUseMe | `none` | Planet/asteroid |
| 221 | *DontUseMe | `none` | Planet/asteroid |
| 222 | *DontUseMe | `none` | Planet/asteroid |
| 223 | *DontUseMe | `none` | Planet/asteroid |
| 224 | *DontUseMe | `none` | Planet/asteroid |
| 225 | *DontUseMe | `none` | Planet/asteroid |
| 226 | *DontUseMe | `none` | Planet/asteroid |
| 227 | Death Star II | `none` | `station` |
| 228 | Calamari Cruiser Type 2 | `calamari_winged` | `starship` |
| 229 | Vict Star Dest II | `victory_star_destroyer2_149` | `starship` |
| 230 | Imp Star Dest II | `imperial_star_destroyer2_150` | `starship` |
| 231 | *DontUseMe | `none` | Planet/asteroid |

## Ship list categories

Values for `ship_list.category` in craft type files. **Shown as** is the text that the game shows. The original game also has the category Planet/asteroid, which a file can't set.

| Name | Shown as |
|---|---|
| `fighter` | Fighter |
| `light_transport` | Shuttle/Light Transport |
| `utility` | Utility Craft |
| `container` | Container |
| `heavy_transport` | Freighter/Heavy Transport |
| `starship` | Starship |
| `station` | Station |
| `weapon_emplacement` | Weapon emplacement |
| `mine` | Mine |
| `satellite_buoy` | Satellite/Buoy |
| `droid` | Droid |

