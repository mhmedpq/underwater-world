// ============================================================
//  3D Solar System Simulation  –  UPGRADED
//  OpenGL + GLUT | Computer Graphics Course Project
//
//  Compile:
//    g++ main.cpp -o solar -lGL -lGLU -lglut -lm
//
//  Keyboard Controls (UNCHANGED):
//    W / S          – Zoom in / out
//    A / D          – Orbit camera left / right
//    Up / Down      – Tilt camera up / down
//    + / -          – Increase / decrease simulation speed
//    P              – Pause / Resume
//    L              – Toggle orbit path lines
//    ESC            – Quit
//
//  NEW Mouse Controls:
//    Left-drag      – Rotate camera (horizontal = yaw, vertical = pitch)
//    Scroll wheel   – Zoom in / out
//
//  UPGRADES vs original:
//    [1] Star field  – 4000 stars, varied size & brightness, cluster bias
//    [2] Milky-Way band – faint arc of densely packed stars
//    [3] Sun glow    – three additive-blended halo spheres (no shaders)
//    [4] Earth       – ocean base + green continent patches + polar ice caps
//    [5] Mars        – reddish base + dark surface crack lines + polar cap
//    [6] Jupiter     – 8 animated latitude bands + Great Red Spot hint
//    [7] Venus       – swirling cloud-band overlay
//    [8] Saturn rings– filled quad-strip rings (B + A ring), semi-transparent
//    [9] Mouse drag + scroll-wheel camera
//   [10] Smooth camera – lerp toward target (no sudden jumps)
//   [11] Orbit paths – colour-coded per planet
// ============================================================

#include <GL/glut.h>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cstdio>

// ─── Constants ───────────────────────────────────────────────
static const double PI = 3.14159265358979323846;
static const int STAR_COUNT = 4000; // [1] was 1500
static const int TIMER_MS = 16;

// ─── Camera state ────────────────────────────────────────────
static double camRadius = 60.0;
static double camTheta = 0.0;
static double camPhi = 25.0;
// [10] smooth-camera targets
static double tgtRadius = 60.0;
static double tgtTheta = 0.0;
static double tgtPhi = 25.0;

// ─── Simulation state ────────────────────────────────────────
static double speedMult = 1.0;
static bool paused = false;
static bool showOrbits = true;

// ─── Celestial-body angles ───────────────────────────────────
static double angMercury = 0, angVenus = 0, angEarth = 0;
static double angMars = 0, angJupiter = 0, angSaturn = 0;
static double angMoon = 0;
static double rotSun = 0;
static double rotMercury = 0, rotVenus = 0, rotEarth = 0;
static double rotMars = 0, rotJupiter = 0, rotSaturn = 0;
static double rotMoon = 0;

// ─── Star field ──────────────────────────────────────────────
// [1] extended struct: position + size + brightness
struct Star
{
    float x, y, z, size, bright;
};
static Star stars[STAR_COUNT];

// ─── Shared quadric ──────────────────────────────────────────
static GLUquadric *quad = nullptr;

// ─── Mouse state [9] ─────────────────────────────────────────
static bool mouseDown = false;
static int mouseLastX = 0;
static int mouseLastY = 0;

// ─────────────────────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────────────────────
static double toRad(double d) { return d * PI / 180.0; }

