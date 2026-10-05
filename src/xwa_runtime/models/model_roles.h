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
	/* Hangar props animated by the hangar and family-base scenes. */
	XWA_ROLE_HANGARCRANE_ARM,
	XWA_ROLE_HANGARDROID_NOD,
	XWA_ROLE_HANGARDROID2_SWING,
	XWA_ROLE_HANGARDROID2_SPIN,
	XWA_ROLE_WORKDROID1_SPIN,
	XWA_ROLE_WORKDROID1_TURN,
	XWA_ROLE_WORKDROID1_NOD_1,
	XWA_ROLE_WORKDROID1_NOD_2,
	/* RebelPilot and ImperialPilot: the four limbs of the crew animation, in
	   order (the head uses the optional crew.forward tag). */
	XWA_ROLE_PILOT_LIMB_1,
	XWA_ROLE_PILOT_LIMB_2,
	XWA_ROLE_PILOT_LIMB_3,
	XWA_ROLE_PILOT_LIMB_4,
	/* AccelRing2/3: the core the proving-grounds course has the player
	   destroy (target box, hit filter, speed boost, reset). */
	XWA_ROLE_ACCELRING_CORE,
	/* Compactor: its four doors, in order; door 2 decides when it is fully
	   open or closed, and door n keeps stock open limit n. */
	XWA_ROLE_COMPACTOR_DOOR_1,
	XWA_ROLE_COMPACTOR_DOOR_2,
	XWA_ROLE_COMPACTOR_DOOR_3,
	XWA_ROLE_COMPACTOR_DOOR_4,
	/* Centrifuge: the spinning part and the door of each of its three
	   mechanisms, numbered by mechanism. */
	XWA_ROLE_CENTRIFUGE_SPIN_1,
	XWA_ROLE_CENTRIFUGE_SPIN_2,
	XWA_ROLE_CENTRIFUGE_SPIN_3,
	XWA_ROLE_CENTRIFUGE_DOOR_1,
	XWA_ROLE_CENTRIFUGE_DOOR_2,
	XWA_ROLE_CENTRIFUGE_DOOR_3,
	/* SmeltingRoom: six turrets in firing order (turret n fires from weapon
	   hardpoint n), the two power components that silence turrets 1-3 and
	   4-6 when destroyed, and the only component the proving-grounds
	   turbolaser can hit. */
	XWA_ROLE_SMELTINGROOM_TURRET_1,
	XWA_ROLE_SMELTINGROOM_TURRET_2,
	XWA_ROLE_SMELTINGROOM_TURRET_3,
	XWA_ROLE_SMELTINGROOM_TURRET_4,
	XWA_ROLE_SMELTINGROOM_TURRET_5,
	XWA_ROLE_SMELTINGROOM_TURRET_6,
	XWA_ROLE_SMELTINGROOM_POWER_1,
	XWA_ROLE_SMELTINGROOM_POWER_2,
	XWA_ROLE_SMELTINGROOM_TARGET,
	/* DSReactorCore: the power regenerator, which must be destroyed before
	   the reactor can be damaged, and the reactor, whose destruction starts
	   the Death Star destruction sequence. */
	XWA_ROLE_DSREACTORCORE_REGENERATOR,
	XWA_ROLE_DSREACTORCORE_REACTOR,
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
