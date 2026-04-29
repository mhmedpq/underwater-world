/*
 * ============================================================
 *   3D SHOOTING ARENA
 *   University-Level Computer Graphics Project
 *   OpenGL + GLUT (Fixed Pipeline) | C++
 * ============================================================
 *
 * COMPILE:
 *   g++ main.cpp -o game -lGL -lGLU -lglut -lm -lopenal
 *
 * INSTALL OpenAL (if missing):
 *   sudo apt-get install libopenal-dev
 *
 * RUN:
 *   ./game
 *
 * CONTROLS:
 *   W/A/S/D     - Move forward/left/backward/right
 *   MOUSE       - Aim (look around)
 *   LEFT CLICK  - Shoot
 *   R           - Reload
 *   SHIFT       - Sprint
 *   SPACE       - Jump
 *   ESC         - Quit
 *   P           - Pause / Resume
 *
 * SOUND SYSTEM:
 *   All sounds are synthesized procedurally (no audio files needed).
 *   Uses OpenAL for 3D positional audio output.
 *   Sounds: gunshot, reload, footstep, jump, land, explosion, hit.
 *
 * ============================================================
 */

#include <GL/glut.h>
#include <GL/glu.h>
#include <AL/al.h>
#include <AL/alc.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <vector>
#include <string>
#include <algorithm>
#include <sndfile.h>

// ============================================================
//  SOUND SYSTEM  (OpenAL + Procedurally Synthesized PCM)
//  No external audio files required — all sounds generated in code.
// ============================================================

/*
 *  Architecture:
 *    - One ALCdevice + ALCcontext for the session
 *    - 8 pre-built ALuint buffers (one per sound type)
 *    - A small pool of ALuint sources (16 slots) for polyphony
 *    - Footstep timing driven by the head-bob phase
 *    - Listener position / orientation updated every frame
 */

// ---- Sound IDs ----
enum SoundID
{
    SND_SHOOT = 0,
    SND_RELOAD = 1,
    SND_FOOTSTEP = 2,
    SND_JUMP = 3,
    SND_LAND = 4,
    SND_EXPLOSION = 5,
    SND_HIT = 6,
    SND_EMPTY = 7, // dry-fire click
    SND_COUNT = 8
};

static ALCdevice *alDevice = nullptr;
static ALCcontext *alContext = nullptr;
static ALuint alBuffers[SND_COUNT];
static const int AL_SOURCES = 16;
static ALuint alSources[AL_SOURCES];
static bool soundOK = false;

// ---- Waveform builders ----
// All return a heap-allocated int16 PCM buffer; caller owns it.

static std::vector<int16_t> buildShoot(int sampleRate = 44100)
{
    // Sharp transient + low-frequency boom + high-frequency crack
    int len = sampleRate / 4; // 250 ms
    std::vector<int16_t> buf(len);
    for (int i = 0; i < len; i++)
    {
        float t = (float)i / sampleRate;
        float env = expf(-t * 18.0f); // fast decay envelope
        // Boom component (low freq noise burst)
        float boom = ((float)rand() / RAND_MAX * 2 - 1) * env;
        // Crack component (filtered noise at higher freq)
        float crack = sinf(2 * M_PI * 180 * t) * expf(-t * 40.0f);
        // Mechanical click at start
        float click = sinf(2 * M_PI * 1200 * t) * expf(-t * 90.0f);
        float s = boom * 0.55f + crack * 0.3f + click * 0.25f;
        s = s > 1 ? 1 : s < -1 ? -1
                               : s;
        buf[i] = (int16_t)(s * 28000);
    }
    return buf;
}

static std::vector<int16_t> buildReload(int sampleRate = 44100)
{
    // Magazine eject click (0–80 ms) + slide pull (80–300 ms) + chamber click (300–400 ms)
    int len = sampleRate * 40 / 100; // 400 ms
    std::vector<int16_t> buf(len, 0);
    for (int i = 0; i < len; i++)
    {
        float t = (float)i / sampleRate;
        float s = 0;
        // Eject click
        if (t < 0.08f)
        {
            float env = expf(-(t) * 80);
            s = sinf(2 * M_PI * 900 * t) * env * 0.6f + sinf(2 * M_PI * 1800 * t) * env * 0.3f;
        }
        // Slide scrape (filtered noise sweep)
        else if (t < 0.30f)
        {
            float tt = (t - 0.08f) / 0.22f;
            float env = sinf(tt * M_PI) * 0.4f;
            float freq = 300 + tt * 1200;
            s = ((float)rand() / RAND_MAX * 2 - 1) * env * 0.4f + sinf(2 * M_PI * freq * t) * env * 0.2f;
        }
        // Chamber snap
        else
        {
            float tt = t - 0.30f;
            float env = expf(-tt * 60);
            s = sinf(2 * M_PI * 1400 * t) * env * 0.7f + ((float)rand() / RAND_MAX * 2 - 1) * env * 0.3f;
        }
        s = s > 1 ? 1 : s < -1 ? -1
                               : s;
        buf[i] = (int16_t)(s * 26000);
    }
    return buf;
}

static std::vector<int16_t> buildFootstep(int sampleRate = 44100)
{
    // Short soft thud — low-freq transient
    int len = sampleRate / 10; // 100 ms
    std::vector<int16_t> buf(len);
    for (int i = 0; i < len; i++)
    {
        float t = (float)i / sampleRate;
        float env = expf(-t * 30.0f);
        float s = ((float)rand() / RAND_MAX * 2 - 1) * env * 0.55f + sinf(2 * M_PI * 60 * t) * env * 0.45f;
        s = s > 1 ? 1 : s < -1 ? -1
                               : s;
        buf[i] = (int16_t)(s * 18000);
    }
    return buf;
}

static std::vector<int16_t> buildJump(int sampleRate = 44100)
{
    // Quick rising pitch swoosh
    int len = sampleRate / 5; // 200 ms
    std::vector<int16_t> buf(len);
    for (int i = 0; i < len; i++)
    {
        float t = (float)i / sampleRate;
        float env = expf(-t * 9.0f);
        float freq = 120 + t * 800; // pitch sweep up
        float s = sinf(2 * M_PI * freq * t) * env * 0.5f + sinf(2 * M_PI * freq * 2 * t) * env * 0.2f + ((float)rand() / RAND_MAX * 2 - 1) * expf(-t * 25) * 0.25f;
        s = s > 1 ? 1 : s < -1 ? -1
                               : s;
        buf[i] = (int16_t)(s * 22000);
    }
    return buf;
}

static std::vector<int16_t> buildLand(int sampleRate = 44100)
{
    // Heavy thud + body settle rumble
    int len = sampleRate * 18 / 100; // 180 ms
    std::vector<int16_t> buf(len);
    for (int i = 0; i < len; i++)
    {
        float t = (float)i / sampleRate;
        float env = expf(-t * 22.0f);
        float s = ((float)rand() / RAND_MAX * 2 - 1) * env * 0.6f + sinf(2 * M_PI * 55 * t) * env * 0.4f + sinf(2 * M_PI * 110 * t) * expf(-t * 40) * 0.25f;
        s = s > 1 ? 1 : s < -1 ? -1
                               : s;
        buf[i] = (int16_t)(s * 26000);
    }
    return buf;
}

static std::vector<int16_t> buildExplosion(int sampleRate = 44100)
{
    // Long rumbling boom with noise burst
    int len = sampleRate * 9 / 10; // 900 ms
    std::vector<int16_t> buf(len);
    for (int i = 0; i < len; i++)
    {
        float t = (float)i / sampleRate;
        float env = expf(-t * 4.5f);
        float rumble = sinf(2 * M_PI * 35 * t) * env * 0.35f + sinf(2 * M_PI * 55 * t) * env * 0.25f;
        float noise = ((float)rand() / RAND_MAX * 2 - 1) * expf(-t * 7.0f) * 0.55f;
        float crack = sinf(2 * M_PI * 300 * t) * expf(-t * 15) * 0.2f;
        float s = rumble + noise + crack;
        s = s > 1 ? 1 : s < -1 ? -1
                               : s;
        buf[i] = (int16_t)(s * 30000);
    }
    return buf;
}

static std::vector<int16_t> buildHit(int sampleRate = 44100)
{
    // Short metallic ping
    int len = sampleRate / 8; // 125 ms
    std::vector<int16_t> buf(len);
    for (int i = 0; i < len; i++)
    {
        float t = (float)i / sampleRate;
        float env = expf(-t * 14.0f);
        float s = sinf(2 * M_PI * 740 * t) * env * 0.5f + sinf(2 * M_PI * 1480 * t) * env * 0.3f + ((float)rand() / RAND_MAX * 2 - 1) * expf(-t * 35) * 0.25f;
        s = s > 1 ? 1 : s < -1 ? -1
                               : s;
        buf[i] = (int16_t)(s * 21000);
    }
    return buf;
}

static std::vector<int16_t> buildEmpty(int sampleRate = 44100)
{
    // Dry-fire click — tiny metallic tap
    int len = sampleRate / 20; // 50 ms
    std::vector<int16_t> buf(len);
    for (int i = 0; i < len; i++)
    {
        float t = (float)i / sampleRate;
        float env = expf(-t * 80.0f);
        float s = sinf(2 * M_PI * 1600 * t) * env;
        buf[i] = (int16_t)(s * 18000);
    }
    return buf;
}

// Upload one PCM buffer to OpenAL
static void uploadBuffer(ALuint buf, const std::vector<int16_t> &pcm, int rate)
{
    alBufferData(buf, AL_FORMAT_MONO16,
                 pcm.data(),
                 (ALsizei)(pcm.size() * sizeof(int16_t)),
                 rate);
}

// ---- Public API ----

