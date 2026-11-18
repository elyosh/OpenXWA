#include "xwa_remaster/menu_beam.h"

#include "aeron/aeron.h"
#include "aeron/scene/draw_list2d.h"
#include "aeron/xr.h"
#include "xwa_remaster/color.h"
#include "xwa_remaster/flight.h"
#include "xwa_remaster/menu_hands.h"
#include "xwa_remaster/vr_hands.h"
#include "xwa_remaster/vr_keyboard.h"

#include <math.h>

/* The drawing's unit is one millimetre. */
#define BEAM_UNIT_M 0.001f
#define BEAM_WIDTH_MM 3.0f
#define BEAM_DOT_MM 10.0f
/* The beam is drawn in pieces, from bright at the hand to faint at the
 * screen. */
#define BEAM_PIECES 12
/* Two beams at most, each a strip and a dot. */
/* The length of the beam of a hand that points away from the screen, and
 * the longest beam to the screen's plane. */
#define BEAM_FREE_M 1.0f
#define BEAM_FREE_MAX_M 6.0f
#define BEAM_ALPHA_NEAR 0.85f
#define BEAM_ALPHA_FAR 0.20f

static AeronDrawList2D* g_beamList;

int XwaRemasterMenuBeam_Wanted(void) {
	return Aeron_XrSessionRunning() &&
		   (Aeron_XrPointerHand() >= 0 || XwaRemasterVrKeyboard_HandOn(0) ||
			XwaRemasterVrKeyboard_HandOn(1) || XwaRemasterVrHands_Moved(0) || XwaRemasterVrHands_Moved(1));
}

static void beam_rotate(const float q[4], const float v[3], float out[3]) {
	/* v + 2 q.xyz x (q.xyz x v + q.w v) */
	const float tx = q[1] * v[2] - q[2] * v[1] + q[3] * v[0];
	const float ty = q[2] * v[0] - q[0] * v[2] + q[3] * v[1];
	const float tz = q[0] * v[1] - q[1] * v[0] + q[3] * v[2];
	out[0] = v[0] + 2.0f * (q[1] * tz - q[2] * ty);
	out[1] = v[1] + 2.0f * (q[2] * tx - q[0] * tz);
	out[2] = v[2] + 2.0f * (q[0] * ty - q[1] * tx);
}

static float beam_normalise(float v[3]) {
	const float length = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
	if (length > 1e-6f) {
		for (int i = 0; i < 3; i++) {
			v[i] /= length;
		}
	}
	return length;
}

static void beam_cross(const float a[3], const float b[3], float out[3]) {
	out[0] = a[1] * b[2] - a[2] * b[1];
	out[1] = a[2] * b[0] - a[0] * b[2];
	out[2] = a[0] * b[1] - a[1] * b[0];
}

/* Adds a plane whose drawing has `origin` at its pixel (0, 0), with X along
 * `right` and Y along `down`, one millimetre for a pixel. The points are in
 * the rig's space. The views' matrices are in the scene's eye space of a
 * seat at the rig's origin, which is the rig's space with Y and Z negated. */
static int beam_plane(const float origin[3], const float right[3], const float down[3]) {
	static const float flip[3] = { 1.0f, -1.0f, -1.0f };
	float plane[9];
	for (int row = 0; row < 3; row++) {
		plane[row * 3 + 0] = flip[row] * right[row] * BEAM_UNIT_M;
		plane[row * 3 + 1] = flip[row] * down[row] * BEAM_UNIT_M;
		plane[row * 3 + 2] = flip[row] * origin[row];
	}
	return AeronDrawList_AddPlane(g_beamList, plane, 1.0f);
}

/* Adds a beam from `start` to `end`, with a dot at its end on a surface
 * whose axes are `right` and `up`. All are in the rig's space. */
