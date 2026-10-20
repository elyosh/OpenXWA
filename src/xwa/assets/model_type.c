#include "xwa/assets/model_type.h"

#include <stddef.h>

#define MODEL_TYPE_ENTRY(recordFlags_, assetFlags_, familyId_, genusId_, maxBoundsExtent_, flags_,           \
						 modelIndex_, textureGroup_, frameCount_)                                            \
	{                                                                                                        \
		.recordFlags = (recordFlags_),                                                                       \
		.assetFlags = (assetFlags_),                                                                         \
		.familyId = (familyId_),                                                                             \
		.genusId = (genusId_),                                                                               \
		.maxBoundsExtent = (maxBoundsExtent_),                                                               \
		.curTexLevel = NULL,                                                                                 \
		.texLevels = NULL,                                                                                   \
		.flags = (flags_),                                                                                   \
		.modelIndex = (modelIndex_),                                                                         \
		.textureGroup = (textureGroup_),                                                                     \
		.frameCount = (frameCount_),                                                                         \
	}

// GLOBAL: XWA 0x5FB240
ModelTypeInfo g_modelTypeTable[OBJ_Count] = {
	[OBJ_None] = MODEL_TYPE_ENTRY(0x00u, 0x00u, 0u, 0u, 0, 0x0000u, -1, 0xffffu, 0u),
	[OBJ_XWing] = MODEL_TYPE_ENTRY(0x03u, 0x01u, 0u, 0u, 0, 0x0043u, 0, MODEL_TYPE_LIST_SPACECRAFT, 0u),
	[OBJ_YWing] = MODEL_TYPE_ENTRY(0x03u, 0x01u, 0u, 0u, 0, 0x0043u, 1, MODEL_TYPE_LIST_SPACECRAFT, 1u),
	[OBJ_AWing] = MODEL_TYPE_ENTRY(0x03u, 0x01u, 0u, 0u, 0, 0x0043u, 2, MODEL_TYPE_LIST_SPACECRAFT, 2u),
	[OBJ_BWing] = MODEL_TYPE_ENTRY(0x03u, 0x01u, 0u, 0u, 0, 0x0043u, 3, MODEL_TYPE_LIST_SPACECRAFT, 3u),
	[OBJ_TIEFighter] = MODEL_TYPE_ENTRY(0x03u, 0x01u, 0u, 0u, 0, 0x0043u, 4, MODEL_TYPE_LIST_SPACECRAFT, 4u),
	[OBJ_TIEInterceptor] =
		MODEL_TYPE_ENTRY(0x03u, 0x01u, 0u, 0u, 0, 0x0043u, 5, MODEL_TYPE_LIST_SPACECRAFT, 5u),
	[OBJ_TIEBomber] = MODEL_TYPE_ENTRY(0x03u, 0x01u, 0u, 0u, 0, 0x0043u, 6, MODEL_TYPE_LIST_SPACECRAFT, 6u),
	[OBJ_TIEAdvanced] = MODEL_TYPE_ENTRY(0x03u, 0x01u, 0u, 0u, 0, 0x0043u, 7, MODEL_TYPE_LIST_SPACECRAFT, 7u),
	[OBJ_TIEDefender] = MODEL_TYPE_ENTRY(0x03u, 0x01u, 0u, 0u, 0, 0x0043u, 8, MODEL_TYPE_LIST_SPACECRAFT, 8u),
	[OBJ_IrdFighter] = MODEL_TYPE_ENTRY(0x03u, 0x01u, 0u, 0u, 0, 0x0043u, 9, MODEL_TYPE_LIST_SPACECRAFT, 9u),
	[OBJ_ToscanFighter] =
		MODEL_TYPE_ENTRY(0x03u, 0x01u, 0u, 0u, 0, 0x0043u, 10, MODEL_TYPE_LIST_SPACECRAFT, 10u),
	[OBJ_MissileBoat] =
		MODEL_TYPE_ENTRY(0x03u, 0x01u, 0u, 0u, 0, 0x0043u, 11, MODEL_TYPE_LIST_SPACECRAFT, 11u),
	[OBJ_Twing] = MODEL_TYPE_ENTRY(0x03u, 0x01u, 0u, 0u, 0, 0x0043u, 12, MODEL_TYPE_LIST_SPACECRAFT, 12u),
	[OBJ_Z95] = MODEL_TYPE_ENTRY(0x03u, 0x01u, 0u, 0u, 0, 0x0043u, 13, MODEL_TYPE_LIST_SPACECRAFT, 13u),
	[OBJ_R41] = MODEL_TYPE_ENTRY(0x03u, 0x01u, 0u, 0u, 0, 0x0043u, 14, MODEL_TYPE_LIST_SPACECRAFT, 14u),
	[OBJ_AssaultGunboat] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 0u, 0, 0x0043u, 15, MODEL_TYPE_LIST_SPACECRAFT, 15u),
	[OBJ_SkiprayBlastBoat] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 0u, 0, 0x0043u, 16, MODEL_TYPE_LIST_SPACECRAFT, 16u),
	[OBJ_TIEBizarro] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 0u, 0, 0x0043u, 17, MODEL_TYPE_LIST_SPACECRAFT, 17u),
	[OBJ_TIEBigGun] = MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 0u, 0, 0x0043u, 18, MODEL_TYPE_LIST_SPACECRAFT, 18u),
	[OBJ_TIEWarheads] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 0u, 0, 0x0043u, 19, MODEL_TYPE_LIST_SPACECRAFT, 19u),
	[OBJ_TIEBomb] = MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 0u, 0, 0x0043u, 20, MODEL_TYPE_LIST_SPACECRAFT, 20u),
	[OBJ_TIEBooster] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 0u, 0, 0x0043u, 21, MODEL_TYPE_LIST_SPACECRAFT, 21u),
	[OBJ_CloakshapeFighter] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 0u, 0, 0x0043u, 22, MODEL_TYPE_LIST_SPACECRAFT, 22u),
	[OBJ_RazorFighter] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 0u, 0, 0x0043u, 23, MODEL_TYPE_LIST_SPACECRAFT, 23u),
	[OBJ_PlanetaryFighter] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 0u, 0, 0x0043u, 24, MODEL_TYPE_LIST_SPACECRAFT, 24u),
	[OBJ_SupaFighter] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 0u, 0, 0x0043u, 25, MODEL_TYPE_LIST_SPACECRAFT, 25u),
	[OBJ_Piggyback] = MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 0u, 0, 0x0043u, 26, MODEL_TYPE_LIST_SPACECRAFT, 26u),
	[OBJ_Booster] = MODEL_TYPE_ENTRY(0x02u, 0x81u, 0u, 0u, 0, 0x0043u, 27, MODEL_TYPE_LIST_SPACECRAFT, 27u),
	[OBJ_PreybirdFighter] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 0u, 0, 0x0043u, 28, MODEL_TYPE_LIST_SPACECRAFT, 28u),
	[OBJ_XWing_030] = MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 0u, 0, 0x0043u, 29, MODEL_TYPE_LIST_SPACECRAFT, 29u),
	[OBJ_SlaveOne] = MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 0u, 0, 0x0043u, 30, MODEL_TYPE_LIST_SPACECRAFT, 30u),
	[OBJ_SlaveTwo] = MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 0u, 0, 0x0043u, 31, MODEL_TYPE_LIST_SPACECRAFT, 31u),
	[OBJ_NoAsset_033] = MODEL_TYPE_ENTRY(0x02u, 0x81u, 0u, 0u, 0, 0x0043u, 32, 0xffffu, 0u),
	[OBJ_NoAsset_034] = MODEL_TYPE_ENTRY(0x02u, 0x81u, 0u, 0u, 0, 0x0043u, 33, 0xffffu, 0u),
	[OBJ_NoAsset_035] = MODEL_TYPE_ENTRY(0x02u, 0x81u, 0u, 0u, 0, 0x0043u, 34, 0xffffu, 0u),
	[OBJ_NoAsset_036] = MODEL_TYPE_ENTRY(0x02u, 0x81u, 0u, 0u, 0, 0x0043u, 35, 0xffffu, 0u),
	[OBJ_NoAsset_037] = MODEL_TYPE_ENTRY(0x02u, 0x81u, 0u, 0u, 0, 0x0043u, 36, 0xffffu, 0u),
	[OBJ_NoAsset_038] = MODEL_TYPE_ENTRY(0x02u, 0x81u, 0u, 0u, 0, 0x0043u, 37, 0xffffu, 0u),
	[OBJ_NoAsset_039] = MODEL_TYPE_ENTRY(0x02u, 0x81u, 0u, 0u, 0, 0x0043u, 38, 0xffffu, 0u),
	[OBJ_NoAsset_040] = MODEL_TYPE_ENTRY(0x02u, 0x81u, 0u, 0u, 0, 0x0043u, 39, 0xffffu, 0u),
	[OBJ_NoAsset_041] = MODEL_TYPE_ENTRY(0x02u, 0x81u, 0u, 0u, 0, 0x0043u, 40, 0xffffu, 0u),
	[OBJ_NoAsset_042] = MODEL_TYPE_ENTRY(0x02u, 0x81u, 0u, 0u, 0, 0x0043u, 41, 0xffffu, 0u),
	[OBJ_Tug] = MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 2u, 0, 0x0043u, 42, MODEL_TYPE_LIST_SPACECRAFT, 32u),
	[OBJ_CombatUtilityVehicle] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 2u, 0, 0x0043u, 43, MODEL_TYPE_LIST_SPACECRAFT, 33u),
	[OBJ_HeavyLifter] =
		MODEL_TYPE_ENTRY(0x03u, 0x01u, 0u, 2u, 0, 0x0043u, 44, MODEL_TYPE_LIST_SPACECRAFT, 34u),
	[OBJ_MoleMiner] = MODEL_TYPE_ENTRY(0x03u, 0x01u, 0u, 2u, 0, 0x0043u, 45, MODEL_TYPE_LIST_SPACECRAFT, 35u),
	[OBJ_EscapePod] = MODEL_TYPE_ENTRY(0x03u, 0x09u, 0u, 2u, 0, 0x0043u, 46, MODEL_TYPE_LIST_SPACECRAFT, 36u),
	[OBJ_EscapePodA] =
		MODEL_TYPE_ENTRY(0x02u, 0x09u, 0u, 2u, 0, 0x0043u, 47, MODEL_TYPE_LIST_SPACECRAFT, 153u),
	[OBJ_NoAsset_049] = MODEL_TYPE_ENTRY(0x02u, 0x81u, 0u, 2u, 0, 0x0043u, 48, 0xffffu, 0u),
	[OBJ_Shuttle] = MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 1u, 0, 0x0043u, 49, MODEL_TYPE_LIST_SPACECRAFT, 37u),
	[OBJ_EscortShuttle] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 1u, 0, 0x0043u, 50, MODEL_TYPE_LIST_SPACECRAFT, 38u),
	[OBJ_StormtrooperTransport] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 1u, 0, 0x0043u, 51, MODEL_TYPE_LIST_SPACECRAFT, 39u),
	[OBJ_AssaultTransport] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 1u, 0, 0x0043u, 52, MODEL_TYPE_LIST_SPACECRAFT, 40u),
	[OBJ_EscortTransport] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 1u, 0, 0x0043u, 53, MODEL_TYPE_LIST_SPACECRAFT, 41u),
	[OBJ_SystemPatrolCraft] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 1u, 0, 0x0043u, 54, MODEL_TYPE_LIST_SPACECRAFT, 42u),
	[OBJ_ScoutCraft] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 1u, 0, 0x0043u, 55, MODEL_TYPE_LIST_SPACECRAFT, 43u),
	[OBJ_MuurianTransport] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 1u, 0, 0x0043u, 56, MODEL_TYPE_LIST_SPACECRAFT, 44u),
	[OBJ_CorellianTransport2] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 1u, 0, 0x0043u, 57, MODEL_TYPE_LIST_SPACECRAFT, 45u),
	[OBJ_MilleniumFalcon2] =
		MODEL_TYPE_ENTRY(0x03u, 0x01u, 0u, 1u, 0, 0x0043u, 58, MODEL_TYPE_LIST_SPACECRAFT, 46u),
	[OBJ_Outrider] = MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 1u, 0, 0x0043u, 59, MODEL_TYPE_LIST_SPACECRAFT, 47u),
	[OBJ_ImperialLandingCraft] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 1u, 0, 0x0043u, 60, MODEL_TYPE_LIST_SPACECRAFT, 48u),
	[OBJ_AssaultShuttle] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 1u, 0, 0x0043u, 61, MODEL_TYPE_LIST_SPACECRAFT, 49u),
	[OBJ_LuxuryYacht] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 1u, 0, 0x0043u, 62, MODEL_TYPE_LIST_SPACECRAFT, 50u),
	[OBJ_FerryboatLiner] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 3u, 0, 0x0043u, 63, MODEL_TYPE_LIST_SPACECRAFT, 51u),
	[OBJ_FamilyTransport] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 1u, 0, 0x0043u, 64, MODEL_TYPE_LIST_SPACECRAFT, 52u),
	[OBJ_NoAsset_066] = MODEL_TYPE_ENTRY(0x02u, 0x81u, 0u, 1u, 0, 0x0043u, 65, 0xffffu, 0u),
	[OBJ_NoAsset_067] = MODEL_TYPE_ENTRY(0x02u, 0x81u, 0u, 1u, 0, 0x0043u, 66, 0xffffu, 0u),
	[OBJ_NoAsset_068] = MODEL_TYPE_ENTRY(0x02u, 0x81u, 0u, 1u, 0, 0x0043u, 67, 0xffffu, 0u),
	[OBJ_NoAsset_069] = MODEL_TYPE_ENTRY(0x02u, 0x81u, 0u, 1u, 0, 0x0043u, 68, 0xffffu, 0u),
	[OBJ_NoAsset_070] = MODEL_TYPE_ENTRY(0x02u, 0x81u, 0u, 1u, 0, 0x0043u, 69, 0xffffu, 0u),
	[OBJ_ContainerBrick] =
		MODEL_TYPE_ENTRY(0x03u, 0x01u, 0u, 17u, 0, 0x0043u, 70, MODEL_TYPE_LIST_SPACECRAFT, 53u),
	[OBJ_ContainerHexBox] =
		MODEL_TYPE_ENTRY(0x03u, 0x01u, 0u, 17u, 0, 0x0043u, 71, MODEL_TYPE_LIST_SPACECRAFT, 54u),
	[OBJ_ContainerTube] =
		MODEL_TYPE_ENTRY(0x03u, 0x01u, 0u, 17u, 0, 0x0043u, 72, MODEL_TYPE_LIST_SPACECRAFT, 55u),
	[OBJ_ContainerPronged] =
		MODEL_TYPE_ENTRY(0x03u, 0x01u, 0u, 17u, 0, 0x0043u, 73, MODEL_TYPE_LIST_SPACECRAFT, 56u),
	[OBJ_ContainerHemisphere] =
		MODEL_TYPE_ENTRY(0x03u, 0x01u, 0u, 17u, 0, 0x0043u, 74, MODEL_TYPE_LIST_SPACECRAFT, 57u),
	[OBJ_ContainerSlotted] =
		MODEL_TYPE_ENTRY(0x03u, 0x01u, 0u, 17u, 0, 0x0043u, 75, MODEL_TYPE_LIST_SPACECRAFT, 58u),
	[OBJ_ContainerHourglass] =
		MODEL_TYPE_ENTRY(0x03u, 0x01u, 0u, 17u, 0, 0x0043u, 76, MODEL_TYPE_LIST_SPACECRAFT, 59u),
	[OBJ_ContainerGem] =
		MODEL_TYPE_ENTRY(0x03u, 0x01u, 0u, 17u, 0, 0x0043u, 77, MODEL_TYPE_LIST_SPACECRAFT, 60u),
	[OBJ_ContainerYshaped] =
		MODEL_TYPE_ENTRY(0x03u, 0x01u, 0u, 17u, 0, 0x0043u, 78, MODEL_TYPE_LIST_SPACECRAFT, 61u),
	[OBJ_CargoCanister] =
		MODEL_TYPE_ENTRY(0x03u, 0x01u, 0u, 17u, 0, 0x0043u, 79, MODEL_TYPE_LIST_SPACECRAFT, 62u),
	[OBJ_PropaneTank] =
		MODEL_TYPE_ENTRY(0x03u, 0x01u, 0u, 17u, 0, 0x0043u, 80, MODEL_TYPE_LIST_SPACECRAFT, 63u),
	[OBJ_ContainerGrande] =
		MODEL_TYPE_ENTRY(0x03u, 0x01u, 0u, 17u, 0, 0x0043u, 81, MODEL_TYPE_LIST_SPACECRAFT, 64u),
	[OBJ_ContainerBox] =
		MODEL_TYPE_ENTRY(0x03u, 0x01u, 0u, 17u, 0, 0x0043u, 82, MODEL_TYPE_LIST_SPACECRAFT, 65u),
	[OBJ_ContainerSphere] =
		MODEL_TYPE_ENTRY(0x03u, 0x01u, 0u, 17u, 0, 0x0043u, 83, MODEL_TYPE_LIST_SPACECRAFT, 66u),
	[OBJ_ContainerHanger] =
		MODEL_TYPE_ENTRY(0x03u, 0x01u, 0u, 17u, 0, 0x0043u, 84, MODEL_TYPE_LIST_SPACECRAFT, 67u),
	[OBJ_NoAsset_086] = MODEL_TYPE_ENTRY(0x02u, 0x01u, 0u, 17u, 0, 0x0043u, 85, 0xffffu, 0u),
	[OBJ_NoAsset_087] = MODEL_TYPE_ENTRY(0x02u, 0x01u, 0u, 17u, 0, 0x0043u, 86, 0xffffu, 0u),
	[OBJ_NoAsset_088] = MODEL_TYPE_ENTRY(0x02u, 0x01u, 0u, 17u, 0, 0x0043u, 87, 0xffffu, 0u),
	[OBJ_NoAsset_089] = MODEL_TYPE_ENTRY(0x02u, 0x01u, 0u, 17u, 0, 0x0043u, 88, 0xffffu, 0u),
	[OBJ_NoAsset_090] = MODEL_TYPE_ENTRY(0x02u, 0x01u, 0u, 17u, 0, 0x0043u, 89, 0xffffu, 0u),
	[OBJ_BulkFreighter] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 3u, 0, 0x0043u, 90, MODEL_TYPE_LIST_SPACECRAFT, 68u),
	[OBJ_CargoFerry] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 3u, 0, 0x0043u, 91, MODEL_TYPE_LIST_SPACECRAFT, 69u),
	[OBJ_ModularConveyor] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 3u, 0, 0x0043u, 92, MODEL_TYPE_LIST_SPACECRAFT, 70u),
	[OBJ_ContainerTransport] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 3u, 0, 0x0043u, 93, MODEL_TYPE_LIST_SPACECRAFT, 71u),
	[OBJ_TunaBoat2] = MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 3u, 0, 0x0043u, 94, MODEL_TYPE_LIST_SPACECRAFT, 72u),
	[OBJ_StarGalleon] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 3u, 0, 0x0043u, 95, MODEL_TYPE_LIST_SPACECRAFT, 73u),
	[OBJ_ModActionTransport] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 3u, 0, 0x0043u, 96, MODEL_TYPE_LIST_SPACECRAFT, 74u),
	[OBJ_MobquetTransport] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 3u, 0, 0x0043u, 97, MODEL_TYPE_LIST_SPACECRAFT, 75u),
	[OBJ_XiytiarTransport] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 3u, 0, 0x0043u, 98, MODEL_TYPE_LIST_SPACECRAFT, 76u),
	[OBJ_FreighterConB] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 3u, 0, 0x0043u, 99, MODEL_TYPE_LIST_SPACECRAFT, 77u),
	[OBJ_FreighterConG] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 3u, 0, 0x0043u, 100, MODEL_TYPE_LIST_SPACECRAFT, 78u),
	[OBJ_FreighterBox] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 3u, 0, 0x0043u, 101, MODEL_TYPE_LIST_SPACECRAFT, 79u),
	[OBJ_XWing_103] = MODEL_TYPE_ENTRY(0x02u, 0x81u, 0u, 3u, 0, 0x0043u, 102, MODEL_TYPE_LIST_SPACECRAFT, 0u),
	[OBJ_XWing_104] = MODEL_TYPE_ENTRY(0x02u, 0x81u, 0u, 3u, 0, 0x0043u, 103, MODEL_TYPE_LIST_SPACECRAFT, 0u),
	[OBJ_CrewCabinFront] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 3u, 0, 0x0043u, 104, MODEL_TYPE_LIST_SPACECRAFT, 136u),
	[OBJ_ConnectorRod] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 3u, 0, 0x0042u, 105, MODEL_TYPE_LIST_SPACECRAFT, 137u),
	[OBJ_EngineBack] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 3u, 0, 0x0043u, 106, MODEL_TYPE_LIST_SPACECRAFT, 138u),
	[OBJ_XWing_108] = MODEL_TYPE_ENTRY(0x02u, 0x81u, 0u, 3u, 0, 0x0043u, 107, MODEL_TYPE_LIST_SPACECRAFT, 0u),
	[OBJ_XWing_109] = MODEL_TYPE_ENTRY(0x02u, 0x81u, 0u, 3u, 0, 0x0043u, 108, MODEL_TYPE_LIST_SPACECRAFT, 0u),
	[OBJ_EngineFront] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 3u, 0, 0x0043u, 109, MODEL_TYPE_LIST_SPACECRAFT, 139u),
	[OBJ_CrewCabinBack] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 3u, 0, 0x0043u, 110, MODEL_TYPE_LIST_SPACECRAFT, 140u),
	[OBJ_XWing_112] = MODEL_TYPE_ENTRY(0x02u, 0x81u, 0u, 3u, 0, 0x0043u, 111, MODEL_TYPE_LIST_SPACECRAFT, 0u),
	[OBJ_XWing_113] = MODEL_TYPE_ENTRY(0x02u, 0x81u, 0u, 3u, 0, 0x0043u, 112, MODEL_TYPE_LIST_SPACECRAFT, 0u),
	[OBJ_XWing_114] = MODEL_TYPE_ENTRY(0x02u, 0x81u, 0u, 3u, 0, 0x0043u, 113, MODEL_TYPE_LIST_SPACECRAFT, 0u),
	[OBJ_Suprosa] = MODEL_TYPE_ENTRY(0x02u, 0x81u, 0u, 3u, 0, 0x0043u, 114, MODEL_TYPE_LIST_SPACECRAFT, 142u),
	[OBJ_NoAsset_116] = MODEL_TYPE_ENTRY(0x02u, 0x81u, 0u, 3u, 0, 0x0043u, 115, 0xffffu, 0u),
	[OBJ_NoAsset_117] = MODEL_TYPE_ENTRY(0x02u, 0x81u, 0u, 3u, 0, 0x0043u, 116, 0xffffu, 0u),
	[OBJ_NoAsset_118] = MODEL_TYPE_ENTRY(0x02u, 0x81u, 0u, 3u, 0, 0x0043u, 117, 0xffffu, 0u),
	[OBJ_NoAsset_119] = MODEL_TYPE_ENTRY(0x02u, 0x81u, 0u, 3u, 0, 0x0043u, 118, 0xffffu, 0u),
	[OBJ_NoAsset_120] = MODEL_TYPE_ENTRY(0x02u, 0x81u, 0u, 3u, 0, 0x0043u, 119, 0xffffu, 0u),
	[OBJ_NoAsset_121] = MODEL_TYPE_ENTRY(0x02u, 0x81u, 0u, 3u, 0, 0x0043u, 120, 0xffffu, 0u),
	[OBJ_NoAsset_122] = MODEL_TYPE_ENTRY(0x02u, 0x81u, 0u, 3u, 0, 0x0043u, 121, 0xffffu, 0u),
	[OBJ_NoAsset_123] = MODEL_TYPE_ENTRY(0x02u, 0x81u, 0u, 3u, 0, 0x0043u, 122, 0xffffu, 0u),
	[OBJ_NoAsset_124] = MODEL_TYPE_ENTRY(0x02u, 0x81u, 0u, 3u, 0, 0x0043u, 123, 0xffffu, 0u),
	[OBJ_NoAsset_125] = MODEL_TYPE_ENTRY(0x02u, 0x81u, 0u, 3u, 0, 0x0043u, 124, 0xffffu, 0u),
	[OBJ_Corvette2] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 3u, 0, 0x0043u, 125, MODEL_TYPE_LIST_SPACECRAFT, 80u),
	[OBJ_ModCorvette] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 3u, 0, 0x0043u, 126, MODEL_TYPE_LIST_SPACECRAFT, 81u),
	[OBJ_Frigate2] = MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 4u, 0, 0x0043u, 127, MODEL_TYPE_LIST_SPACECRAFT, 82u),
	[OBJ_ModFrigate] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 4u, 0, 0x0043u, 128, MODEL_TYPE_LIST_SPACECRAFT, 83u),
	[OBJ_PassengerLiner] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 4u, 0, 0x0043u, 129, MODEL_TYPE_LIST_SPACECRAFT, 84u),
	[OBJ_CarrackCruiser] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 4u, 0, 0x0043u, 130, MODEL_TYPE_LIST_SPACECRAFT, 85u),
	[OBJ_StrikeCruiser] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 4u, 0, 0x0043u, 131, MODEL_TYPE_LIST_SPACECRAFT, 86u),
	[OBJ_EscortCarrier] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 4u, 0, 0x0043u, 132, MODEL_TYPE_LIST_SPACECRAFT, 87u),
	[OBJ_Dreadnaught2] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 4u, 0, 0x0843u, 133, MODEL_TYPE_LIST_SPACECRAFT, 88u),
	[OBJ_CalamariCruiserNew] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 4u, 0, 0x0843u, 134, MODEL_TYPE_LIST_SPACECRAFT, 89u),
	[OBJ_LightCalamariCruiser] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 4u, 0, 0x0043u, 135, MODEL_TYPE_LIST_SPACECRAFT, 90u),
	[OBJ_Interdictor2] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 4u, 0, 0x0843u, 136, MODEL_TYPE_LIST_SPACECRAFT, 91u),
	[OBJ_VictoryStarDestroyer2] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 4u, 0, 0x0843u, 137, MODEL_TYPE_LIST_SPACECRAFT, 92u),
	[OBJ_ImperialStarDestroyer2] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 4u, 0, 0x0843u, 138, MODEL_TYPE_LIST_SPACECRAFT, 93u),
	[OBJ_SuperStarDestroyer] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 4u, 0, 0x0843u, 139, MODEL_TYPE_LIST_SPACECRAFT, 94u),
	[OBJ_ModStrikeCruiser] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 4u, 0, 0x0043u, 163, MODEL_TYPE_LIST_SPACECRAFT, 95u),
	[OBJ_LancerFrigate] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 4u, 0, 0x0843u, 141, MODEL_TYPE_LIST_SPACECRAFT, 96u),
	[OBJ_BulkCruiser] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 4u, 0, 0x0843u, 142, MODEL_TYPE_LIST_SPACECRAFT, 97u),
	[OBJ_AssaultFrigate] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 4u, 0, 0x0843u, 143, MODEL_TYPE_LIST_SPACECRAFT, 98u),
	[OBJ_CorellianGunship] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 3u, 0, 0x0043u, 144, MODEL_TYPE_LIST_SPACECRAFT, 99u),
	[OBJ_MarauderCorvette] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 4u, 0, 0x0043u, 145, MODEL_TYPE_LIST_SPACECRAFT, 100u),
	[OBJ_ImperialResearchShip] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 4u, 0, 0x0043u, 146, MODEL_TYPE_LIST_SPACECRAFT, 101u),
	[OBJ_CalamariWinged] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 4u, 0, 0x0843u, 147, MODEL_TYPE_LIST_SPACECRAFT, 135u),
	[OBJ_VictoryStarDestroyer2_149] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 4u, 0, 0x0843u, 148, MODEL_TYPE_LIST_SPACECRAFT, 92u),
	[OBJ_ImperialStarDestroyer2_150] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 4u, 0, 0x0843u, 149, MODEL_TYPE_LIST_SPACECRAFT, 141u),
	[OBJ_NoAsset_151] = MODEL_TYPE_ENTRY(0x02u, 0x81u, 0u, 4u, 0, 0x0043u, 150, 0xffffu, 0u),
	[OBJ_NoAsset_152] = MODEL_TYPE_ENTRY(0x02u, 0x81u, 0u, 5u, 0, 0x0043u, 151, 0xffffu, 0u),
	[OBJ_Platform1] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 5u, 0, 0x0043u, 152, MODEL_TYPE_LIST_SPACECRAFT, 102u),
	[OBJ_Platform2] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 5u, 0, 0x0043u, 153, MODEL_TYPE_LIST_SPACECRAFT, 103u),
	[OBJ_Platform3] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 5u, 0, 0x0043u, 154, MODEL_TYPE_LIST_SPACECRAFT, 104u),
	[OBJ_Platform4] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 5u, 0, 0x0043u, 155, MODEL_TYPE_LIST_SPACECRAFT, 105u),
	[OBJ_Platform5] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 5u, 0, 0x0043u, 156, MODEL_TYPE_LIST_SPACECRAFT, 106u),
	[OBJ_Platform6] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 5u, 0, 0x0043u, 157, MODEL_TYPE_LIST_SPACECRAFT, 107u),
	[OBJ_AsteroidBase] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 5u, 0, 0x0043u, 158, MODEL_TYPE_LIST_SPACECRAFT, 108u),
	[OBJ_Factory] = MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 5u, 0, 0x0043u, 159, MODEL_TYPE_LIST_SPACECRAFT, 109u),
	[OBJ_ShipYard] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 5u, 0, 0x0043u, 160, MODEL_TYPE_LIST_SPACECRAFT, 110u),
	[OBJ_RepairYard] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 5u, 0, 0x0043u, 161, MODEL_TYPE_LIST_SPACECRAFT, 111u),
	[OBJ_GolanOne] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 5u, 0, 0x0043u, 162, MODEL_TYPE_LIST_SPACECRAFT, 112u),
	[OBJ_GolanTwo] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 5u, 0, 0x0043u, 163, MODEL_TYPE_LIST_SPACECRAFT, 113u),
	[OBJ_GolanThree] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 5u, 0, 0x0043u, 164, MODEL_TYPE_LIST_SPACECRAFT, 114u),
	[OBJ_DerilynPlatform] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 5u, 0, 0x0043u, 165, MODEL_TYPE_LIST_SPACECRAFT, 115u),
	[OBJ_SensorArray] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 5u, 0, 0x0043u, 166, MODEL_TYPE_LIST_SPACECRAFT, 116u),
	[OBJ_CommRelay] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 5u, 0, 0x0043u, 167, MODEL_TYPE_LIST_SPACECRAFT, 117u),
	[OBJ_SpaceColony1] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 5u, 0, 0x0043u, 168, MODEL_TYPE_LIST_SPACECRAFT, 118u),
	[OBJ_SpaceColony2] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 5u, 0, 0x0043u, 169, MODEL_TYPE_LIST_SPACECRAFT, 119u),
	[OBJ_SpaceColony3] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 5u, 0, 0x0043u, 170, MODEL_TYPE_LIST_SPACECRAFT, 120u),
	[OBJ_Casino] = MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 5u, 0, 0x0043u, 171, MODEL_TYPE_LIST_SPACECRAFT, 121u),
	[OBJ_CargoFacility1] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 5u, 0, 0x0043u, 172, MODEL_TYPE_LIST_SPACECRAFT, 122u),
	[OBJ_CargoFacility2] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 5u, 0, 0x0043u, 173, MODEL_TYPE_LIST_SPACECRAFT, 123u),
	[OBJ_AsteroidMiningUnit] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 5u, 0, 0x0043u, 174, MODEL_TYPE_LIST_SPACECRAFT, 124u),
	[OBJ_ProcessingPlant] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 5u, 0, 0x0043u, 175, MODEL_TYPE_LIST_SPACECRAFT, 125u),
	[OBJ_RebelPlatform] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 5u, 0, 0x0843u, 176, MODEL_TYPE_LIST_SPACECRAFT, 126u),
	[OBJ_ImperialResearchCenter] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 5u, 0, 0x0043u, 177, MODEL_TYPE_LIST_SPACECRAFT, 127u),
	[OBJ_FamilyBase] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 5u, 0, 0x0043u, 178, MODEL_TYPE_LIST_SPACECRAFT, 128u),
	[OBJ_FamilyRepairYard] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 5u, 0, 0x0043u, 179, MODEL_TYPE_LIST_SPACECRAFT, 129u),
	[OBJ_PirateShipyard] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 5u, 0, 0x0043u, 180, MODEL_TYPE_LIST_SPACECRAFT, 130u),
	[OBJ_IndustrialComplex] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 5u, 0, 0x0043u, 181, MODEL_TYPE_LIST_SPACECRAFT, 131u),
	[OBJ_PirateJunkyardBase] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 5u, 0, 0x0043u, 182, MODEL_TYPE_LIST_SPACECRAFT, 132u),
	[OBJ_NoAsset_184] = MODEL_TYPE_ENTRY(0x02u, 0x81u, 0u, 5u, 0, 0x0043u, 183, 0xffffu, 0u),
	[OBJ_NoAsset_185] = MODEL_TYPE_ENTRY(0x02u, 0x81u, 0u, 5u, 0, 0x0043u, 184, 0xffffu, 0u),
	[OBJ_NoAsset_186] = MODEL_TYPE_ENTRY(0x02u, 0x81u, 0u, 5u, 0, 0x0043u, 185, 0xffffu, 0u),
	[OBJ_NoAsset_187] = MODEL_TYPE_ENTRY(0x02u, 0x81u, 0u, 5u, 0, 0x0043u, 186, 0xffffu, 0u),
	[OBJ_NoAsset_188] = MODEL_TYPE_ENTRY(0x02u, 0x81u, 0u, 5u, 0, 0x0043u, 187, 0xffffu, 0u),
	[OBJ_NoAsset_189] = MODEL_TYPE_ENTRY(0x02u, 0x81u, 0u, 5u, 0, 0x0043u, 188, 0xffffu, 0u),
	[OBJ_NoAsset_190] = MODEL_TYPE_ENTRY(0x02u, 0x81u, 0u, 5u, 0, 0x0043u, 189, 0xffffu, 0u),
	[OBJ_NoAsset_191] = MODEL_TYPE_ENTRY(0x02u, 0x81u, 0u, 5u, 0, 0x0043u, 190, 0xffffu, 0u),
	[OBJ_LaserBat] = MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 19u, 0, 0x0043u, 191, MODEL_TYPE_LIST_EQUIPMENT, 40u),
	[OBJ_IonBat] = MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 19u, 0, 0x0043u, 192, MODEL_TYPE_LIST_EQUIPMENT, 39u),
	[OBJ_AsteroidLaserBattery] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 19u, 0, 0x0043u, 193, MODEL_TYPE_LIST_SPACECRAFT, 133u),
	[OBJ_AsteroidWarheadLauncher] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 19u, 0, 0x0043u, 194, MODEL_TYPE_LIST_SPACECRAFT, 134u),
	[OBJ_GunPlatform] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 19u, 0, 0x1043u, 195, MODEL_TYPE_LIST_EQUIPMENT, 6u),
	[OBJ_GunPad] = MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 19u, 0, 0x1043u, 196, MODEL_TYPE_LIST_EQUIPMENT, 43u),
	[OBJ_GunWarheadPad] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 19u, 0, 0x1043u, 197, MODEL_TYPE_LIST_EQUIPMENT, 44u),
	[OBJ_HomingMineA] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 19u, 0, 0x1043u, 198, MODEL_TYPE_LIST_EQUIPMENT, 41u),
	[OBJ_HomingMineB] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 19u, 0, 0x1043u, 199, MODEL_TYPE_LIST_EQUIPMENT, 42u),
	[OBJ_ProximityMineA] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 19u, 0, 0x1043u, 200, MODEL_TYPE_LIST_EQUIPMENT, 45u),
	[OBJ_ProximityMineB] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 19u, 0, 0x1043u, 201, MODEL_TYPE_LIST_EQUIPMENT, 46u),
	[OBJ_RebelPilot] =
		MODEL_TYPE_ENTRY(0x03u, 0x09u, 0u, 18u, 0, 0x0043u, 202, MODEL_TYPE_LIST_EQUIPMENT, 51u),
	[OBJ_ImperialPilot] =
		MODEL_TYPE_ENTRY(0x03u, 0x09u, 0u, 18u, 0, 0x0043u, 203, MODEL_TYPE_LIST_EQUIPMENT, 52u),
	[OBJ_CivilianPilot] =
		MODEL_TYPE_ENTRY(0x03u, 0x09u, 0u, 18u, 0, 0x0043u, 204, MODEL_TYPE_LIST_EQUIPMENT, 53u),
	[OBJ_ZeroGStormtrooper] =
		MODEL_TYPE_ENTRY(0x03u, 0x01u, 0u, 18u, 0, 0x0043u, 205, MODEL_TYPE_LIST_EQUIPMENT, 54u),
	[OBJ_ZeroGUtility] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 18u, 0, 0x0043u, 206, MODEL_TYPE_LIST_EQUIPMENT, 55u),
	[OBJ_Marko] = MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 18u, 0, 0x0043u, 207, MODEL_TYPE_LIST_EQUIPMENT, 56u),
	[OBJ_R2D2] = MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 18u, 0, 0x0043u, 208, MODEL_TYPE_LIST_EQUIPMENT, 57u),
	[OBJ_CommSat1] = MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 9u, 0, 0x1043u, 209, MODEL_TYPE_LIST_EQUIPMENT, 0u),
	[OBJ_CommSat2] = MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 9u, 0, 0x1043u, 210, MODEL_TYPE_LIST_EQUIPMENT, 1u),
	[OBJ_CommSat3] = MODEL_TYPE_ENTRY(0x03u, 0x81u, 2u, 9u, 0, 0x1043u, 211, MODEL_TYPE_LIST_EQUIPMENT, 2u),
	[OBJ_Probe] = MODEL_TYPE_ENTRY(0x03u, 0x81u, 2u, 9u, 0, 0x1043u, 212, MODEL_TYPE_LIST_EQUIPMENT, 7u),
	[OBJ_ProbeCapsule] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 9u, 0, 0x0043u, 213, MODEL_TYPE_LIST_EQUIPMENT, 50u),
	[OBJ_NavBuoy1] = MODEL_TYPE_ENTRY(0x03u, 0x01u, 0u, 9u, 0, 0x1047u, 214, MODEL_TYPE_LIST_EQUIPMENT, 47u),
	[OBJ_NavBuoy2] = MODEL_TYPE_ENTRY(0x03u, 0x01u, 0u, 9u, 0, 0x1047u, 215, MODEL_TYPE_LIST_EQUIPMENT, 48u),
	[OBJ_NavBuoy3] = MODEL_TYPE_ENTRY(0x03u, 0x01u, 0u, 9u, 0, 0x1047u, 216, MODEL_TYPE_LIST_EQUIPMENT, 49u),
	[OBJ_HyperBuoy] = MODEL_TYPE_ENTRY(0x03u, 0x01u, 0u, 9u, 0, 0x1047u, 217, MODEL_TYPE_LIST_EQUIPMENT, 8u),
	[OBJ_Mine1] = MODEL_TYPE_ENTRY(0x03u, 0x81u, 1u, 8u, 0, 0x0083u, -1, MODEL_TYPE_LIST_EQUIPMENT, 3u),
	[OBJ_Mine2] = MODEL_TYPE_ENTRY(0x03u, 0x81u, 1u, 8u, 0, 0x0083u, -1, MODEL_TYPE_LIST_EQUIPMENT, 4u),
	[OBJ_Mine3] = MODEL_TYPE_ENTRY(0x03u, 0x81u, 1u, 8u, 0, 0x0083u, -1, MODEL_TYPE_LIST_EQUIPMENT, 5u),
	[OBJ_NoAsset_222] = MODEL_TYPE_ENTRY(0x00u, 0x00u, 3u, 11u, 0, 0x0040u, -1, 0xffffu, 0u),
	[OBJ_AsteroidHR1] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 3u, 10u, 0, 0x0080u, -1, MODEL_TYPE_LIST_EQUIPMENT, 11u),
	[OBJ_AsteroidHR2] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 3u, 10u, 0, 0x0080u, -1, MODEL_TYPE_LIST_EQUIPMENT, 12u),
	[OBJ_AsteroidHR3] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 3u, 10u, 0, 0x0080u, -1, MODEL_TYPE_LIST_EQUIPMENT, 13u),
	[OBJ_AsteroidHR4] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 3u, 10u, 0, 0x0080u, -1, MODEL_TYPE_LIST_EQUIPMENT, 14u),
	[OBJ_AsteroidHR5] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 3u, 10u, 0, 0x0080u, -1, MODEL_TYPE_LIST_EQUIPMENT, 15u),
	[OBJ_AsteroidHR6] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 3u, 10u, 0, 0x0080u, -1, MODEL_TYPE_LIST_EQUIPMENT, 16u),
	[OBJ_NoAsset_229] = MODEL_TYPE_ENTRY(0x00u, 0x00u, 3u, 10u, 480, 0x0080u, -1, 0xffffu, 0u),
	[OBJ_NoAsset_230] = MODEL_TYPE_ENTRY(0x00u, 0x00u, 3u, 10u, 480, 0x0080u, -1, 0xffffu, 0u),
	[OBJ_NoAsset_231] = MODEL_TYPE_ENTRY(0x00u, 0x00u, 3u, 10u, 480, 0x0080u, -1, 0xffffu, 0u),
	[OBJ_NoAsset_232] = MODEL_TYPE_ENTRY(0x00u, 0x00u, 3u, 10u, 480, 0x0080u, -1, 0xffffu, 0u),
	[OBJ_DebrisTextureGroup4000] = MODEL_TYPE_ENTRY(0x03u, 0x0Eu, 3u, 11u, 512, 0x8380u, -1, 0x0FA0u, 0u),
	[OBJ_DebrisTextureGroup4001] = MODEL_TYPE_ENTRY(0x03u, 0x0Eu, 3u, 11u, 512, 0x8380u, -1, 0x0FA1u, 0u),
	[OBJ_DebrisTextureGroup4002] = MODEL_TYPE_ENTRY(0x03u, 0x0Eu, 3u, 11u, 512, 0x8380u, -1, 0x0FA2u, 0u),
	[OBJ_DebrisTextureGroup4003] = MODEL_TYPE_ENTRY(0x03u, 0x0Eu, 3u, 11u, 512, 0x8380u, -1, 0x0FA3u, 0u),
	[OBJ_LightingEffectTextureGroup1000] =
		MODEL_TYPE_ENTRY(0x03u, 0x0Eu, 4u, 12u, 512, 0x8128u, -1, 0x03E8u, 0u),
	[OBJ_AnimationTextureGroup3051] = MODEL_TYPE_ENTRY(0x03u, 0x0Eu, 4u, 12u, 512, 0x4320u, -1, 0x0BEBu, 0u),
	[OBJ_HudTextureGroup12000] = MODEL_TYPE_ENTRY(0x03u, 0x0Eu, 4u, 12u, 512, 0x8128u, -1, 0x2EE0u, 0u),
	[OBJ_TextureGroup6000_Sprite100] =
		MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4020u, -1, 0x1770u, 100u),
	[OBJ_TextureGroup6000_Sprite200] =
		MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4020u, -1, 0x1770u, 200u),
	[OBJ_TextureGroup6000_Sprite300] =
		MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4020u, -1, 0x1770u, 300u),
	[OBJ_TextureGroup6000_Sprite400] =
		MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4020u, -1, 0x1770u, 400u),
	[OBJ_TextureGroup6000_Sprite500] =
		MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4020u, -1, 0x1770u, 500u),
	[OBJ_TextureGroup6000_Sprite600] =
		MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4020u, -1, 0x1770u, 600u),
	[OBJ_TextureGroup6000_Sprite700] =
		MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4020u, -1, 0x1770u, 700u),
	[OBJ_TextureGroup7000_Sprite100] =
		MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4020u, -1, 0x1B58u, 100u),
	[OBJ_TextureGroup7000_Sprite200] =
		MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4020u, -1, 0x1B58u, 200u),
	[OBJ_TextureGroup7000_Sprite300] =
		MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4020u, -1, 0x1B58u, 300u),
	[OBJ_TextureGroup7000_Sprite400] =
		MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4020u, -1, 0x1B58u, 400u),
	[OBJ_TextureGroup8000_Sprite100] =
		MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4020u, -1, 0x1F40u, 100u),
	[OBJ_TextureGroup8000_Sprite200] =
		MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4020u, -1, 0x1F40u, 200u),
	[OBJ_TextureGroup8000_Sprite300] =
		MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4020u, -1, 0x1F40u, 300u),
	[OBJ_TextureGroup8000_Sprite400] =
		MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4020u, -1, 0x1F40u, 400u),
	[OBJ_TextureGroup8000_Sprite500] =
		MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4020u, -1, 0x1F40u, 500u),
	[OBJ_TextureGroup8000_Sprite600] =
		MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4020u, -1, 0x1F40u, 600u),
	[OBJ_WrapBackTextureGroup19000_Sprite100] =
		MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4020u, -1, 0x4A38u, 100u),
	[OBJ_WrapBackTextureGroup18000_Sprite100] =
		MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4020u, -1, 0x4650u, 100u),
	[OBJ_BackdropTextureGroup9001_Sprite1100] =
		MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4020u, -1, 0x2329u, 1100u),
	[OBJ_BackdropTextureGroup9001_Sprite1100_260] =
		MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4020u, -1, 0x2329u, 1100u),
	[OBJ_BackdropTextureGroup9001_Sprite1100_261] =
		MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4020u, -1, 0x2329u, 1100u),
	[OBJ_BackdropTextureGroup9001_Sprite1100_262] =
		MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4020u, -1, 0x2329u, 1100u),
	[OBJ_BackdropTextureGroup9001_Sprite1100_263] =
		MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4020u, -1, 0x2329u, 1100u),
	[OBJ_ExplosionTextureGroup2000] = MODEL_TYPE_ENTRY(0x03u, 0x0Eu, 5u, 13u, 2560, 0x8140u, -1, 0x07D0u, 0u),
	[OBJ_ExplosionTextureGroup2001] = MODEL_TYPE_ENTRY(0x03u, 0x0Eu, 5u, 13u, 2560, 0x8140u, -1, 0x07D1u, 0u),
	[OBJ_ExplosionTextureGroup2002] = MODEL_TYPE_ENTRY(0x03u, 0x0Eu, 5u, 13u, 2560, 0x8140u, -1, 0x07D2u, 0u),
	[OBJ_ExplosionTextureGroup2003] = MODEL_TYPE_ENTRY(0x03u, 0x0Eu, 5u, 13u, 2560, 0x8140u, -1, 0x07D3u, 0u),
	[OBJ_ExplosionTextureGroup2004] = MODEL_TYPE_ENTRY(0x03u, 0x0Eu, 5u, 13u, 2560, 0x8140u, -1, 0x07D4u, 0u),
	[OBJ_ExplosionTextureGroup2005] = MODEL_TYPE_ENTRY(0x03u, 0x0Eu, 5u, 13u, 2560, 0x8140u, -1, 0x07D5u, 0u),
	[OBJ_ExplosionTextureGroup2006] = MODEL_TYPE_ENTRY(0x03u, 0x0Eu, 5u, 13u, 2560, 0x8140u, -1, 0x07D6u, 0u),
	[OBJ_SparkTextureGroup3000] = MODEL_TYPE_ENTRY(0x03u, 0x0Eu, 5u, 13u, 1664, 0x8140u, -1, 0x0BB8u, 0u),
	[OBJ_SparkTextureGroup3001] = MODEL_TYPE_ENTRY(0x03u, 0x0Eu, 5u, 13u, 1664, 0x8140u, -1, 0x0BB9u, 0u),
	[OBJ_SparkTextureGroup3002] = MODEL_TYPE_ENTRY(0x03u, 0x0Eu, 5u, 13u, 1280, 0x8140u, -1, 0x0BBAu, 0u),
	[OBJ_SparkTextureGroup3003] = MODEL_TYPE_ENTRY(0x03u, 0x0Eu, 5u, 13u, 1280, 0x8140u, -1, 0x0BBBu, 0u),
	[OBJ_SparkTextureGroup3004] = MODEL_TYPE_ENTRY(0x03u, 0x0Eu, 5u, 13u, 1664, 0x8348u, -1, 0x0BBCu, 0u),
	[OBJ_AnimationTextureGroup3005] = MODEL_TYPE_ENTRY(0x03u, 0x0Eu, 5u, 13u, 1664, 0x8740u, -1, 0x0BBDu, 0u),
	[OBJ_AnimationTextureGroup3006_Sprite000] =
		MODEL_TYPE_ENTRY(0x03u, 0x0Eu, 5u, 13u, 1664, 0x8548u, -1, 0x0BBEu, 0u),
	[OBJ_AnimationTextureGroup2007] = MODEL_TYPE_ENTRY(0x03u, 0x0Eu, 5u, 13u, 1664, 0x8540u, -1, 0x07D7u, 0u),
	[OBJ_AnimationTextureGroup2008] = MODEL_TYPE_ENTRY(0x03u, 0x0Eu, 5u, 13u, 1664, 0x8540u, -1, 0x07D8u, 0u),
	[OBJ_LaserRebel] =
		MODEL_TYPE_ENTRY(0x03u, 0x09u, 1u, 7u, 2048, 0x0040u, -1, MODEL_TYPE_LIST_EQUIPMENT, 17u),
	[OBJ_LaserRebelTurbo] =
		MODEL_TYPE_ENTRY(0x03u, 0x09u, 1u, 7u, 2048, 0x0040u, -1, MODEL_TYPE_LIST_EQUIPMENT, 18u),
	[OBJ_LaserImperial] =
		MODEL_TYPE_ENTRY(0x03u, 0x09u, 1u, 7u, 2048, 0x0040u, -1, MODEL_TYPE_LIST_EQUIPMENT, 19u),
	[OBJ_LaserImperialTurbo] =
		MODEL_TYPE_ENTRY(0x03u, 0x09u, 1u, 7u, 2048, 0x0040u, -1, MODEL_TYPE_LIST_EQUIPMENT, 20u),
	[OBJ_LaserIon] =
		MODEL_TYPE_ENTRY(0x03u, 0x09u, 1u, 7u, 2048, 0x0040u, -1, MODEL_TYPE_LIST_EQUIPMENT, 22u),
	[OBJ_LaserIonTurbo] =
		MODEL_TYPE_ENTRY(0x03u, 0x09u, 1u, 7u, 2048, 0x0040u, -1, MODEL_TYPE_LIST_EQUIPMENT, 23u),
	[OBJ_WarheadTorpedo] =
		MODEL_TYPE_ENTRY(0x03u, 0x09u, 1u, 7u, 2048, 0x0043u, -1, MODEL_TYPE_LIST_EQUIPMENT, 24u),
	[OBJ_WarheadMissile] =
		MODEL_TYPE_ENTRY(0x03u, 0x09u, 1u, 7u, 2048, 0x0043u, -1, MODEL_TYPE_LIST_EQUIPMENT, 25u),
	[OBJ_WarheadLaser1] =
		MODEL_TYPE_ENTRY(0x03u, 0x09u, 1u, 7u, 2048, 0x0000u, -1, MODEL_TYPE_LIST_EQUIPMENT, 18u),
	[OBJ_WarheadLaser2] =
		MODEL_TYPE_ENTRY(0x03u, 0x09u, 1u, 7u, 2048, 0x0000u, -1, MODEL_TYPE_LIST_EQUIPMENT, 20u),
	[OBJ_WarheadIon] =
		MODEL_TYPE_ENTRY(0x03u, 0x09u, 1u, 7u, 2048, 0x0000u, -1, MODEL_TYPE_LIST_EQUIPMENT, 23u),
	[OBJ_WarheadAdvancedTorpedo] =
		MODEL_TYPE_ENTRY(0x03u, 0x09u, 1u, 7u, 2048, 0x0043u, -1, MODEL_TYPE_LIST_EQUIPMENT, 24u),
	[OBJ_WarheadAdvancedMissile] =
		MODEL_TYPE_ENTRY(0x03u, 0x09u, 1u, 7u, 2048, 0x0043u, -1, MODEL_TYPE_LIST_EQUIPMENT, 25u),
	[OBJ_WarheadSpaceBomb] =
		MODEL_TYPE_ENTRY(0x03u, 0x89u, 1u, 7u, 2048, 0x0043u, -1, MODEL_TYPE_LIST_EQUIPMENT, 26u),
	[OBJ_WarheadRocket] =
		MODEL_TYPE_ENTRY(0x03u, 0x09u, 1u, 7u, 2048, 0x0043u, -1, MODEL_TYPE_LIST_EQUIPMENT, 27u),
	[OBJ_WarheadMagPulse] =
		MODEL_TYPE_ENTRY(0x03u, 0x09u, 1u, 7u, 2048, 0x0043u, -1, MODEL_TYPE_LIST_EQUIPMENT, 28u),
	[OBJ_WarheadIonPulse] =
		MODEL_TYPE_ENTRY(0x03u, 0x09u, 1u, 7u, 2048, 0x0043u, -1, MODEL_TYPE_LIST_EQUIPMENT, 28u),
	[OBJ_WarheadLaser3] =
		MODEL_TYPE_ENTRY(0x03u, 0x09u, 1u, 7u, 2048, 0x0000u, -1, MODEL_TYPE_LIST_EQUIPMENT, 18u),
	[OBJ_WarheadFlare] =
		MODEL_TYPE_ENTRY(0x03u, 0x09u, 1u, 7u, 597, 0x0043u, -1, MODEL_TYPE_LIST_EQUIPMENT, 29u),
	[OBJ_SparkTextureGroup3002_299] = MODEL_TYPE_ENTRY(0x02u, 0x0Eu, 1u, 7u, 2048, 0x8140u, -1, 0x0BBAu, 0u),
	[OBJ_ChaffTextureGroup5000] = MODEL_TYPE_ENTRY(0x03u, 0x0Eu, 5u, 13u, 1280, 0x8140u, -1, 0x1388u, 0u),
	[OBJ_LaserRebelTurbo_301] =
		MODEL_TYPE_ENTRY(0x03u, 0x09u, 1u, 7u, 2048, 0x0000u, -1, MODEL_TYPE_LIST_EQUIPMENT, 18u),
	[OBJ_LaserRebelTurbo_302] =
		MODEL_TYPE_ENTRY(0x03u, 0x09u, 1u, 7u, 2048, 0x0000u, -1, MODEL_TYPE_LIST_EQUIPMENT, 18u),
	[OBJ_LaserImperialTurbo_303] =
		MODEL_TYPE_ENTRY(0x03u, 0x09u, 1u, 7u, 2048, 0x0040u, -1, MODEL_TYPE_LIST_EQUIPMENT, 20u),
	[OBJ_LaserImperialTurbo_304] =
		MODEL_TYPE_ENTRY(0x03u, 0x09u, 1u, 7u, 2048, 0x0000u, -1, MODEL_TYPE_LIST_EQUIPMENT, 20u),
	[OBJ_LaserImperialTurbo_305] =
		MODEL_TYPE_ENTRY(0x03u, 0x09u, 1u, 7u, 2048, 0x0000u, -1, MODEL_TYPE_LIST_EQUIPMENT, 20u),
	[OBJ_LaserImperialDS] =
		MODEL_TYPE_ENTRY(0x03u, 0x09u, 1u, 7u, 0, 0x0000u, -1, MODEL_TYPE_LIST_EQUIPMENT, 21u),
	[OBJ_NoAsset_307] = MODEL_TYPE_ENTRY(0x01u, 0x00u, 1u, 7u, 2048, 0x0000u, -1, 0xffffu, 0u),
	[OBJ_Hangar] = MODEL_TYPE_ENTRY(0x03u, 0x01u, 6u, 2u, 0, 0x0000u, 249, MODEL_TYPE_LIST_EQUIPMENT, 30u),
	[OBJ_HangarCrane] =
		MODEL_TYPE_ENTRY(0x03u, 0x01u, 6u, 2u, 0, 0x0000u, 250, MODEL_TYPE_LIST_EQUIPMENT, 31u),
	[OBJ_HangarCrate] =
		MODEL_TYPE_ENTRY(0x03u, 0x01u, 6u, 2u, 0, 0x0000u, 251, MODEL_TYPE_LIST_EQUIPMENT, 32u),
	[OBJ_HangarDroid] =
		MODEL_TYPE_ENTRY(0x03u, 0x01u, 6u, 2u, 0, 0x0000u, 252, MODEL_TYPE_LIST_EQUIPMENT, 33u),
	[OBJ_HangarDroid2] =
		MODEL_TYPE_ENTRY(0x03u, 0x01u, 6u, 2u, 0, 0x0000u, 253, MODEL_TYPE_LIST_EQUIPMENT, 34u),
	[OBJ_HangarGenerator] =
		MODEL_TYPE_ENTRY(0x03u, 0x01u, 6u, 2u, 0, 0x0000u, 254, MODEL_TYPE_LIST_EQUIPMENT, 35u),
	[OBJ_HangarMonitor] =
		MODEL_TYPE_ENTRY(0x03u, 0x01u, 6u, 2u, 0, 0x0000u, 255, MODEL_TYPE_LIST_EQUIPMENT, 36u),
	[OBJ_HangarWorkStand] =
		MODEL_TYPE_ENTRY(0x03u, 0x01u, 6u, 2u, 0, 0x0000u, 256, MODEL_TYPE_LIST_EQUIPMENT, 37u),
	[OBJ_HangarRoofCrane] =
		MODEL_TYPE_ENTRY(0x03u, 0x01u, 6u, 2u, 0, 0x0000u, 257, MODEL_TYPE_LIST_EQUIPMENT, 38u),
	[OBJ_CorellianTransportGunner] =
		MODEL_TYPE_ENTRY(0x03u, 0x01u, 6u, 2u, 0, 0x0000u, 218, MODEL_TYPE_LIST_EQUIPMENT, 58u),
	[OBJ_NoTextureAsset_318] =
		MODEL_TYPE_ENTRY(0x00u, 0x00u, 6u, 14u, 0, 0x0043u, -1, MODEL_TYPE_LIST_SPACECRAFT, 0u),
	[OBJ_NoTextureAsset_319] =
		MODEL_TYPE_ENTRY(0x00u, 0x00u, 6u, 14u, 0, 0x0043u, -1, MODEL_TYPE_LIST_SPACECRAFT, 0u),
	[OBJ_DeathStarIITextureGroup17001] =
		MODEL_TYPE_ENTRY(0x03u, 0x2Eu, 4u, 12u, 480, 0x4128u, -1, 0x4269u, 0u),
	[OBJ_DeathStarIITextureGroup17002] =
		MODEL_TYPE_ENTRY(0x03u, 0x2Eu, 4u, 12u, 5000, 0x8300u, -1, 0x426Au, 0u),
	[OBJ_DSContainer] =
		MODEL_TYPE_ENTRY(0x03u, 0x29u, 0u, 0u, 0, 0x0040u, 259, MODEL_TYPE_LIST_EQUIPMENT, 59u),
	[OBJ_DSReactorCore] =
		MODEL_TYPE_ENTRY(0x03u, 0x29u, 0u, 5u, 0, 0x0043u, 263, MODEL_TYPE_LIST_EQUIPMENT, 60u),
	[OBJ_DSReactorCylinder] =
		MODEL_TYPE_ENTRY(0x03u, 0x29u, 6u, 5u, 0, 0x0080u, 264, MODEL_TYPE_LIST_EQUIPMENT, 61u),
	[OBJ_DSTripodGun] =
		MODEL_TYPE_ENTRY(0x03u, 0x29u, 0u, 18u, 0, 0x0043u, 261, MODEL_TYPE_LIST_EQUIPMENT, 62u),
	[OBJ_DSFocusLens] =
		MODEL_TYPE_ENTRY(0x03u, 0x29u, 6u, 20u, 0, 0x0040u, 258, MODEL_TYPE_LIST_EQUIPMENT, 63u),
	[OBJ_DSTankwlights] =
		MODEL_TYPE_ENTRY(0x03u, 0x29u, 6u, 15u, 0, 0x0080u, -1, MODEL_TYPE_LIST_EQUIPMENT, 64u),
	[OBJ_DSTankwlights2] =
		MODEL_TYPE_ENTRY(0x03u, 0x29u, 6u, 15u, 0, 0x0080u, -1, MODEL_TYPE_LIST_EQUIPMENT, 65u),
	[OBJ_DSBentTubeGray] =
		MODEL_TYPE_ENTRY(0x03u, 0x29u, 6u, 15u, 0, 0x0080u, -1, MODEL_TYPE_LIST_EQUIPMENT, 66u),
	[OBJ_DSTankwPipeRed01] =
		MODEL_TYPE_ENTRY(0x03u, 0x29u, 6u, 15u, 0, 0x0080u, -1, MODEL_TYPE_LIST_EQUIPMENT, 67u),
	[OBJ_DSBentTubeRed] =
		MODEL_TYPE_ENTRY(0x03u, 0x29u, 6u, 15u, 0, 0x0080u, -1, MODEL_TYPE_LIST_EQUIPMENT, 68u),
	[OBJ_DSBentTubeRedLites01] =
		MODEL_TYPE_ENTRY(0x03u, 0x29u, 6u, 15u, 0, 0x0080u, -1, MODEL_TYPE_LIST_EQUIPMENT, 69u),
	[OBJ_DSBentTubeRedLites02] =
		MODEL_TYPE_ENTRY(0x03u, 0x29u, 6u, 15u, 0, 0x0080u, -1, MODEL_TYPE_LIST_EQUIPMENT, 70u),
	[OBJ_DSBentTubeRedLites03] =
		MODEL_TYPE_ENTRY(0x03u, 0x29u, 6u, 15u, 0, 0x0080u, -1, MODEL_TYPE_LIST_EQUIPMENT, 71u),
	[OBJ_DSBentTubeRedLites04] =
		MODEL_TYPE_ENTRY(0x03u, 0x29u, 6u, 15u, 0, 0x0080u, -1, MODEL_TYPE_LIST_EQUIPMENT, 72u),
	[OBJ_DSBentTubeRedLites05] =
		MODEL_TYPE_ENTRY(0x03u, 0x29u, 6u, 15u, 0, 0x0080u, -1, MODEL_TYPE_LIST_EQUIPMENT, 73u),
	[OBJ_DSTankwPipeBlue01] =
		MODEL_TYPE_ENTRY(0x03u, 0x29u, 6u, 15u, 0, 0x0080u, -1, MODEL_TYPE_LIST_EQUIPMENT, 74u),
	[OBJ_DSBentTubeBlue01] =
		MODEL_TYPE_ENTRY(0x03u, 0x29u, 6u, 15u, 0, 0x0080u, -1, MODEL_TYPE_LIST_EQUIPMENT, 75u),
	[OBJ_DSBentTubeBlueLites01] =
		MODEL_TYPE_ENTRY(0x03u, 0x29u, 6u, 15u, 0, 0x0080u, -1, MODEL_TYPE_LIST_EQUIPMENT, 76u),
	[OBJ_DSBentTubeBlueLites02] =
		MODEL_TYPE_ENTRY(0x03u, 0x29u, 6u, 15u, 0, 0x0080u, -1, MODEL_TYPE_LIST_EQUIPMENT, 77u),
	[OBJ_DSBentTubeBlueLites03] =
		MODEL_TYPE_ENTRY(0x03u, 0x29u, 6u, 15u, 0, 0x0080u, -1, MODEL_TYPE_LIST_EQUIPMENT, 78u),
	[OBJ_DSBentTubeBlueLites04] =
		MODEL_TYPE_ENTRY(0x03u, 0x29u, 6u, 15u, 0, 0x0080u, -1, MODEL_TYPE_LIST_EQUIPMENT, 79u),
	[OBJ_DSTwoTank] = MODEL_TYPE_ENTRY(0x03u, 0x29u, 6u, 15u, 0, 0x0080u, -1, MODEL_TYPE_LIST_EQUIPMENT, 80u),
	[OBJ_DSTwoTank2] =
		MODEL_TYPE_ENTRY(0x03u, 0x29u, 6u, 15u, 0, 0x0080u, -1, MODEL_TYPE_LIST_EQUIPMENT, 81u),
	[OBJ_DSGirder] = MODEL_TYPE_ENTRY(0x03u, 0x29u, 6u, 15u, 0, 0x0080u, -1, MODEL_TYPE_LIST_EQUIPMENT, 82u),
	[OBJ_DSGirder02] =
		MODEL_TYPE_ENTRY(0x03u, 0x29u, 6u, 15u, 0, 0x0080u, -1, MODEL_TYPE_LIST_EQUIPMENT, 83u),
	[OBJ_DSGirder03] =
		MODEL_TYPE_ENTRY(0x03u, 0x29u, 6u, 15u, 0, 0x0080u, -1, MODEL_TYPE_LIST_EQUIPMENT, 84u),
	[OBJ_DSReactorCoreRoom] =
		MODEL_TYPE_ENTRY(0x03u, 0x29u, 0u, 5u, 0, 0x0080u, 262, MODEL_TYPE_LIST_EQUIPMENT, 85u),
	[OBJ_DSAccelChamber] =
		MODEL_TYPE_ENTRY(0x03u, 0x29u, 6u, 15u, 0, 0x0080u, -1, MODEL_TYPE_LIST_EQUIPMENT, 86u),
	[OBJ_DSFocusChamber] =
		MODEL_TYPE_ENTRY(0x03u, 0x29u, 6u, 15u, 0, 0x0080u, -1, MODEL_TYPE_LIST_EQUIPMENT, 87u),
	[OBJ_DS3rdRoom] = MODEL_TYPE_ENTRY(0x03u, 0x29u, 0u, 5u, 0, 0x0080u, 260, MODEL_TYPE_LIST_EQUIPMENT, 88u),
	[OBJ_DSEntrance] =
		MODEL_TYPE_ENTRY(0x03u, 0x29u, 6u, 15u, 0, 0x0080u, -1, MODEL_TYPE_LIST_EQUIPMENT, 89u),
	[OBJ_DSReactorRoomTube] =
		MODEL_TYPE_ENTRY(0x03u, 0x29u, 6u, 15u, 0, 0x0080u, -1, MODEL_TYPE_LIST_EQUIPMENT, 90u),
	[OBJ_DSAccelTube] =
		MODEL_TYPE_ENTRY(0x03u, 0x29u, 6u, 15u, 0, 0x0080u, -1, MODEL_TYPE_LIST_EQUIPMENT, 91u),
	[OBJ_DSYNotTube] =
		MODEL_TYPE_ENTRY(0x03u, 0x29u, 6u, 15u, 0, 0x0080u, -1, MODEL_TYPE_LIST_EQUIPMENT, 92u),
	[OBJ_DSAngleTube] =
		MODEL_TYPE_ENTRY(0x03u, 0x29u, 6u, 15u, 0, 0x0080u, -1, MODEL_TYPE_LIST_EQUIPMENT, 93u),
	[OBJ_DSDeadEnd] = MODEL_TYPE_ENTRY(0x03u, 0x29u, 6u, 15u, 0, 0x0080u, -1, MODEL_TYPE_LIST_EQUIPMENT, 94u),
	[OBJ_DSTubeEntry] =
		MODEL_TYPE_ENTRY(0x03u, 0x29u, 6u, 15u, 0, 0x0080u, -1, MODEL_TYPE_LIST_EQUIPMENT, 95u),
	[OBJ_DSTriRoomEntrance] =
		MODEL_TYPE_ENTRY(0x03u, 0x29u, 6u, 15u, 0, 0x0080u, -1, MODEL_TYPE_LIST_EQUIPMENT, 96u),
	[OBJ_DSTriRoom] = MODEL_TYPE_ENTRY(0x03u, 0x29u, 6u, 15u, 0, 0x0080u, -1, MODEL_TYPE_LIST_EQUIPMENT, 97u),
	[OBJ_DSTriRoomExit] =
		MODEL_TYPE_ENTRY(0x03u, 0x29u, 6u, 15u, 0, 0x0080u, -1, MODEL_TYPE_LIST_EQUIPMENT, 98u),
	[OBJ_DSBigTubeEntrance] =
		MODEL_TYPE_ENTRY(0x03u, 0x29u, 6u, 15u, 0, 0x0080u, -1, MODEL_TYPE_LIST_EQUIPMENT, 99u),
	[OBJ_DSBigTube] =
		MODEL_TYPE_ENTRY(0x03u, 0x29u, 6u, 15u, 0, 0x0080u, -1, MODEL_TYPE_LIST_EQUIPMENT, 100u),
	[OBJ_DSBigTubeExit] =
		MODEL_TYPE_ENTRY(0x03u, 0x29u, 6u, 15u, 0, 0x0080u, -1, MODEL_TYPE_LIST_EQUIPMENT, 101u),
	[OBJ_DSSquareTube01] =
		MODEL_TYPE_ENTRY(0x03u, 0x29u, 6u, 15u, 0, 0x0080u, -1, MODEL_TYPE_LIST_EQUIPMENT, 102u),
	[OBJ_DSBasement] =
		MODEL_TYPE_ENTRY(0x03u, 0x29u, 6u, 15u, 0, 0x0080u, -1, MODEL_TYPE_LIST_EQUIPMENT, 103u),
	[OBJ_DSBasement01] =
		MODEL_TYPE_ENTRY(0x03u, 0x29u, 6u, 15u, 0, 0x0080u, -1, MODEL_TYPE_LIST_EQUIPMENT, 104u),
	[OBJ_DSBasement02] =
		MODEL_TYPE_ENTRY(0x03u, 0x29u, 6u, 15u, 0, 0x0080u, -1, MODEL_TYPE_LIST_EQUIPMENT, 105u),
	[OBJ_DSAlphaWalls] =
		MODEL_TYPE_ENTRY(0x03u, 0x29u, 6u, 15u, 0, 0x0080u, -1, MODEL_TYPE_LIST_EQUIPMENT, 106u),
	[OBJ_DSAlphaWalls01] =
		MODEL_TYPE_ENTRY(0x03u, 0x29u, 6u, 15u, 0, 0x0080u, -1, MODEL_TYPE_LIST_EQUIPMENT, 107u),
	[OBJ_DSAlphaWalls02] =
		MODEL_TYPE_ENTRY(0x03u, 0x29u, 6u, 15u, 0, 0x0080u, -1, MODEL_TYPE_LIST_EQUIPMENT, 108u),
	[OBJ_DS3LayerTube] =
		MODEL_TYPE_ENTRY(0x03u, 0x29u, 6u, 15u, 0, 0x0080u, -1, MODEL_TYPE_LIST_EQUIPMENT, 109u),
	[OBJ_DSBoxInBox1] =
		MODEL_TYPE_ENTRY(0x03u, 0x29u, 6u, 15u, 0, 0x0080u, -1, MODEL_TYPE_LIST_EQUIPMENT, 110u),
	[OBJ_DSBoxInBox2] =
		MODEL_TYPE_ENTRY(0x03u, 0x29u, 6u, 15u, 0, 0x0080u, -1, MODEL_TYPE_LIST_EQUIPMENT, 111u),
	[OBJ_DSBoxInBox3] =
		MODEL_TYPE_ENTRY(0x03u, 0x29u, 6u, 15u, 0, 0x0080u, -1, MODEL_TYPE_LIST_EQUIPMENT, 112u),
	[OBJ_ChuteMouth] =
		MODEL_TYPE_ENTRY(0x03u, 0x41u, 6u, 21u, 0, 0x0040u, 219, MODEL_TYPE_LIST_EQUIPMENT, 115u),
	[OBJ_ChuteTunnel] =
		MODEL_TYPE_ENTRY(0x03u, 0x41u, 6u, 21u, 0, 0x0040u, 220, MODEL_TYPE_LIST_EQUIPMENT, 116u),
	[OBJ_SalvageRoom] =
		MODEL_TYPE_ENTRY(0x03u, 0x41u, 6u, 21u, 0, 0x0040u, 221, MODEL_TYPE_LIST_EQUIPMENT, 117u),
	[OBJ_Compactor] =
		MODEL_TYPE_ENTRY(0x03u, 0x41u, 6u, 14u, 0, 0x0040u, 222, MODEL_TYPE_LIST_EQUIPMENT, 118u),
	[OBJ_AccelRing] =
		MODEL_TYPE_ENTRY(0x03u, 0x49u, 6u, 21u, 0, 0x0043u, 223, MODEL_TYPE_LIST_EQUIPMENT, 119u),
	[OBJ_AccelRing2] =
		MODEL_TYPE_ENTRY(0x03u, 0x41u, 6u, 14u, 0, 0x0043u, 224, MODEL_TYPE_LIST_EQUIPMENT, 120u),
	[OBJ_SmeltingRoom] =
		MODEL_TYPE_ENTRY(0x03u, 0x41u, 6u, 14u, 0, 0x0040u, 225, MODEL_TYPE_LIST_EQUIPMENT, 121u),
	[OBJ_SRTubeNOBend] =
		MODEL_TYPE_ENTRY(0x03u, 0x41u, 6u, 21u, 0, 0x0040u, 226, MODEL_TYPE_LIST_EQUIPMENT, 122u),
	[OBJ_SRTubeUP] =
		MODEL_TYPE_ENTRY(0x03u, 0x41u, 6u, 21u, 0, 0x0040u, 227, MODEL_TYPE_LIST_EQUIPMENT, 123u),
	[OBJ_SRTubeDown] =
		MODEL_TYPE_ENTRY(0x03u, 0x41u, 6u, 21u, 0, 0x0040u, 228, MODEL_TYPE_LIST_EQUIPMENT, 124u),
	[OBJ_SRTubeLH] =
		MODEL_TYPE_ENTRY(0x03u, 0x41u, 6u, 21u, 0, 0x0040u, 229, MODEL_TYPE_LIST_EQUIPMENT, 125u),
	[OBJ_SRTubeRH] =
		MODEL_TYPE_ENTRY(0x03u, 0x41u, 6u, 21u, 0, 0x0040u, 230, MODEL_TYPE_LIST_EQUIPMENT, 126u),
	[OBJ_Centrifuge] =
		MODEL_TYPE_ENTRY(0x03u, 0x41u, 6u, 14u, 0, 0x0040u, 231, MODEL_TYPE_LIST_EQUIPMENT, 127u),
	[OBJ_AccelRing3] =
		MODEL_TYPE_ENTRY(0x03u, 0x41u, 6u, 14u, 0, 0x0043u, 232, MODEL_TYPE_LIST_EQUIPMENT, 128u),
	[OBJ_ContainerGrandePG] =
		MODEL_TYPE_ENTRY(0x03u, 0x41u, 6u, 21u, 0, 0x0043u, 233, MODEL_TYPE_LIST_EQUIPMENT, 129u),
	[OBJ_Asteroid01] =
		MODEL_TYPE_ENTRY(0x03u, 0x41u, 6u, 21u, 0, 0x0040u, 234, MODEL_TYPE_LIST_EQUIPMENT, 130u),
	[OBJ_Asteroid02] =
		MODEL_TYPE_ENTRY(0x03u, 0x41u, 6u, 21u, 0, 0x0040u, 235, MODEL_TYPE_LIST_EQUIPMENT, 131u),
	[OBJ_Asteroid03] =
		MODEL_TYPE_ENTRY(0x03u, 0x41u, 6u, 14u, 0, 0x0040u, 236, MODEL_TYPE_LIST_EQUIPMENT, 132u),
	[OBJ_Junk01] = MODEL_TYPE_ENTRY(0x03u, 0x41u, 6u, 21u, 0, 0x0040u, 237, MODEL_TYPE_LIST_EQUIPMENT, 133u),
	[OBJ_Junk02] = MODEL_TYPE_ENTRY(0x03u, 0x41u, 6u, 21u, 0, 0x0040u, 238, MODEL_TYPE_LIST_EQUIPMENT, 134u),
	[OBJ_Junk03] = MODEL_TYPE_ENTRY(0x03u, 0x41u, 6u, 21u, 0, 0x0040u, 239, MODEL_TYPE_LIST_EQUIPMENT, 135u),
	[OBJ_Junk04] = MODEL_TYPE_ENTRY(0x03u, 0x41u, 6u, 21u, 0, 0x0040u, 240, MODEL_TYPE_LIST_EQUIPMENT, 136u),
	[OBJ_Junk05] = MODEL_TYPE_ENTRY(0x03u, 0x41u, 6u, 21u, 0, 0x0040u, 241, MODEL_TYPE_LIST_EQUIPMENT, 137u),
	[OBJ_Junk06] = MODEL_TYPE_ENTRY(0x03u, 0x41u, 6u, 21u, 0, 0x0040u, 242, MODEL_TYPE_LIST_EQUIPMENT, 138u),
	[OBJ_Junk07] = MODEL_TYPE_ENTRY(0x03u, 0x41u, 6u, 21u, 0, 0x0040u, 243, MODEL_TYPE_LIST_EQUIPMENT, 139u),
	[OBJ_Junk08] = MODEL_TYPE_ENTRY(0x03u, 0x41u, 6u, 21u, 0, 0x0040u, 244, MODEL_TYPE_LIST_EQUIPMENT, 140u),
	[OBJ_Junk09] = MODEL_TYPE_ENTRY(0x03u, 0x41u, 6u, 21u, 0, 0x0040u, 245, MODEL_TYPE_LIST_EQUIPMENT, 141u),
	[OBJ_Junk10] = MODEL_TYPE_ENTRY(0x03u, 0x41u, 6u, 21u, 0, 0x0040u, 246, MODEL_TYPE_LIST_EQUIPMENT, 142u),
	[OBJ_JunkBlock] =
		MODEL_TYPE_ENTRY(0x03u, 0x41u, 6u, 21u, 0, 0x0043u, 247, MODEL_TYPE_LIST_EQUIPMENT, 143u),
	[OBJ_MoltenBlock] =
		MODEL_TYPE_ENTRY(0x03u, 0x41u, 6u, 21u, 0, 0x0043u, 248, MODEL_TYPE_LIST_EQUIPMENT, 144u),
	[OBJ_Rubble01] = MODEL_TYPE_ENTRY(0x03u, 0x09u, 3u, 20u, 0, 0x0040u, -1, MODEL_TYPE_LIST_EQUIPMENT, 145u),
	[OBJ_Rubble02] = MODEL_TYPE_ENTRY(0x03u, 0x09u, 3u, 20u, 0, 0x0040u, -1, MODEL_TYPE_LIST_EQUIPMENT, 146u),
	[OBJ_Rubble03] = MODEL_TYPE_ENTRY(0x03u, 0x09u, 3u, 20u, 0, 0x0040u, -1, MODEL_TYPE_LIST_EQUIPMENT, 147u),
	[OBJ_Rubble04] = MODEL_TYPE_ENTRY(0x03u, 0x09u, 3u, 20u, 0, 0x0040u, -1, MODEL_TYPE_LIST_EQUIPMENT, 148u),
	[OBJ_Rubble05] = MODEL_TYPE_ENTRY(0x03u, 0x09u, 3u, 20u, 0, 0x0040u, -1, MODEL_TYPE_LIST_EQUIPMENT, 149u),
	[OBJ_Rubble06] = MODEL_TYPE_ENTRY(0x03u, 0x09u, 3u, 20u, 0, 0x0040u, -1, MODEL_TYPE_LIST_EQUIPMENT, 150u),
	[OBJ_Rubble07] = MODEL_TYPE_ENTRY(0x03u, 0x09u, 3u, 20u, 0, 0x0040u, -1, MODEL_TYPE_LIST_EQUIPMENT, 151u),
	[OBJ_Rubble08] = MODEL_TYPE_ENTRY(0x03u, 0x09u, 3u, 20u, 0, 0x0040u, -1, MODEL_TYPE_LIST_EQUIPMENT, 152u),
	[OBJ_Rubble09] = MODEL_TYPE_ENTRY(0x03u, 0x09u, 3u, 20u, 0, 0x0040u, -1, MODEL_TYPE_LIST_EQUIPMENT, 153u),
	[OBJ_Rubble10] = MODEL_TYPE_ENTRY(0x03u, 0x09u, 3u, 20u, 0, 0x0040u, -1, MODEL_TYPE_LIST_EQUIPMENT, 154u),
	[OBJ_Rubble11] = MODEL_TYPE_ENTRY(0x03u, 0x09u, 3u, 20u, 0, 0x0040u, -1, MODEL_TYPE_LIST_EQUIPMENT, 155u),
	[OBJ_Rubble12] = MODEL_TYPE_ENTRY(0x03u, 0x09u, 3u, 20u, 0, 0x0040u, -1, MODEL_TYPE_LIST_EQUIPMENT, 156u),
	[OBJ_FontTextureGroup16000] = MODEL_TYPE_ENTRY(0x03u, 0x0Eu, 4u, 12u, 512, 0x8128u, -1, 0x3E80u, 0u),
	[OBJ_BlastMarkTextureGroup3050] = MODEL_TYPE_ENTRY(0x03u, 0x0Eu, 5u, 13u, 1664, 0x8548u, -1, 0x0BEAu, 0u),
	[OBJ_HudTextureGroup13000_Sprite000] =
		MODEL_TYPE_ENTRY(0x03u, 0x0Eu, 4u, 12u, 512, 0x8128u, -1, 0x32C8u, 0u),
	[OBJ_WrapBackTextureGroup18000] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4128u, -1, 0x4650u, 0u),
	[OBJ_WrapBackTextureGroup19000] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4128u, -1, 0x4A38u, 0u),
	[OBJ_WrapBackTextureGroup19100] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4128u, -1, 0x4A9Cu, 0u),
	[OBJ_WrapBackTextureGroup19200] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4128u, -1, 0x4B00u, 0u),
	[OBJ_WrapBackTextureGroup19300] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4128u, -1, 0x4B64u, 0u),
	[OBJ_WrapBackTextureGroup19400] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4128u, -1, 0x4BC8u, 0u),
	[OBJ_PlanetTextureGroup6010] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4120u, -1, 0x177Au, 0u),
	[OBJ_PlanetTextureGroup6011] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4120u, -1, 0x177Bu, 0u),
	[OBJ_PlanetTextureGroup6012] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4120u, -1, 0x177Cu, 0u),
	[OBJ_PlanetTextureGroup6013] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4120u, -1, 0x177Du, 0u),
	[OBJ_PlanetTextureGroup6014] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4120u, -1, 0x177Eu, 0u),
	[OBJ_PlanetTextureGroup6020] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4120u, -1, 0x1784u, 0u),
	[OBJ_PlanetTextureGroup6021] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4120u, -1, 0x1785u, 0u),
	[OBJ_PlanetTextureGroup6022] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4120u, -1, 0x1786u, 0u),
	[OBJ_PlanetTextureGroup6023] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4120u, -1, 0x1787u, 0u),
	[OBJ_PlanetTextureGroup6024] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4120u, -1, 0x1788u, 0u),
	[OBJ_PlanetTextureGroup6025] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4120u, -1, 0x1789u, 0u),
	[OBJ_PlanetTextureGroup6026] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4120u, -1, 0x178Au, 0u),
	[OBJ_PlanetTextureGroup6027] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4120u, -1, 0x178Bu, 0u),
	[OBJ_PlanetTextureGroup6028] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4120u, -1, 0x178Cu, 0u),
	[OBJ_PlanetTextureGroup6029] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4120u, -1, 0x178Du, 0u),
	[OBJ_PlanetTextureGroup6030] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4120u, -1, 0x178Eu, 0u),
	[OBJ_PlanetTextureGroup6031] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4120u, -1, 0x178Fu, 0u),
	[OBJ_PlanetTextureGroup6032] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4120u, -1, 0x1790u, 0u),
	[OBJ_PlanetTextureGroup6033] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4120u, -1, 0x1791u, 0u),
	[OBJ_PlanetTextureGroup6034] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4120u, -1, 0x1792u, 0u),
	[OBJ_PlanetTextureGroup6040] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4120u, -1, 0x1798u, 0u),
	[OBJ_PlanetTextureGroup6041] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4120u, -1, 0x1799u, 0u),
	[OBJ_PlanetTextureGroup6042_Sprite000] =
		MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4120u, -1, 0x179Au, 0u),
	[OBJ_PlanetTextureGroup6043] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4120u, -1, 0x179Bu, 0u),
	[OBJ_TextureGroup6050_Sprite000] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4120u, -1, 0x17A2u, 0u),
	[OBJ_PlanetTextureGroup6060] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4120u, -1, 0x17ACu, 0u),
	[OBJ_PlanetTextureGroup6061] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4120u, -1, 0x17ADu, 0u),
	[OBJ_PlanetTextureGroup6062] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4120u, -1, 0x17AEu, 0u),
	[OBJ_PlanetTextureGroup6063] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4120u, -1, 0x17AFu, 0u),
	[OBJ_PlanetTextureGroup6064] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4120u, -1, 0x17B0u, 0u),
	[OBJ_PlanetTextureGroup6070] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4120u, -1, 0x17B6u, 0u),
	[OBJ_PlanetTextureGroup6071] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4120u, -1, 0x17B7u, 0u),
	[OBJ_PlanetTextureGroup6072] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4120u, -1, 0x17B8u, 0u),
	[OBJ_PlanetTextureGroup6073] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4120u, -1, 0x17B9u, 0u),
	[OBJ_PlanetTextureGroup6074] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4120u, -1, 0x17BAu, 0u),
	[OBJ_PlanetTextureGroup6075] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4120u, -1, 0x17BBu, 0u),
	[OBJ_PlanetTextureGroup6076] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4120u, -1, 0x17BCu, 0u),
	[OBJ_PlanetTextureGroup6077] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4120u, -1, 0x17BDu, 0u),
	[OBJ_PlanetTextureGroup6078] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4120u, -1, 0x17BEu, 0u),
	[OBJ_PlanetTextureGroup6079] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4120u, -1, 0x17BFu, 0u),
	[OBJ_PlanetTextureGroup6080] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4120u, -1, 0x17C0u, 0u),
	[OBJ_PlanetTextureGroup6081] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4120u, -1, 0x17C1u, 0u),
	[OBJ_PlanetTextureGroup6082] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4120u, -1, 0x17C2u, 0u),
	[OBJ_PlanetTextureGroup6083] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4120u, -1, 0x17C3u, 0u),
	[OBJ_PlanetTextureGroup6084] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4120u, -1, 0x17C4u, 0u),
	[OBJ_PlanetTextureGroup6090] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4120u, -1, 0x17CAu, 0u),
	[OBJ_PlanetTextureGroup6091] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4120u, -1, 0x17CBu, 0u),
	[OBJ_PlanetTextureGroup6092] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4120u, -1, 0x17CCu, 0u),
	[OBJ_PlanetTextureGroup6093] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4120u, -1, 0x17CDu, 0u),
	[OBJ_PlanetTextureGroup6094] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4120u, -1, 0x17CEu, 0u),
	[OBJ_PlanetTextureGroup6100] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4120u, -1, 0x17D4u, 0u),
	[OBJ_PlanetTextureGroup6101] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4120u, -1, 0x17D5u, 0u),
	[OBJ_PlanetTextureGroup6102] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4120u, -1, 0x17D6u, 0u),
	[OBJ_PlanetTextureGroup6103] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4120u, -1, 0x17D7u, 0u),
	[OBJ_PlanetTextureGroup6104] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4120u, -1, 0x17D8u, 0u),
	[OBJ_PlanetTextureGroup6110] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4120u, -1, 0x17DEu, 0u),
	[OBJ_PlanetTextureGroup6111] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4120u, -1, 0x17DFu, 0u),
	[OBJ_PlanetTextureGroup6112] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4120u, -1, 0x17E0u, 0u),
	[OBJ_PlanetTextureGroup6113] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4120u, -1, 0x17E1u, 0u),
	[OBJ_PlanetTextureGroup6114] = MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4120u, -1, 0x17E2u, 0u),
	[OBJ_DeathStarFireTextureGroup6250_Sprite000] =
		MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4120u, -1, 0x186Au, 0u),
	[OBJ_DeathStarFireTextureGroup6251] =
		MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 120, 0x8120u, -1, 0x186Bu, 0u),
	[OBJ_WorkDroid1] =
		MODEL_TYPE_ENTRY(0x03u, 0x01u, 6u, 2u, 0, 0x0040u, -1, MODEL_TYPE_LIST_EQUIPMENT, 157u),
	[OBJ_WorkDroid2] =
		MODEL_TYPE_ENTRY(0x03u, 0x01u, 6u, 2u, 0, 0x0040u, -1, MODEL_TYPE_LIST_EQUIPMENT, 158u),
	[OBJ_CorvetteExplode] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 3u, 0, 0x0040u, -1, MODEL_TYPE_LIST_SPACECRAFT, 144u),
	[OBJ_CorvetteExplode_492] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 3u, 0, 0x0040u, -1, MODEL_TYPE_LIST_SPACECRAFT, 145u),
	[OBJ_CorvetteExplode_493] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 3u, 0, 0x0040u, -1, MODEL_TYPE_LIST_SPACECRAFT, 146u),
	[OBJ_CorvetteExplode_494] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 3u, 0, 0x0040u, -1, MODEL_TYPE_LIST_SPACECRAFT, 147u),
	[OBJ_CorvetteExplode_495] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 3u, 0, 0x0040u, -1, MODEL_TYPE_LIST_SPACECRAFT, 148u),
	[OBJ_CorvetteExplode_496] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 3u, 0, 0x0040u, -1, MODEL_TYPE_LIST_SPACECRAFT, 149u),
	[OBJ_CorvetteExplode_497] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 3u, 0, 0x0040u, -1, MODEL_TYPE_LIST_SPACECRAFT, 150u),
	[OBJ_CorvetteExplode_498] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 3u, 0, 0x0040u, -1, MODEL_TYPE_LIST_SPACECRAFT, 151u),
	[OBJ_CorvetteExplode_499] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 3u, 0, 0x0040u, -1, MODEL_TYPE_LIST_SPACECRAFT, 152u),
	[OBJ_EscapePodA_500] =
		MODEL_TYPE_ENTRY(0x03u, 0x81u, 0u, 3u, 0, 0x0040u, -1, MODEL_TYPE_LIST_SPACECRAFT, 153u),
	[OBJ_NebulaTextureGroup7001_Sprite100] =
		MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4020u, -1, 0x1B59u, 100u),
	[OBJ_NebulaTextureGroup7002_Sprite200] =
		MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4020u, -1, 0x1B5Au, 200u),
	[OBJ_NebulaTextureGroup7003_Sprite300] =
		MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4020u, -1, 0x1B5Bu, 300u),
	[OBJ_NebulaTextureGroup7004_Sprite400] =
		MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4020u, -1, 0x1B5Cu, 400u),
	[OBJ_NebulaTextureGroup7005_Sprite500] =
		MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4020u, -1, 0x1B5Du, 500u),
	[OBJ_NebulaTextureGroup7006_Sprite600] =
		MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4020u, -1, 0x1B5Eu, 600u),
	[OBJ_NebulaTextureGroup7007_Sprite700] =
		MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4020u, -1, 0x1B5Fu, 700u),
	[OBJ_NebulaTextureGroup7008_Sprite800] =
		MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4020u, -1, 0x1B60u, 800u),
	[OBJ_NebulaTextureGroup7009_Sprite900] =
		MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4020u, -1, 0x1B61u, 900u),
	[OBJ_NebulaTextureGroup7010_Sprite1000] =
		MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4020u, -1, 0x1B62u, 1000u),
	[OBJ_GalaxyTextureGroup8001_Sprite100] =
		MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4020u, -1, 0x1F41u, 100u),
	[OBJ_GalaxyTextureGroup8002_Sprite200] =
		MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4020u, -1, 0x1F42u, 200u),
	[OBJ_GalaxyTextureGroup8003_Sprite300] =
		MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4020u, -1, 0x1F43u, 300u),
	[OBJ_GalaxyTextureGroup8004_Sprite400] =
		MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4020u, -1, 0x1F44u, 400u),
	[OBJ_GalaxyTextureGroup8005_Sprite500] =
		MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4020u, -1, 0x1F45u, 500u),
	[OBJ_GalaxyTextureGroup8006_Sprite600] =
		MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4020u, -1, 0x1F46u, 600u),
	[OBJ_GalaxyTextureGroup8007_Sprite700] =
		MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4020u, -1, 0x1F47u, 700u),
	[OBJ_GalaxyTextureGroup8008_Sprite800] =
		MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4020u, -1, 0x1F48u, 800u),
	[OBJ_GalaxyTextureGroup8009_Sprite900] =
		MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4020u, -1, 0x1F49u, 900u),
	[OBJ_GalaxyTextureGroup8010_Sprite1000] =
		MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4020u, -1, 0x1F4Au, 1000u),
	[OBJ_BackdropTextureGroup9001_Sprite1100_521] =
		MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4020u, -1, 0x2329u, 1100u),
	[OBJ_BackdropTextureGroup9002_Sprite1200] =
		MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4020u, -1, 0x232Au, 1200u),
	[OBJ_BackdropTextureGroup9003_Sprite1300] =
		MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4020u, -1, 0x232Bu, 1300u),
	[OBJ_BackdropTextureGroup9004_Sprite1400] =
		MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4020u, -1, 0x232Cu, 1400u),
	[OBJ_BackdropTextureGroup9005_Sprite1500] =
		MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4020u, -1, 0x232Du, 1500u),
	[OBJ_BackdropTextureGroup9006_Sprite1600] =
		MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4020u, -1, 0x232Eu, 1600u),
	[OBJ_BackdropTextureGroup9007_Sprite1700] =
		MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4020u, -1, 0x232Fu, 1700u),
	[OBJ_BackdropTextureGroup9008_Sprite1800] =
		MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4020u, -1, 0x2330u, 1800u),
	[OBJ_BackdropTextureGroup9009_Sprite1900] =
		MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4020u, -1, 0x2331u, 1900u),
	[OBJ_BackdropTextureGroup9010_Sprite2000] =
		MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4020u, -1, 0x2332u, 2000u),
	[OBJ_BackdropTextureGroup9100_Sprite5100] =
		MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4020u, -1, 0x238Cu, 5100u),
	[OBJ_NebulaTextureGroup7011_Sprite1100] =
		MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4020u, -1, 0x1B63u, 1100u),
	[OBJ_NebulaTextureGroup7012_Sprite1200] =
		MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4020u, -1, 0x1B64u, 1200u),
	[OBJ_NebulaTextureGroup7013_Sprite1300] =
		MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4020u, -1, 0x1B65u, 1300u),
	[OBJ_NebulaTextureGroup7014_Sprite1400] =
		MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4020u, -1, 0x1B66u, 1400u),
	[OBJ_NebulaTextureGroup7015_Sprite1500] =
		MODEL_TYPE_ENTRY(0x03u, 0x06u, 4u, 12u, 480, 0x4020u, -1, 0x1B67u, 1500u),
	[OBJ_MapIconTextureGroup14800] = MODEL_TYPE_ENTRY(0x03u, 0x0Eu, 4u, 12u, 512, 0x8120u, -1, 0x39D0u, 0u),
	[OBJ_TrailTextureGroup21000_Sprite000] =
		MODEL_TYPE_ENTRY(0x03u, 0x0Eu, 3u, 11u, 512, 0x8148u, -1, 0x5208u, 0u),
	[OBJ_TrailTextureGroup21005] = MODEL_TYPE_ENTRY(0x03u, 0x0Eu, 3u, 11u, 512, 0x8148u, -1, 0x520Du, 0u),
	[OBJ_TrailTextureGroup21010] = MODEL_TYPE_ENTRY(0x03u, 0x0Eu, 3u, 11u, 512, 0x8148u, -1, 0x5212u, 0u),
	[OBJ_TrailTextureGroup21015] = MODEL_TYPE_ENTRY(0x03u, 0x0Eu, 3u, 11u, 512, 0x8148u, -1, 0x5217u, 0u),
	[OBJ_TrailTextureGroup21020] = MODEL_TYPE_ENTRY(0x03u, 0x0Eu, 3u, 11u, 512, 0x8148u, -1, 0x521Cu, 0u),
	[OBJ_TrailTextureGroup21025] = MODEL_TYPE_ENTRY(0x03u, 0x0Eu, 3u, 11u, 512, 0x8148u, -1, 0x5221u, 0u),
	[OBJ_ParticleTextureGroup22000] = MODEL_TYPE_ENTRY(0x03u, 0x0Eu, 3u, 11u, 512, 0x8148u, -1, 0x55F0u, 0u),
	[OBJ_ParticleTextureGroup22003] = MODEL_TYPE_ENTRY(0x03u, 0x0Eu, 3u, 11u, 512, 0x8148u, -1, 0x55F3u, 0u),
	[OBJ_ParticleTextureGroup22005] = MODEL_TYPE_ENTRY(0x03u, 0x0Eu, 3u, 11u, 512, 0x8148u, -1, 0x55F5u, 0u),
	[OBJ_ParticleTextureGroup22007] = MODEL_TYPE_ENTRY(0x03u, 0x0Eu, 3u, 11u, 512, 0x8148u, -1, 0x55F7u, 0u),
	[OBJ_AnimationTextureGroup3055] = MODEL_TYPE_ENTRY(0x03u, 0x0Eu, 5u, 13u, 1664, 0x8348u, -1, 0x0BEFu, 0u),
	[OBJ_HullIconTextureGroup26000] = MODEL_TYPE_ENTRY(0x03u, 0x0Eu, 4u, 12u, 512, 0x8120u, -1, 0x6590u, 0u),
	[OBJ_DSLaserInternal] =
		MODEL_TYPE_ENTRY(0x03u, 0x29u, 1u, 7u, 0, 0x0000u, -1, MODEL_TYPE_LIST_EQUIPMENT, 113u),
	[OBJ_AnimationTextureGroup3200] = MODEL_TYPE_ENTRY(0x03u, 0x0Eu, 5u, 13u, 1664, 0x8548u, -1, 0x0C80u, 0u),
	[OBJ_AnimationTextureGroup3100] = MODEL_TYPE_ENTRY(0x03u, 0x0Eu, 5u, 13u, 1664, 0x8548u, -1, 0x0C1Cu, 0u),
	[OBJ_AnimationTextureGroup3300] = MODEL_TYPE_ENTRY(0x03u, 0x0Eu, 5u, 13u, 1664, 0x8548u, -1, 0x0CE4u, 0u),
	[OBJ_AnimationTextureGroup3400] = MODEL_TYPE_ENTRY(0x03u, 0x0Eu, 5u, 13u, 1664, 0x8548u, -1, 0x0D48u, 0u),
	[OBJ_AnimationTextureGroup3500] = MODEL_TYPE_ENTRY(0x03u, 0x0Eu, 5u, 13u, 1664, 0x8548u, -1, 0x0DACu, 0u),
	[OBJ_DSLaserPowerSource] =
		MODEL_TYPE_ENTRY(0x03u, 0x29u, 0u, 15u, 0, 0x0083u, -1, MODEL_TYPE_LIST_EQUIPMENT, 114u),
};

#undef MODEL_TYPE_ENTRY

// FUNCTION: XWA 0x4DCE30
ModelIndex GetModelIndexFromType(uint16_t objectType) {
	if ((uint16_t)objectType >= OBJ_Count) {
		return -1;
	}

	return g_modelTypeTable[(uint16_t)objectType].modelIndex;
}