void soundInit()
{
    alDevice = alcOpenDevice(nullptr);
    if (!alDevice)
    {
        fprintf(stderr, "[Audio] No OpenAL device found — running silent.\n");
        return;
    }
    alContext = alcCreateContext(alDevice, nullptr);
    if (!alContext)
    {
        fprintf(stderr, "[Audio] Cannot create OpenAL context.\n");
        return;
    }
    alcMakeContextCurrent(alContext);

    // Generate buffers & sources
    alGenBuffers(SND_COUNT, alBuffers);
    alGenSources(AL_SOURCES, alSources);

    const int SR = 44100;
    uploadBuffer(alBuffers[SND_SHOOT], buildShoot(SR), SR);
    uploadBuffer(alBuffers[SND_RELOAD], buildReload(SR), SR);
    uploadBuffer(alBuffers[SND_FOOTSTEP], buildFootstep(SR), SR);
    uploadBuffer(alBuffers[SND_JUMP], buildJump(SR), SR);
    uploadBuffer(alBuffers[SND_LAND], buildLand(SR), SR);
    uploadBuffer(alBuffers[SND_EXPLOSION], buildExplosion(SR), SR);
    uploadBuffer(alBuffers[SND_HIT], buildHit(SR), SR);
    uploadBuffer(alBuffers[SND_EMPTY], buildEmpty(SR), SR);

    // Configure all sources with sensible defaults
    for (int i = 0; i < AL_SOURCES; i++)
    {
        alSourcef(alSources[i], AL_GAIN, 1.5f);
        alSourcef(alSources[i], AL_PITCH, 0.0f);
        alSourcei(alSources[i], AL_LOOPING, AL_FALSE);
        alSource3f(alSources[i], AL_POSITION, 0, 0, 0);
        alSource3f(alSources[i], AL_VELOCITY, 0, 0, 0);
        alSourcei(alSources[i], AL_SOURCE_RELATIVE, AL_TRUE); // default: relative to listener
    }

    // Listener at origin, facing -Z
    alListener3f(AL_POSITION, 0, 0, 0);
    alListener3f(AL_VELOCITY, 0, 0, 0);
    float ori[6] = {0, 0, -1, 0, 1, 0};
    alListenerfv(AL_ORIENTATION, ori);
    alListenerf(AL_GAIN, 1.0f);

    soundOK = true;
    printf("[Audio] OpenAL initialized — procedural sound system active.\n");
}

void soundShutdown()
{
    if (!soundOK)
        return;
    alDeleteSources(AL_SOURCES, alSources);
    alDeleteBuffers(SND_COUNT, alBuffers);
    alcMakeContextCurrent(nullptr);
    alcDestroyContext(alContext);
    alcCloseDevice(alDevice);
}

// Find a free (stopped) source from the pool
static ALuint getFreeSource()
{
    for (int i = 0; i < AL_SOURCES; i++)
    {
        ALint state;
        alGetSourcei(alSources[i], AL_SOURCE_STATE, &state);
        if (state != AL_PLAYING)
            return alSources[i];
    }
    return alSources[0]; // steal oldest if pool full
}

/*
 *  playSound(id, gain, pitch, relative, worldPos)
 *    relative = true  → sound attached to listener (UI / player sounds)
 *    relative = false → 3D positional sound at worldPos
 */
void playSound(SoundID id, float gain = 1.0f, float pitch = 1.0f,
               bool relative = true, float wx = 0, float wy = 0, float wz = 0)
{
    if (!soundOK)
        return;
    ALuint src = getFreeSource();
    alSourceStop(src);
    alSourcei(src, AL_BUFFER, alBuffers[id]);
    alSourcef(src, AL_GAIN, gain);
    alSourcef(src, AL_PITCH, pitch);
    alSourcei(src, AL_SOURCE_RELATIVE, relative ? AL_TRUE : AL_FALSE);
    if (relative)
        alSource3f(src, AL_POSITION, 0, 0, 0);
    else
        alSource3f(src, AL_POSITION, wx, wy, wz);
    alSourcePlay(src);
}

// Update OpenAL listener to match camera every frame
void soundUpdateListener(float px, float py, float pz,
                         float fx, float fy, float fz)
{
    if (!soundOK)
        return;
    alListener3f(AL_POSITION, px, py, pz);
    float ori[6] = {fx, fy, fz, 0, 1, 0};
    alListenerfv(AL_ORIENTATION, ori);
}

// ---- Footstep timing state ----
static float lastBobPhase = 0;
static bool wasOnGround = true;
static float footstepCooldown = 0;

// Called every update tick — handles footstep, jump, and land triggers
void soundUpdateMovement(float bobPhase, bool onGround,
                         bool isMoving, float velY, float dt)
{
    if (!soundOK)
        return;

    footstepCooldown -= dt;

    // Footstep: trigger on each bob cycle peak (every half-cycle = one step)
    if (isMoving && onGround && footstepCooldown <= 0)
    {
        // Detect zero-crossing of sin(bobPhase) going negative→positive
        float cur = sinf(bobPhase);
        float prev = sinf(lastBobPhase);
        if (prev < 0 && cur >= 0)
        {
            float pitchVar = 0.92f + ((float)rand() / RAND_MAX) * 0.18f; // slight variation
            playSound(SND_FOOTSTEP, 0.55f, pitchVar);
            footstepCooldown = 0.18f; // guard against rapid double triggers
        }
    }

    // Land: detect transition from air → ground with downward velocity
    if (!wasOnGround && onGround)
    {
        float impactGain = fminf(fmaxf(fabsf(velY) / 15.0f, 0.3f), 1.0f);
        float impactPitch = 0.8f + impactGain * 0.4f;
        playSound(SND_LAND, impactGain * 0.9f, impactPitch);
    }

    wasOnGround = onGround;
    lastBobPhase = bobPhase;
}

// background music: simple looping ambient hum (not positional)
// ============================================================
//  BACKGROUND MUSIC SYSTEM
//  Loads any audio file from assets/ and loops it forever.
//  Supported formats: .wav  .ogg  .flac  .mp3 (if libsndfile built with mp3)
// ============================================================

static ALuint musicBuffer = 0;
static ALuint musicSource = 0;

void musicInit(const char *filepath)
{
    if (!soundOK)
        return;

    // Open audio file with libsndfile
    SF_INFO sfInfo;
    memset(&sfInfo, 0, sizeof(sfInfo));
    SNDFILE *sndFile = sf_open(filepath, SFM_READ, &sfInfo);

    if (!sndFile)
    {
        fprintf(stderr, "[Music] Cannot open file: %s\n       Reason: %s\n",
                filepath, sf_strerror(nullptr));
        return;
    }

    // Read all samples into memory
    sf_count_t totalSamples = sfInfo.frames * sfInfo.channels;
    std::vector<int16_t> samples(totalSamples);
    sf_read_short(sndFile, samples.data(), totalSamples);
    sf_close(sndFile);

    // Pick correct OpenAL format
    ALenum format;
    if (sfInfo.channels == 1)
        format = AL_FORMAT_MONO16;
    else if (sfInfo.channels == 2)
        format = AL_FORMAT_STEREO16;
    else
    {
        fprintf(stderr, "[Music] Unsupported channel count: %d\n", sfInfo.channels);
        return;
    }

    // Upload to OpenAL buffer
    alGenBuffers(1, &musicBuffer);
    alBufferData(musicBuffer, format,
                 samples.data(),
                 (ALsizei)(samples.size() * sizeof(int16_t)),
                 sfInfo.samplerate);

    // Create a dedicated source for music
    alGenSources(1, &musicSource);
    alSourcei(musicSource, AL_BUFFER, musicBuffer);
    alSourcei(musicSource, AL_LOOPING, AL_TRUE); // ← loops forever
    alSourcef(musicSource, AL_GAIN, 0.5f);       // ← volume (0.0 - 1.0)
    alSourcef(musicSource, AL_PITCH, 1.0f);
    alSourcei(musicSource, AL_SOURCE_RELATIVE, AL_TRUE); // not 3D positioned
    alSource3f(musicSource, AL_POSITION, 0, 0, 0);

    alSourcePlay(musicSource);

    printf("[Music] Now playing: %s\n", filepath);
    printf("[Music] Sample rate: %d Hz | Channels: %d | Duration: %.1f sec\n",
           sfInfo.samplerate, sfInfo.channels,
           (float)sfInfo.frames / sfInfo.samplerate);
}

void musicShutdown()
{
    if (musicSource)
    {
        alSourceStop(musicSource);
        alDeleteSources(1, &musicSource);
        musicSource = 0;
    }
    if (musicBuffer)
    {
        alDeleteBuffers(1, &musicBuffer);
        musicBuffer = 0;
    }
}

void musicSetVolume(float vol)
{
    if (musicSource)
        alSourcef(musicSource, AL_GAIN, vol);
}

// ============================================================
//  SECTION 1: CONSTANTS & CONFIGURATION
// ============================================================

static const int WINDOW_W = 1280;
static const int WINDOW_H = 720;
static const float PI = 3.14159265f;
static const float DEG2RAD = PI / 180.0f;
static const float ARENA_SIZE = 50.0f; // half-extent of arena
static const float GRAVITY = -18.0f;
static const float PLAYER_HEIGHT = 1.75f;
static const float PLAYER_SPEED = 7.0f;
static const float SPRINT_MULT = 1.9f;
static const float JUMP_VEL = 7.5f;
static const float MOUSE_SENS = 0.12f;
static const int MAX_AMMO = 30;
static const float RELOAD_TIME = 1.8f;
static const float SHOOT_COOLDOWN = 0.12f;
static const float MUZZLE_DURATION = 0.07f;
static const float RECOIL_AMOUNT = 1.8f;
static const float RECOIL_RECOVER = 8.0f;

// ============================================================
//  SECTION 2: MATH HELPERS
// ============================================================

