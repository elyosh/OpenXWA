<!-- Made by xwa_mod_tables. Change the tool, not this page. -->

# Object types

An object type is a kind of object in flight, such as a craft, a weapon, a mine or an explosion. Each object type has a number and a name. Mod files use the name. The community tools show the number.

An object type file can give its object type a new name with `name`. The other files of the mod must then use the new name. The slots named `slot_<number>` are free. A mod can use them for new object types.

**Model** is the model that the object type loads in the original game. The mod replaces it with `flight/models/<model>.glb`. **List** is the model list that the model is in. Object type files give it in `list`. Object types without a model are drawn with the images of an image group.

| Number | Name | Model | List | Genus | Family |
|---:|---|---|---|---|---|
| 0 | `none` |  |  |  |  |
| 1 | `xwing` | `xwing` | spacecraft | `fighter` | `craft` |
| 2 | `ywing` | `ywing` | spacecraft | `fighter` | `craft` |
| 3 | `awing` | `awing` | spacecraft | `fighter` | `craft` |
| 4 | `bwing` | `bwing` | spacecraft | `fighter` | `craft` |
| 5 | `tie_fighter` | `tiefighter` | spacecraft | `fighter` | `craft` |
| 6 | `tie_interceptor` | `tieinterceptor` | spacecraft | `fighter` | `craft` |
| 7 | `tie_bomber` | `tiebomber` | spacecraft | `fighter` | `craft` |
| 8 | `tie_advanced` | `tieadvanced` | spacecraft | `fighter` | `craft` |
| 9 | `tie_defender` | `tiedefender` | spacecraft | `fighter` | `craft` |
| 10 | `ird_fighter` | `irdfighter` | spacecraft | `fighter` | `craft` |
| 11 | `toscan_fighter` | `toscanfighter` | spacecraft | `fighter` | `craft` |
| 12 | `missile_boat` | `missileboat` | spacecraft | `fighter` | `craft` |
| 13 | `twing` | `twing` | spacecraft | `fighter` | `craft` |
| 14 | `z95` | `z-95` | spacecraft | `fighter` | `craft` |
| 15 | `r41` | `r41` | spacecraft | `fighter` | `craft` |
| 16 | `assault_gunboat` | `assaultgunboat` | spacecraft | `fighter` | `craft` |
| 17 | `skipray_blast_boat` | `skiprayblastboat` | spacecraft | `fighter` | `craft` |
| 18 | `tie_bizarro` | `tiebizarro` | spacecraft | `fighter` | `craft` |
| 19 | `tie_big_gun` | `tiebiggun` | spacecraft | `fighter` | `craft` |
| 20 | `tie_warheads` | `tiewarheads` | spacecraft | `fighter` | `craft` |
| 21 | `tie_bomb` | `tiebomb` | spacecraft | `fighter` | `craft` |
| 22 | `tie_booster` | `tiebooster` | spacecraft | `fighter` | `craft` |
| 23 | `cloakshape_fighter` | `cloakshapefighter` | spacecraft | `fighter` | `craft` |
| 24 | `razor_fighter` | `razorfighter` | spacecraft | `fighter` | `craft` |
| 25 | `planetary_fighter` | `planetaryfighter` | spacecraft | `fighter` | `craft` |
| 26 | `supa_fighter` | `supafighter` | spacecraft | `fighter` | `craft` |
| 27 | `piggyback` | `piggyback` | spacecraft | `fighter` | `craft` |
| 28 | `booster` | `booster` | spacecraft | `fighter` | `craft` |
| 29 | `preybird_fighter` | `preybirdfighter` | spacecraft | `fighter` | `craft` |
| 30 | `xwing_30` | `xwing` | spacecraft | `fighter` | `craft` |
| 31 | `slave_one` | `slaveone` | spacecraft | `fighter` | `craft` |
| 32 | `slave_two` | `slavetwo` | spacecraft | `fighter` | `craft` |
| 33 | `slot_33` |  |  | `fighter` | `craft` |
| 34 | `slot_34` |  |  | `fighter` | `craft` |
| 35 | `slot_35` |  |  | `fighter` | `craft` |
| 36 | `slot_36` |  |  | `fighter` | `craft` |
| 37 | `slot_37` |  |  | `fighter` | `craft` |
| 38 | `slot_38` |  |  | `fighter` | `craft` |
| 39 | `slot_39` |  |  | `fighter` | `craft` |
| 40 | `slot_40` |  |  | `fighter` | `craft` |
| 41 | `slot_41` |  |  | `fighter` | `craft` |
| 42 | `slot_42` |  |  | `fighter` | `craft` |
| 43 | `tug` | `tug` | spacecraft | `utility` | `craft` |
| 44 | `combat_utility_vehicle` | `combatutilityvehicle` | spacecraft | `utility` | `craft` |
| 45 | `heavy_lifter` | `heavylifter` | spacecraft | `utility` | `craft` |
| 46 | `mole_miner` | `moleminer` | spacecraft | `utility` | `craft` |
| 47 | `escape_pod` | `escapepod` | spacecraft | `utility` | `craft` |
| 48 | `escape_pod_a` | `escapepoda` | spacecraft | `utility` | `craft` |
| 49 | `slot_49` |  |  | `utility` | `craft` |
| 50 | `shuttle` | `shuttle` | spacecraft | `transport` | `craft` |
| 51 | `escort_shuttle` | `escortshuttle` | spacecraft | `transport` | `craft` |
| 52 | `stormtrooper_transport` | `stormtroopertransport` | spacecraft | `transport` | `craft` |
| 53 | `assault_transport` | `assaulttransport` | spacecraft | `transport` | `craft` |
| 54 | `escort_transport` | `escorttransport` | spacecraft | `transport` | `craft` |
| 55 | `system_patrol_craft` | `systempatrolcraft` | spacecraft | `transport` | `craft` |
| 56 | `scout_craft` | `scoutcraft` | spacecraft | `transport` | `craft` |
| 57 | `muurian_transport` | `muuriantransport` | spacecraft | `transport` | `craft` |
| 58 | `corellian_transport2` | `corelliantransport2` | spacecraft | `transport` | `craft` |
| 59 | `millenium_falcon2` | `milleniumfalcon2` | spacecraft | `transport` | `craft` |
| 60 | `outrider` | `outrider` | spacecraft | `transport` | `craft` |
| 61 | `imperial_landing_craft` | `implandingcraft` | spacecraft | `transport` | `craft` |
| 62 | `assault_shuttle` | `assaultshuttle` | spacecraft | `transport` | `craft` |
| 63 | `luxury_yacht` | `luxuryyacht` | spacecraft | `transport` | `craft` |
| 64 | `ferryboat_liner` | `ferryboatliner` | spacecraft | `freighter` | `craft` |
| 65 | `family_transport` | `familytransport` | spacecraft | `transport` | `craft` |
| 66 | `slot_66` |  |  | `transport` | `craft` |
| 67 | `slot_67` |  |  | `transport` | `craft` |
| 68 | `slot_68` |  |  | `transport` | `craft` |
| 69 | `slot_69` |  |  | `transport` | `craft` |
| 70 | `slot_70` |  |  | `transport` | `craft` |
| 71 | `container_brick` | `containerbrick` | spacecraft | `container` | `craft` |
| 72 | `container_hex_box` | `containerhexbox` | spacecraft | `container` | `craft` |
| 73 | `container_tube` | `containertube` | spacecraft | `container` | `craft` |
| 74 | `container_pronged` | `containerpronged` | spacecraft | `container` | `craft` |
| 75 | `container_hemisphere` | `containerhemisphere` | spacecraft | `container` | `craft` |
| 76 | `container_slotted` | `containerslotted` | spacecraft | `container` | `craft` |
| 77 | `container_hourglass` | `containerhourglass` | spacecraft | `container` | `craft` |
| 78 | `container_gem` | `containergem` | spacecraft | `container` | `craft` |
| 79 | `container_yshaped` | `containeryshaped` | spacecraft | `container` | `craft` |
| 80 | `cargo_canister` | `cargocanister` | spacecraft | `container` | `craft` |
| 81 | `propane_tank` | `propanetank` | spacecraft | `container` | `craft` |
| 82 | `container_grande` | `containergrande` | spacecraft | `container` | `craft` |
| 83 | `container_box` | `containerbox` | spacecraft | `container` | `craft` |
| 84 | `container_sphere` | `containersphere` | spacecraft | `container` | `craft` |
| 85 | `container_hanger` | `containerhanger` | spacecraft | `container` | `craft` |
| 86 | `slot_86` |  |  | `container` | `craft` |
| 87 | `slot_87` |  |  | `container` | `craft` |
| 88 | `slot_88` |  |  | `container` | `craft` |
| 89 | `slot_89` |  |  | `container` | `craft` |
| 90 | `slot_90` |  |  | `container` | `craft` |
| 91 | `bulk_freighter` | `bulkfreighter` | spacecraft | `freighter` | `craft` |
| 92 | `cargo_ferry` | `cargoferry` | spacecraft | `freighter` | `craft` |
| 93 | `modular_conveyor` | `modularconveyor` | spacecraft | `freighter` | `craft` |
| 94 | `container_transport` | `containertransport` | spacecraft | `freighter` | `craft` |
| 95 | `tuna_boat2` | `tunaboat2` | spacecraft | `freighter` | `craft` |
| 96 | `star_galleon` | `stargalleon` | spacecraft | `freighter` | `craft` |
| 97 | `mod_action_transport` | `modactiontransport` | spacecraft | `freighter` | `craft` |
| 98 | `mobquet_transport` | `mobquettransport` | spacecraft | `freighter` | `craft` |
| 99 | `xiytiar_transport` | `xiytiartransport` | spacecraft | `freighter` | `craft` |
| 100 | `freighter_con_b` | `freighterconb` | spacecraft | `freighter` | `craft` |
| 101 | `freighter_con_g` | `freightercong` | spacecraft | `freighter` | `craft` |
| 102 | `freighter_box` | `freighterbox` | spacecraft | `freighter` | `craft` |
| 103 | `xwing_103` | `xwing` | spacecraft | `freighter` | `craft` |
| 104 | `xwing_104` | `xwing` | spacecraft | `freighter` | `craft` |
| 105 | `crew_cabin_front` | `crewcabinfront` | spacecraft | `freighter` | `craft` |
| 106 | `connector_rod` | `connectorrod` | spacecraft | `freighter` | `craft` |
| 107 | `engine_back` | `engineback` | spacecraft | `freighter` | `craft` |
| 108 | `xwing_108` | `xwing` | spacecraft | `freighter` | `craft` |
| 109 | `xwing_109` | `xwing` | spacecraft | `freighter` | `craft` |
| 110 | `engine_front` | `enginefront` | spacecraft | `freighter` | `craft` |
| 111 | `crew_cabin_back` | `crewcabinback` | spacecraft | `freighter` | `craft` |
| 112 | `xwing_112` | `xwing` | spacecraft | `freighter` | `craft` |
| 113 | `xwing_113` | `xwing` | spacecraft | `freighter` | `craft` |
| 114 | `xwing_114` | `xwing` | spacecraft | `freighter` | `craft` |
| 115 | `suprosa` | `suprosa` | spacecraft | `freighter` | `craft` |
| 116 | `slot_116` |  |  | `freighter` | `craft` |
| 117 | `slot_117` |  |  | `freighter` | `craft` |
| 118 | `slot_118` |  |  | `freighter` | `craft` |
| 119 | `slot_119` |  |  | `freighter` | `craft` |
| 120 | `slot_120` |  |  | `freighter` | `craft` |
| 121 | `slot_121` |  |  | `freighter` | `craft` |
| 122 | `slot_122` |  |  | `freighter` | `craft` |
| 123 | `slot_123` |  |  | `freighter` | `craft` |
| 124 | `slot_124` |  |  | `freighter` | `craft` |
| 125 | `slot_125` |  |  | `freighter` | `craft` |
| 126 | `corvette2` | `corvette2` | spacecraft | `freighter` | `craft` |
| 127 | `mod_corvette` | `modcorvette` | spacecraft | `freighter` | `craft` |
| 128 | `frigate2` | `frigate2` | spacecraft | `starship` | `craft` |
| 129 | `mod_frigate` | `modfrigate` | spacecraft | `starship` | `craft` |
| 130 | `passenger_liner` | `passengerliner` | spacecraft | `starship` | `craft` |
| 131 | `carrack_cruiser` | `carrackcruiser` | spacecraft | `starship` | `craft` |
| 132 | `strike_cruiser` | `strikecruiser` | spacecraft | `starship` | `craft` |
| 133 | `escort_carrier` | `escortcarrier` | spacecraft | `starship` | `craft` |
| 134 | `dreadnaught2` | `dreadnaught2` | spacecraft | `starship` | `craft` |
| 135 | `calamari_cruiser_new` | `calamaricruisernew` | spacecraft | `starship` | `craft` |
| 136 | `light_calamari_cruiser` | `lightcalamaricruiser` | spacecraft | `starship` | `craft` |
| 137 | `interdictor2` | `interdictor2` | spacecraft | `starship` | `craft` |
| 138 | `victory_star_destroyer2` | `victorystardestroyer2` | spacecraft | `starship` | `craft` |
| 139 | `imperial_star_destroyer2` | `imperialstardestroyer2` | spacecraft | `starship` | `craft` |
| 140 | `super_star_destroyer` | `superstardestroyer` | spacecraft | `starship` | `craft` |
| 141 | `mod_strike_cruiser` | `modstrikecruiser` | spacecraft | `starship` | `craft` |
| 142 | `lancer_frigate` | `lancerfrigate` | spacecraft | `starship` | `craft` |
| 143 | `bulk_cruiser` | `bulkcruiser` | spacecraft | `starship` | `craft` |
| 144 | `assault_frigate` | `assaultfrigate` | spacecraft | `starship` | `craft` |
| 145 | `corellian_gunship` | `corelliangunship` | spacecraft | `freighter` | `craft` |
| 146 | `marauder_corvette` | `maraudercorvette` | spacecraft | `starship` | `craft` |
| 147 | `imperial_research_ship` | `impresearchship` | spacecraft | `starship` | `craft` |
| 148 | `calamari_winged` | `calamariwinged` | spacecraft | `starship` | `craft` |
| 149 | `victory_star_destroyer2_149` | `victorystardestroyer2` | spacecraft | `starship` | `craft` |
| 150 | `imperial_star_destroyer2_150` | `imperialstardestroyer2` | spacecraft | `starship` | `craft` |
| 151 | `slot_151` |  |  | `starship` | `craft` |
| 152 | `slot_152` |  |  | `platform` | `craft` |
| 153 | `platform1` | `platform1` | spacecraft | `platform` | `craft` |
| 154 | `platform2` | `platform2` | spacecraft | `platform` | `craft` |
| 155 | `platform3` | `platform3` | spacecraft | `platform` | `craft` |
| 156 | `platform4` | `platform4` | spacecraft | `platform` | `craft` |
| 157 | `platform5` | `platform5` | spacecraft | `platform` | `craft` |
| 158 | `platform6` | `platform6` | spacecraft | `platform` | `craft` |
| 159 | `asteroid_base` | `asteroidbase` | spacecraft | `platform` | `craft` |
| 160 | `factory` | `factory` | spacecraft | `platform` | `craft` |
| 161 | `ship_yard` | `shipyard` | spacecraft | `platform` | `craft` |
| 162 | `repair_yard` | `repairyard` | spacecraft | `platform` | `craft` |
| 163 | `golan_one` | `golanone` | spacecraft | `platform` | `craft` |
| 164 | `golan_two` | `golantwo` | spacecraft | `platform` | `craft` |
| 165 | `golan_three` | `golanthree` | spacecraft | `platform` | `craft` |
| 166 | `derilyn_platform` | `derilynplatform` | spacecraft | `platform` | `craft` |
| 167 | `sensor_array` | `sensorarray` | spacecraft | `platform` | `craft` |
| 168 | `comm_relay` | `commrelay` | spacecraft | `platform` | `craft` |
| 169 | `space_colony1` | `spacecolony1` | spacecraft | `platform` | `craft` |
| 170 | `space_colony2` | `spacecolony2` | spacecraft | `platform` | `craft` |
| 171 | `space_colony3` | `spacecolony3` | spacecraft | `platform` | `craft` |
| 172 | `casino` | `casino` | spacecraft | `platform` | `craft` |
| 173 | `cargo_facility1` | `cargofacility1` | spacecraft | `platform` | `craft` |
| 174 | `cargo_facility2` | `cargofacility2` | spacecraft | `platform` | `craft` |
| 175 | `asteroid_mining_unit` | `asteroidminingunit` | spacecraft | `platform` | `craft` |
| 176 | `processing_plant` | `processingplant` | spacecraft | `platform` | `craft` |
| 177 | `rebel_platform` | `rebelplatform` | spacecraft | `platform` | `craft` |
| 178 | `imperial_research_center` | `impresearchcenter` | spacecraft | `platform` | `craft` |
| 179 | `family_base` | `familybase` | spacecraft | `platform` | `craft` |
| 180 | `family_repair_yard` | `familyrepairyard` | spacecraft | `platform` | `craft` |
| 181 | `pirate_shipyard` | `pirateshipyard` | spacecraft | `platform` | `craft` |
| 182 | `industrial_complex` | `industrialcomplex` | spacecraft | `platform` | `craft` |
| 183 | `pirate_junkyard_base` | `piratejunkyardbase` | spacecraft | `platform` | `craft` |
| 184 | `slot_184` |  |  | `platform` | `craft` |
| 185 | `slot_185` |  |  | `platform` | `craft` |
| 186 | `slot_186` |  |  | `platform` | `craft` |
| 187 | `slot_187` |  |  | `platform` | `craft` |
| 188 | `slot_188` |  |  | `platform` | `craft` |
| 189 | `slot_189` |  |  | `platform` | `craft` |
| 190 | `slot_190` |  |  | `platform` | `craft` |
| 191 | `slot_191` |  |  | `platform` | `craft` |
| 192 | `laser_bat` | `laserbat` | equipment | `weapon_emplacement` | `craft` |
| 193 | `ion_bat` | `ionbat` | equipment | `weapon_emplacement` | `craft` |
| 194 | `asteroid_laser_battery` | `asteroidlaserbattery` | spacecraft | `weapon_emplacement` | `craft` |
| 195 | `asteroid_warhead_launcher` | `asteroidwarheadlauncher` | spacecraft | `weapon_emplacement` | `craft` |
| 196 | `gun_platform` | `gunplatform` | equipment | `weapon_emplacement` | `craft` |
| 197 | `gun_pad` | `gunpad` | equipment | `weapon_emplacement` | `craft` |
| 198 | `gun_warhead_pad` | `gunwarheadpad` | equipment | `weapon_emplacement` | `craft` |
| 199 | `homing_mine_a` | `homingminea` | equipment | `weapon_emplacement` | `craft` |
| 200 | `homing_mine_b` | `homingmineb` | equipment | `weapon_emplacement` | `craft` |
| 201 | `proximity_mine_a` | `proximityminea` | equipment | `weapon_emplacement` | `craft` |
| 202 | `proximity_mine_b` | `proximitymineb` | equipment | `weapon_emplacement` | `craft` |
| 203 | `rebel_pilot` | `rebelpilot` | equipment | `pilot_droid` | `craft` |
| 204 | `imperial_pilot` | `imperialpilot` | equipment | `pilot_droid` | `craft` |
| 205 | `civilian_pilot` | `civilianpilot` | equipment | `pilot_droid` | `craft` |
| 206 | `zero_g_stormtrooper` | `zerogstormtrooper` | equipment | `pilot_droid` | `craft` |
| 207 | `zero_g_utility` | `zerogutility` | equipment | `pilot_droid` | `craft` |
| 208 | `marko` | `marko` | equipment | `pilot_droid` | `craft` |
| 209 | `r2d2` | `r2d2` | equipment | `pilot_droid` | `craft` |
| 210 | `comm_sat1` | `satb` | equipment | `satellite_buoy` | `craft` |
| 211 | `comm_sat2` | `satc` | equipment | `satellite_buoy` | `craft` |
| 212 | `comm_sat3` | `satd` | equipment | `satellite_buoy` | `object` |
| 213 | `probe` | `probe` | equipment | `satellite_buoy` | `object` |
| 214 | `probe_capsule` | `probecapsule` | equipment | `satellite_buoy` | `craft` |
| 215 | `nav_buoy1` | `buoyc` | equipment | `satellite_buoy` | `craft` |
| 216 | `nav_buoy2` | `buoyb` | equipment | `satellite_buoy` | `craft` |
| 217 | `nav_buoy3` | `buoyrendez` | equipment | `satellite_buoy` | `craft` |
| 218 | `hyper_buoy` | `buoyfaux` | equipment | `satellite_buoy` | `craft` |
| 219 | `mine1` | `minea` | equipment | `mine` | `weapon` |
| 220 | `mine2` | `mineb` | equipment | `mine` | `weapon` |
| 221 | `mine3` | `minec` | equipment | `mine` | `weapon` |
| 222 | `slot_222` |  |  |  |  |
| 223 | `asteroid_hr1` | `asteroidhr1` | equipment | `asteroid` | `debris` |
| 224 | `asteroid_hr2` | `asteroidhr2` | equipment | `asteroid` | `debris` |
| 225 | `asteroid_hr3` | `asteroidhr3` | equipment | `asteroid` | `debris` |
| 226 | `asteroid_hr4` | `asteroidhr4` | equipment | `asteroid` | `debris` |
| 227 | `asteroid_hr5` | `asteroidhr5` | equipment | `asteroid` | `debris` |
| 228 | `asteroid_hr6` | `asteroidhr6` | equipment | `asteroid` | `debris` |
| 229 | `slot_229` |  |  |  |  |
| 230 | `slot_230` |  |  |  |  |
| 231 | `slot_231` |  |  |  |  |
| 232 | `slot_232` |  |  |  |  |
| 233 | `debris_group_4000` | image group 4000 |  | `debris` | `debris` |
| 234 | `debris_group_4001` | image group 4001 |  | `debris` | `debris` |
| 235 | `debris_group_4002` | image group 4002 |  | `debris` | `debris` |
| 236 | `debris_group_4003` | image group 4003 |  | `debris` | `debris` |
| 237 | `lighting_effect_group_1000` | image group 1000 |  | `sprite` | `sprite` |
| 238 | `animation_group_3051` | image group 3051 |  | `sprite` | `sprite` |
| 239 | `hud_group_12000` | image group 12000 |  | `sprite` | `sprite` |
| 240 | `group_6000_sprite_100` | image group 6000 |  | `sprite` | `sprite` |
| 241 | `group_6000_sprite_200` | image group 6000 |  | `sprite` | `sprite` |
| 242 | `group_6000_sprite_300` | image group 6000 |  | `sprite` | `sprite` |
| 243 | `group_6000_sprite_400` | image group 6000 |  | `sprite` | `sprite` |
| 244 | `group_6000_sprite_500` | image group 6000 |  | `sprite` | `sprite` |
| 245 | `group_6000_sprite_600` | image group 6000 |  | `sprite` | `sprite` |
| 246 | `group_6000_sprite_700` | image group 6000 |  | `sprite` | `sprite` |
| 247 | `group_7000_sprite_100` | image group 7000 |  | `sprite` | `sprite` |
| 248 | `group_7000_sprite_200` | image group 7000 |  | `sprite` | `sprite` |
| 249 | `group_7000_sprite_300` | image group 7000 |  | `sprite` | `sprite` |
| 250 | `group_7000_sprite_400` | image group 7000 |  | `sprite` | `sprite` |
| 251 | `group_8000_sprite_100` | image group 8000 |  | `sprite` | `sprite` |
| 252 | `group_8000_sprite_200` | image group 8000 |  | `sprite` | `sprite` |
| 253 | `group_8000_sprite_300` | image group 8000 |  | `sprite` | `sprite` |
| 254 | `group_8000_sprite_400` | image group 8000 |  | `sprite` | `sprite` |
| 255 | `group_8000_sprite_500` | image group 8000 |  | `sprite` | `sprite` |
| 256 | `group_8000_sprite_600` | image group 8000 |  | `sprite` | `sprite` |
| 257 | `wrap_back_group_19000_sprite_100` | image group 19000 |  | `sprite` | `sprite` |
| 258 | `wrap_back_group_18000_sprite_100` | image group 18000 |  | `sprite` | `sprite` |
| 259 | `backdrop_group_9001_sprite_1100` | image group 9001 |  | `sprite` | `sprite` |
| 260 | `backdrop_group_9001_sprite_1100_260` | image group 9001 |  | `sprite` | `sprite` |
| 261 | `backdrop_group_9001_sprite_1100_261` | image group 9001 |  | `sprite` | `sprite` |
| 262 | `backdrop_group_9001_sprite_1100_262` | image group 9001 |  | `sprite` | `sprite` |
| 263 | `backdrop_group_9001_sprite_1100_263` | image group 9001 |  | `sprite` | `sprite` |
| 264 | `explosion_group_2000` | image group 2000 |  | `explosion` | `explosion` |
| 265 | `explosion_group_2001` | image group 2001 |  | `explosion` | `explosion` |
| 266 | `explosion_group_2002` | image group 2002 |  | `explosion` | `explosion` |
| 267 | `explosion_group_2003` | image group 2003 |  | `explosion` | `explosion` |
| 268 | `explosion_group_2004` | image group 2004 |  | `explosion` | `explosion` |
| 269 | `explosion_group_2005` | image group 2005 |  | `explosion` | `explosion` |
| 270 | `explosion_group_2006` | image group 2006 |  | `explosion` | `explosion` |
| 271 | `spark_group_3000` | image group 3000 |  | `explosion` | `explosion` |
| 272 | `spark_group_3001` | image group 3001 |  | `explosion` | `explosion` |
| 273 | `spark_group_3002` | image group 3002 |  | `explosion` | `explosion` |
| 274 | `spark_group_3003` | image group 3003 |  | `explosion` | `explosion` |
| 275 | `spark_group_3004` | image group 3004 |  | `explosion` | `explosion` |
| 276 | `animation_group_3005` | image group 3005 |  | `explosion` | `explosion` |
| 277 | `animation_group_3006_sprite_0` | image group 3006 |  | `explosion` | `explosion` |
| 278 | `animation_group_2007` | image group 2007 |  | `explosion` | `explosion` |
| 279 | `animation_group_2008` | image group 2008 |  | `explosion` | `explosion` |
| 280 | `rebel_laser` | `laserrebel` | equipment | `projectile` | `weapon` |
| 281 | `rebel_laser_turbo` | `laserrebelturbo` | equipment | `projectile` | `weapon` |
| 282 | `imperial_laser` | `laserimp` | equipment | `projectile` | `weapon` |
| 283 | `imperial_laser_turbo` | `laserimpturbo` | equipment | `projectile` | `weapon` |
| 284 | `ion_cannon` | `laserion` | equipment | `projectile` | `weapon` |
| 285 | `ion_cannon_turbo` | `laserionturbo` | equipment | `projectile` | `weapon` |
| 286 | `torpedo` | `protontorpedo` | equipment | `projectile` | `weapon` |
| 287 | `missile` | `concussionmissile` | equipment | `projectile` | `weapon` |
| 288 | `turbolaser_red` | `laserrebelturbo` | equipment | `projectile` | `weapon` |
| 289 | `turbolaser_green` | `laserimpturbo` | equipment | `projectile` | `weapon` |
| 290 | `turbolaser_ion` | `laserionturbo` | equipment | `projectile` | `weapon` |
| 291 | `advanced_torpedo` | `protontorpedo` | equipment | `projectile` | `weapon` |
| 292 | `advanced_missile` | `concussionmissile` | equipment | `projectile` | `weapon` |
| 293 | `space_bomb` | `spacebomb` | equipment | `projectile` | `weapon` |
| 294 | `heavy_rocket` | `heavyrocket` | equipment | `projectile` | `weapon` |
| 295 | `mag_pulse` | `magpulse` | equipment | `projectile` | `weapon` |
| 296 | `ion_pulse` | `magpulse` | equipment | `projectile` | `weapon` |
| 297 | `turbolaser_red_light` | `laserrebelturbo` | equipment | `projectile` | `weapon` |
| 298 | `flare` | `flare` | equipment | `projectile` | `weapon` |
| 299 | `spark_projectile` | image group 3002 |  | `projectile` | `weapon` |
| 300 | `chaff` | image group 5000 |  | `explosion` | `explosion` |
| 301 | `turbolaser_red_medium` | `laserrebelturbo` | equipment | `projectile` | `weapon` |
| 302 | `turbolaser_red_heavy` | `laserrebelturbo` | equipment | `projectile` | `weapon` |
| 303 | `turbolaser_green_light` | `laserimpturbo` | equipment | `projectile` | `weapon` |
| 304 | `turbolaser_green_medium` | `laserimpturbo` | equipment | `projectile` | `weapon` |
| 305 | `turbolaser_green_heavy` | `laserimpturbo` | equipment | `projectile` | `weapon` |
| 306 | `death_star_laser` | `laserimpds` | equipment | `projectile` | `weapon` |
| 307 | `custom_weapon` |  |  | `projectile` | `weapon` |
| 308 | `hangar` | `hangar` | equipment | `utility` | `scenery` |
| 309 | `hangar_crane` | `hangarcrane` | equipment | `utility` | `scenery` |
| 310 | `hangar_crate` | `hangarcrate` | equipment | `utility` | `scenery` |
| 311 | `hangar_droid` | `hangardroid` | equipment | `utility` | `scenery` |
| 312 | `hangar_droid2` | `hangardroid2` | equipment | `utility` | `scenery` |
| 313 | `hangar_generator` | `hangargenerator` | equipment | `utility` | `scenery` |
| 314 | `hangar_monitor` | `hangarmonitor` | equipment | `utility` | `scenery` |
| 315 | `hangar_work_stand` | `hangarworkstand` | equipment | `utility` | `scenery` |
| 316 | `hangar_roof_crane` | `hangarroofcrane` | equipment | `utility` | `scenery` |
| 317 | `corellian_transport_gunner` | `corelliantransportgunner` | equipment | `utility` | `scenery` |
| 318 | `slot_318` |  |  |  |  |
| 319 | `slot_319` |  |  |  |  |
| 320 | `death_star_ii_group_17001` | image group 17001 |  | `sprite` | `sprite` |
| 321 | `death_star_ii_group_17002` | image group 17002 |  | `sprite` | `sprite` |
| 322 | `ds_container` | `dscontainer` | equipment | `fighter` | `craft` |
| 323 | `ds_reactor_core` | `dsreactorcore` | equipment | `platform` | `craft` |
| 324 | `ds_reactor_cylinder` | `dsreactorcylinder` | equipment | `platform` | `scenery` |
| 325 | `ds_tripod_gun` | `dstripodgun` | equipment | `pilot_droid` | `craft` |
| 326 | `ds_focus_lens` | `dsfocuslens` | equipment | `rubble` | `scenery` |
| 327 | `ds_tankwlights` | `dstankwlights` | equipment | `death_star_tunnel` | `scenery` |
| 328 | `ds_tankwlights2` | `dstankwlights2` | equipment | `death_star_tunnel` | `scenery` |
| 329 | `ds_bent_tube_gray` | `dsbenttubegray` | equipment | `death_star_tunnel` | `scenery` |
| 330 | `ds_tankw_pipe_red01` | `dstankwpipered01` | equipment | `death_star_tunnel` | `scenery` |
| 331 | `ds_bent_tube_red` | `dsbenttubered` | equipment | `death_star_tunnel` | `scenery` |
| 332 | `ds_bent_tube_red_lites01` | `dsbenttuberedlites01` | equipment | `death_star_tunnel` | `scenery` |
| 333 | `ds_bent_tube_red_lites02` | `dsbenttuberedlites02` | equipment | `death_star_tunnel` | `scenery` |
| 334 | `ds_bent_tube_red_lites03` | `dsbenttuberedlites03` | equipment | `death_star_tunnel` | `scenery` |
| 335 | `ds_bent_tube_red_lites04` | `dsbenttuberedlites04` | equipment | `death_star_tunnel` | `scenery` |
| 336 | `ds_bent_tube_red_lites05` | `dsbenttuberedlites05` | equipment | `death_star_tunnel` | `scenery` |
| 337 | `ds_tankw_pipe_blue01` | `dstankwpipeblue01` | equipment | `death_star_tunnel` | `scenery` |
| 338 | `ds_bent_tube_blue01` | `dsbenttubeblue01` | equipment | `death_star_tunnel` | `scenery` |
| 339 | `ds_bent_tube_blue_lites01` | `dsbenttubebluelites01` | equipment | `death_star_tunnel` | `scenery` |
| 340 | `ds_bent_tube_blue_lites02` | `dsbenttubebluelites02` | equipment | `death_star_tunnel` | `scenery` |
| 341 | `ds_bent_tube_blue_lites03` | `dsbenttubebluelites03` | equipment | `death_star_tunnel` | `scenery` |
| 342 | `ds_bent_tube_blue_lites04` | `dsbenttubebluelites04` | equipment | `death_star_tunnel` | `scenery` |
| 343 | `ds_two_tank` | `dstwotank` | equipment | `death_star_tunnel` | `scenery` |
| 344 | `ds_two_tank2` | `dstwotank2` | equipment | `death_star_tunnel` | `scenery` |
| 345 | `ds_girder` | `dsgirder` | equipment | `death_star_tunnel` | `scenery` |
| 346 | `ds_girder02` | `dsgirder02` | equipment | `death_star_tunnel` | `scenery` |
| 347 | `ds_girder03` | `dsgirder03` | equipment | `death_star_tunnel` | `scenery` |
| 348 | `ds_reactor_core_room` | `dsreactorcoreroom` | equipment | `platform` | `craft` |
| 349 | `ds_accel_chamber` | `dsaccelchamber` | equipment | `death_star_tunnel` | `scenery` |
| 350 | `ds_focus_chamber` | `dsfocuschamber` | equipment | `death_star_tunnel` | `scenery` |
| 351 | `ds_3rd_room` | `ds3rdroom` | equipment | `platform` | `craft` |
| 352 | `ds_entrance` | `dsentrance` | equipment | `death_star_tunnel` | `scenery` |
| 353 | `ds_reactor_room_tube` | `dsreactorroomtube` | equipment | `death_star_tunnel` | `scenery` |
| 354 | `ds_accel_tube` | `dsacceltube` | equipment | `death_star_tunnel` | `scenery` |
| 355 | `ds_ynot_tube` | `dsynottube` | equipment | `death_star_tunnel` | `scenery` |
| 356 | `ds_angle_tube` | `dsangletube` | equipment | `death_star_tunnel` | `scenery` |
| 357 | `ds_dead_end` | `dsdeadend` | equipment | `death_star_tunnel` | `scenery` |
| 358 | `ds_tube_entry` | `dstubeentry` | equipment | `death_star_tunnel` | `scenery` |
| 359 | `ds_tri_room_entrance` | `dstriroomentrance` | equipment | `death_star_tunnel` | `scenery` |
| 360 | `ds_tri_room` | `dstriroom` | equipment | `death_star_tunnel` | `scenery` |
| 361 | `ds_tri_room_exit` | `dstriroomexit` | equipment | `death_star_tunnel` | `scenery` |
| 362 | `ds_big_tube_entrance` | `dsbigtubeentrance` | equipment | `death_star_tunnel` | `scenery` |
| 363 | `ds_big_tube` | `dsbigtube` | equipment | `death_star_tunnel` | `scenery` |
| 364 | `ds_big_tube_exit` | `dsbigtubeexit` | equipment | `death_star_tunnel` | `scenery` |
| 365 | `ds_square_tube01` | `dssquaretube01` | equipment | `death_star_tunnel` | `scenery` |
| 366 | `ds_basement` | `dsbasement` | equipment | `death_star_tunnel` | `scenery` |
| 367 | `ds_basement01` | `dsbasement01` | equipment | `death_star_tunnel` | `scenery` |
| 368 | `ds_basement02` | `dsbasement02` | equipment | `death_star_tunnel` | `scenery` |
| 369 | `ds_alpha_walls` | `dsalphawalls` | equipment | `death_star_tunnel` | `scenery` |
| 370 | `ds_alpha_walls01` | `dsalphawalls01` | equipment | `death_star_tunnel` | `scenery` |
| 371 | `ds_alpha_walls02` | `dsalphawalls02` | equipment | `death_star_tunnel` | `scenery` |
| 372 | `ds_3_layer_tube` | `ds3layertube` | equipment | `death_star_tunnel` | `scenery` |
| 373 | `ds_box_in_box1` | `dsboxinbox1` | equipment | `death_star_tunnel` | `scenery` |
| 374 | `ds_box_in_box2` | `dsboxinbox2` | equipment | `death_star_tunnel` | `scenery` |
| 375 | `ds_box_in_box3` | `dsboxinbox3` | equipment | `death_star_tunnel` | `scenery` |
| 376 | `chute_mouth` | `chutemouth` | equipment | `salvage_junk` | `scenery` |
| 377 | `chute_tunnel` | `chutetunnel` | equipment | `salvage_junk` | `scenery` |
| 378 | `salvage_room` | `salvageroom` | equipment | `salvage_junk` | `scenery` |
| 379 | `compactor` | `compactor` | equipment | `large_scenery` | `scenery` |
| 380 | `accel_ring` | `accelring` | equipment | `salvage_junk` | `scenery` |
| 381 | `accel_ring2` | `accelring2` | equipment | `large_scenery` | `scenery` |
| 382 | `smelting_room` | `smeltingroom` | equipment | `large_scenery` | `scenery` |
| 383 | `sr_tube_nobend` | `sr_tubenobend` | equipment | `salvage_junk` | `scenery` |
| 384 | `sr_tube_up` | `sr_tubeup` | equipment | `salvage_junk` | `scenery` |
| 385 | `sr_tube_down` | `sr_tubedown` | equipment | `salvage_junk` | `scenery` |
| 386 | `sr_tube_lh` | `sr_tubelh` | equipment | `salvage_junk` | `scenery` |
| 387 | `sr_tube_rh` | `sr_tuberh` | equipment | `salvage_junk` | `scenery` |
| 388 | `centrifuge` | `centrifuge` | equipment | `large_scenery` | `scenery` |
| 389 | `accel_ring3` | `accelring3` | equipment | `large_scenery` | `scenery` |
| 390 | `container_grande_pg` | `containergrandepg` | equipment | `salvage_junk` | `scenery` |
| 391 | `asteroid01` | `asteroid01` | equipment | `salvage_junk` | `scenery` |
| 392 | `asteroid02` | `asteroid02` | equipment | `salvage_junk` | `scenery` |
| 393 | `asteroid03` | `asteroid03` | equipment | `large_scenery` | `scenery` |
| 394 | `junk01` | `junk01` | equipment | `salvage_junk` | `scenery` |
| 395 | `junk02` | `junk02` | equipment | `salvage_junk` | `scenery` |
| 396 | `junk03` | `junk03` | equipment | `salvage_junk` | `scenery` |
| 397 | `junk04` | `junk04` | equipment | `salvage_junk` | `scenery` |
| 398 | `junk05` | `junk05` | equipment | `salvage_junk` | `scenery` |
| 399 | `junk06` | `junk06` | equipment | `salvage_junk` | `scenery` |
| 400 | `junk07` | `junk07` | equipment | `salvage_junk` | `scenery` |
| 401 | `junk08` | `junk08` | equipment | `salvage_junk` | `scenery` |
| 402 | `junk09` | `junk09` | equipment | `salvage_junk` | `scenery` |
| 403 | `junk10` | `junk10` | equipment | `salvage_junk` | `scenery` |
| 404 | `junk_block` | `junkblock` | equipment | `salvage_junk` | `scenery` |
| 405 | `molten_block` | `moltenblock` | equipment | `salvage_junk` | `scenery` |
| 406 | `rubble01` | `rubble01` | equipment | `rubble` | `debris` |
| 407 | `rubble02` | `rubble02` | equipment | `rubble` | `debris` |
| 408 | `rubble03` | `rubble03` | equipment | `rubble` | `debris` |
| 409 | `rubble04` | `rubble04` | equipment | `rubble` | `debris` |
| 410 | `rubble05` | `rubble05` | equipment | `rubble` | `debris` |
| 411 | `rubble06` | `rubble06` | equipment | `rubble` | `debris` |
| 412 | `rubble07` | `rubble07` | equipment | `rubble` | `debris` |
| 413 | `rubble08` | `rubble08` | equipment | `rubble` | `debris` |
| 414 | `rubble09` | `rubble09` | equipment | `rubble` | `debris` |
| 415 | `rubble10` | `rubble10` | equipment | `rubble` | `debris` |
| 416 | `rubble11` | `rubble11` | equipment | `rubble` | `debris` |
| 417 | `rubble12` | `rubble12` | equipment | `rubble` | `debris` |
| 418 | `font_group_16000` | image group 16000 |  | `sprite` | `sprite` |
| 419 | `blast_mark_group_3050` | image group 3050 |  | `explosion` | `explosion` |
| 420 | `hud_group_13000_sprite_0` | image group 13000 |  | `sprite` | `sprite` |
| 421 | `wrap_back_group_18000` | image group 18000 |  | `sprite` | `sprite` |
| 422 | `wrap_back_group_19000` | image group 19000 |  | `sprite` | `sprite` |
| 423 | `wrap_back_group_19100` | image group 19100 |  | `sprite` | `sprite` |
| 424 | `wrap_back_group_19200` | image group 19200 |  | `sprite` | `sprite` |
| 425 | `wrap_back_group_19300` | image group 19300 |  | `sprite` | `sprite` |
| 426 | `wrap_back_group_19400` | image group 19400 |  | `sprite` | `sprite` |
| 427 | `planet_group_6010` | image group 6010 |  | `sprite` | `sprite` |
| 428 | `planet_group_6011` | image group 6011 |  | `sprite` | `sprite` |
| 429 | `planet_group_6012` | image group 6012 |  | `sprite` | `sprite` |
| 430 | `planet_group_6013` | image group 6013 |  | `sprite` | `sprite` |
| 431 | `planet_group_6014` | image group 6014 |  | `sprite` | `sprite` |
| 432 | `planet_group_6020` | image group 6020 |  | `sprite` | `sprite` |
| 433 | `planet_group_6021` | image group 6021 |  | `sprite` | `sprite` |
| 434 | `planet_group_6022` | image group 6022 |  | `sprite` | `sprite` |
| 435 | `planet_group_6023` | image group 6023 |  | `sprite` | `sprite` |
| 436 | `planet_group_6024` | image group 6024 |  | `sprite` | `sprite` |
| 437 | `planet_group_6025` | image group 6025 |  | `sprite` | `sprite` |
| 438 | `planet_group_6026` | image group 6026 |  | `sprite` | `sprite` |
| 439 | `planet_group_6027` | image group 6027 |  | `sprite` | `sprite` |
| 440 | `planet_group_6028` | image group 6028 |  | `sprite` | `sprite` |
| 441 | `planet_group_6029` | image group 6029 |  | `sprite` | `sprite` |
| 442 | `planet_group_6030` | image group 6030 |  | `sprite` | `sprite` |
| 443 | `planet_group_6031` | image group 6031 |  | `sprite` | `sprite` |
| 444 | `planet_group_6032` | image group 6032 |  | `sprite` | `sprite` |
| 445 | `planet_group_6033` | image group 6033 |  | `sprite` | `sprite` |
| 446 | `planet_group_6034` | image group 6034 |  | `sprite` | `sprite` |
| 447 | `planet_group_6040` | image group 6040 |  | `sprite` | `sprite` |
| 448 | `planet_group_6041` | image group 6041 |  | `sprite` | `sprite` |
| 449 | `planet_group_6042_sprite_0` | image group 6042 |  | `sprite` | `sprite` |
| 450 | `planet_group_6043` | image group 6043 |  | `sprite` | `sprite` |
| 451 | `group_6050_sprite_0` | image group 6050 |  | `sprite` | `sprite` |
| 452 | `planet_group_6060` | image group 6060 |  | `sprite` | `sprite` |
| 453 | `planet_group_6061` | image group 6061 |  | `sprite` | `sprite` |
| 454 | `planet_group_6062` | image group 6062 |  | `sprite` | `sprite` |
| 455 | `planet_group_6063` | image group 6063 |  | `sprite` | `sprite` |
| 456 | `planet_group_6064` | image group 6064 |  | `sprite` | `sprite` |
| 457 | `planet_group_6070` | image group 6070 |  | `sprite` | `sprite` |
| 458 | `planet_group_6071` | image group 6071 |  | `sprite` | `sprite` |
| 459 | `planet_group_6072` | image group 6072 |  | `sprite` | `sprite` |
| 460 | `planet_group_6073` | image group 6073 |  | `sprite` | `sprite` |
| 461 | `planet_group_6074` | image group 6074 |  | `sprite` | `sprite` |
| 462 | `planet_group_6075` | image group 6075 |  | `sprite` | `sprite` |
| 463 | `planet_group_6076` | image group 6076 |  | `sprite` | `sprite` |
| 464 | `planet_group_6077` | image group 6077 |  | `sprite` | `sprite` |
| 465 | `planet_group_6078` | image group 6078 |  | `sprite` | `sprite` |
| 466 | `planet_group_6079` | image group 6079 |  | `sprite` | `sprite` |
| 467 | `planet_group_6080` | image group 6080 |  | `sprite` | `sprite` |
| 468 | `planet_group_6081` | image group 6081 |  | `sprite` | `sprite` |
| 469 | `planet_group_6082` | image group 6082 |  | `sprite` | `sprite` |
| 470 | `planet_group_6083` | image group 6083 |  | `sprite` | `sprite` |
| 471 | `planet_group_6084` | image group 6084 |  | `sprite` | `sprite` |
| 472 | `planet_group_6090` | image group 6090 |  | `sprite` | `sprite` |
| 473 | `planet_group_6091` | image group 6091 |  | `sprite` | `sprite` |
| 474 | `planet_group_6092` | image group 6092 |  | `sprite` | `sprite` |
| 475 | `planet_group_6093` | image group 6093 |  | `sprite` | `sprite` |
| 476 | `planet_group_6094` | image group 6094 |  | `sprite` | `sprite` |
| 477 | `planet_group_6100` | image group 6100 |  | `sprite` | `sprite` |
| 478 | `planet_group_6101` | image group 6101 |  | `sprite` | `sprite` |
| 479 | `planet_group_6102` | image group 6102 |  | `sprite` | `sprite` |
| 480 | `planet_group_6103` | image group 6103 |  | `sprite` | `sprite` |
| 481 | `planet_group_6104` | image group 6104 |  | `sprite` | `sprite` |
| 482 | `planet_group_6110` | image group 6110 |  | `sprite` | `sprite` |
| 483 | `planet_group_6111` | image group 6111 |  | `sprite` | `sprite` |
| 484 | `planet_group_6112` | image group 6112 |  | `sprite` | `sprite` |
| 485 | `planet_group_6113` | image group 6113 |  | `sprite` | `sprite` |
| 486 | `planet_group_6114` | image group 6114 |  | `sprite` | `sprite` |
| 487 | `death_star_fire_group_6250_sprite_0` | image group 6250 |  | `sprite` | `sprite` |
| 488 | `death_star_fire_group_6251` | image group 6251 |  | `sprite` | `sprite` |
| 489 | `work_droid1` | `workdroid1` | equipment | `utility` | `scenery` |
| 490 | `work_droid2` | `workdroid2` | equipment | `utility` | `scenery` |
| 491 | `corvette_explode` | `corvetteexplode` | spacecraft | `freighter` | `craft` |
| 492 | `corvette_explode_492` | `corvetteexplode` | spacecraft | `freighter` | `craft` |
| 493 | `corvette_explode_493` | `corvetteexplode` | spacecraft | `freighter` | `craft` |
| 494 | `corvette_explode_494` | `corvetteexplode` | spacecraft | `freighter` | `craft` |
| 495 | `corvette_explode_495` | `corvetteexplode` | spacecraft | `freighter` | `craft` |
| 496 | `corvette_explode_496` | `corvetteexplode` | spacecraft | `freighter` | `craft` |
| 497 | `corvette_explode_497` | `corvetteexplode` | spacecraft | `freighter` | `craft` |
| 498 | `corvette_explode_498` | `corvetteexplode` | spacecraft | `freighter` | `craft` |
| 499 | `corvette_explode_499` | `corvetteexplode` | spacecraft | `freighter` | `craft` |
| 500 | `escape_pod_a_500` | `escapepoda` | spacecraft | `freighter` | `craft` |
| 501 | `nebula_group_7001_sprite_100` | image group 7001 |  | `sprite` | `sprite` |
| 502 | `nebula_group_7002_sprite_200` | image group 7002 |  | `sprite` | `sprite` |
| 503 | `nebula_group_7003_sprite_300` | image group 7003 |  | `sprite` | `sprite` |
| 504 | `nebula_group_7004_sprite_400` | image group 7004 |  | `sprite` | `sprite` |
| 505 | `nebula_group_7005_sprite_500` | image group 7005 |  | `sprite` | `sprite` |
| 506 | `nebula_group_7006_sprite_600` | image group 7006 |  | `sprite` | `sprite` |
| 507 | `nebula_group_7007_sprite_700` | image group 7007 |  | `sprite` | `sprite` |
| 508 | `nebula_group_7008_sprite_800` | image group 7008 |  | `sprite` | `sprite` |
| 509 | `nebula_group_7009_sprite_900` | image group 7009 |  | `sprite` | `sprite` |
| 510 | `nebula_group_7010_sprite_1000` | image group 7010 |  | `sprite` | `sprite` |
| 511 | `galaxy_group_8001_sprite_100` | image group 8001 |  | `sprite` | `sprite` |
| 512 | `galaxy_group_8002_sprite_200` | image group 8002 |  | `sprite` | `sprite` |
| 513 | `galaxy_group_8003_sprite_300` | image group 8003 |  | `sprite` | `sprite` |
| 514 | `galaxy_group_8004_sprite_400` | image group 8004 |  | `sprite` | `sprite` |
| 515 | `galaxy_group_8005_sprite_500` | image group 8005 |  | `sprite` | `sprite` |
| 516 | `galaxy_group_8006_sprite_600` | image group 8006 |  | `sprite` | `sprite` |
| 517 | `galaxy_group_8007_sprite_700` | image group 8007 |  | `sprite` | `sprite` |
| 518 | `galaxy_group_8008_sprite_800` | image group 8008 |  | `sprite` | `sprite` |
| 519 | `galaxy_group_8009_sprite_900` | image group 8009 |  | `sprite` | `sprite` |
| 520 | `galaxy_group_8010_sprite_1000` | image group 8010 |  | `sprite` | `sprite` |
| 521 | `backdrop_group_9001_sprite_1100_521` | image group 9001 |  | `sprite` | `sprite` |
| 522 | `backdrop_group_9002_sprite_1200` | image group 9002 |  | `sprite` | `sprite` |
| 523 | `backdrop_group_9003_sprite_1300` | image group 9003 |  | `sprite` | `sprite` |
| 524 | `backdrop_group_9004_sprite_1400` | image group 9004 |  | `sprite` | `sprite` |
| 525 | `backdrop_group_9005_sprite_1500` | image group 9005 |  | `sprite` | `sprite` |
| 526 | `backdrop_group_9006_sprite_1600` | image group 9006 |  | `sprite` | `sprite` |
| 527 | `backdrop_group_9007_sprite_1700` | image group 9007 |  | `sprite` | `sprite` |
| 528 | `backdrop_group_9008_sprite_1800` | image group 9008 |  | `sprite` | `sprite` |
| 529 | `backdrop_group_9009_sprite_1900` | image group 9009 |  | `sprite` | `sprite` |
| 530 | `backdrop_group_9010_sprite_2000` | image group 9010 |  | `sprite` | `sprite` |
| 531 | `backdrop_group_9100_sprite_5100` | image group 9100 |  | `sprite` | `sprite` |
| 532 | `nebula_group_7011_sprite_1100` | image group 7011 |  | `sprite` | `sprite` |
| 533 | `nebula_group_7012_sprite_1200` | image group 7012 |  | `sprite` | `sprite` |
| 534 | `nebula_group_7013_sprite_1300` | image group 7013 |  | `sprite` | `sprite` |
| 535 | `nebula_group_7014_sprite_1400` | image group 7014 |  | `sprite` | `sprite` |
| 536 | `nebula_group_7015_sprite_1500` | image group 7015 |  | `sprite` | `sprite` |
| 537 | `map_icon_group_14800` | image group 14800 |  | `sprite` | `sprite` |
| 538 | `trail_group_21000_sprite_0` | image group 21000 |  | `debris` | `debris` |
| 539 | `trail_group_21005` | image group 21005 |  | `debris` | `debris` |
| 540 | `trail_group_21010` | image group 21010 |  | `debris` | `debris` |
| 541 | `trail_group_21015` | image group 21015 |  | `debris` | `debris` |
| 542 | `trail_group_21020` | image group 21020 |  | `debris` | `debris` |
| 543 | `trail_group_21025` | image group 21025 |  | `debris` | `debris` |
| 544 | `particle_group_22000` | image group 22000 |  | `debris` | `debris` |
| 545 | `particle_group_22003` | image group 22003 |  | `debris` | `debris` |
| 546 | `particle_group_22005` | image group 22005 |  | `debris` | `debris` |
| 547 | `particle_group_22007` | image group 22007 |  | `debris` | `debris` |
| 548 | `animation_group_3055` | image group 3055 |  | `explosion` | `explosion` |
| 549 | `hull_icon_group_26000` | image group 26000 |  | `sprite` | `sprite` |
| 550 | `ds_laser_internal` | `dslaserinternal` | equipment | `projectile` | `weapon` |
| 551 | `animation_group_3200` | image group 3200 |  | `explosion` | `explosion` |
| 552 | `animation_group_3100` | image group 3100 |  | `explosion` | `explosion` |
| 553 | `animation_group_3300` | image group 3300 |  | `explosion` | `explosion` |
| 554 | `animation_group_3400` | image group 3400 |  | `explosion` | `explosion` |
| 555 | `animation_group_3500` | image group 3500 |  | `explosion` | `explosion` |
| 556 | `ds_laser_power_source` | `dslaserpowersource` | equipment | `death_star_tunnel` | `craft` |