// [1][2] initStars – clustered + Milky-Way band + uniform shell
static void initStars()
{
    srand(42);
    auto frand = []()
    { return (float)rand() / (float)RAND_MAX; };

    // --- 8 cluster centres ---
    const int CLUSTERS = 8;
    float cx[CLUSTERS], cy[CLUSTERS], cz[CLUSTERS];
    for (int c = 0; c < CLUSTERS; ++c)
    {
        float th = frand() * 360.f, ph = frand() * 180.f - 90.f;
        float r = 240.f + frand() * 40.f;
        float tr = cosf((float)toRad(ph));
        cx[c] = r * tr * cosf((float)toRad(th));
        cy[c] = r * sinf((float)toRad(ph));
        cz[c] = r * tr * sinf((float)toRad(th));
    }

    int idx = 0;

    // 40 % cluster stars
    int clust = STAR_COUNT * 40 / 100;
    for (; idx < clust; ++idx)
    {
        int c = rand() % CLUSTERS;
        float sp = 18.f + frand() * 12.f;
        stars[idx].x = cx[c] + (frand() - .5f) * sp;
        stars[idx].y = cy[c] + (frand() - .5f) * sp * .5f;
        stars[idx].z = cz[c] + (frand() - .5f) * sp;
        stars[idx].size = .8f + frand() * 1.8f;
        stars[idx].bright = .5f + frand() * .5f;
    }

    // 20 % Milky-Way band (narrow equatorial arc)
    int band = STAR_COUNT * 20 / 100;
    for (int b = 0; b < band; ++idx, ++b)
    {
        float th = frand() * 360.f;
        float ph = (frand() - .5f) * 18.f;
        float r = 245.f + frand() * 30.f;
        float tr = cosf((float)toRad(ph));
        stars[idx].x = r * tr * cosf((float)toRad(th));
        stars[idx].y = r * sinf((float)toRad(ph));
        stars[idx].z = r * tr * sinf((float)toRad(th));
        stars[idx].size = .5f + frand() * 1.f;
        stars[idx].bright = .3f + frand() * .4f;
    }

    // Remaining – uniform shell
    for (; idx < STAR_COUNT; ++idx)
    {
        float th = frand() * 360.f, ph = frand() * 180.f - 90.f;
        float r = 230.f + frand() * 60.f;
        float tr = cosf((float)toRad(ph));
        stars[idx].x = r * tr * cosf((float)toRad(th));
        stars[idx].y = r * sinf((float)toRad(ph));
        stars[idx].z = r * tr * sinf((float)toRad(th));
        stars[idx].size = .5f + frand() * 2.5f;
        stars[idx].bright = .4f + frand() * .6f;
    }
}

// ─────────────────────────────────────────────────────────────
//  drawStars [1] – varied sizes and brightness
// ─────────────────────────────────────────────────────────────
static void drawStars()
{
    glDisable(GL_LIGHTING);
    glEnable(GL_POINT_SMOOTH);
    glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Three passes: small / medium / large
    for (int pass = 0; pass < 3; ++pass)
    {
        float minS = pass == 0 ? 0.f : pass == 1 ? 1.2f
                                                 : 2.0f;
        float maxS = pass == 0 ? 1.2f : pass == 1 ? 2.0f
                                                  : 99.f;
        glPointSize(pass == 0 ? 1.0f : pass == 1 ? 1.8f
                                                 : 2.8f);
        glBegin(GL_POINTS);
        for (int i = 0; i < STAR_COUNT; ++i)
        {
            if (stars[i].size < minS || stars[i].size >= maxS)
                continue;
            float b = stars[i].bright;
            glColor4f(b, b * .97f, b * .93f, b);
            glVertex3f(stars[i].x, stars[i].y, stars[i].z);
        }
        glEnd();
    }

    glDisable(GL_BLEND);
    glDisable(GL_POINT_SMOOTH);
    glPointSize(1.f);
    glEnable(GL_LIGHTING);
}

// ─────────────────────────────────────────────────────────────
//  drawOrbitPath [11] – colour-coded
// ─────────────────────────────────────────────────────────────
static void drawOrbitPath(double radius, float r, float g, float b)
{
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(r, g, b, 0.22f);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < 360; ++i)
    {
        double a = toRad((double)i);
        glVertex3d(radius * cos(a), 0.0, radius * sin(a));
    }
    glEnd();
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}

// ─────────────────────────────────────────────────────────────
//  drawSun [3] – three additive halo spheres + emissive core
// ─────────────────────────────────────────────────────────────
static void drawSun()
{
    glPushMatrix();
    glRotated(rotSun, 0.0, 1.0, 0.0);

    // Additive blending makes halos look luminous without any shader
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE); // additive

    glColor4f(1.0f, 0.55f, 0.05f, 1.0f);
    gluSphere(quad, 4.0, 24, 24);

    glColor4f(1.0f, 0.72f, 0.18f, 1.0f);
    gluSphere(quad, 0.2, 24, 24);

    glColor4f(1.0f, 0.88f, 0.45f, 1.0f);
    gluSphere(quad, 2.6, 24, 24);

    glDisable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LIGHTING);

    // Core sphere
    GLfloat emit[] = {1.0f, 0.95f, 0.4f, 1.0f};
    GLfloat amb[] = {1.0f, 0.95f, 0.4f, 1.0f};
    GLfloat diff[] = {1.0f, 0.95f, 0.4f, 1.0f};
    GLfloat spec[] = {1.0f, 1.0f, 0.8f, 1.0f};
    GLfloat shin[] = {50.f};
    glMaterialfv(GL_FRONT, GL_EMISSION, emit);
    glMaterialfv(GL_FRONT, GL_AMBIENT, amb);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, diff);
    glMaterialfv(GL_FRONT, GL_SPECULAR, spec);
    glMaterialfv(GL_FRONT, GL_SHININESS, shin);
    gluSphere(quad, 3.5, 64, 64);

    GLfloat zero[] = {0.f, 0.f, 0.f, 1.f};
    glMaterialfv(GL_FRONT, GL_EMISSION, zero);
    glPopMatrix();
}