struct Vec3
{
    float x, y, z;
    Vec3(float x = 0, float y = 0, float z = 0) : x(x), y(y), z(z) {}
    Vec3 operator+(const Vec3 &o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3 &o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    Vec3 &operator+=(const Vec3 &o)
    {
        x += o.x;
        y += o.y;
        z += o.z;
        return *this;
    }
    Vec3 &operator-=(const Vec3 &o)
    {
        x -= o.x;
        y -= o.y;
        z -= o.z;
        return *this;
    }
    float dot(const Vec3 &o) const { return x * o.x + y * o.y + z * o.z; }
    Vec3 cross(const Vec3 &o) const
    {
        return {y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x};
    }
    float len() const { return sqrtf(x * x + y * y + z * z); }
    Vec3 norm() const
    {
        float l = len();
        return l > 1e-6f ? Vec3(x / l, y / l, z / l) : Vec3(0, 0, 0);
    }
};

inline float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi
                                                                               : v; }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline float randf(float lo = 0, float hi = 1)
{
    return lo + (hi - lo) * (float)rand() / (float)RAND_MAX;
}

// Ray-sphere intersection — returns distance or -1
float raySphere(Vec3 ro, Vec3 rd, Vec3 c, float r)
{
    Vec3 oc = ro - c;
    float b = oc.dot(rd);
    float disc = b * b - oc.dot(oc) + r * r;
    if (disc < 0)
        return -1;
    float sq = sqrtf(disc);
    float t1 = -b - sq;
    float t2 = -b + sq;
    if (t1 > 0)
        return t1;
    if (t2 > 0)
        return t2;
    return -1;
}

// ============================================================
//  SECTION 3: PARTICLE SYSTEM
// ============================================================

struct Particle
{
    Vec3 pos, vel;
    float life, maxLife;
    float size;
    float r, g, b, a;
    bool active;
};

static const int MAX_PARTICLES = 600;
Particle particles[MAX_PARTICLES];

void initParticles()
{
    for (int i = 0; i < MAX_PARTICLES; i++)
        particles[i].active = false;
}

Particle *allocParticle()
{
    for (int i = 0; i < MAX_PARTICLES; i++)
        if (!particles[i].active)
            return &particles[i];
    return nullptr; // pool full
}

void spawnImpact(Vec3 p, float nr, float ng, float nb)
{
    for (int i = 0; i < 18; i++)
    {
        Particle *pk = allocParticle();
        if (!pk)
            break;
        pk->active = true;
        pk->pos = p;
        Vec3 dir = Vec3(randf(-1, 1), randf(0.2f, 1), randf(-1, 1)).norm();
        pk->vel = dir * randf(2.5f, 7.0f);
        pk->life = randf(0.3f, 0.7f);
        pk->maxLife = pk->life;
        pk->size = randf(0.06f, 0.2f);
        pk->r = nr;
        pk->g = ng;
        pk->b = nb;
        pk->a = 1;
    }
}

void spawnExplosion(Vec3 p)
{
    for (int i = 0; i < 60; i++)
    {
        Particle *pk = allocParticle();
        if (!pk)
            break;
        pk->active = true;
        pk->pos = p;
        Vec3 dir = Vec3(randf(-1, 1), randf(-1, 1), randf(-1, 1)).norm();
        pk->vel = dir * randf(3.0f, 12.0f);
        pk->life = randf(0.5f, 1.4f);
        pk->maxLife = pk->life;
        pk->size = randf(0.1f, 0.45f);
        float heat = randf(0, 1);
        pk->r = 1.0f;
        pk->g = heat * 0.6f;
        pk->b = 0.0f;
        pk->a = 1;
    }
}

void spawnMuzzleFlash(Vec3 p)
{
    for (int i = 0; i < 8; i++)
    {
        Particle *pk = allocParticle();
        if (!pk)
            break;
        pk->active = true;
        pk->pos = p;
        Vec3 dir = Vec3(randf(-1, 1), randf(-0.5f, 0.5f), randf(-1, 1)).norm();
        pk->vel = dir * randf(1, 4);
        pk->life = randf(0.05f, 0.12f);
        pk->maxLife = pk->life;
        pk->size = randf(0.05f, 0.15f);
        pk->r = 1;
        pk->g = 0.85f;
        pk->b = 0.3f;
        pk->a = 1;
    }
}

void updateParticles(float dt)
{
    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        Particle &p = particles[i];
        if (!p.active)
            continue;
        p.pos += p.vel * dt;
        p.vel.y += GRAVITY * 0.35f * dt;
        p.life -= dt;
        p.a = clampf(p.life / p.maxLife, 0, 1);
        if (p.life <= 0)
            p.active = false;
    }
}

// ============================================================
//  SECTION 4: CAMERA SYSTEM
// ============================================================

struct Camera
{
    Vec3 pos;
    float yaw, pitch;
    float velY; // vertical velocity (jump)
    bool onGround;
    float recoil;   // current recoil offset (pitch)
    float bobPhase; // head-bob phase
    float bobAmt;   // current bob amplitude

    Vec3 forward() const
    {
        float p = (pitch + recoil) * DEG2RAD;
        float y = yaw * DEG2RAD;
        return Vec3(cosf(p) * sinf(y), -sinf(p), cosf(p) * cosf(y));
    }
    Vec3 right() const
    {
        float y = yaw * DEG2RAD;
        return Vec3(cosf(y), 0, -sinf(y));
    }
    Vec3 flatForward() const
    {
        float y = yaw * DEG2RAD;
        return Vec3(sinf(y), 0, cosf(y));
    }
};

Camera cam;

void initCamera()
{
    cam.pos = Vec3(0, PLAYER_HEIGHT, 0);
    cam.yaw = 0;
    cam.pitch = 0;
    cam.velY = 0;
    cam.onGround = true;
    cam.recoil = 0;
    cam.bobPhase = 0;
    cam.bobAmt = 0;
}

// ============================================================
//  SECTION 5: TARGET TYPES & SYSTEM
// ============================================================

enum TargetType
{
    TGT_STATIC = 0,
    TGT_PATROL,
    TGT_ROTATE,
    TGT_DRONE,
    TGT_BOUNCER
};

struct Target
{
    Vec3 pos;
    Vec3 patrolA, patrolB; // patrol waypoints
    float angle;           // current rotation angle
    float patrolT;         // lerp parameter [0..1]
    float patrolDir;       // +1 or -1
    float hitFlash;        // red flash timer when hit
    float deathTimer;      // countdown after destruction
    float respawnTimer;    // respawn countdown
    float radius;          // collision sphere
    int hp, maxHp;
    int scoreValue;
    TargetType type;
    bool alive;
    float bobOffset; // per-drone phase offset
};

static const int MAX_TARGETS = 20;
Target targets[MAX_TARGETS];
int numTargets = 0;

void addTarget(TargetType type, Vec3 pos, float radius, int hp, int score)
{
    if (numTargets >= MAX_TARGETS)
        return;
    Target &t = targets[numTargets++];
    t.pos = pos;
    t.angle = randf(0, 360);
    t.patrolT = randf(0, 1);
    t.patrolDir = (rand() % 2) * 2 - 1;
    t.hitFlash = 0;
    t.deathTimer = 0;
    t.respawnTimer = 0;
    t.radius = radius;
    t.hp = t.maxHp = hp;
    t.scoreValue = score;
    t.type = type;
    t.alive = true;
    t.bobOffset = randf(0, 2 * PI);
    // build patrol path
    float dx = randf(-18, 18);
    float dz = randf(-18, 18);
    t.patrolA = pos;
    t.patrolB = Vec3(
        clampf(pos.x + dx, -ARENA_SIZE + 3, ARENA_SIZE - 3),
        pos.y,
        clampf(pos.z + dz, -ARENA_SIZE + 3, ARENA_SIZE - 3));
}

void initTargets()
{
    numTargets = 0;
    // Static targets (range = red pillars)
    addTarget(TGT_STATIC, Vec3(12, 1.2f, 8), 1.0f, 1, 50);
    addTarget(TGT_STATIC, Vec3(-15, 1.2f, 10), 1.0f, 1, 50);
    addTarget(TGT_STATIC, Vec3(20, 1.2f, -12), 1.0f, 1, 50);
    addTarget(TGT_STATIC, Vec3(0, 1.2f, 22), 1.0f, 1, 50);
    // Patrol targets
    addTarget(TGT_PATROL, Vec3(-8, 1.2f, -8), 0.9f, 2, 100);
    addTarget(TGT_PATROL, Vec3(16, 1.2f, 16), 0.9f, 2, 100);
    addTarget(TGT_PATROL, Vec3(-20, 1.2f, -20), 0.9f, 2, 100);
    // Rotating targets
    addTarget(TGT_ROTATE, Vec3(5, 1.5f, -5), 0.8f, 2, 120);
    addTarget(TGT_ROTATE, Vec3(-10, 1.5f, 15), 0.8f, 2, 120);
    // Drones (flying)
    addTarget(TGT_DRONE, Vec3(8, 6, -15), 0.7f, 3, 200);
    addTarget(TGT_DRONE, Vec3(-12, 7, -8), 0.7f, 3, 200);
    addTarget(TGT_DRONE, Vec3(18, 5, 12), 0.7f, 3, 200);
    // Bouncers
    addTarget(TGT_BOUNCER, Vec3(0, 1.2f, -18), 0.85f, 3, 150);
    addTarget(TGT_BOUNCER, Vec3(-18, 1.2f, 0), 0.85f, 3, 150);
}

