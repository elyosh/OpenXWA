#ifndef XWA_RUNTIME_INPUT_CONTROLLER_MAPPING_H
#define XWA_RUNTIME_INPUT_CONTROLLER_MAPPING_H

#include "aeron/input.h"
#include "xwa_runtime/config/modern_input_options.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct XwaControllerLogicalState {
	uint32_t axes[XWA_CONTROLLER_LOGICAL_AXIS_COUNT];
	uint32_t buttons;
	int pov_direction; /* -1 centered, otherwise up/right/down/left = 0..3. */
	int has_pov;
	int8_t source_axes[XWA_CONTROLLER_LOGICAL_AXIS_COUNT];
	int16_t source_axis_values[XWA_CONTROLLER_LOGICAL_AXIS_COUNT];
} XwaControllerLogicalState;

void XwaControllerMapping_SetOptions(const XwaControllerOptions* options);
const AeronControllerSnapshot* XwaControllerMapping_SelectedController(void);
uint32_t XwaControllerMapping_SelectedInstanceId(void);
int XwaControllerMapping_SelectedHasRumble(void);
int XwaControllerMapping_Rumble(uint16_t low_frequency_rumble, uint16_t high_frequency_rumble,
								uint32_t duration_ms);
int XwaControllerMapping_ConsumeSelectionChange(void);
int XwaControllerMapping_GetState(XwaControllerLogicalState* state);
void XwaControllerMapping_CopySelectedActions(uint16_t actions[XWA_CONTROLLER_ACTION_COUNT]);

/* Pure conversion entry point used by the runtime facade and unit tests. */
void XwaControllerMapping_MapSnapshot(const XwaControllerOptions* options,
									  const AeronControllerSnapshot* controller, int has_focus,
									  XwaControllerLogicalState* state);

#ifdef __cplusplus
}
#endif

#endif