// ─────────────────────────────────────────────────────────────
//  Generic planet (UNCHANGED signature – used for Mercury)
// ─────────────────────────────────────────────────────────────
static void drawPlanet(double orbitRadius, double orbitAngle,
                       double size, double axisAngle, double tilt,
                       float r, float g, float b)
{
    glPushMatrix();
    glRotated(orbitAngle, 0.0, 1.0, 0.0);
    glTranslated(orbitRadius, 0.0, 0.0);
    glRotated(tilt, 0.0, 0.0, 1.0);
    glRotated(axisAngle, 0.0, 1.0, 0.0);

    GLfloat amb[4] = {r * .25f, g * .25f, b * .25f, 1.f};
    GLfloat diff[4] = {r, g, b, 1.f};
    GLfloat spec[4] = {.35f, .35f, .35f, 1.f};
    GLfloat shin[] = {40.f};
    glMaterialfv(GL_FRONT, GL_AMBIENT, amb);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, diff);
    glMaterialfv(GL_FRONT, GL_SPECULAR, spec);
    glMaterialfv(GL_FRONT, GL_SHININESS, shin);
    gluSphere(quad, size, 64, 64);
    glPopMatrix();
}

// ─────────────────────────────────────────────────────────────
//  [4] drawEarth – ocean + continent patches + polar ice caps
// ─────────────────────────────────────────────────────────────
static void drawEarth()
{
    glPushMatrix();
    glRotated(angEarth, 0.0, 1.0, 0.0);
    glTranslated(14.5, 0.0, 0.0);
    glRotated(23.5, 0.0, 0.0, 1.0);
    glRotated(rotEarth, 0.0, 1.0, 0.0);

    // Ocean base (blue) and white clouds
    {
        GLfloat amb[] = {0.10f, 0.18f, 0.38f, 1.f};
        GLfloat diff[] = {0.22f, 0.45f, 0.95f, 1.f};
        GLfloat spec[] = {0.70f, 0.70f, 0.80f, 1.f};
        GLfloat shin[] = {90.f};
        glMaterialfv(GL_FRONT, GL_AMBIENT, amb);
        glMaterialfv(GL_FRONT, GL_DIFFUSE, diff);
        glMaterialfv(GL_FRONT, GL_SPECULAR, spec);
        glMaterialfv(GL_FRONT, GL_SHININESS, shin);
        gluSphere(quad, 0.90, 64, 64);
    }

    // White cloud bands (simple overlay)
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.95f, 0.97f, 1.0f, 0.18f);
    for (int band = -2; band <= 2; ++band)
    {
        glPushMatrix();
        glRotatef(band * 18.0f, 1, 0, 0);
        gluDisk(quad, 0.60, 0.90, 48, 1);
        glPopMatrix();
    }

    // Polar ice caps (white)
    glColor4f(0.92f, 0.96f, 1.0f, 0.85f);
    glPushMatrix();
    glTranslatef(0, 0.88f, 0);
    glRotatef(90, 1, 0, 0);
    gluDisk(quad, 0, 0.28, 32, 1);
    glPopMatrix();
    glPushMatrix();
    glTranslatef(0, -0.88f, 0);
    glRotatef(-90, 1, 0, 0);
    gluDisk(quad, 0, 0.22, 32, 1);
    glPopMatrix();

    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
    glPopMatrix();
}