static int beam_add(const AeronViewRig* rig, const float start[3], const float end[3], const float right[3],
					const float up[3]) {
	float along[3], to_head[3], side[3];
	for (int i = 0; i < 3; i++) {
		along[i] = end[i] - start[i];
		to_head[i] = rig->center.position_m[i] - start[i];
	}
	const float length_m = beam_normalise(along);
	if (length_m < 0.02f) {
		return 0;
	}
	/* The beam is a strip that faces the head. */
	beam_cross(along, to_head, side);
	if (beam_normalise(side) < 1e-6f) {
		const float world_up[3] = { 0.0f, 1.0f, 0.0f };
		beam_cross(along, world_up, side);
		beam_normalise(side);
	}
	const int strip = beam_plane(start, along, side);
	const float down[3] = { -up[0], -up[1], -up[2] };
	const int dot = beam_plane(end, right, down);
	if (strip < 0 || dot < 0) {
		return 0;
	}
	const float tint[3] = { XwaRemaster_SrgbToLinear(0.55f), XwaRemaster_SrgbToLinear(0.85f), 1.0f };
	const float length_mm = length_m / BEAM_UNIT_M;
	AeronDrawList_UsePlane(g_beamList, strip);
	for (int piece = 0; piece < BEAM_PIECES; piece++) {
		const float t = ((float)piece + 0.5f) / BEAM_PIECES;
		const float alpha = BEAM_ALPHA_NEAR + (BEAM_ALPHA_FAR - BEAM_ALPHA_NEAR) * t;
		const float rgba[4] = { tint[0] * alpha, tint[1] * alpha, tint[2] * alpha, alpha };
		AeronDrawList_AddFill(g_beamList, length_mm * (float)piece / BEAM_PIECES, -BEAM_WIDTH_MM * 0.5f,
							  length_mm / BEAM_PIECES, BEAM_WIDTH_MM, rgba, AERON_BLIT2D_BLEND_PMA, NULL);
	}
	const float dot_rgba[4] = { tint[0], tint[1], tint[2], 1.0f };
	AeronDrawList_UsePlane(g_beamList, dot);
	AeronDrawList_AddFill(g_beamList, -BEAM_DOT_MM * 0.5f, -BEAM_DOT_MM * 0.5f, BEAM_DOT_MM, BEAM_DOT_MM,
						  dot_rgba, AERON_BLIT2D_BLEND_PMA, NULL);
	return 1;
}

/* Adds the beam of a hand that points at nothing, so that the player sees
 * where it points and can bring it to the screen. It ends on the plane of
 * the virtual screen when the hand points towards it, and after
 * BEAM_FREE_M otherwise. */
static int beam_add_free(const AeronViewRig* rig, const AeronXrHand* hand, int has_screen,
						 const AeronViewPose* screen) {
	static const float across[3] = { 1.0f, 0.0f, 0.0f }, up[3] = { 0.0f, 1.0f, 0.0f };
	static const float ahead[3] = { 0.0f, 0.0f, -1.0f }, back[3] = { 0.0f, 0.0f, 1.0f };
	float direction[3], right[3], dot_up[3], end[3];
	float length_m = BEAM_FREE_M;
	beam_rotate(hand->aim.orientation, ahead, direction);
	beam_rotate(hand->aim.orientation, across, right);
	beam_rotate(hand->aim.orientation, up, dot_up);
	if (has_screen) {
		/* The screen faces its +Z. */
		float normal[3];
		beam_rotate(screen->orientation, back, normal);
		float approach = 0.0f, distance = 0.0f;
		for (int i = 0; i < 3; i++) {
			approach -= direction[i] * normal[i];
			distance += (hand->aim.position_m[i] - screen->position_m[i]) * normal[i];
		}
		if (approach > 1e-3f && distance > 0.0f && distance / approach < BEAM_FREE_MAX_M) {
			length_m = distance / approach;
			beam_rotate(screen->orientation, across, right);
			beam_rotate(screen->orientation, up, dot_up);
		}
	}
	for (int i = 0; i < 3; i++) {
		end[i] = hand->aim.position_m[i] + direction[i] * length_m;
	}
	return beam_add(rig, hand->aim.position_m, end, right, dot_up);
}

/* Fills the list with the beams: the hand that points at the virtual
 * screen, each hand that points at the keyboard, and each other hand that
 * moved lately. Returns 0 when there is nothing to draw. */
