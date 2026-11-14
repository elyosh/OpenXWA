#ifndef XWA_RUNTIME_INPUT_CONTROLLER_OPTIONS_H
#define XWA_RUNTIME_INPUT_CONTROLLER_OPTIONS_H
#include "aeron/input.h"
#include "xwa_runtime/input/actions.h"
#include <stddef.h>

typedef enum XwaInputAxis {
	XWA_INPUT_AXIS_YAW,
	XWA_INPUT_AXIS_PITCH,
	XWA_INPUT_AXIS_ROLL,
	XWA_INPUT_AXIS_THROTTLE,
	XWA_INPUT_AXIS_COUNT
} XwaInputAxis;

typedef struct XwaInputAxisBinding {
	int8_t source;
	bool invert;
	float deadzone;
} XwaInputAxisBinding;

typedef struct XwaInputMapping {
	XwaInputAxisBinding axes[XWA_INPUT_AXIS_COUNT];
} XwaInputMapping;

enum {
	XWA_CONTROLLER_BINDING_CAP =
		AERON_CONTROLLER_BUTTON_MAX + 2 * AERON_CONTROLLER_AXIS_MAX + 4 * AERON_CONTROLLER_HAT_MAX
};

typedef struct XwaInputActionBinding {
	AeronControllerDigitalSource source;
	XwaInputAction action;
} XwaInputActionBinding;

typedef struct XwaControllerProfile {
	XwaInputMapping mapping;
	XwaInputActionBinding bindings[XWA_CONTROLLER_BINDING_CAP];
	size_t binding_count;
} XwaControllerProfile;

enum { XWA_CONTROLLER_MODEL_CAP = 16 };

typedef struct XwaControllerModel {
	char guid[33];
	char name[AERON_CONTROLLER_NAME_CAPACITY];
	AeronControllerKind kind; /* Saved Aeron kind; must match the connected snapshot. */
	/* Set when the player changed this model, or when it was loaded from
	 * the configuration. Only these models are saved. The others hold the
	 * defaults, which discovery gives them again at the next start. */
	bool configured;
	XwaControllerProfile profile;
} XwaControllerModel;

/* The models are kept in the order they were last connected, most recent
 * first. When the list is full, AddModel() drops the last one that isn't
 * connected. Several models may bind the same flight axis
 * (controller_mapping.c merges the connected ones). */
typedef struct XwaControllerOptions {
	XwaControllerModel models[XWA_CONTROLLER_MODEL_CAP];
	size_t count;
} XwaControllerOptions;

bool XwaControllerOptions_Equals(const XwaControllerOptions* left, const XwaControllerOptions* right);
bool XwaControllerOptions_ValidateProfile(const XwaControllerProfile* profile, AeronControllerKind kind,
										  char* error, size_t capacity);
bool XwaControllerOptions_EffectiveAxisInvert(AeronControllerKind kind, XwaInputAxis axis, bool invert);
bool XwaControllerOptions_ProfileEqual(const XwaControllerProfile* left, const XwaControllerProfile* right);
void XwaControllerOptions_ClearProfile(XwaControllerProfile* profile, AeronControllerKind kind);
int XwaControllerOptions_FindModel(const XwaControllerOptions* options, const char* guid);
bool XwaControllerOptions_Validate(const XwaControllerOptions* options, char* error, size_t capacity);
bool XwaControllerOptions_AddModel(XwaControllerOptions* options, const AeronControllerSnapshot* device,
								   const AeronInputSnapshot* input, char* error, size_t capacity);
/* Moves the connected models to the front, keeping the order of the others. */
void XwaControllerOptions_MoveConnectedFirst(XwaControllerOptions* options, const AeronInputSnapshot* input);
/* Removes the models that aren't configured. */
void XwaControllerOptions_KeepConfigured(XwaControllerOptions* options);
bool XwaControllerOptions_InitializeGamepads(XwaControllerOptions* options,
											 const XwaControllerProfile* defaults,
											 const AeronInputSnapshot* input, char* error, size_t capacity);
#endif