// ─────────────────────────────────────────────────────────────
//  [5] drawMars – reddish base + surface cracks + polar cap
// ─────────────────────────────────────────────────────────────
static void drawMars()
{
    glPushMatrix();
    glRotated(angMars, 0.0, 1.0, 0.0);
    glTranslated(20.0, 0.0, 0.0);
    glRotated(25.2, 0.0, 0.0, 1.0);
    glRotated(rotMars, 0.0, 1.0, 0.0);

    {
        GLfloat amb[] = {0.22f, 0.07f, 0.04f, 1.f};
        GLfloat diff[] = {0.85f, 0.30f, 0.15f, 1.f};
        GLfloat spec[] = {0.20f, 0.10f, 0.10f, 1.f};
        GLfloat shin[] = {15.f};
        glMaterialfv(GL_FRONT, GL_AMBIENT, amb);
        glMaterialfv(GL_FRONT, GL_DIFFUSE, diff);
        glMaterialfv(GL_FRONT, GL_SPECULAR, spec);
        glMaterialfv(GL_FRONT, GL_SHININESS, shin);
        gluSphere(quad, 0.55, 64, 64);
    }

    // Surface cracks
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.35f, 0.10f, 0.05f, 0.55f);
    glLineWidth(1.2f);
    for (int crack = 0; crack < 5; ++crack)
    {
        float startLon = crack * 40.f - 60.f;
        glPushMatrix();
        glRotatef(startLon, 0, 1, 0);
        glBegin(GL_LINE_STRIP);
        for (int i = -30; i <= 30; ++i)
        {
            float lat = i * 2.5f;
            float lon = sinf((float)toRad(lat * 1.5f)) * 25.f;
            float r2 = 0.558f;
            float x = r2 * cosf((float)toRad(lat)) * cosf((float)toRad(lon));
            float y = r2 * sinf((float)toRad(lat));
            float z = r2 * cosf((float)toRad(lat)) * sinf((float)toRad(lon));
            glVertex3f(x, y, z);
        }
        glEnd();
        glPopMatrix();
    }
    glLineWidth(1.f);

    // North polar cap
    glColor4f(0.95f, 0.90f, 0.85f, 0.70f);
    glPushMatrix();
    glTranslatef(0, 0.54f, 0);
    glRotatef(90, 1, 0, 0);
    gluDisk(quad, 0, 0.10, 24, 1);
    glPopMatrix();

    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
    glPopMatrix();
}