static int beam_build(const AeronViewRig* rig) {
	static const float across[3] = { 1.0f, 0.0f, 0.0f }, up[3] = { 0.0f, 1.0f, 0.0f };
	AeronXrHand hands[2];
	int any = 0;
	if (!Aeron_XrHands(hands)) {
		return 0;
	}
	int on_target[2] = { 0, 0 };
	for (int hand = 0; hand < 2; hand++) {
		float end[3];
		on_target[hand] = XwaRemasterVrKeyboard_HandOn(hand);
		/* The dot is small: the keyboard's tilt doesn't show on it. */
		if (hands[hand].active && XwaRemasterVrKeyboard_HandPoint(hand, end)) {
			any |= beam_add(rig, hands[hand].aim.position_m, end, across, up);
		}
	}
	AeronViewPose screen;
	float width_m, height_m;
	int logical_w = 0, logical_h = 0;
	const int hand = Aeron_XrPointerHand();
	const AeronInputSnapshot* input = Aeron_InputSnapshot();
	const int has_screen = Aeron_XrScreenQuad(&screen, &width_m, &height_m);
	if (hand >= 0 && input && hands[hand].active && !XwaRemasterVrKeyboard_HandOn(hand) && has_screen &&
		Aeron_GetLogicalSize(&logical_w, &logical_h) && logical_w > 0 && logical_h > 0) {
		on_target[hand] = 1;
		/* The beam ends where the mouse pointer is: the pointer is the hand's
		 * ray on the screen, smoothed. */
		const float u = ((float)input->mouse.x + 0.5f) / (float)logical_w;
		const float v = ((float)input->mouse.y + 0.5f) / (float)logical_h;
		float screen_right[3], screen_up[3], end[3];
		beam_rotate(screen.orientation, across, screen_right);
		beam_rotate(screen.orientation, up, screen_up);
		for (int i = 0; i < 3; i++) {
			end[i] = screen.position_m[i] + screen_right[i] * (u - 0.5f) * width_m +
					 screen_up[i] * (0.5f - v) * height_m;
		}
		any |= beam_add(rig, hands[hand].aim.position_m, end, screen_right, screen_up);
	}
	for (int other = 0; other < 2; other++) {
		if (!on_target[other] && XwaRemasterVrHands_Moved(other)) {
			any |= beam_add_free(rig, &hands[other], has_screen, &screen);
		}
	}
	return any;
}

int XwaRemasterMenuBeam_Render(const AeronViewRig* rig, AeronRenderTarget* const* targets) {
	if (!rig || !targets || rig->count <= 0 || rig->width <= 0 || rig->height <= 0) {
		return 0;
	}
	for (int view = 0; view < rig->count; view++) {
		if (!targets[view]) {
			return 0;
		}
	}
	if (!g_beamList) {
		g_beamList = AeronDrawList_Create(64);
		if (!g_beamList) {
			return 0;
		}
	}
	AeronCommandBuffer* cmd = Aeron_AcquireCommandBuffer();
	if (!cmd) {
		return 0;
	}
	AeronDrawList_Begin(g_beamList, NULL, rig->width, rig->height, AERON_DRAWLIST2D_LOAD, NULL);
	const int has_beam = beam_build(rig);
	int ok = AeronDrawList_Prepare(g_beamList, cmd) != 0;
	/* The eye images are cleared even when nothing is drawn: the headset
	 * shows them over the screen, and an image that was never drawn has
	 * anything in it. */
	for (int view = 0; ok && view < rig->count; view++) {
		/* The hands are rendered in a scene of their own, before the pass. */
		const int has_hands = XwaRemasterMenuHands_RenderView(cmd, rig, view);
		AeronRenderPass* pass = Aeron_BeginRenderPass(&(AeronRenderPassDesc) {
			.color_target = targets[view],
			.clear_color = 1,
			.clear_color_rgba = { 0, 0, 0, 0 },
			.command_buffer = cmd,
		});
		if (!pass) {
			ok = 0;
			break;
		}
		if (has_hands) {
			XwaRemasterMenuHands_DrawView(pass, targets[view]);
		}
		if (has_beam) {
			float rows[12];
			XwaRemasterFlight_RigViewPlaneRows(&rig->views[view], rig->width, rig->height, rows);
			AeronDrawList_RenderPlanes(g_beamList, cmd, pass, targets[view], rows, 1.0f);
		}
		Aeron_EndRenderPass(pass);
	}
	if (!ok) {
		Aeron_CancelCommandBuffer(cmd);
		return 0;
	}
	return Aeron_SubmitCommandBuffer(cmd) != 0;
}

void XwaRemasterMenuBeam_Shutdown(void) {
	AeronDrawList_Destroy(g_beamList);
	g_beamList = NULL;
}
