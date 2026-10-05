#ifndef XWA_RUNTIME_MODELS_MODEL_ROLES_H
#define XWA_RUNTIME_MODELS_MODEL_ROLES_H

/* Component roles: recovered code that reaches a model's component by a fixed
 * number names it through XWA_MODEL_ROLE instead. A stock model keeps the
 * stock number; a GLB replacement supplies the component through a
 * `<model>.<part>` tag, so its layout is free. The synthesizer rejects a
 * replacement that lacks a required role.
 *
 * Recovered sources include this header in every build. Outside XWA_MODERN
 * the macro is the stock number, so the original code is unchanged. */

#ifdef __cplusplus
extern "C" {
#endif

typedef enum XwaModelRole {
	/* Platform1..5: components hidden when the flight group mounts a beam
	   weapon (any beam), and those also hidden for jamming and decoy beams.
	   Sets; a platform may tag any number of components, including none. */
	XWA_ROLE_PLATFORM_HIDDEN_BY_BEAM,
	XWA_ROLE_PLATFORM_HIDDEN_BY_JAMMING_DECOY,
	XWA_MODEL_ROLE_COUNT
} XwaModelRole;

#ifdef XWA_MODERN
/* `stockComponent` for a stock model; for a replacement, the component
   carrying the role's tag (always present for a required role). */
int XwaModelRole_Resolve(unsigned short objectType, XwaModelRole role, int stockComponent);
/* Nonzero when `objectType`'s loaded model is a GLB replacement. */
int XwaModelRole_IsReplacement(unsigned short objectType);
/* For a replacement, writes the components carrying a set role to
   `components` (at most `capacity`) and returns how many there are. */
int XwaModelRole_Collect(unsigned short objectType, XwaModelRole role, int* components, int capacity);
#define XWA_MODEL_ROLE(objectType, role, stockComponent)                                                     \
	XwaModelRole_Resolve((unsigned short)(objectType), (role), (stockComponent))

struct AeronFlightModel;
/* Checks that replacement `key` carries every role required of it: each
   single role on exactly one component. Writes the reason on failure. */
int XwaModelRole_Validate(const char* key, const struct AeronFlightModel* model, char* reason,
						  unsigned int reasonSize);
#else
#define XWA_MODEL_ROLE(objectType, role, stockComponent) (stockComponent)
#endif

#ifdef __cplusplus
}
#endif

#endif