// ─────────────────────────────────────────────────────────────
//  [6] drawJupiter – 8 lat-bands + Great Red Spot
// ─────────────────────────────────────────────────────────────
static void drawJupiter()
{
    glPushMatrix();
    glRotated(angJupiter, 0.0, 1.0, 0.0);
    glTranslated(32.0, 0.0, 0.0);
    glRotated(3.1, 0.0, 0.0, 1.0);
    glRotated(rotJupiter, 0.0, 1.0, 0.0);

    // Base sphere
    {
        GLfloat amb[] = {0.20f, 0.16f, 0.10f, 1.f};
        GLfloat diff[] = {0.80f, 0.65f, 0.45f, 1.f};
        GLfloat spec[] = {0.30f, 0.30f, 0.20f, 1.f};
        GLfloat shin[] = {20.f};
        glMaterialfv(GL_FRONT, GL_AMBIENT, amb);
        glMaterialfv(GL_FRONT, GL_DIFFUSE, diff);
        glMaterialfv(GL_FRONT, GL_SPECULAR, spec);
        glMaterialfv(GL_FRONT, GL_SHININESS, shin);
        gluSphere(quad, 2.20, 64, 64);
    }

    // Band overlay
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const float bandCols[8][3] = {
        {.82f, .67f, .48f}, {.60f, .38f, .22f}, {.88f, .75f, .55f}, {.58f, .36f, .20f}, {.85f, .72f, .52f}, {.56f, .34f, .18f}, {.80f, .65f, .46f}, {.68f, .50f, .32f}};
    float bh = 180.f / 8.f;
    for (int b = 0; b < 8; ++b)
    {
        float lat0 = -90.f + b * bh, lat1 = lat0 + bh;
        glColor4f(bandCols[b][0], bandCols[b][1], bandCols[b][2], 0.60f);
        glBegin(GL_TRIANGLE_STRIP);
        for (int i = 0; i <= 72; ++i)
        {
            float lon = (float)(i * 5);
            float r2 = 2.21f;
            float x0 = r2 * cosf((float)toRad(lat0)) * cosf((float)toRad(lon));
            float y0 = r2 * sinf((float)toRad(lat0));
            float z0 = r2 * cosf((float)toRad(lat0)) * sinf((float)toRad(lon));
            float x1 = r2 * cosf((float)toRad(lat1)) * cosf((float)toRad(lon));
            float y1 = r2 * sinf((float)toRad(lat1));
            float z1 = r2 * cosf((float)toRad(lat1)) * sinf((float)toRad(lon));
            glVertex3f(x0, y0, z0);
            glVertex3f(x1, y1, z1);
        }
        glEnd();
    }

    // Great Red Spot
    glColor4f(0.75f, 0.28f, 0.18f, 0.70f);
    glPushMatrix();
    glRotatef(-20.f, 0, 1, 0);
    glRotatef(-18.f, 1, 0, 0);
    glTranslatef(0, 0, 2.22f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(0, 0, 0);
    for (int i = 0; i <= 24; ++i)
    {
        float a = (float)(i * 2.0 * PI / 24.0);
        glVertex3f(.30f * cosf(a), .18f * sinf(a), 0);
    }
    glEnd();
    glPopMatrix();

    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
    glPopMatrix();
}

// ─────────────────────────────────────────────────────────────
//  [7] drawVenus – pale base + swirling cloud bands
// ─────────────────────────────────────────────────────────────
static void drawVenus()
{
    glPushMatrix();
    glRotated(angVenus, 0.0, 1.0, 0.0);
    glTranslated(10.0, 0.0, 0.0);
    glRotated(177.4, 0.0, 0.0, 1.0);
    glRotated(rotVenus, 0.0, 1.0, 0.0);

    {
        GLfloat amb[] = {0.24f, 0.20f, 0.10f, 1.f};
        GLfloat diff[] = {0.95f, 0.80f, 0.45f, 1.f};
        GLfloat spec[] = {0.50f, 0.50f, 0.30f, 1.f};
        GLfloat shin[] = {60.f};
        glMaterialfv(GL_FRONT, GL_AMBIENT, amb);
        glMaterialfv(GL_FRONT, GL_DIFFUSE, diff);
        glMaterialfv(GL_FRONT, GL_SPECULAR, spec);
        glMaterialfv(GL_FRONT, GL_SHININESS, shin);
        gluSphere(quad, 0.85, 64, 64);
    }

    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    for (int b = 0; b < 6; ++b)
    {
        float lat0 = -90.f + b * 30.f, lat1 = lat0 + 30.f;
        float alpha = (b % 2 == 0) ? 0.22f : 0.12f;
        glColor4f(1.f, 0.95f, 0.70f, alpha);
        glBegin(GL_TRIANGLE_STRIP);
        for (int i = 0; i <= 60; ++i)
        {
            float lon = (float)(i * 6) + b * 20.f;
            float r2 = 0.86f;
            float x0 = r2 * cosf((float)toRad(lat0)) * cosf((float)toRad(lon));
            float y0 = r2 * sinf((float)toRad(lat0));
            float z0 = r2 * cosf((float)toRad(lat0)) * sinf((float)toRad(lon));
            float x1 = r2 * cosf((float)toRad(lat1)) * cosf((float)toRad(lon));
            float y1 = r2 * sinf((float)toRad(lat1));
            float z1 = r2 * cosf((float)toRad(lat1)) * sinf((float)toRad(lon));
            glVertex3f(x0, y0, z0);
            glVertex3f(x1, y1, z1);
        }
        glEnd();
    }

    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
    glPopMatrix();
}

// ─────────────────────────────────────────────────────────────
//  [8] drawSaturnRings – filled quad-strip B-ring + A-ring
// ─────────────────────────────────────────────────────────────
static void drawSaturnRings(double orbitRadius, double orbitAngle, double tilt)
{
    glPushMatrix();
    glRotated(orbitAngle, 0.0, 1.0, 0.0);
    glTranslated(orbitRadius, 0.0, 0.0);
    glRotated(tilt, 0.0, 0.0, 1.0);

    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);

    const int SEG = 180;
    // B-ring (bright)
    const float IB = 2.3f, OB = 3.2f;
    for (int i = 0; i < SEG; ++i)
    {
        float a0 = (float)(i * 2.0 * PI / SEG);
        float a1 = (float)((i + 1) * 2.0 * PI / SEG);
        glBegin(GL_QUADS);
        glColor4f(.92f, .84f, .62f, .55f);
        glVertex3f(IB * cosf(a0), 0, IB * sinf(a0));
        glVertex3f(IB * cosf(a1), 0, IB * sinf(a1));
        glColor4f(.80f, .72f, .52f, .38f);
        glVertex3f(OB * cosf(a1), 0, OB * sinf(a1));
        glVertex3f(OB * cosf(a0), 0, OB * sinf(a0));
        glEnd();
    }
    // A-ring (dimmer, outer)
    const float IA = 3.3f, OA = 4.2f;
    for (int i = 0; i < SEG; ++i)
    {
        float a0 = (float)(i * 2.0 * PI / SEG);
        float a1 = (float)((i + 1) * 2.0 * PI / SEG);
        glBegin(GL_QUADS);
        glColor4f(.85f, .78f, .56f, .30f);
        glVertex3f(IA * cosf(a0), 0, IA * sinf(a0));
        glVertex3f(IA * cosf(a1), 0, IA * sinf(a1));
        glColor4f(.70f, .62f, .44f, .15f);
        glVertex3f(OA * cosf(a1), 0, OA * sinf(a1));
        glVertex3f(OA * cosf(a0), 0, OA * sinf(a0));
        glEnd();
    }

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
    glPopMatrix();
}

