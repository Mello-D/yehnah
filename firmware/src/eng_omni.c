/* SPDX-License-Identifier: GPL-3.0-only
 * The Omnichord's voices, from FoMni (charlesvestal/fm1-fomni 7e7547f, firmware/src/dsp/omni.c, GPL-3.0); its waves,
 * envelope shapes and shimmer after Chordian (Jan125/pb.chordian, CC0 1.0), Jan125's OM-84 emulator.
 * Modifications (chord-harp fork): one voice of Yehnah's engine API (FoMni plays its 16 strings, chord and bass
 * itself), the three as MODEs, the strings' damping on a key's release (DAMP), the shimmer's rate from the track's ROOT
 * or a knob, brightness on a knob; FoMni's plate reverb, drums and rhythms left out (Yehnah's own sends). */
/* OMNI: the OM-84 Omnichord.
 *   HARP   the strum plate's string: a pluck that rings down (SUS), two voices mixed, as the OM-84's: HARP 1 the
 *          shimmering one (a second wave whose level a |sin|^0.6 tremolo moves, less on the high strings), HARP 2 the
 *          plain one; each through a one-pole low-pass that is darker on high notes and closes as the note decays. On
 *          the release it rings on (the OM's strings) unless DAMP shortens it
 *   CHORD  the chord buttons' organ-like voice: a decay to 95 % over 1/3 s, held, a 0.18 s release
 *   BASS   the bass: held, its second wave fading in as it decays (1 - e^2), a release that slows as it falls
 *   AUTO   (with HARP's chord keys) the OM itself: a chord key's notes the CHORD voice, under the strings at the OM's
 *          balance (OM_AUTO_CHORD), held and latched as the chord is; the plate's (and any other note) a HARP string
 *          (CHORD OMNI's chords: their first note, the root below, the BASS voice, at the same balance)
 * EDIT 1  MODE HARP1 HARP2 SUS       EDIT 2  SHIM RATE BRIT DAMP   ((chord-harp fork) SUS, BRIT and SHIM past 64 move
 *          CHORD and BASS too: their release, tone, a tremolo; at 64 / 0 / 64 the OM's)
 *   SHIM   the shimmer's depth (64: the OM's); RATE 0 AUTO (the OM's rate for the chord root: the track's ROOT), else
 *          0.5 .. 10 Hz; BRIT the strings' brightness (-63 .. 63, 0 the OM's); DAMP 0 the string rings on after its key
 * In float (the core's FPU; fastmath.h). Its own envelopes (ownenv): the ADSR is not used. Voice state: om_vs[][]. */
#include "fastmath.h"

enum { OM_HARP, OM_CHORD, OM_BASS, OM_AUTO };
static const char *const N_OMODE[] = {"HARP", "CHORD", "BASS", "AUTO"};
#define OM_AUTO_CHORD 0.3f             /* AUTO: the chord voice's level under the strings (FoMni's: 0.17 against ~0.74);
                                        * (chord-harp fork) SCL > OMNI's AUTO, P_OAUTO 0..127 to 0..0.8 (48: this) */
static float om_auto(const track_t *t) { return (float)clamp(t->p[P_OAUTO], 0, 127) * (0.8f / 127.0f); }
static int harp_chord_note(const track_t *t, uint32_t note);   /* (seq.c: a HARP chord's note, a key's or a step's) */
enum { E_OFF, E_DEC, E_SUS, E_REL };