void updateTargets(float dt, float totalTime)
{
    for (int i = 0; i < numTargets; i++)
    {
        Target &t = targets[i];
        if (!t.alive)
        {
            t.respawnTimer -= dt;
            if (t.respawnTimer <= 0)
            {
                t.alive = true;
                t.hp = t.maxHp;
                // respawn at random spot
                t.pos.x = randf(-ARENA_SIZE + 4, ARENA_SIZE - 4);
                t.pos.z = randf(-ARENA_SIZE + 4, ARENA_SIZE - 4);
            }
            continue;
        }
        // Decay hit flash
        if (t.hitFlash > 0)
            t.hitFlash -= dt * 4.0f;

        switch (t.type)
        {
        case TGT_PATROL:
        {
            t.patrolT += dt * 0.4f * t.patrolDir;
            if (t.patrolT > 1.0f)
            {
                t.patrolT = 1.0f;
                t.patrolDir = -1;
            }
            if (t.patrolT < 0.0f)
            {
                t.patrolT = 0.0f;
                t.patrolDir = 1;
            }
            t.pos.x = lerpf(t.patrolA.x, t.patrolB.x, t.patrolT);
            t.pos.z = lerpf(t.patrolA.z, t.patrolB.z, t.patrolT);
            break;
        }
        case TGT_ROTATE:
        {
            t.angle += dt * 90.0f;
            break;
        }
        case TGT_DRONE:
        {
            // Sinusoidal patrol + bob
            t.pos.x += sinf(totalTime * 0.7f + t.bobOffset) * dt * 2.5f;
            t.pos.z += cosf(totalTime * 0.5f + t.bobOffset) * dt * 2.5f;
            t.pos.y = 5.5f + sinf(totalTime * 1.2f + t.bobOffset) * 1.2f;
            // Keep in bounds
            t.pos.x = clampf(t.pos.x, -ARENA_SIZE + 3, ARENA_SIZE - 3);
            t.pos.z = clampf(t.pos.z, -ARENA_SIZE + 3, ARENA_SIZE - 3);
            t.angle += dt * 120.0f;
            break;
        }
        case TGT_BOUNCER:
        {
            // Move toward a random direction, bounce off walls
            float speed = 3.5f;
            t.pos.x += sinf(t.angle * DEG2RAD) * speed * dt;
            t.pos.z += cosf(t.angle * DEG2RAD) * speed * dt;
            if (t.pos.x < -ARENA_SIZE + 3 || t.pos.x > ARENA_SIZE - 3)
                t.angle = 180 - t.angle;
            if (t.pos.z < -ARENA_SIZE + 3 || t.pos.z > ARENA_SIZE - 3)
                t.angle = -t.angle;
            break;
        }
        default:
            break;
        }
    }
}

// ============================================================
//  SECTION 6: GAME STATE
// ============================================================

struct GameState
{
    int score;
    int health; // 0..100
    int ammo;
    bool reloading;
    float reloadTimer;
    float shootTimer;  // cooldown
    float muzzleTimer; // muzzle flash duration
    float totalTime;
    float gameTime; // countdown timer
    bool gameOver;
    bool paused;
    bool started;
    int kills;
    int shots;
    int hits;
    int combo;
    float comboTimer;
    float difficulty; // scales over time
    // head-bob
    float lastMoveSpeed;
};

GameState gs;

void initGame()
{
    gs.score = 0;
    gs.health = 100;
    gs.ammo = MAX_AMMO;
    gs.reloading = false;
    gs.reloadTimer = 0;
    gs.shootTimer = 0;
    gs.muzzleTimer = 0;
    gs.totalTime = 0;
    gs.gameTime = 120.0f; // 2 minutes
    gs.gameOver = false;
    gs.paused = false;
    gs.started = true;
    gs.kills = 0;
    gs.shots = 0;
    gs.hits = 0;
    gs.combo = 0;
    gs.comboTimer = 0;
    gs.difficulty = 1.0f;
    gs.lastMoveSpeed = 0;
    initCamera();
    initTargets();
    initParticles();
}

// ============================================================
//  SECTION 7: INPUT HANDLING
// ============================================================

bool keys[256] = {};
bool mouseDown = false;
int lastMouseX = -1;
int lastMouseY = -1;
bool mouseCaptured = false;

void onSpecialKey(int k, int, int)
{
    if (k == GLUT_KEY_UP)
        keys[200] = true;
    if (k == GLUT_KEY_DOWN)
        keys[201] = true;
    if (k == GLUT_KEY_LEFT)
        keys[203] = true;
    if (k == GLUT_KEY_RIGHT)
        keys[202] = true;
}

void onSpecialKeyUp(int k, int, int)
{
    if (k == GLUT_KEY_UP)
        keys[200] = false;
    if (k == GLUT_KEY_DOWN)
        keys[201] = false;
    if (k == GLUT_KEY_LEFT)
        keys[203] = false;
    if (k == GLUT_KEY_RIGHT)
        keys[202] = false;
}
void onKey(unsigned char k, int, int)
{
    keys[(unsigned char)tolower(k)] = true;
    if (k == 27)
    {
        exit(0);
    } // ESC
    if (tolower(k) == 'p')
    {
        gs.paused = !gs.paused;
    }
    if (tolower(k) == 'r' && !gs.reloading && gs.ammo < MAX_AMMO)
    {
        gs.reloading = true;
        gs.reloadTimer = RELOAD_TIME;
        playSound(SND_RELOAD, 1.0f, 1.0f); // reload start click
    }
}
void onKeyUp(unsigned char k, int, int)
{
    keys[(unsigned char)tolower(k)] = false;
}

// ============================================================
//  SECTION 8: SHOOTING / WEAPON SYSTEM
// ============================================================

void doShoot()
{
    if (gs.gameOver || gs.paused || !gs.started)
        return;
    if (gs.reloading)
        return;
    if (gs.shootTimer > 0)
        return;
    if (gs.ammo <= 0)
    {
        // Auto-reload
        playSound(SND_EMPTY, 0.7f, 1.0f);
        gs.reloading = true;
        gs.reloadTimer = RELOAD_TIME;
        return;
    }
    gs.ammo--;
    gs.shots++;
    gs.shootTimer = SHOOT_COOLDOWN;
    gs.muzzleTimer = MUZZLE_DURATION;

    // --- SOUND: gunshot (slight pitch variation each shot) ---
    float shotPitch = 0.93f + randf(0, 0.14f);
    playSound(SND_SHOOT, 1.0f, shotPitch);

    // Apply recoil
    cam.recoil += RECOIL_AMOUNT;

    // Muzzle flash particles — slightly in front of camera
    Vec3 muzzlePos = cam.pos + cam.forward() * 0.5f + cam.right() * 0.25f;
    muzzlePos.y -= 0.15f;
    spawnMuzzleFlash(muzzlePos);

    // Raycast against all targets
    Vec3 ro = cam.pos;
    Vec3 rd = cam.forward();
    float bestDist = 1e9f;
    int bestIdx = -1;
    for (int i = 0; i < numTargets; i++)
    {
        if (!targets[i].alive)
            continue;
        float d = raySphere(ro, rd, targets[i].pos, targets[i].radius);
        if (d > 0 && d < bestDist)
        {
            bestDist = d;
            bestIdx = i;
        }
    }

    if (bestIdx >= 0)
    {
        Target &t = targets[bestIdx];
        gs.hits++;
        t.hitFlash = 1.0f;
        t.hp--;
        // Impact particles
        Vec3 hitPt = ro + rd * bestDist;
        spawnImpact(hitPt, 1.0f, 0.8f, 0.2f);

        // --- SOUND: hit ping (3D positional at target) ---
        float hitPitch = 0.8f + randf(0, 0.4f);
        playSound(SND_HIT, 0.8f, hitPitch, false, t.pos.x, t.pos.y, t.pos.z);

        if (t.hp <= 0)
        {
            t.alive = false;
            t.respawnTimer = randf(4.0f, 8.0f) / gs.difficulty;
            spawnExplosion(t.pos);

            // --- SOUND: explosion (3D positional) ---
            float expPitch = 0.8f + randf(0, 0.3f);
            playSound(SND_EXPLOSION, 1.0f, expPitch, false, t.pos.x, t.pos.y, t.pos.z);

            // Score + combo
            gs.combo++;
            gs.comboTimer = 2.5f;
            int bonus = (gs.combo >= 3) ? gs.combo * 10 : 0;
            gs.score += t.scoreValue + bonus;
            gs.kills++;
        }
    }
    else
    {
        // Missed shot — spawn small bullet impact on floor/walls
        // (fake: just forward a bit)
        Vec3 imp = ro + rd * 25.0f;
        spawnImpact(imp, 0.6f, 0.6f, 0.6f);
    }
}

void onMouseClick(int button, int state, int x, int y)
{
    if (button == GLUT_LEFT_BUTTON)
    {
        mouseDown = (state == GLUT_DOWN);
        if (state == GLUT_DOWN)
            doShoot();
    }
}

void onMouseMove(int x, int y)
{
    if (gs.gameOver || gs.paused)
        return;
    if (lastMouseX < 0)
    {
        lastMouseX = x;
        lastMouseY = y;
        return;
    }
    float dx = (x - lastMouseX) * MOUSE_SENS;
    float dy = (y - lastMouseY) * MOUSE_SENS;
    cam.yaw -= dx;
    cam.pitch += dy;
    cam.pitch = clampf(cam.pitch, -80, 80);
    lastMouseX = x;
    lastMouseY = y;
    // Warp cursor to center to allow infinite rotation
    int cx = WINDOW_W / 2, cy = WINDOW_H / 2;
    if (abs(x - cx) > 100 || abs(y - cy) > 100)
    {
        glutWarpPointer(cx, cy);
        lastMouseX = cx;
        lastMouseY = cy;
    }
}

// ============================================================
//  SECTION 9: PHYSICS & PLAYER UPDATE
// ============================================================