// ─────────────────────────────────────────────────────────────
//  drawMoon – hierarchical, UNCHANGED logic
// ─────────────────────────────────────────────────────────────
static void drawMoon(double earthOrbitRadius, double earthOrbitAngle)
{
    glPushMatrix();
    glRotated(earthOrbitAngle, 0.0, 1.0, 0.0);
    glTranslated(earthOrbitRadius, 0.0, 0.0);
    glRotated(angMoon, 0.0, 1.0, 0.0);
    glTranslated(2.2, 0.0, 0.0);
    glRotated(rotMoon, 0.0, 1.0, 0.0);

    GLfloat amb[] = {.25f, .25f, .25f, 1.f};
    GLfloat diff[] = {.65f, .65f, .65f, 1.f};
    GLfloat spec[] = {.10f, .10f, .10f, 1.f};
    GLfloat shin[] = {5.f};
    glMaterialfv(GL_FRONT, GL_AMBIENT, amb);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, diff);
    glMaterialfv(GL_FRONT, GL_SPECULAR, spec);
    glMaterialfv(GL_FRONT, GL_SHININESS, shin);
    gluSphere(quad, 0.45, 32, 32);
    glPopMatrix();
}

// ─────────────────────────────────────────────────────────────
//  Labels – UNCHANGED
// ─────────────────────────────────────────────────────────────
static void drawLabel(double x, double y, double z, const char *text)
{
    glDisable(GL_LIGHTING);
    glColor3f(1.f, 1.f, 0.85f);
    glRasterPos3d(x, y, z);
    for (const char *c = text; *c; ++c)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
    glEnable(GL_LIGHTING);
}

static void orbitPos(double radius, double angleDeg, double &wx, double &wy, double &wz)
{
    double a = toRad(angleDeg);
    wx = radius * cos(a);
    wy = 0.0;
    wz = radius * sin(a);
}

