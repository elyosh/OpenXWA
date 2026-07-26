#include "xwa/assets/flight_model.h"
#include "xwa/flight/hangar.h"

#include "xwa/assets/file_io.h"
#include "xwa/assets/model_def.h"
#include "xwa/assets/model_mesh.h"
#include "xwa/assets/model_texture.h"
#include "xwa/assets/model_type.h"
#include "xwa/assets/object_type.h"
#include "xwa/assets/opt_model.h"
#include "xwa/flight/fediskio.h"
#include "xwa/flight/flight.h"
#include "xwa/frontend/flight_loading.h"
#include "xwa/frontend/frontend_display.h"
#include "xwa/frontend/mission_setup.h"
#include "xwa/flight/mission/mission.h"
#include "xwa/net/net.h"
#include "xwa/flight/player/player.h"
#include "xwa/render/renderer.h"
#include "xwa/render/renderer_internal.h"
#include "xwa/util/debug.h"
#include "xwa/util/memory.h"

#include <stdint.h>
#include <string.h>

// FUNCTION: XWA 0x422B50
int Craft_FindCraftTypeForObjectType(unsigned short objectType) {
	int i;

	for (i = 0; i < XWA_CRAFT_TYPE_TO_OBJECT_TYPE_COUNT; ++i) {
		if (g_objectTypeTables.craftTypeToObjectType[i] == objectType) {
			return i;
		}
	}

	return 0;
}

// FUNCTION: XWA 0x490E70
int Craft_GetObjectMaxShield(unsigned short objIdx) {
	CraftData* craft;

	craft = g_objectTable[(uint16_t)objIdx].mobj->pCraft;
	return 2 * g_modelDefs[craft->modelIndex].shieldStrength;
}