void updatePlayer(float dt)
{
    if (gs.gameOver || gs.paused)
        return;

    bool sprint = keys[(unsigned char)'s' + 32] && keys['w']; // detect shift
    // (GLUT gives shift state via glutGetModifiers in keyboard callback;
    //  here we use a simpler approach: dedicated flag via modifier check)
    // We'll track shift via special key
    float speed = PLAYER_SPEED * (sprint ? SPRINT_MULT : 1.0f);

    Vec3 move(0, 0, 0);
    if (keys['w'])
        move += cam.flatForward();
    if (keys['s'])
        move -= cam.flatForward();
    if (keys['a'])
        move += cam.right();
    if (keys['d'])
        move -= cam.right();
    // Arrow keys — rotate camera
    float arrowSpeed = 90.0f; // degrees per second, adjust to taste
    if (keys[200])
        cam.pitch -= arrowSpeed * dt; // UP arrow   → look up
    if (keys[201])
        cam.pitch += arrowSpeed * dt; // DOWN arrow → look down
    if (keys[202])
        cam.yaw -= arrowSpeed * dt; // LEFT arrow → look left
    if (keys[203])
        cam.yaw += arrowSpeed * dt; // RIGHT arrow→ look right

    // Re-clamp pitch after arrow rotation
    cam.pitch = clampf(cam.pitch, -80, 80);
    float moveLen = move.len();
    if (moveLen > 0.01f)
    {
        move = move.norm() * speed;
        cam.pos.x += move.x * dt;
        cam.pos.z += move.z * dt;
        gs.lastMoveSpeed = speed;
    }
    else
    {
        gs.lastMoveSpeed = 0;
    }

    // Clamp to arena
    cam.pos.x = clampf(cam.pos.x, -ARENA_SIZE + 1, ARENA_SIZE - 1);
    cam.pos.z = clampf(cam.pos.z, -ARENA_SIZE + 1, ARENA_SIZE - 1);

    // Jump
    if (keys[' '] && cam.onGround)
    {
        cam.velY = JUMP_VEL;
        cam.onGround = false;
        // --- SOUND: jump whoosh ---
        playSound(SND_JUMP, 0.75f, 0.95f + randf(0, 0.1f));
    }

    // Gravity
    cam.velY += GRAVITY * dt;
    cam.pos.y += cam.velY * dt;

    if (cam.pos.y <= PLAYER_HEIGHT)
    {
        cam.pos.y = PLAYER_HEIGHT;
        cam.velY = 0;
        cam.onGround = true;
    }

    // Head bob
    if (gs.lastMoveSpeed > 0.1f && cam.onGround)
    {
        cam.bobPhase += dt * 9.0f;
        cam.bobAmt = lerpf(cam.bobAmt, 0.07f, dt * 6);
    }
    else
    {
        cam.bobAmt = lerpf(cam.bobAmt, 0, dt * 8);
    }

    // Recoil recovery
    cam.recoil = lerpf(cam.recoil, 0, dt * RECOIL_RECOVER);
}

// ============================================================
//  SECTION 10: RENDERING HELPERS
// ============================================================

// Draw a solid box centered at current origin with given half-dimensions
void drawBox(float hw, float hh, float hd)
{
    glPushMatrix();
    glScalef(hw * 2, hh * 2, hd * 2);
    glutSolidCube(1.0);
    glPopMatrix();
}

// Draw a flat grid quad (ground)
void drawGround()
{
    float S = ARENA_SIZE;
    int N = 20; // grid subdivisions
    float step = (2 * S) / N;

    for (int i = 0; i < N; i++)
    {
        for (int j = 0; j < N; j++)
        {
            float x0 = -S + i * step;
            float z0 = -S + j * step;
            float x1 = x0 + step;
            float z1 = z0 + step;

            bool checker = (i + j) % 2 == 0;
            if (checker)
                glColor3f(0.22f, 0.22f, 0.25f);
            else
                glColor3f(0.18f, 0.18f, 0.20f);

            glBegin(GL_QUADS);
            glNormal3f(0, 1, 0);
            glVertex3f(x0, 0, z0);
            glVertex3f(x1, 0, z0);
            glVertex3f(x1, 0, z1);
            glVertex3f(x0, 0, z1);
            glEnd();
        }
    }

    // Grid lines
    glLineWidth(0.5f);
    glColor4f(0.35f, 0.35f, 0.4f, 0.4f);
    glBegin(GL_LINES);
    for (int i = 0; i <= N; i++)
    {
        float v = -S + i * step;
        glVertex3f(v, 0.002f, -S);
        glVertex3f(v, 0.002f, S);
        glVertex3f(-S, 0.002f, v);
        glVertex3f(S, 0.002f, v);
    }
    glEnd();
    glLineWidth(1.0f);
}

// Draw arena walls
void drawWalls()
{
    float S = ARENA_SIZE;
    float WH = 8.0f; // wall height
    float WT = 0.5f; // wall thickness half

    glColor3f(0.28f, 0.30f, 0.35f);

    // North / South walls
    glPushMatrix();
    glTranslatef(0, WH * 0.5f, S);
    drawBox(S, WH * 0.5f, WT);
    glPopMatrix();
    glPushMatrix();
    glTranslatef(0, WH * 0.5f, -S);
    drawBox(S, WH * 0.5f, WT);
    glPopMatrix();
    // East / West walls
    glPushMatrix();
    glTranslatef(S, WH * 0.5f, 0);
    drawBox(WT, WH * 0.5f, S);
    glPopMatrix();
    glPushMatrix();
    glTranslatef(-S, WH * 0.5f, 0);
    drawBox(WT, WH * 0.5f, S);
    glPopMatrix();

    // Wall trim (neon stripe)
    glColor3f(0.0f, 0.7f, 1.0f);
    glLineWidth(2.0f);
    // Top edges
    glBegin(GL_LINE_LOOP);
    glVertex3f(-S, WH, -S);
    glVertex3f(S, WH, -S);
    glVertex3f(S, WH, S);
    glVertex3f(-S, WH, S);
    glEnd();
    glLineWidth(1.0f);
}

// Draw a sci-fi pillar
void drawPillar(float x, float z, float h, float r)
{
    glPushMatrix();
    glTranslatef(x, h * 0.5f, z);
    glColor3f(0.30f, 0.32f, 0.38f);
    GLUquadric *q = gluNewQuadric();
    gluQuadricNormals(q, GLU_SMOOTH);
    glPushMatrix();
    glRotatef(-90, 1, 0, 0);
    gluCylinder(q, r, r, h, 12, 4);
    glPopMatrix();
    // Cap
    glColor3f(0.0f, 0.8f, 1.0f);
    glPushMatrix();
    glTranslatef(0, h * 0.5f, 0);
    glutSolidSphere(r * 1.2f, 12, 8);
    glPopMatrix();
    gluDeleteQuadric(q);
    glPopMatrix();
}

// Draw a crate
void drawCrate(float x, float y, float z, float s)
{
    glPushMatrix();
    glTranslatef(x, y + s * 0.5f, z);
    glColor3f(0.55f, 0.42f, 0.25f);
    drawBox(s * 0.5f, s * 0.5f, s * 0.5f);
    // Edge lines
    glColor3f(0.35f, 0.25f, 0.1f);
    glLineWidth(1.5f);
    glPushMatrix();
    glScalef(s, s, s);
    glutWireCube(1.0);
    glPopMatrix();
    glLineWidth(1.0f);
    glPopMatrix();
}

// Wrapper: glutWiresCube doesn't exist, use wireframe manually
void myWireCube(float s)
{
    glPushMatrix();
    glScalef(s, s, s);
    glutWireCube(1.0);
    glPopMatrix();
}

void drawCrateFull(float x, float y, float z, float s)
{
    glPushMatrix();
    glTranslatef(x, y + s * 0.5f, z);
    glColor3f(0.55f, 0.42f, 0.25f);
    drawBox(s * 0.5f, s * 0.5f, s * 0.5f);
    glColor3f(0.3f, 0.2f, 0.05f);
    glPolygonOffset(1, 1);
    glEnable(GL_POLYGON_OFFSET_LINE);
    myWireCube(s);
    glDisable(GL_POLYGON_OFFSET_LINE);
    glPopMatrix();
}