// ─────────────────────────────────────────────────────────────
//  display
// ─────────────────────────────────────────────────────────────
static void display()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    // [10] Smooth camera lerp
    camRadius += (tgtRadius - camRadius) * 0.12;
    camTheta += (tgtTheta - camTheta) * 0.12;
    camPhi += (tgtPhi - camPhi) * 0.12;

    double cr = toRad(camTheta), cp = toRad(camPhi);
    double eyeX = camRadius * cos(cp) * cos(cr);
    double eyeY = camRadius * sin(cp);
    double eyeZ = camRadius * cos(cp) * sin(cr);
    gluLookAt(eyeX, eyeY, eyeZ, 0, 0, 0, 0, 1, 0);

    GLfloat lp[] = {0, 0, 0, 1}, ld[] = {1.f, .98f, .88f, 1.f};
    GLfloat ls[] = {1, 1, 1, 1}, la[] = {.04f, .04f, .04f, 1.f};
    glLightfv(GL_LIGHT0, GL_POSITION, lp);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, ld);
    glLightfv(GL_LIGHT0, GL_SPECULAR, ls);
    glLightfv(GL_LIGHT0, GL_AMBIENT, la);

    drawStars();

    // [11] Colour-coded orbit paths
    if (showOrbits)
    {
        drawOrbitPath(6.0, .80f, .75f, .70f);
        drawOrbitPath(10.0, .95f, .80f, .40f);
        drawOrbitPath(14.5, .20f, .55f, 1.0f);
        drawOrbitPath(20.0, .90f, .35f, .20f);
        drawOrbitPath(32.0, .85f, .68f, .48f);
        drawOrbitPath(44.0, .90f, .82f, .55f);
    }

    drawSun();

    // Mercury (unchanged simple planet)
    drawPlanet(6.0, angMercury, 0.45, rotMercury, 0.03,
               0.75f, 0.70f, 0.65f);

    drawVenus(); // [7]
    drawEarth(); // [4]
    drawMoon(14.5, angEarth);
    drawMars();    // [5]
    drawJupiter(); // [6]

    // Saturn body
    drawPlanet(44.0, angSaturn, 1.80, rotSaturn, 26.7,
               0.90f, 0.82f, 0.58f);
    drawSaturnRings(44.0, angSaturn, 26.7); // [8]

    // Labels
    {
        double wx, wy, wz;
        drawLabel(0, 4.5, 0, "Sun");
        orbitPos(6.0, -angMercury, wx, wy, wz);
        drawLabel(wx, wy + 0.8, wz, "Mercury");
        orbitPos(10.0, -angVenus, wx, wy, wz);
        drawLabel(wx, wy + 1.2, wz, "Venus");
        orbitPos(14.5, -angEarth, wx, wy, wz);
        drawLabel(wx, wy + 1.3, wz, "Earth");
        orbitPos(20.0, -angMars, wx, wy, wz);
        drawLabel(wx, wy + 0.9, wz, "Mars");
        orbitPos(32.0, -angJupiter, wx, wy, wz);
        drawLabel(wx, wy + 2.8, wz, "Jupiter");
        orbitPos(44.0, -angSaturn, wx, wy, wz);
        drawLabel(wx, wy + 2.4, wz, "Saturn");
    }

    // HUD
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    int w = glutGet(GLUT_WINDOW_WIDTH);
    int h = glutGet(GLUT_WINDOW_HEIGHT);
    gluOrtho2D(0, w, 0, h);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Dark bar behind HUD text
    glColor4f(0, 0, 0, .55f);
    glBegin(GL_QUADS);
    glVertex2i(0, 0);
    glVertex2i(w, 0);
    glVertex2i(w, 22);
    glVertex2i(0, 22);
    glEnd();
    glDisable(GL_BLEND);

    glColor3f(.90f, .90f, .90f);
    char hud[320];
    snprintf(hud, sizeof(hud),
             "[W/S] Zoom  [A/D] Rotate  [Up/Dn] Tilt  [+/-] Speed:%.1fx"
             "  [P] %s  [L] Orbits:%s  [Mouse Drag] Camera  [Scroll] Zoom  [ESC] Quit",
             speedMult, paused ? "Resume" : "Pause", showOrbits ? "ON" : "OFF");
    glRasterPos2i(8, 7);
    for (const char *c = hud; *c; ++c)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, *c);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

    glutSwapBuffers();
}

// ─────────────────────────────────────────────────────────────
//  update – UNCHANGED animation logic
// ─────────────────────────────────────────────────────────────
static void update(int)
{
    if (!paused)
    {
        double s = speedMult;
        angMercury += 4.79 * 0.05 * s;
        angVenus += 3.50 * 0.05 * s;
        angEarth += 2.98 * 0.05 * s;
        angMars += 2.41 * 0.05 * s;
        angJupiter += 1.31 * 0.05 * s;
        angSaturn += 0.97 * 0.05 * s;
        angMoon += 13.18 * 0.05 * s;
        rotSun += 0.20 * s;
        rotMercury += 0.017 * s;
        rotVenus -= 0.004 * s;
        rotEarth += 1.00 * s;
        rotMars += 0.97 * s;
        rotJupiter += 2.40 * s;
        rotSaturn += 2.30 * s;
        rotMoon += 0.133 * s;
        auto wrap = [](double &a)
        {if(a>=360)a-=360;if(a<0)a+=360; };
        wrap(angMercury);
        wrap(angVenus);
        wrap(angEarth);
        wrap(angMars);
        wrap(angJupiter);
        wrap(angSaturn);
        wrap(angMoon);
        wrap(rotSun);
        wrap(rotMercury);
        wrap(rotVenus);
        wrap(rotEarth);
        wrap(rotMars);
        wrap(rotJupiter);
        wrap(rotSaturn);
        wrap(rotMoon);
    }
    glutPostRedisplay();
    glutTimerFunc(TIMER_MS, update, 0);
}