static const float OM_WAVE_HARP[100] = {
    -0.39799f, -0.28373f, -0.17060f, -0.06152f, 0.03960f, 0.12917f, 0.20803f, 0.28191f, 0.34960f, 0.40549f,
    0.45319f, 0.49331f, 0.52432f, 0.54801f, 0.56355f, 0.57970f, 0.59063f, 0.59940f, 0.60419f, 0.60170f,
    0.59836f, 0.58910f, 0.57496f, 0.56113f, 0.55027f, 0.53575f, 0.51674f, 0.49654f, 0.47841f, 0.46009f,
    0.43877f, 0.41723f, 0.39391f, 0.36987f, 0.34912f, 0.33028f, 0.30880f, 0.29015f, 0.28134f, 0.29110f,
    0.31765f, 0.34676f, 0.36668f, 0.38591f, 0.40244f, 0.40430f, 0.40296f, 0.39942f, 0.38620f, 0.36451f,
    0.33784f, 0.30256f, 0.26601f, 0.22525f, 0.18284f, 0.13896f, 0.09116f, 0.03944f, 0.01095f, -0.01930f,
    -0.06511f, -0.10682f, -0.14985f, -0.18620f, -0.22334f, -0.25563f, -0.28897f, -0.31479f, -0.33961f, -0.36659f,
    -0.38817f, -0.40030f, -0.42051f, -0.43494f, -0.44113f, -0.44422f, -0.44546f, -0.45175f, -0.45011f, -0.44338f,
    -0.43919f, -0.43432f, -0.42536f, -0.41496f, -0.40234f, -0.38533f, -0.36692f, -0.35351f, -0.35702f, -0.43757f,
    -0.63473f, -0.83644f, -0.95357f, -1.00000f, -0.99320f, -0.94574f, -0.86106f, -0.75809f, -0.64193f, -0.51749f,
};
static const float OM_WAVE_HARP_BASE[100] = {
    -0.00266f, 0.08777f, 0.18882f, 0.27388f, 0.35537f, 0.42081f, 0.48144f, 0.52300f, 0.55629f, 0.58197f,
    0.60333f, 0.61264f, 0.61685f, 0.61537f, 0.60836f, 0.59744f, 0.58345f, 0.56607f, 0.55001f, 0.53169f,
    0.51361f, 0.49479f, 0.47017f, 0.44361f, 0.42593f, 0.40630f, 0.38623f, 0.36775f, 0.34250f, 0.32309f,
    0.30239f, 0.27810f, 0.26201f, 0.25645f, 0.25338f, 0.25813f, 0.27431f, 0.29847f, 0.32439f, 0.34800f,
    0.37128f, 0.39039f, 0.40919f, 0.41807f, 0.41568f, 0.40982f, 0.39650f, 0.37395f, 0.33759f, 0.29898f,
    0.25897f, 0.21323f, 0.16193f, 0.10889f, 0.05952f, 0.01079f, -0.03981f, -0.09210f, -0.14137f, -0.18540f,
    -0.22825f, -0.26861f, -0.30275f, -0.33373f, -0.36122f, -0.38607f, -0.40691f, -0.42389f, -0.43982f, -0.44371f,
    -0.44622f, -0.45460f, -0.45697f, -0.45735f, -0.45656f, -0.45357f, -0.44317f, -0.43492f, -0.42851f, -0.41657f,
    -0.40513f, -0.39718f, -0.40593f, -0.46330f, -0.56800f, -0.68940f, -0.80868f, -0.91354f, -0.97598f, -0.99815f,
    -0.99494f, -0.95903f, -0.88775f, -0.79334f, -0.68826f, -0.56428f, -0.43958f, -0.31235f, -0.18632f, -0.05672f,
};
static const float OM_WAVE_HARP_MOD[100] = {
    0.03591f, 0.05333f, 0.00665f, -0.07080f, -0.20051f, -0.33095f, -0.45188f, -0.54887f, -0.61908f, -0.64957f,
    -0.64591f, -0.61414f, -0.55623f, -0.48737f, -0.43005f, -0.39239f, -0.35708f, -0.32986f, -0.31270f, -0.30772f,
    -0.31699f, -0.32549f, -0.31438f, -0.29113f, -0.27528f, -0.25266f, -0.22139f, -0.19135f, -0.15194f, -0.12423f,
    -0.09851f, -0.07167f, -0.05874f, -0.06330f, -0.07224f, -0.08866f, -0.11028f, -0.13624f, -0.17134f, -0.17631f,
    -0.16792f, -0.15376f, -0.12114f, -0.07936f, -0.03368f, 0.00084f, 0.01017f, 0.01480f, 0.03150f, 0.04565f,
    0.04785f, 0.04195f, 0.04979f, 0.07251f, 0.09360f, 0.11757f, 0.15122f, 0.19612f, 0.24289f, 0.27687f,
    0.29556f, 0.31982f, 0.33608f, 0.33593f, 0.33783f, 0.33024f, 0.32839f, 0.33266f, 0.33194f, 0.32889f,
    0.32574f, 0.32389f, 0.32332f, 0.31700f, 0.29872f, 0.28969f, 0.28337f, 0.26531f, 0.25065f, 0.23648f,
    0.22462f, 0.21367f, 0.21425f, 0.26892f, 0.36992f, 0.48870f, 0.61436f, 0.71259f, 0.71137f, 0.59136f,
    0.40159f, 0.16527f, -0.05000f, -0.20454f, -0.28171f, -0.28334f, -0.23589f, -0.15229f, -0.06082f, 0.01612f,
};
static const float OM_WAVE_CHORD[100] = {
    -0.02066f, -0.07349f, -0.11447f, -0.15546f, -0.17499f, -0.20789f, -0.22363f, -0.24069f, -0.24759f, -0.24786f,
    -0.24152f, -0.24213f, -0.22794f, -0.20883f, -0.19391f, -0.17264f, -0.14044f, -0.11041f, -0.08777f, -0.05341f,
    -0.02347f, 0.01569f, 0.05283f, 0.09225f, 0.13516f, 0.18604f, 0.22260f, 0.28229f, 0.33029f, 0.40244f,
    0.48843f, 0.61011f, 0.59525f, 0.36401f, 0.14844f, -0.02924f, -0.14352f, -0.22665f, -0.27698f, -0.30371f,
    -0.32297f, -0.33246f, -0.31961f, -0.30585f, -0.28152f, -0.25522f, -0.23318f, -0.20383f, -0.17807f, -0.14395f,
    -0.11713f, -0.08859f, -0.06641f, -0.03763f, -0.01340f, 0.00580f, 0.03024f, 0.03815f, 0.06296f, 0.07126f,
    0.08575f, 0.09390f, 0.09076f, 0.10995f, 0.10983f, 0.11481f, 0.11566f, 0.11978f, 0.11533f, 0.12241f,
    0.11511f, 0.11298f, 0.10468f, 0.10153f, 0.10123f, 0.10568f, 0.10623f, 0.12488f, 0.13446f, -0.00720f,
    -0.67249f, -0.89246f, -0.99982f, -0.60101f, -0.22653f, 0.16098f, 0.40948f, 0.53842f, 0.59985f, 0.61566f,
    0.60013f, 0.55948f, 0.50540f, 0.44528f, 0.38342f, 0.29938f, 0.23203f, 0.15576f, 0.09366f, 0.03244f,
};
static const float OM_WAVE_BASS_BASE[100] = {
    -0.00127f, 0.02153f, 0.05140f, 0.07860f, 0.10591f, 0.13052f, 0.15795f, 0.21062f, 0.30353f, 0.40509f,
    0.50274f, 0.59331f, 0.67504f, 0.74692f, 0.80934f, 0.86013f, 0.90257f, 0.93491f, 0.95844f, 0.97198f,
    0.97817f, 0.97783f, 0.97255f, 0.96099f, 0.94428f, 0.92447f, 0.89793f, 0.86889f, 0.83667f, 0.80436f,
    0.76746f, 0.73043f, 0.69100f, 0.65183f, 0.61064f, 0.56795f, 0.52679f, 0.48429f, 0.44318f, 0.40070f,
    0.35951f, 0.32046f, 0.27929f, 0.23955f, 0.20138f, 0.16337f, 0.12756f, 0.09125f, 0.05716f, 0.02448f,
    -0.00869f, -0.03782f, -0.06884f, -0.09790f, -0.12481f, -0.14969f, -0.17703f, -0.25523f, -0.35739f, -0.45473f,
    -0.55140f, -0.63676f, -0.71645f, -0.78462f, -0.84240f, -0.89023f, -0.92914f, -0.95886f, -0.98047f, -0.99486f,
    -0.99971f, -0.99702f, -0.98918f, -0.97594f, -0.95976f, -0.93811f, -0.91033f, -0.88134f, -0.85067f, -0.81619f,
    -0.77932f, -0.74148f, -0.70287f, -0.66264f, -0.62278f, -0.58182f, -0.54003f, -0.49791f, -0.45769f, -0.41586f,
    -0.37461f, -0.33490f, -0.29491f, -0.25536f, -0.21798f, -0.18131f, -0.14417f, -0.10862f, -0.07473f, -0.04269f,
};
static const float OM_WAVE_BASS_MOD[100] = {
    -0.00448f, -0.00705f, -0.00478f, -0.00391f, 0.00741f, 0.01567f, 0.03788f, 0.03105f, -0.00329f, -0.04752f,
    -0.07722f, -0.10576f, -0.11968f, -0.12832f, -0.13347f, -0.11799f, -0.09787f, -0.08222f, -0.08048f, -0.06319f,
    -0.03018f, -0.00641f, 0.01257f, 0.03627f, 0.04987f, 0.06188f, 0.07064f, 0.08970f, 0.10527f, 0.11141f,
    0.12157f, 0.12652f, 0.12532f, 0.13204f, 0.13086f, 0.13182f, 0.12755f, 0.13139f, 0.13563f, 0.11979f,
    0.11302f, 0.10904f, 0.11102f, 0.10994f, 0.10365f, 0.09378f, 0.09282f, 0.08368f, 0.07496f, 0.07094f,
    0.07263f, 0.06537f, 0.06082f, 0.06098f, 0.05636f, 0.01748f, -0.05889f, -0.07261f, -0.05893f, -0.04876f,
    -0.02982f, -0.02084f, -0.00310f, 0.01120f, 0.02000f, 0.03773f, 0.05199f, 0.07363f, 0.08961f, 0.10627f,
    0.11584f, 0.11749f, 0.12552f, 0.12553f, 0.13058f, 0.14154f, 0.13955f, 0.13963f, 0.14574f, 0.13697f,
    0.13019f, 0.12994f, 0.13143f, 0.13041f, 0.12576f, 0.12342f, 0.12139f, 0.11294f, 0.11118f, 0.10252f,
    0.09785f, 0.09858f, 0.08992f, 0.07477f, 0.05977f, 0.05171f, 0.04051f, 0.03711f, 0.03526f, 0.02337f,
};
static const float OM_SHIM[360] YEHNAH_RES = {   /* (chord-harp fork: the resource region, res.c) */
    0.887194f, 0.880592f, 0.873794f, 0.866801f, 0.859610f, 0.852221f, 0.844633f, 0.836843f, 0.828851f, 0.820655f,
    0.812252f, 0.803642f, 0.794822f, 0.785790f, 0.776543f, 0.767080f, 0.757396f, 0.747490f, 0.737357f, 0.726995f,
    0.716399f, 0.705565f, 0.694489f, 0.683165f, 0.671589f, 0.659754f, 0.647654f, 0.635281f, 0.622627f, 0.609685f,
    0.596443f, 0.582892f, 0.569019f, 0.554811f, 0.540253f, 0.525329f, 0.510018f, 0.494299f, 0.478147f, 0.461533f,
    0.444424f, 0.426781f, 0.408557f, 0.389697f, 0.370134f, 0.349786f, 0.328548f, 0.306290f, 0.282837f, 0.257953f,
    0.231301f, 0.202372f, 0.170326f, 0.133565f, 0.088128f, 0.000000f, -0.088128f, -0.133565f, -0.170326f, -0.202372f,
    -0.231301f, -0.257953f, -0.282837f, -0.306290f, -0.328548f, -0.349786f, -0.370134f, -0.389697f, -0.408557f, -0.426781f,
    -0.444424f, -0.461533f, -0.478147f, -0.494299f, -0.510018f, -0.525329f, -0.540253f, -0.554811f, -0.569019f, -0.582892f,
    -0.596443f, -0.609685f, -0.622627f, -0.635281f, -0.647654f, -0.659754f, -0.671589f, -0.683165f, -0.694489f, -0.705565f,
    -0.716399f, -0.726995f, -0.737357f, -0.747490f, -0.757396f, -0.767080f, -0.776543f, -0.785790f, -0.794822f, -0.803642f,
    -0.812252f, -0.820655f, -0.828851f, -0.836843f, -0.844633f, -0.852221f, -0.859610f, -0.866801f, -0.873794f, -0.880592f,
    -0.887194f, -0.893603f, -0.899818f, -0.905842f, -0.911674f, -0.917315f, -0.922766f, -0.928028f, -0.933101f, -0.937985f,
    -0.942682f, -0.947192f, -0.951515f, -0.955651f, -0.959602f, -0.963366f, -0.966946f, -0.970340f, -0.973549f, -0.976574f,
    -0.979414f, -0.982070f, -0.984542f, -0.986831f, -0.988935f, -0.990857f, -0.992595f, -0.994149f, -0.995521f, -0.996710f,
    -0.997715f, -0.998538f, -0.999177f, -0.999634f, -0.999909f, -1.000000f, -0.999909f, -0.999634f, -0.999177f, -0.998538f,
    -0.997715f, -0.996710f, -0.995521f, -0.994149f, -0.992595f, -0.990857f, -0.988935f, -0.986831f, -0.984542f, -0.982070f,
    -0.979414f, -0.976574f, -0.973549f, -0.970340f, -0.966946f, -0.963366f, -0.959602f, -0.955651f, -0.951515f, -0.947192f,
    -0.942682f, -0.937985f, -0.933101f, -0.928028f, -0.922766f, -0.917315f, -0.911674f, -0.905842f, -0.899818f, -0.893603f,
    -0.887194f, -0.880592f, -0.873794f, -0.866801f, -0.859610f, -0.852221f, -0.844633f, -0.836843f, -0.828851f, -0.820655f,
    -0.812252f, -0.803642f, -0.794822f, -0.785790f, -0.776543f, -0.767080f, -0.757396f, -0.747490f, -0.737357f, -0.726995f,
    -0.716399f, -0.705565f, -0.694489f, -0.683165f, -0.671589f, -0.659754f, -0.647654f, -0.635281f, -0.622627f, -0.609685f,
    -0.596443f, -0.582892f, -0.569019f, -0.554811f, -0.540253f, -0.525329f, -0.510018f, -0.494299f, -0.478147f, -0.461533f,
    -0.444424f, -0.426781f, -0.408557f, -0.389697f, -0.370134f, -0.349786f, -0.328548f, -0.306290f, -0.282837f, -0.257953f,
    -0.231301f, -0.202372f, -0.170326f, -0.133565f, -0.088128f, -0.000000f, 0.088128f, 0.133565f, 0.170326f, 0.202372f,
    0.231301f, 0.257953f, 0.282837f, 0.306290f, 0.328548f, 0.349786f, 0.370134f, 0.389697f, 0.408557f, 0.426781f,
    0.444424f, 0.461533f, 0.478147f, 0.494299f, 0.510018f, 0.525329f, 0.540253f, 0.554811f, 0.569019f, 0.582892f,
    0.596443f, 0.609685f, 0.622627f, 0.635281f, 0.647654f, 0.659754f, 0.671589f, 0.683165f, 0.694489f, 0.705565f,
    0.716399f, 0.726995f, 0.737357f, 0.747490f, 0.757396f, 0.767080f, 0.776543f, 0.785790f, 0.794822f, 0.803642f,
    0.812252f, 0.820655f, 0.828851f, 0.836843f, 0.844633f, 0.852221f, 0.859610f, 0.866801f, 0.873794f, 0.880592f,
    0.887194f, 0.893603f, 0.899818f, 0.905842f, 0.911674f, 0.917315f, 0.922766f, 0.928028f, 0.933101f, 0.937985f,
    0.942682f, 0.947192f, 0.951515f, 0.955651f, 0.959602f, 0.963366f, 0.966946f, 0.970340f, 0.973549f, 0.976574f,
    0.979414f, 0.982070f, 0.984542f, 0.986831f, 0.988935f, 0.990857f, 0.992595f, 0.994149f, 0.995521f, 0.996710f,
    0.997715f, 0.998538f, 0.999177f, 0.999634f, 0.999909f, 1.000000f, 0.999909f, 0.999634f, 0.999177f, 0.998538f,
    0.997715f, 0.996710f, 0.995521f, 0.994149f, 0.992595f, 0.990857f, 0.988935f, 0.986831f, 0.984542f, 0.982070f,
    0.979414f, 0.976574f, 0.973549f, 0.970340f, 0.966946f, 0.963366f, 0.959602f, 0.955651f, 0.951515f, 0.947192f,
    0.942682f, 0.937985f, 0.933101f, 0.928028f, 0.922766f, 0.917315f, 0.911674f, 0.905842f, 0.899818f, 0.893603f,
};
/* the shimmer's rate for the chord root, Hz (Chordian), C..B */
static const float OM_SHIM_HZ[12] = {3.761f, 2.910f, 4.275f, 4.565f, 5.050f, 6.333f, 2.843f, 3.125f, 3.162f, 3.419f,
                                     3.667f, 3.953f};