// Fake shadow (dark ellipse on ground)
void drawShadow(float x, float z, float radius)
{
    glPushMatrix();
    glTranslatef(x, 0.01f, z);
    glColor4f(0, 0, 0, 0.35f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    GLUquadric *q = gluNewQuadric();
    glRotatef(-90, 1, 0, 0);
    gluDisk(q, 0, radius, 20, 1);
    gluDeleteQuadric(q);
    glDisable(GL_BLEND);
    glPopMatrix();
}

// Draw target based on type
void drawTarget(Target &t, float totalTime)
{
    if (!t.alive)
        return;

    glPushMatrix();
    glTranslatef(t.pos.x, t.pos.y, t.pos.z);

    float flash = clampf(t.hitFlash, 0, 1);
    float hpFrac = (float)t.hp / (float)t.maxHp;

    switch (t.type)
    {

    case TGT_STATIC:
    {
        // Classic target: cylinder stack
        glRotatef(t.angle, 0, 1, 0);
        glColor3f(1.0f - flash * 0.5f, 0.2f + flash * 0.3f, 0.2f);
        GLUquadric *q = gluNewQuadric();
        gluQuadricNormals(q, GLU_SMOOTH);
        glPushMatrix();
        glRotatef(-90, 1, 0, 0);
        gluCylinder(q, 0.8f, 0.8f, 0.2f, 24, 2);
        glPopMatrix();
        // Bullseye rings
        glColor3f(1, 1, 1);
        glPushMatrix();
        glRotatef(-90, 1, 0, 0);
        gluDisk(q, 0.5f, 0.6f, 20, 1);
        glPopMatrix();
        glColor3f(1, 0.1f, 0.1f);
        glPushMatrix();
        glRotatef(-90, 1, 0, 0);
        gluDisk(q, 0, 0.25f, 20, 1);
        glPopMatrix();
        gluDeleteQuadric(q);
        break;
    }

    case TGT_PATROL:
    {
        glRotatef(t.patrolT * 180, 0, 1, 0);
        glColor3f(0.2f + flash, 0.7f - flash * 0.5f, 0.2f);
        glutSolidSphere(t.radius, 16, 12);
        // Core glow
        glColor3f(0.5f, 1.0f, 0.5f);
        glutSolidSphere(t.radius * 0.4f, 10, 8);
        break;
    }

    case TGT_ROTATE:
    {
        glRotatef(t.angle, 0, 1, 0);
        glColor3f(0.1f + flash, 0.4f, 0.9f);
        drawBox(0.6f, 0.6f, 0.6f);
        // Orbit ring
        glColor4f(0.3f, 0.6f, 1.0f, 0.7f);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < 32; i++)
        {
            float a = i * 2 * PI / 32;
            glVertex3f(cosf(a) * 1.1f, 0, sinf(a) * 1.1f);
        }
        glEnd();
        glDisable(GL_BLEND);
        break;
    }

    case TGT_DRONE:
    {
        glRotatef(t.angle, 0, 1, 0);
        // Body
        glColor3f(0.15f + flash * 0.5f, 0.15f, 0.2f);
        drawBox(0.5f, 0.15f, 0.5f);
        // Rotors (4)
        glColor3f(0.6f, 0.6f, 0.6f);
        float arms[4][2] = {{0.6f, 0.6f}, {-0.6f, 0.6f}, {0.6f, -0.6f}, {-0.6f, -0.6f}};
        for (int k = 0; k < 4; k++)
        {
            glPushMatrix();
            glTranslatef(arms[k][0], 0, arms[k][1]);
            // Rotor disk (spinning fast)
            glColor4f(0.8f, 0.8f, 0.8f, 0.6f);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            GLUquadric *q = gluNewQuadric();
            glRotatef(-90, 1, 0, 0);
            gluDisk(q, 0, 0.28f, 12, 1);
            gluDeleteQuadric(q);
            glDisable(GL_BLEND);
            glPopMatrix();
        }
        // LED glow
        glColor3f(1.0f, 0.1f + flash, 0.1f);
        glutSolidSphere(0.12f, 8, 6);
        break;
    }

    case TGT_BOUNCER:
    {
        // Bouncing diamond / octahedron
        glRotatef(totalTime * 70, 1, 1, 0);
        glColor3f(0.9f + flash * 0.1f, 0.5f, 0.0f);
        glutSolidOctahedron();
        glColor4f(1, 0.7f, 0.2f, 0.5f);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glutWireOctahedron();
        glDisable(GL_BLEND);
        break;
    }
    }

    // HP bar (billboard quad above target)
    if (t.maxHp > 1)
    {
        float bw = 1.2f;
        float bh = 0.15f;
        float yOff = t.radius * 1.8f;
        glPushMatrix();
        glTranslatef(0, yOff, 0);
        // Always face camera (billboard)
        float dx = cam.pos.x - t.pos.x;
        float dz = cam.pos.z - t.pos.z;
        float ang = atan2f(dx, dz) * 180.0f / PI;
        glRotatef(ang, 0, 1, 0);
        glDisable(GL_LIGHTING);
        // Background
        glColor3f(0.2f, 0.05f, 0.05f);
        glBegin(GL_QUADS);
        glVertex3f(-bw * 0.5f, 0, 0);
        glVertex3f(bw * 0.5f, 0, 0);
        glVertex3f(bw * 0.5f, bh, 0);
        glVertex3f(-bw * 0.5f, bh, 0);
        glEnd();
        // Fill
        float fw = bw * hpFrac;
        glColor3f(lerpf(1.0f, 0.1f, hpFrac), lerpf(0.1f, 0.8f, hpFrac), 0.1f);
        glBegin(GL_QUADS);
        glVertex3f(-bw * 0.5f, 0, 0.01f);
        glVertex3f(-bw * 0.5f + fw, 0, 0.01f);
        glVertex3f(-bw * 0.5f + fw, bh, 0.01f);
        glVertex3f(-bw * 0.5f, bh, 0.01f);
        glEnd();
        glEnable(GL_LIGHTING);
        glPopMatrix();
    }

    glPopMatrix();

    // Fake shadow
    if (t.type != TGT_DRONE)
        drawShadow(t.pos.x, t.pos.z, t.radius * 0.9f);
}

// ============================================================
//  SECTION 11: SCENE DECORATIONS
// ============================================================

float envRotation = 0; // slowly rotating sky elements

void drawSkybox()
{
    // Simple gradient sky using a large quad dome
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    float S = 200.0f;

    glBegin(GL_QUADS);
    // Top
    glColor3f(0.03f, 0.05f, 0.12f);
    glVertex3f(-S, S, -S);
    glVertex3f(S, S, -S);
    glVertex3f(S, S, S);
    glVertex3f(-S, S, S);
    // North face
    glColor3f(0.03f, 0.05f, 0.12f);
    glVertex3f(-S, S, -S);
    glVertex3f(S, S, -S);
    glColor3f(0.08f, 0.15f, 0.28f);
    glVertex3f(S, 0, -S);
    glVertex3f(-S, 0, -S);
    // South
    glColor3f(0.03f, 0.05f, 0.12f);
    glVertex3f(-S, S, S);
    glVertex3f(S, S, S);
    glColor3f(0.08f, 0.15f, 0.28f);
    glVertex3f(S, 0, S);
    glVertex3f(-S, 0, S);
    // East
    glColor3f(0.03f, 0.05f, 0.12f);
    glVertex3f(S, S, -S);
    glVertex3f(S, S, S);
    glColor3f(0.08f, 0.15f, 0.28f);
    glVertex3f(S, 0, S);
    glVertex3f(S, 0, -S);
    // West
    glColor3f(0.03f, 0.05f, 0.12f);
    glVertex3f(-S, S, S);
    glVertex3f(-S, S, -S);
    glColor3f(0.08f, 0.15f, 0.28f);
    glVertex3f(-S, 0, -S);
    glVertex3f(-S, 0, S);
    glEnd();

    // Stars (random points, regenerated each frame from seed)
    glPointSize(1.5f);
    glColor4f(0.9f, 0.9f, 1.0f, 0.8f);
    srand(42);
    glBegin(GL_POINTS);
    for (int i = 0; i < 200; i++)
    {
        float sx = randf(-180, 180);
        float sy = randf(10, 180);
        float sz = randf(-180, 180);
        glVertex3f(sx, sy, sz);
    }
    glEnd();
    srand(time(nullptr)); // restore random

    // Orbiting moon / planet
    glPushMatrix();
    glRotatef(envRotation * 0.2f, 0, 1, 0);
    glTranslatef(120, 80, 0);
    glColor3f(0.7f, 0.75f, 0.8f);
    glutSolidSphere(8, 20, 16);
    glPopMatrix();

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
}

