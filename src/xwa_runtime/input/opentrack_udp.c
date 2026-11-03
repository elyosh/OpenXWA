/* OpenTrack "UDP over network" output:
 * 48 bytes, six little-endian IEEE754 doubles: X,Y,Z (cm), yaw,pitch,roll
 * (degrees). Bind loopback only; neither OpenTrack nor its libraries are linked. */
#include "xwa_runtime/input/opentrack_udp.h"
#include "xwa_runtime/timing/host_clock.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET OpenTrackSocket;
#define OT_INVALID_SOCKET INVALID_SOCKET
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int OpenTrackSocket;
#define OT_INVALID_SOCKET (-1)
#endif

enum { OPENTRACK_UDP_PORT = 4242, OPENTRACK_STALE_MS = 500,
       OPENTRACK_RETRY_MS = 3000, OPENTRACK_MAX_DRAIN = 64 };

static OpenTrackSocket s_socket = OT_INVALID_SOCKET;
static uint32_t s_next_attempt_ms;
static uint32_t s_last_received_ms;
static int s_last_pose_valid;
static XwaTrackIRPose s_last_pose;
#ifdef _WIN32
static int s_winsock_started;
#endif

void XwaOpenTrackUdp_Shutdown(void) {
    if (s_socket != OT_INVALID_SOCKET) {
#ifdef _WIN32
        closesocket(s_socket);
#else
        close(s_socket);
#endif
        s_socket = OT_INVALID_SOCKET;
    }
#ifdef _WIN32
    if (s_winsock_started) {
        WSACleanup();
        s_winsock_started = 0;
    }
#endif
    s_next_attempt_ms = 0;
    s_last_received_ms = 0;
    s_last_pose_valid = 0;
    memset(&s_last_pose, 0, sizeof s_last_pose);
}

static int open_loopback_listener(void) {
    struct sockaddr_in bind_address;
#ifdef _WIN32
    u_long nonblocking = 1;
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) return 0;
    s_winsock_started = 1;
#else
    int flags;
#endif

    s_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s_socket == OT_INVALID_SOCKET) goto failed;
#ifdef _WIN32
    if (ioctlsocket(s_socket, FIONBIO, &nonblocking) != 0) goto failed;
#else
    flags = fcntl(s_socket, F_GETFL, 0);
    if (flags < 0 || fcntl(s_socket, F_SETFL, flags | O_NONBLOCK) < 0) goto failed;
#endif

    memset(&bind_address, 0, sizeof bind_address);
    bind_address.sin_family = AF_INET;
    bind_address.sin_port = htons(OPENTRACK_UDP_PORT);
    bind_address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(s_socket, (const struct sockaddr*)&bind_address, sizeof bind_address) != 0) goto failed;
    return 1;

failed:
    XwaOpenTrackUdp_Shutdown();
    return 0;
}

/* Bytewise decode avoids aliasing, alignment, and host-endian assumptions. */
static double read_le_double(const unsigned char* bytes) {
    uint64_t bits = 0;
    double value;
    for (int i = 0; i < 8; ++i) {
        bits |= (uint64_t)bytes[i] << (i * 8);
    }
    memcpy(&value, &bits, sizeof value);
    return value;
}

static int16_t degrees_to_q16(double angle, int sign) {
    double scaled = angle * (32768.0 / 180.0) * (double)sign;
    if (scaled > 32767.0) scaled = 32767.0;
    if (scaled < -32767.0) scaled = -32767.0;
    return (int16_t)lrint(scaled);
}

static int decode_packet(const unsigned char* bytes, int size,
                         const XwaHeadTrackingOptions* options, XwaTrackIRPose* pose) {
    double values[6];
    if (!bytes || size != 48 || !options || !pose || sizeof(double) != 8) return 0;
    for (int i = 0; i < 6; ++i) {
        values[i] = read_le_double(&bytes[8 * i]);
        if (!isfinite(values[i])) return 0;
    }
    /* Reject wildly out-of-range input rather than moving the camera
     * through geometry. OpenTrack normally sends far smaller values. */
    for (int i = 0; i < 3; ++i) {
        if (fabs(values[i]) > 200.0) return 0;
    }
    for (int i = 3; i < 6; ++i) {
        if (fabs(values[i]) > 360.0) return 0;
    }

    /* OpenTrack convention: +X right, +Y up. The common pose uses +left,
     * +up and +back; inversion switches remain user-configurable. */
    pose->left_cm = (float)(-values[0]) *
        (options->invert[XWA_HEAD_TRACK_AXIS_X] ? -1.0f : 1.0f);
    pose->up_cm = (float)values[1] *
        (options->invert[XWA_HEAD_TRACK_AXIS_Y] ? -1.0f : 1.0f);
    pose->back_cm = (float)values[2] *
        (options->invert[XWA_HEAD_TRACK_AXIS_Z] ? -1.0f : 1.0f);
    pose->yaw_q16 = degrees_to_q16(values[3],
        options->invert[XWA_HEAD_TRACK_AXIS_YAW] ? -1 : 1);
    pose->pitch_q16 = degrees_to_q16(values[4],
        options->invert[XWA_HEAD_TRACK_AXIS_PITCH] ? -1 : 1);
    pose->roll_q16 = degrees_to_q16(values[5],
        options->invert[XWA_HEAD_TRACK_AXIS_ROLL] ? -1 : 1);
    return 1;
}

int XwaOpenTrackUdp_Poll(XwaTrackIRPose* pose, const XwaHeadTrackingOptions* options) {
    uint32_t now = XwaTime_GetElapsedTicks();
    unsigned char packet[49]; /* One extra byte detects oversized datagrams. */
    XwaTrackIRPose latest;

    if (!pose || !options) return 0;
    memset(pose, 0, sizeof *pose);

    if (s_socket == OT_INVALID_SOCKET) {
        if (s_next_attempt_ms && (int32_t)(now - s_next_attempt_ms) < 0) return 0;
        if (!open_loopback_listener()) {
            s_next_attempt_ms = now + OPENTRACK_RETRY_MS;
            return 0;
        }
    }

    /* Drain queued packets without blocking the simulation, preferring the
     * newest pose. Bound the work so a flooded socket cannot stall a frame. */
    for (int i = 0; i < OPENTRACK_MAX_DRAIN; ++i) {
        int received = (int)recv(s_socket, (char*)packet, (int)sizeof packet, 0);
        if (received < 0) break; /* No pending data on a nonblocking socket. */
        if (decode_packet(packet, received, options, &latest)) {
            s_last_pose = latest;
            s_last_received_ms = now;
            s_last_pose_valid = 1;
        }
    }

    if (!s_last_pose_valid || (uint32_t)(now - s_last_received_ms) > OPENTRACK_STALE_MS) {
        s_last_pose_valid = 0;
        return 0;
    }
    *pose = s_last_pose;
    return 1;
}