typedef struct {
    float pos, v, g, y1, y2, k, c, m, modamt;
    uint16_t shim_off;
    uint8_t on, st, mode, quiet;           /* quiet: AUTO's chord, at OM_AUTO_CHORD */
} om_voice_t;
static om_voice_t om_vs[NPART][NVOICE];
static float om_shim_ph[NPART];                 /* the part's shimmer, degrees (FoMni: one for the instrument) */

static om_voice_t *om_v(const track_t *t, const voice_t *v)
{
    return &om_vs[(uint32_t)(t - trk) % NPART][(uint32_t)(v - t->v) % NVOICE];
}
static inline float om_wave(const float *w, float pos)
{
    int i = (int)pos;
    float f = pos - (float)i, a = w[i], b = w[i + 1 < 100 ? i + 1 : 0];
    return a + (b - a) * f;
}

static void omni_note_on(track_t *t, voice_t *v)
{
    om_voice_t *s = om_v(t, v);
    uint32_t mode = (uint32_t)clamp(t->p[P_E0], 0, OM_AUTO), qt = 0;
    float u = fm_exp2f((float)((int32_t)v->note - 69) * (1.0f / 12.0f)), base;
    int32_t i = clamp(((int32_t)v->note - 42) / 4, 0, 15) * 12 / 15;   /* (FoMni: the string's place on the plate) */
    if (mode == OM_AUTO) {                          /* a chord key's note the chord (an OMNI chord's bass the
                                                     * bass), the plate's a string */
        qt = (uint32_t)harp_chord_note(t, v->note);
        mode = qt == 2u ? OM_BASS : qt ? OM_CHORD : OM_HARP;
    }
    if (!s->on || s->mode != mode) {                /* from rest: the filters clear */
        s->y1 = s->y2 = 0.0f;
        s->g = 0.0f;
        s->pos = 0.0f;
    }
    s->on = 1;
    s->mode = (uint8_t)mode;
    s->quiet = (uint8_t)qt;
    s->v = 1.0f;
    s->st = mode == OM_HARP ? E_SUS : mode == OM_CHORD ? E_DEC : E_SUS;
    /* the voice filter: Chordian's per-note brightness, darker as the note decays */
    base = fm_maxf(((18.0f - u) / 18.0f) * 0.31f + 0.34f, 0.05f);
    s->k = fm_minf(fm_powf(base, 2.85f) * fm_exp2f((float)clamp(t->p[P_E6], -63, 63) * (1.0f / 32.0f)), 0.9f);
    s->modamt = 1.0f - (float)i / 17.0f;            /* the shimmer: less on the higher strings, each its own phase */
    s->shim_off = (uint16_t)(((i & 1) ? 27 * i : 14 * i) % 360);
    s->c = 0.0f;
    s->m = 0.0f;
}
static int omni_done(track_t *t, voice_t *v)
{
    return !om_v(t, v)->on;
}
static void omni_block(track_t *t)
{
    int32_t r = t->p[P_E5];
    float hz = r ? 0.5f + 9.5f * (float)(r - 1) * (1.0f / 126.0f) : OM_SHIM_HZ[((t->p[P_ROOT] % 12) + 12) % 12];
    float *ph = &om_shim_ph[(uint32_t)(t - trk) % NPART];
    *ph += hz * (360.0f * CTL / (float)FS);
    while (*ph >= 360.0f)
        *ph -= 360.0f;
}