void drawDecorations(float totalTime)
{
    // Pillars
    float pillarPos[6][2] = {
        {-30, -30}, {30, -30}, {-30, 30}, {30, 30}, {0, -35}, {0, 35}};
    for (int i = 0; i < 6; i++)
        drawPillar(pillarPos[i][0], pillarPos[i][1], 6.0f, 0.6f);

    // Crates cluster
    drawCrateFull(-25, 0, -25, 1.8f);
    drawCrateFull(-23, 0, -25, 1.4f);
    drawCrateFull(-24, 1.8f, -25, 1.2f);
    drawCrateFull(25, 0, 20, 1.8f);
    drawCrateFull(23, 0, 20, 1.4f);
    drawCrateFull(-10, 0, 30, 2.0f);

    // Sci-fi platform (elevated pad)
    glPushMatrix();
    glTranslatef(15, 0, -20);
    glColor3f(0.25f, 0.27f, 0.32f);
    drawBox(4, 0.3f, 4);
    // Glowing edge
    glColor4f(0.0f, 0.9f, 0.5f, 0.8f);
    glLineWidth(2);
    glBegin(GL_LINE_LOOP);
    glVertex3f(-4, 0.31f, -4);
    glVertex3f(4, 0.31f, -4);
    glVertex3f(4, 0.31f, 4);
    glVertex3f(-4, 0.31f, 4);
    glEnd();
    glLineWidth(1);
    glPopMatrix();

    // Orbiting decorative rings around arena center
    glPushMatrix();
    glTranslatef(0, 4, 0);
    glRotatef(totalTime * 20.0f, 0, 1, 0);
    glColor4f(0.0f, 0.6f, 1.0f, 0.5f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glLineWidth(2);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < 64; i++)
    {
        float a = i * 2 * PI / 64;
        glVertex3f(cosf(a) * 18, sinf(a * 2) * 0.5f, sinf(a) * 18);
    }
    glEnd();
    glLineWidth(1);
    glDisable(GL_BLEND);
    glPopMatrix();

    // Ground accent lines (runway)
    glColor4f(0.0f, 0.8f, 0.3f, 0.6f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glLineWidth(2);
    glBegin(GL_LINES);
    for (int i = -4; i <= 4; i++)
    {
        float x = i * 10.0f;
        glVertex3f(x, 0.02f, -ARENA_SIZE + 2);
        glVertex3f(x, 0.02f, ARENA_SIZE - 2);
    }
    glEnd();
    glLineWidth(1);
    glDisable(GL_BLEND);
}

// ============================================================
//  SECTION 12: PARTICLE RENDERING
// ============================================================

void renderParticles()
{
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glPointSize(6.0f);

    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        Particle &p = particles[i];
        if (!p.active)
            continue;
        float size = p.size * (p.life / p.maxLife);
        // Billboard quad
        Vec3 right = cam.right() * size;
        Vec3 up = Vec3(0, size, 0);
        glColor4f(p.r, p.g, p.b, p.a);
        glBegin(GL_QUADS);
        glVertex3f(p.pos.x - right.x - up.x, p.pos.y - right.y - up.y, p.pos.z - right.z - up.z);
        glVertex3f(p.pos.x + right.x - up.x, p.pos.y + right.y - up.y, p.pos.z + right.z - up.z);
        glVertex3f(p.pos.x + right.x + up.x, p.pos.y + right.y + up.y, p.pos.z + right.z + up.z);
        glVertex3f(p.pos.x - right.x + up.x, p.pos.y - right.y + up.y, p.pos.z - right.z + up.z);
        glEnd();
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}

// ============================================================
//  SECTION 13: GUN / WEAPON VIEW MODEL
// ============================================================

void drawWeaponViewModel()
{
    glDisable(GL_LIGHTING);
    // Save and set up orthographic-like perspective for viewmodel
    glPushMatrix();
    glLoadIdentity();

    // Position gun in lower-right of view
    float recoilOff = cam.recoil * 0.04f;
    float bobOff = sinf(cam.bobPhase) * cam.bobAmt;
    glTranslatef(0.28f, -0.28f + bobOff + recoilOff, -0.5f);
    glRotatef(-8 + cam.recoil, 1, 0, 0);
    glRotatef(5, 0, 1, 0);

    // Barrel
    glColor3f(0.25f, 0.25f, 0.28f);
    glPushMatrix();
    glTranslatef(0, 0, 0.1f);
    drawBox(0.03f, 0.04f, 0.2f);
    glPopMatrix();

    // Body / slide
    glColor3f(0.18f, 0.18f, 0.20f);
    drawBox(0.07f, 0.08f, 0.18f);

    // Grip
    glColor3f(0.15f, 0.15f, 0.16f);
    glPushMatrix();
    glTranslatef(0, -0.1f, 0.05f);
    glRotatef(15, 1, 0, 0);
    drawBox(0.055f, 0.1f, 0.06f);
    glPopMatrix();

    // Trigger guard
    glColor3f(0.2f, 0.2f, 0.22f);
    glPushMatrix();
    glTranslatef(0, -0.06f, 0.05f);
    drawBox(0.04f, 0.025f, 0.04f);
    glPopMatrix();

    // Muzzle flash
    if (gs.muzzleTimer > 0)
    {
        float ft = gs.muzzleTimer / MUZZLE_DURATION;
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glColor4f(1.0f, 0.85f, 0.3f, ft * 0.9f);
        glPushMatrix();
        glTranslatef(0, 0, -0.35f);
        glutSolidSphere(0.06f * ft, 8, 6);
        glPopMatrix();
        glDisable(GL_BLEND);
    }

    // Scope / rail
    glColor3f(0.3f, 0.3f, 0.35f);
    glPushMatrix();
    glTranslatef(0, 0.08f, -0.02f);
    drawBox(0.015f, 0.015f, 0.12f);
    glPopMatrix();

    glPopMatrix();
    glEnable(GL_LIGHTING);
}

// ============================================================
//  SECTION 14: HUD SYSTEM
// ============================================================

// Draw text at normalized screen coords [0..1, 0..1]
void drawText2D(float nx, float ny, const char *str, float r, float g, float b, float a = 1.0f, void *font = GLUT_BITMAP_HELVETICA_18)
{
    int pw = WINDOW_W, ph = WINDOW_H;
    glColor4f(r, g, b, a);
    glRasterPos2f(nx * pw, ny * ph);
    while (*str)
    {
        glutBitmapCharacter(font, *str++);
    }
}

void drawText2DLarge(float nx, float ny, const char *str, float r, float g, float b)
{
    drawText2D(nx, ny, str, r, g, b, 1.0f, GLUT_BITMAP_HELVETICA_18);
}

// Simple rectangle
void drawRect2D(float x, float y, float w, float h, float r, float g, float b, float a)
{
    glColor4f(r, g, b, a);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}

// Draw crosshair
void drawCrosshair()
{
    int cx = WINDOW_W / 2, cy = WINDOW_H / 2;
    int sz = 12, gap = 4;
    float spread = cam.recoil * 0.5f;
    glColor4f(0, 1, 0.5f, 0.9f);
    glLineWidth(1.5f);
    glBegin(GL_LINES);
    // Horizontal
    glVertex2f(cx - sz - spread, cy);
    glVertex2f(cx - gap - spread, cy);
    glVertex2f(cx + gap + spread, cy);
    glVertex2f(cx + sz + spread, cy);
    // Vertical
    glVertex2f(cx, cy - sz - spread);
    glVertex2f(cx, cy - gap - spread);
    glVertex2f(cx, cy + gap + spread);
    glVertex2f(cx, cy + sz + spread);
    glEnd();
    // Center dot
    glPointSize(2.5f);
    glBegin(GL_POINTS);
    glVertex2f(cx, cy);
    glEnd();
    glLineWidth(1.0f);
}

// Draw ammo bar
void drawAmmoBar()
{
    float barW = 200.0f;
    float barH = 16.0f;
    float x = WINDOW_W - barW - 30;
    float y = 30;

    // Background
    drawRect2D(x - 2, y - 2, barW + 4, barH + 4, 0.1f, 0.1f, 0.1f, 0.7f);
    // Fill
    float frac = (float)gs.ammo / (float)MAX_AMMO;
    float cr = lerpf(1, 0.2f, frac), cg = lerpf(0.2f, 1, frac);
    drawRect2D(x, y, barW * frac, barH, cr, cg, 0.1f, 0.9f);

    char buf[64];
    if (gs.reloading)
        sprintf(buf, "RELOADING...");
    else
        sprintf(buf, "AMMO  %d / %d", gs.ammo, MAX_AMMO);
    drawText2D((x) / WINDOW_W, (y + 2) / WINDOW_H, buf, 1, 1, 1, 1, GLUT_BITMAP_HELVETICA_12);
}

// Draw health bar
void drawHealthBar()
{
    float barW = 200.0f;
    float barH = 16.0f;
    float x = 30;
    float y = 30;

    drawRect2D(x - 2, y - 2, barW + 4, barH + 4, 0.1f, 0.1f, 0.1f, 0.7f);
    float frac = gs.health / 100.0f;
    drawRect2D(x, y, barW * frac, barH, 1.0f - frac, frac * 0.8f, 0.1f, 0.9f);

    char buf[32];
    sprintf(buf, "HP  %d", gs.health);
    drawText2D((x) / WINDOW_W, (y + 2) / WINDOW_H, buf, 1, 1, 1, 1, GLUT_BITMAP_HELVETICA_12);
}

// Draw HUD
static float fps = 0;
static int frameCount = 0;
static float fpsTimer = 0;

void drawHUD()
{
    // Setup 2D ortho
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, WINDOW_W, 0, WINDOW_H);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    drawCrosshair();
    drawAmmoBar();
    drawHealthBar();

    // Score
    char buf[64];
    sprintf(buf, "SCORE  %d", gs.score);
    drawText2D(0.38f, 0.96f, buf, 0.1f, 1.0f, 0.6f, 1.0f, GLUT_BITMAP_HELVETICA_18);

    // Timer
    int timeLeft = (int)gs.gameTime;
    sprintf(buf, "TIME  %02d:%02d", timeLeft / 60, timeLeft % 60);
    float tColor = (timeLeft < 20) ? 1.0f : 0.9f;
    drawText2D(0.44f, 0.92f, buf, tColor, timeLeft < 20 ? 0.2f : 0.9f, 0.2f, 1.0f, GLUT_BITMAP_HELVETICA_12);

    // Kills
    sprintf(buf, "KILLS %d", gs.kills);
    drawText2D(0.02f, 0.92f, buf, 0.8f, 0.8f, 0.8f, 1.0f, GLUT_BITMAP_HELVETICA_12);

    // Accuracy
    float acc = (gs.shots > 0) ? 100.0f * gs.hits / gs.shots : 0.0f;
    sprintf(buf, "ACC %.0f%%", acc);
    drawText2D(0.02f, 0.88f, buf, 0.8f, 0.8f, 0.8f, 1.0f, GLUT_BITMAP_HELVETICA_12);

    // FPS
    sprintf(buf, "FPS %.0f", fps);
    drawText2D(0.02f, 0.04f, buf, 0.5f, 0.5f, 0.5f, 1.0f, GLUT_BITMAP_HELVETICA_12);

    // Combo
    if (gs.combo >= 3 && gs.comboTimer > 0)
    {
        sprintf(buf, "COMBO x%d!", gs.combo);
        float pulse = 0.7f + 0.3f * sinf(gs.totalTime * 10);
        drawText2D(0.42f, 0.85f, buf, 1.0f, pulse, 0.0f, 1.0f, GLUT_BITMAP_HELVETICA_18);
    }

    // Difficulty badge
    sprintf(buf, "LVL %d", (int)gs.difficulty);
    drawText2D(0.92f, 0.92f, buf, 0.4f, 0.8f, 1.0f, 1.0f, GLUT_BITMAP_HELVETICA_12);

    // Reloading flash
    if (gs.reloading)
    {
        float alpha = 0.4f + 0.3f * sinf(gs.totalTime * 12);
        drawRect2D(0, 0, WINDOW_W, WINDOW_H, 0.8f, 0.4f, 0, alpha * 0.2f);
    }

    // Edge damage vignette when low health
    if (gs.health < 40)
    {
        float intensity = (40 - gs.health) / 40.0f;
        // Top
        drawRect2D(0, WINDOW_H - 60, WINDOW_W, 60, 0.8f, 0, 0, intensity * 0.4f);
        // Bottom
        drawRect2D(0, 0, WINDOW_W, 60, 0.8f, 0, 0, intensity * 0.4f);
        // Left
        drawRect2D(0, 0, 60, WINDOW_H, 0.8f, 0, 0, intensity * 0.4f);
        // Right
        drawRect2D(WINDOW_W - 60, 0, 60, WINDOW_H, 0.8f, 0, 0, intensity * 0.4f);
    }

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

// ============================================================
//  SECTION 15: GAME OVER / PAUSE SCREENS
// ============================================================

void drawGameOverScreen()
{
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, WINDOW_W, 0, WINDOW_H);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Dark overlay
    drawRect2D(0, 0, WINDOW_W, WINDOW_H, 0, 0, 0, 0.75f);

    char buf[128];

    drawText2D(0.34f, 0.68f, "=== GAME OVER ===", 1.0f, 0.2f, 0.2f);

    sprintf(buf, "FINAL SCORE:  %d", gs.score);
    drawText2D(0.38f, 0.58f, buf, 1, 1, 0.3f);

    sprintf(buf, "KILLS: %d   SHOTS: %d   HITS: %d", gs.kills, gs.shots, gs.hits);
    drawText2D(0.30f, 0.50f, buf, 0.9f, 0.9f, 0.9f, 1, GLUT_BITMAP_HELVETICA_12);

    float acc = (gs.shots > 0) ? 100.0f * gs.hits / gs.shots : 0;
    sprintf(buf, "ACCURACY: %.1f%%", acc);
    drawText2D(0.40f, 0.44f, buf, 0.7f, 1.0f, 0.7f, 1, GLUT_BITMAP_HELVETICA_12);

    drawText2D(0.38f, 0.32f, "Press  R  to Restart", 0.5f, 0.9f, 1.0f, 1, GLUT_BITMAP_HELVETICA_12);
    drawText2D(0.38f, 0.26f, "Press ESC to Quit", 0.6f, 0.6f, 0.6f, 1, GLUT_BITMAP_HELVETICA_12);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

void drawPauseScreen()
{
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, WINDOW_W, 0, WINDOW_H);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    drawRect2D(0, 0, WINDOW_W, WINDOW_H, 0, 0, 0, 0.5f);
    drawText2D(0.42f, 0.58f, "PAUSED", 0.3f, 1.0f, 1.0f);
    drawText2D(0.38f, 0.48f, "Press P to Resume", 0.8f, 0.8f, 0.8f, 1, GLUT_BITMAP_HELVETICA_12);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

// ============================================================
//  SECTION 16: LIGHTING SETUP
// ============================================================

void setupLighting()
{
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHT1);
    glEnable(GL_LIGHT2);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glShadeModel(GL_SMOOTH);

    // Sun light (directional)
    GLfloat sunDir[] = {0.5f, 1.0f, 0.3f, 0.0f};
    GLfloat sunDiff[] = {0.7f, 0.75f, 0.9f, 1.0f};
    GLfloat sunAmb[] = {0.1f, 0.1f, 0.15f, 1.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, sunDir);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, sunDiff);
    glLightfv(GL_LIGHT0, GL_AMBIENT, sunAmb);

    // Blue accent point light
    GLfloat bluePos[] = {0, 5, 0, 1};
    GLfloat blueDiff[] = {0.0f, 0.3f, 0.6f, 1.0f};
    glLightfv(GL_LIGHT1, GL_POSITION, bluePos);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, blueDiff);
    glLightf(GL_LIGHT1, GL_CONSTANT_ATTENUATION, 0.0f);
    glLightf(GL_LIGHT1, GL_LINEAR_ATTENUATION, 0.02f);
    glLightf(GL_LIGHT1, GL_QUADRATIC_ATTENUATION, 0.001f);

    // Red accent point light
    GLfloat redPos[] = {20, 4, 20, 1};
    GLfloat redDiff[] = {0.6f, 0.1f, 0.0f, 1.0f};
    glLightfv(GL_LIGHT2, GL_POSITION, redPos);
    glLightfv(GL_LIGHT2, GL_DIFFUSE, redDiff);
    glLightf(GL_LIGHT2, GL_CONSTANT_ATTENUATION, 0.0f);
    glLightf(GL_LIGHT2, GL_LINEAR_ATTENUATION, 0.03f);
    glLightf(GL_LIGHT2, GL_QUADRATIC_ATTENUATION, 0.002f);

    // Specular material
    GLfloat specColor[] = {0.4f, 0.4f, 0.5f, 1.0f};
    glMaterialfv(GL_FRONT, GL_SPECULAR, specColor);
    glMateriali(GL_FRONT, GL_SHININESS, 48);
}