## Genus

Values for `genus` in object type files.

| Number | Name |
|---:|---|
| 0 | `fighter` |
| 1 | `transport` |
| 2 | `utility` |
| 3 | `freighter` |
| 4 | `starship` |
| 5 | `platform` |
| 7 | `projectile` |
| 8 | `mine` |
| 9 | `satellite_buoy` |
| 10 | `asteroid` |
| 11 | `debris` |
| 12 | `sprite` |
| 13 | `explosion` |
| 14 | `large_scenery` |
| 15 | `death_star_tunnel` |
| 17 | `container` |
| 18 | `pilot_droid` |
| 19 | `weapon_emplacement` |
| 20 | `rubble` |
| 21 | `salvage_junk` |

## Family

Values for `family` in object type files.

| Number | Name |
|---:|---|
| 0 | `craft` |
| 1 | `weapon` |
| 2 | `object` |
| 3 | `debris` |
| 4 | `sprite` |
| 5 | `explosion` |
| 6 | `scenery` |

## Flags

The `flags` list of an object type file gives the flags that are on. A flag that a file can change is off when the list doesn't name it. Without `flags`, the object type keeps the flags of the original game.

| Name | Meaning |
|---|---|
| `player_targetable` | The player can target it. |
| `ai_targetable` | Other craft can target it. |
| `nav_buoy` | It is a navigation buoy. The key that targets navigation buoys selects it. |
| `hardware_only` | Set by the game. A file can't change it. |
| `single_mip` | Set by the game. A file can't change it. |
| `static_object` | Its flight groups are placed when the mission starts, instead of flying in. Mines and buoys use it. |
| `animated_sprite` | Set by the game. A file can't change it. |
| `animation_loops` | Set by the game. A file can't change it. |
| `color_key_border` | Set by the game. A file can't change it. |
| `float_hardpoints` | It fires from points spread over its hull, not from its weapon hardpoints. The large starships use it. |
| `spins` | It turns all the time. Gun platforms, mines, satellites and buoys use it. |
| `collision_face_groups` | Shots between AI craft test their hits against a lower detail level of the model. |
| `color_key` | Set by the game. A file can't change it. |
| `alpha` | Set by the game. A file can't change it. |