/* one envelope step of CHORD / BASS (FoMni env_step), a sample; rr the release's speed (1 the OM's: SUS 64) */
static inline float om_env(om_voice_t *s, int bass, float rr)
{
    switch (s->st) {
    case E_DEC:
        s->v -= (1.0f / (float)FS) / 0.333f;
        if (s->v <= 0.95f) {
            s->v = 0.95f;
            s->st = E_SUS;
        }
        break;
    case E_REL:
        s->v -= (bass ? (1.0f / (float)FS) / 0.333f * (0.5f + s->v) : (1.0f / (float)FS) / 0.18f) * rr;
        break;
    default:
        break;
    }
    if (s->v <= 0.0f) {
        s->v = 0.0f;
        if (s->g < 1e-4f)
            s->st = E_OFF;
    }
    return s->v;
}

static void omni_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    if (!RES_OK()) return;                          /* (chord-harp fork) the resource region missing (res.c): silent */
    om_voice_t *s = om_v(t, v);
    const int16_t *p = t->p;
    float pos = s->pos, g = s->g, inc = (float)(m->inc >> 8) * (100.0f / 16777216.0f);   /* wave points a sample */
    float amp = (float)m->amp0, damp = (float)(m->amp1 - m->amp0) * (1.0f / CTL);
    float k = (float)VOICE_FS / (32768.0f * 32768.0f) * (float)(v->vel * 258);   /* (Q15 amp, the velocity) */
    uint32_t j;
    if (!s->on)
        return;
    if (s->mode == OM_HARP) {
        float v1 = (float)clamp(p[P_E1], 0, 127) * (1.0f / 127.0f), v2 = (float)clamp(p[P_E2], 0, 127) * (1.0f / 127.0f);
        const float g1 = v1 * (1.0f - 0.5f * v2) * 1.2f, g2 = v2 * (1.0f - 0.5f * v1) * 0.85f * 1.2f;
        float sus = (1.0f / (float)FS) / (0.366f + 2.734f * (float)clamp(p[P_E3], 0, 127) * (1.0f / 127.0f));
        float shim = (float)clamp(p[P_E4], 0, 127) * (1.0f / 64.0f), y1 = s->y1, y2 = s->y2, e = s->v, c = s->c;
        float mm = s->m, ph0 = om_shim_ph[(uint32_t)(t - trk) % NPART];
        int32_t dk = p[P_E7];
        if (!v->gate && dk > 0)                     /* DAMP: a key let go shortens its string */
            sus += (1.0f / (float)FS) * (float)(dk * dk) * (1.0f / 400.0f);
        for (j = 0; j < n; j++, amp += damp) {
            float x1, x2, o;
            if (!(j & 15u)) {                       /* every 16 samples: the filter and the shimmer */
                float c96 = (0.15f + 0.85f * e) * s->k;
                int32_t ph = (int32_t)ph0 + s->shim_off;
                c = 1.0f - fm_exp2f(2.1768707f * fm_log2f(1.0f - c96));   /* (Chordian's 96 kHz coefficient at 44.1) */
                mm = (0.61f + 0.61f * OM_SHIM[ph % 360]) * s->modamt * shim;
            }
            x1 = om_wave(OM_WAVE_HARP_BASE, pos) + om_wave(OM_WAVE_HARP_MOD, pos) * mm;
            x2 = om_wave(OM_WAVE_HARP, pos);
            y1 += c * (x1 - y1);
            y2 += c * (x2 - y2);
            g += 0.05f * (e - g);
            o = (y1 * g1 + y2 * g2) * g;
            out[j] += (int32_t)(o * amp * k);
            pos += inc;
            if (pos >= 100.0f)
                pos -= 100.0f;
            e -= sus * (0.34f + 8.5f * e * e * e);
            if (e <= 0.0f) {
                e = 0.0f;
                if (g < 1e-4f) {
                    s->on = 0;
                    break;
                }
            }
        }
        s->y1 = fm_flush(y1);
        s->y2 = fm_flush(y2);
        s->v = e;
        s->c = c;
        s->m = mm;
    } else {
        /* (chord-harp fork) the OM's chord and bass are one fixed sound; here SUS sets their release (64: the OM's;
         * each 21 below twice as fast, above twice as slow), BRIT their tone (0: as the OM's; below a low-pass closing
         * to ~90 Hz, above the highs lifted up to x4 over a ~1.7 kHz low-pass), SHIM past 64 a tremolo on the
         * shimmer's clock (RATE), up to 70 % of the level. At SUS 64, BRIT 0, SHIM <= 64 the samples are the OM's */
        int bass = s->mode == OM_BASS;
        float qa = s->quiet ? om_auto(t) : 0.8f;        /* (AUTO's chord: SCL > OMNI's AUTO) */
        int32_t su = clamp(p[P_E3], 0, 127);
        float rr = su == 64 ? 1.0f : fm_exp2f((float)(64 - su) * (1.0f / 21.0f));   /* (64: exactly the OM's) */
        int32_t br = clamp(p[P_E6], -63, 63);
        float lc = br < 0 ? 0.5f * fm_exp2f((float)br * (1.0f / 12.0f)) : 0.25f, hb = br > 0 ? (float)br * (3.0f / 63.0f) : 0.0f;
        float td = p[P_E4] > 64 ? (float)(clamp(p[P_E4], 0, 127) - 64) * (0.7f / 63.0f) : 0.0f, tr = 1.0f;
        float y1 = s->y1, ph0 = om_shim_ph[(uint32_t)(t - trk) % NPART];
        if (!v->gate && s->st != E_OFF)
            s->st = E_REL;
        for (j = 0; j < n && s->st != E_OFF; j++, amp += damp) {
            float e = om_env(s, bass, rr), o;
            g += 0.05f * (e - g);
            o = bass ? (om_wave(OM_WAVE_BASS_BASE, pos) + om_wave(OM_WAVE_BASS_MOD, pos) * (1.0f - e * e)) * g *
                           0.7f * qa * 1.25f
                     : om_wave(OM_WAVE_CHORD, pos) * g * qa;
            if (br) {                                   /* (BRIT: its tone) */
                y1 += lc * (o - y1);
                o = br < 0 ? y1 : o + hb * (o - y1);
            }
            if (td > 0.0f) {                            /* (SHIM past 64: the tremolo, every 16 samples) */
                if (!(j & 15u))
                    tr = 1.0f - td * (0.5f + 0.5f * OM_SHIM[((int32_t)ph0 + s->shim_off) % 360]);
                o *= tr;
            }
            out[j] += (int32_t)(o * amp * k);
            pos += inc;
            if (pos >= 100.0f)
                pos -= 100.0f;
        }
        s->y1 = fm_flush(y1);
        if (s->st == E_OFF)
            s->on = 0;
    }
    s->pos = pos;
    s->g = g;
}