// ============================================================
//  SECTION 17: MAIN DISPLAY / RENDER
// ============================================================

void display()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glClearColor(0.03f, 0.05f, 0.1f, 1);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(70.0, (double)WINDOW_W / WINDOW_H, 0.05, 400.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Apply head bob to camera Y
    float bobY = sinf(cam.bobPhase) * cam.bobAmt;
    float bobX = cosf(cam.bobPhase * 0.5f) * cam.bobAmt * 0.5f;

    Vec3 eye = Vec3(cam.pos.x + bobX, cam.pos.y + bobY, cam.pos.z);
    Vec3 fwd = cam.forward();
    Vec3 target = eye + fwd;
    gluLookAt(eye.x, eye.y, eye.z,
              target.x, target.y, target.z,
              0, 1, 0);

    // FOG
    glEnable(GL_FOG);
    GLfloat fogColor[] = {0.04f, 0.06f, 0.12f, 1.0f};
    glFogfv(GL_FOG_COLOR, fogColor);
    glFogi(GL_FOG_MODE, GL_LINEAR);
    glFogf(GL_FOG_START, 40.0f);
    glFogf(GL_FOG_END, 120.0f);

    setupLighting();

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    // Draw skybox first (no depth write)
    drawSkybox();

    // Ground
    drawGround();

    // Walls
    drawWalls();

    // Decorations
    drawDecorations(gs.totalTime);

    // Targets
    for (int i = 0; i < numTargets; i++)
        drawTarget(targets[i], gs.totalTime);

    // Particles
    renderParticles();

    glDisable(GL_FOG);

    // Weapon view model (after scene, no depth clear)
    glClear(GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(70.0, (double)WINDOW_W / WINDOW_H, 0.01, 10.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    drawWeaponViewModel();

    // HUD (2D overlay)
    drawHUD();

    if (gs.gameOver)
        drawGameOverScreen();
    if (gs.paused)
        drawPauseScreen();

    glutSwapBuffers();
}

// ============================================================
//  SECTION 18: UPDATE LOOP (delta time)
// ============================================================

static float lastTime = 0;

void update(int)
{
    float now = glutGet(GLUT_ELAPSED_TIME) / 1000.0f;
    float dt = now - lastTime;
    lastTime = now;
    if (dt > 0.05f)
        dt = 0.05f; // clamp to avoid spiral of death

    envRotation += dt * 5.0f;

    if (!gs.paused && !gs.gameOver)
    {
        gs.totalTime += dt;
        gs.gameTime -= dt;
        if (gs.gameTime <= 0)
        {
            gs.gameTime = 0;
            gs.gameOver = true;
        }

        // Difficulty ramp
        gs.difficulty = 1.0f + gs.totalTime / 60.0f;

        // Update player
        updatePlayer(dt);

        // --- SOUND: update OpenAL listener position & orientation ---
        Vec3 fwd = cam.forward();
        soundUpdateListener(cam.pos.x, cam.pos.y, cam.pos.z, fwd.x, fwd.y, fwd.z);

        // --- SOUND: footstep / land events ---
        bool isMoving = (gs.lastMoveSpeed > 0.5f);
        soundUpdateMovement(cam.bobPhase, cam.onGround, isMoving, cam.velY, dt);

        // Shoot cooldown
        if (gs.shootTimer > 0)
            gs.shootTimer -= dt;
        if (gs.muzzleTimer > 0)
            gs.muzzleTimer -= dt;

        // Reload
        if (gs.reloading)
        {
            gs.reloadTimer -= dt;
            if (gs.reloadTimer <= 0)
            {
                gs.ammo = MAX_AMMO;
                gs.reloading = false;
                // --- SOUND: chamber snap on reload finish ---
                playSound(SND_RELOAD, 0.7f, 1.2f);
            }
        }

        // Combo decay
        if (gs.comboTimer > 0)
        {
            gs.comboTimer -= dt;
            if (gs.comboTimer <= 0)
                gs.combo = 0;
        }

        // Auto-fire when mouse held
        if (mouseDown)
            doShoot();

        // Update targets
        updateTargets(dt, gs.totalTime);

        // Update particles
        updateParticles(dt);

        // FPS tracking
        frameCount++;
        fpsTimer += dt;
        if (fpsTimer >= 0.5f)
        {
            fps = frameCount / fpsTimer;
            frameCount = 0;
            fpsTimer = 0;
        }
    }

    // Restart hotkey
    if (gs.gameOver && keys['r'])
    {
        initGame();
        keys['r'] = false;
    }

    glutPostRedisplay();
    glutTimerFunc(16, update, 0); // ~60 FPS
}

// ============================================================
//  SECTION 19: RESHAPE
// ============================================================

void reshape(int w, int h)
{
    if (h == 0)
        h = 1;
    glViewport(0, 0, w, h);
}

// ============================================================
//  SECTION 20: MAIN ENTRY
// ============================================================

int main(int argc, char **argv)
{
    srand((unsigned)time(nullptr));

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(WINDOW_W, WINDOW_H);
    glutCreateWindow("3D Shooting Arena");

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(onKey);
    glutKeyboardUpFunc(onKeyUp);
    glutMouseFunc(onMouseClick);
    glutPassiveMotionFunc(onMouseMove);
    glutMotionFunc(onMouseMove);
    glutSpecialFunc(onSpecialKey);
    glutSpecialUpFunc(onSpecialKeyUp);
    // Hide cursor for FPS feel
    glutSetCursor(GLUT_CURSOR_NONE);
    // Warp to center
    glutWarpPointer(WINDOW_W / 2, WINDOW_H / 2);
    lastMouseX = WINDOW_W / 2;
    lastMouseY = WINDOW_H / 2;

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_NORMALIZE);
    glEnable(GL_MULTISAMPLE);

    // --- SOUND SYSTEM INIT ---
    soundInit();
    soundInit();
    musicInit("assets/lovesong.mp3"); // ← change filename to yours
    initGame();
    lastTime = glutGet(GLUT_ELAPSED_TIME) / 1000.0f;
    glutTimerFunc(16, update, 0);

    printf("\n");
    printf("  =========================================\n");
    printf("    3D SHOOTING ARENA — CG Project\n");
    printf("  =========================================\n");
    printf("  Controls:\n");
    printf("    W/A/S/D  — Move\n");
    printf("    MOUSE    — Look around\n");
    printf("    L-CLICK  — Shoot\n");
    printf("    R        — Reload / Restart on game over\n");
    printf("    SPACE    — Jump\n");
    printf("    P        — Pause\n");
    printf("    ESC      — Quit\n");
    printf("  Compile:  g++ main.cpp -o game -lGL -lGLU -lglut -lm -lopenal -lsndfile\n");
    printf("  =========================================\n\n");

    glutMainLoop();
    musicShutdown(); // ← add this line
    soundShutdown(); // cleanup (reached if glutMainLoop returns)
    return 0;
}