// FUNCTION: XWA 0x456FA0
int16_t Hangar_LoadCraftModelByType(uint16_t objectType) {
	char modelName[100];
	uint16_t modelHandle;
	char path[256];
	uint16_t modelType;
	TexLevel* texLevel;
	int meshCount;
	int meshIndex;

	g_loadingModel = 1;
	modelType = (uint16_t)objectType;

	switch (modelType) {
		case OBJ_AWing:
			strcpy(modelName, "Awing.opt");
			break;
		case OBJ_XWing:
			strcpy(modelName, "Xwing.opt");
			break;
		case OBJ_YWing:
			strcpy(modelName, "Ywing.opt");
			break;
		case OBJ_BWing:
			strcpy(modelName, "Bwing.opt");
			break;
		case OBJ_Z95:
			strcpy(modelName, "Z-95.opt");
			break;
		case OBJ_Tug:
			strcpy(modelName, "Tug.opt");
			break;
		case OBJ_Shuttle:
			strcpy(modelName, "Shuttle.opt");
			break;
		case OBJ_CorellianTransport2:
			strcpy(modelName, "CorellianTransport2.opt");
			break;
		case OBJ_MilleniumFalcon2:
			strcpy(modelName, "MilleniumFalcon2.opt");
			break;
		case OBJ_FamilyTransport:
			strcpy(modelName, "FamilyTransport.opt");
			break;
		case OBJ_CargoCanister:
			strcpy(modelName, "CargoCanister.opt");
			break;
		case OBJ_ContainerBox:
			strcpy(modelName, "ContainerBox.opt");
			break;
		case OBJ_ContainerSphere:
			strcpy(modelName, "ContainerSphere.opt");
			break;
		case OBJ_FamilyBase:
			strcpy(modelName, "FamilyBase.opt");
			break;
		case OBJ_Hangar:
			strcpy(modelName, "Hangar.opt");
			break;
		case OBJ_HangarCrane:
			strcpy(modelName, "HangarCrane.opt");
			break;
		case OBJ_HangarCrate:
			strcpy(modelName, "HangarCrate.opt");
			break;
		case OBJ_HangarDroid:
			strcpy(modelName, "HangarDroid.opt");
			break;
		case OBJ_HangarDroid2:
			strcpy(modelName, "HangarDroid2.opt");
			break;
		case OBJ_HangarGenerator:
			strcpy(modelName, "HangarGenerator.opt");
			break;
		case OBJ_HangarMonitor:
			strcpy(modelName, "HangarMonitor.opt");
			break;
		case OBJ_HangarWorkStand:
			strcpy(modelName, "HangarWorkstand.opt");
			break;
		case OBJ_HangarRoofCrane:
			strcpy(modelName, "HangarRoofCrane.opt");
			break;
		case OBJ_WorkDroid1:
			strcpy(modelName, "WorkDroid1.opt");
			break;
		case OBJ_WorkDroid2:
			strcpy(modelName, "WorkDroid2.opt");
			break;
		case OBJ_CorellianTransportGunner:
			strcpy(modelName, "CorellianTransportGunner.opt");
			break;
		case OBJ_ChuteMouth:
			strcpy(modelName, "ChuteMouth.opt");
			break;
		case OBJ_ChuteTunnel:
			strcpy(modelName, "ChuteTunnel.opt");
			break;
		case OBJ_SalvageRoom:
			strcpy(modelName, "SalvageRoom.opt");
			break;
		case OBJ_Compactor:
			strcpy(modelName, "Compactor.opt");
			break;
		case OBJ_AccelRing:
			strcpy(modelName, "AccelRing.opt");
			break;
		case OBJ_AccelRing2:
			strcpy(modelName, "AccelRing2.opt");
			break;
		case OBJ_SmeltingRoom:
			strcpy(modelName, "SmeltingRoom.opt");
			break;
		case OBJ_SRTubeNOBend:
			strcpy(modelName, "SR_TubeNOBend.opt");
			break;
		case OBJ_SRTubeUP:
			strcpy(modelName, "SR_TubeUP.opt");
			break;
		case OBJ_SRTubeDown:
			strcpy(modelName, "SR_TubeDown.opt");
			break;
		case OBJ_SRTubeLH:
			strcpy(modelName, "SR_TubeLH.opt");
			break;
		case OBJ_SRTubeRH:
			strcpy(modelName, "SR_TubeRH.opt");
			break;
		case OBJ_Centrifuge:
			strcpy(modelName, "Centrifuge.opt");
			break;
		case OBJ_AccelRing3:
			strcpy(modelName, "AccelRing3.opt");
			break;
		case OBJ_ContainerGrandePG:
			strcpy(modelName, "ContainerGrandePG.opt");
			break;
		case OBJ_Asteroid01:
			strcpy(modelName, "Asteroid01.opt");
			break;
		case OBJ_Asteroid02:
			strcpy(modelName, "Asteroid02.opt");
			break;
		case OBJ_Asteroid03:
			strcpy(modelName, "Asteroid03.opt");
			break;
		case OBJ_Junk01:
			strcpy(modelName, "Junk01.opt");
			break;
		case OBJ_Junk02:
			strcpy(modelName, "Junk02.opt");
			break;
		case OBJ_Junk03:
			strcpy(modelName, "Junk03.opt");
			break;
		case OBJ_Junk04:
			strcpy(modelName, "Junk04.opt");
			break;
		case OBJ_Junk05:
			strcpy(modelName, "Junk05.opt");
			break;
		case OBJ_Junk06:
			strcpy(modelName, "Junk06.opt");
			break;
		case OBJ_Junk07:
			strcpy(modelName, "Junk07.opt");
			break;
		case OBJ_Junk08:
			strcpy(modelName, "Junk08.opt");
			break;
		case OBJ_Junk09:
			strcpy(modelName, "Junk09.opt");
			break;
		case OBJ_Junk10:
			strcpy(modelName, "Junk10.opt");
			break;
		case OBJ_JunkBlock:
			strcpy(modelName, "JunkBlock.opt");
			break;
		case OBJ_MoltenBlock:
			strcpy(modelName, "MoltenBlock.opt");
			break;
		default:
			return 0;
	}

	strcpy(path, "FlightModels\\");
	strcat(path, modelName);

	modelHandle = OptModel_LoadHandle(path);
	if (modelHandle == 0) {
		DebugPrintf("Failed to load hangar model '%s'", path);
		g_loadingModel = 0;
		return 0;
	}
	texLevel = (TexLevel*)Memory_LockHandle(modelHandle);
	if (texLevel == NULL) {
		DebugPrintf("Failed to lock hangar model '%s' handle %u", path, (unsigned int)modelHandle);
		OptModel_FreeHandle(modelHandle);
		g_loadingModel = 0;
		return 0;
	}
	g_loadedModels.byObjectType[modelType] = modelHandle;
	g_modelTypeTable[modelType].curTexLevel = texLevel;
	FeDiskIo_BuildModelDef((uint16_t)g_modelTypeTable[modelType].modelIndex, modelType);

	meshCount = ModelMesh_GetCount(modelType);
	g_objectTypeMeshCache[modelType].meshCount = meshCount;
	for (meshIndex = 0; meshIndex < meshCount; ++meshIndex) {
		g_objectTypeMeshCache[modelType].meshTypes[meshIndex] = ModelMesh_GetType(modelType, meshIndex);
		g_objectTypeMeshCache[modelType].meshDescriptors[meshIndex] =
			ModelMesh_GetDescriptor(modelType, meshIndex);
		DebugPrintf((const char*)(uintptr_t)modelType, meshIndex);
	}

	g_loadingModel = 0;
	return (int16_t)modelHandle;
}