static const preset_t OMNI_PRESETS[] YEHNAH_RES = {
    /* name, {MODE HARP1 HARP2 SUS SHIM RATE BRIT DAMP}, {A D S R} (not used: its own envelopes), fenv, mono, sends */
    {"OMNICHORD", {OM_AUTO, 102, 57, 64, 64, 0, 0, 0}, {0, 64, 127, 40}, 0, 0, FX(0, 0, 0, 40)},
    /* (chord-harp fork) the HARP presets from here (the OM's own: OMNI HARP) revoiced past the OM's narrow range, with
     * the part's own tools too (its filter, TRANS, sends, the LFO): the OM's two harp waves are near alike, so HARP1 /
     * HARP2 / BRIT alone left them all one sound */
    {"SHIMMER", {OM_HARP, 127, 0, 70, 127, 40, 10, 0}, {0, 64, 127, 40}, 0, 0, FX(0, 50, 0, 55)},
    {"PLAIN HARP", {OM_HARP, 0, 127, 64, 0, 0, 45, 0}, {0, 64, 127, 40}, 0, 0, FX(0, 0, 0, 30)},
    {"LONG HARP", {OM_HARP, 102, 57, 127, 64, 0, 10, 0}, {0, 64, 127, 40}, 0, 0, FX(0, 0, 35, 75)},
    {"SHORT HARP", {OM_HARP, 102, 57, 0, 40, 0, 20, 127}, {0, 64, 127, 40}, 0, 0, FX(0, 0, 0, 15)},
    {"DARK HARP", {OM_HARP, 90, 70, 90, 50, 0, -63, 0}, {0, 64, 127, 40}, 0, 0, FX(0, 0, 0, 45),
     XP(P_FCUT + 1, 70, P_TRANS + 1, -12)},
    {"FAST SHIMMER", {OM_HARP, 127, 0, 64, 127, 127, 10, 0}, {0, 64, 127, 40}, 0, 0, FX(0, 0, 15, 40)},
    {"OMNI CHORD", {OM_CHORD, 102, 57, 64, 64, 0, 0, 0}, {0, 64, 127, 40}, 0, 0, FX(0, 15, 0, 30)},
    {"OMNI BASS", {OM_BASS, 102, 57, 64, 64, 0, 0, 0}, {0, 64, 127, 40}, 0, 1, FX(0, 0, 0, 10)},
    /* AUTO's voicings: Chordian's balance (both strings full, its defaults), then ours. The chord under the strings
     * has no level of its own (OM_AUTO_CHORD): quieter strings bring it forward. (chord-harp fork) SUS, BRIT and SHIM
     * reach the chord now: CHORDIAN brighter, SHIMMER's chord trembles, LONG's rings on, DARK's darker, STRUM's short */
    {"AUTO DARK", {OM_AUTO, 90, 70, 80, 50, 0, -50, 0}, {0, 64, 127, 40}, 0, 0, FX(0, 0, 0, 45)},
    {"AUTO STRUM", {OM_AUTO, 70, 40, 0, 64, 0, 30, 40}, {0, 64, 127, 40}, 0, 0, FX(0, 0, 0, 25)},
    /* (chord-harp fork, appended) a dreamy harp (long, shimmering slowly, dark), a bright harp, a music box (CHORD and
     * BASS ignore BRIT and SHIM: no variants of them) */
    {"DREAM HARP", {OM_HARP, 110, 40, 127, 90, 20, -20, 0}, {0, 64, 127, 40}, 0, 0, FX(0, 70, 45, 90),
     XP(P_LRATE + 1, 50, P_LD_PIT + 1, 2)},
    {"BRIGHT HARP", {OM_HARP, 110, 60, 50, 64, 0, 63, 0}, {0, 64, 127, 40}, 0, 0, FX(15, 0, 0, 35), XP(P_TRANS + 1, 12)},
    {"MUSIC BOX", {OM_HARP, 0, 127, 20, 0, 0, 63, 60}, {0, 64, 127, 40}, 0, 0, FX(0, 0, 20, 50), XP(P_TRANS + 1, 24)},
    /* (chord-harp fork) CHORD and BASS variants, now that SUS, BRIT and SHIM reach them: a warm chord ringing on, a
     * trembling chord, a bright short organ, a soft bass */
    {"WARM CHORD", {OM_CHORD, 102, 57, 110, 64, 0, -35, 0}, {0, 64, 127, 40}, 0, 0, FX(0, 30, 0, 50)},
    {"BRIGHT ORGAN", {OM_CHORD, 102, 57, 30, 64, 0, 50, 0}, {0, 64, 127, 40}, 0, 0, FX(5, 10, 0, 25)},
    {"SOFT BASS", {OM_BASS, 102, 57, 110, 64, 0, -40, 0}, {0, 64, 127, 40}, 0, 1, FX(0, 0, 0, 10)},
    {"INIT", {0, 102, 57, 64, 64, 0, 0, 0}, {0, 64, 127, 30}, 0, 0, FX(0, 0, 0, 0)},   /* (chord-harp fork) the EDIT defaults, dry */
};
static const char OMNI_PT[] = "HHHHHHHOBHHHHEOOBN";
_Static_assert(sizeof OMNI_PT == NELEM(OMNI_PRESETS) + 1u, "a type a preset");
/* (engine 16: its favourites in favorites.factory[0]'s upper half, favorites.c favorite_byte: 128 presets at most) */
_Static_assert(NELEM(OMNI_PRESETS) <= 128u, "OMNI's favourites");
/* (2026-11-11) the SOUND page's rows (core.h sound_row_t): the engine's eight settings, then params.c's generic rows */
static const sound_row_t OMNI_ROWS[] = {
    {"OMNI", {P_E0, P_E1, P_E2}, {0, 0, 0}},
    {"TONE", {P_E3, P_E6, P_E7}, {0, 0, 0}},
    {"SHIMMER", {P_E4, P_E5, 0xFF}, {0, 0, 0}},
};

static const engine_t ENG_OMNI = {
    .rows = OMNI_ROWS,
    .nrows = (uint8_t)(sizeof OMNI_ROWS / sizeof OMNI_ROWS[0]),
    .name = "OMNI",
    .page_title = {"OMNI", "TONE"},
    .edit = {
        {"MODE", F_ENUM, 0, 3, OM_HARP, N_OMODE, 0},
        {"HARP1", F_PCT, 0, 127, 102, 0, 0},
        {"HARP2", F_PCT, 0, 127, 57, 0, 0},
        {"SUS", F_PCT, 0, 127, 64, 0, 0},
        {"SHIM", F_PCT, 0, 127, 64, 0, 0},
        {"RATE", F_INT, 0, 127, 0, 0, 0},
        {"BRIT", F_INT, -63, 63, 0, 0, 0},
        {"DAMP", F_PCT, 0, 127, 0, 0, 0},
    },
    .presets = OMNI_PRESETS,
    .npresets = NELEM(OMNI_PRESETS),
    .ptype = OMNI_PT,
    .note_on = omni_note_on,
    .render = omni_render,
    .knob = {P_E1, P_E2, P_E3, P_E7},
    .ownenv = 1,
    .done = omni_done,
    .block = omni_block,
};