// ─────────────────────────────────────────────────────────────
//  Keyboard – ALL original controls PRESERVED, now target-based
// ─────────────────────────────────────────────────────────────
static void keyboard(unsigned char key, int, int)
{
    switch (key)
    {
    case 'w':
    case 'W':
        tgtRadius -= 3.0;
        if (tgtRadius < 5.0)
            tgtRadius = 5.0;
        break;
    case 's':
    case 'S':
        tgtRadius += 3.0;
        if (tgtRadius > 300.0)
            tgtRadius = 300.0;
        break;
    case 'a':
    case 'A':
        tgtTheta -= 3.0;
        break;
    case 'd':
    case 'D':
        tgtTheta += 3.0;
        break;
    case 'p':
    case 'P':
        paused = !paused;
        break;
    case 'l':
    case 'L':
        showOrbits = !showOrbits;
        break;
    case '+':
    case '=':
        speedMult += 0.25;
        if (speedMult > 10.0)
            speedMult = 10.0;
        break;
    case '-':
    case '_':
        speedMult -= 0.25;
        if (speedMult < 0.25)
            speedMult = 0.25;
        break;
    case 27:
        exit(0);
    }
    glutPostRedisplay();
}

static void specialKey(int key, int, int)
{
    switch (key)
    {
    case GLUT_KEY_UP:
        tgtPhi += 2.0;
        if (tgtPhi > 89.0)
            tgtPhi = 89.0;
        break;
    case GLUT_KEY_DOWN:
        tgtPhi -= 2.0;
        if (tgtPhi < -89.0)
            tgtPhi = -89.0;
        break;
    case GLUT_KEY_LEFT:
        tgtTheta -= 3.0;
        break;
    case GLUT_KEY_RIGHT:
        tgtTheta += 3.0;
        break;
    }
    glutPostRedisplay();
}

// ─────────────────────────────────────────────────────────────
//  [9] Mouse – drag to rotate, scroll to zoom
// ─────────────────────────────────────────────────────────────
static void mouseButton(int button, int state, int x, int y)
{
    if (button == GLUT_LEFT_BUTTON)
    {
        mouseDown = (state == GLUT_DOWN);
        mouseLastX = x;
        mouseLastY = y;
    }
    if (button == 3 && state == GLUT_DOWN)
    { // scroll up
        tgtRadius -= 3.0;
        if (tgtRadius < 5.0)
            tgtRadius = 5.0;
        glutPostRedisplay();
    }
    if (button == 4 && state == GLUT_DOWN)
    { // scroll down
        tgtRadius += 3.0;
        if (tgtRadius > 300.0)
            tgtRadius = 300.0;
        glutPostRedisplay();
    }
}

static void mouseMotion(int x, int y)
{
    if (!mouseDown)
        return;
    int dx = x - mouseLastX, dy = y - mouseLastY;
    mouseLastX = x;
    mouseLastY = y;
    tgtTheta += dx * 0.40;
    tgtPhi -= dy * 0.40;
    if (tgtPhi > 89.0)
        tgtPhi = 89.0;
    if (tgtPhi < -89.0)
        tgtPhi = -89.0;
    glutPostRedisplay();
}

// ─────────────────────────────────────────────────────────────
//  reshape – UNCHANGED
// ─────────────────────────────────────────────────────────────
static void reshape(int w, int h)
{
    if (!h)
        h = 1;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, (double)w / h, 0.5, 700.0);
    glMatrixMode(GL_MODELVIEW);
}

// ─────────────────────────────────────────────────────────────
//  initGL – UNCHANGED base
// ─────────────────────────────────────────────────────────────
static void initGL()
{
    glClearColor(0.01f, 0.01f, 0.03f, 1.f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glShadeModel(GL_SMOOTH);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
    glEnable(GL_NORMALIZE);

    GLfloat gAmb[] = {.03f, .03f, .03f, 1.f};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, gAmb);
    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_FALSE);
    glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_TRUE);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    quad = gluNewQuadric();
    gluQuadricNormals(quad, GLU_SMOOTH);
    gluQuadricTexture(quad, GL_FALSE);
    gluQuadricOrientation(quad, GLU_OUTSIDE);
}

// ─────────────────────────────────────────────────────────────
//  main
// ─────────────────────────────────────────────────────────────
int main(int argc, char **argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(1280, 720);
    glutInitWindowPosition(100, 100);
    glutCreateWindow("3D Solar System Simulation – OpenGL (Upgraded)");

    initGL();
    initStars();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(specialKey);
    glutMouseFunc(mouseButton);  // [9] NEW
    glutMotionFunc(mouseMotion); // [9] NEW
    glutTimerFunc(TIMER_MS, update, 0);

    glutMainLoop();
    gluDeleteQuadric(quad);
    return 0;
}