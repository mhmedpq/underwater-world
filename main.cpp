//* Name : Muhammed Awad Farag Hamed
//* ID : 232611000037
//* This program simulates an underwater scene with animated fish and bubbles using OpenGL and GLUT.
//*  The user can interact with the scene by adding more fish, generating more bubbles, changing fish colors, or resetting the scene.
//* Controls:
//*   F   – add a new fish
//*   B   – add more bubbles
//*   C   – randomise fish colours
//*   R   – reset the scene
//*   ESC – exit
//* how to compile: g++ main.cpp -o underwater -lGL -lGLU -lglut
//* run the program: ./underwater

#include <GL/glut.h>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <vector>
#include <cstdio>

static const int WIN_W = 1920;
static const int WIN_H = 1080;
static const float PI = 3.14159265f;
static const int TIMER_MS = 16;
static const int MAX_FISH = 100;
static const int INIT_BUBBLES = 300;

struct Fish
{
    float x, y;
    float baseY;
    float speed;
    float direction;
    float size;
    float r, g, b;
    float time;
};

struct Bubble
{
    float x, y;
    float speed;
    float radius;
};

static std::vector<Fish> fishes;
static std::vector<Bubble> bubbles;
static float globalTime = 0.0f;

static float randRange(float x, float y)
{
    return x + (y - x) * (static_cast<float>(rand()) / RAND_MAX);
}

static void drawFilledCircle(float cx, float cy, float radius, int segments = 32)
{
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    for (int i = 0; i <= segments; ++i)
    {
        float angle = 2.0f * PI * i / segments;
        glVertex2f(cx + radius * cosf(angle),
                   cy + radius * sinf(angle));
    }
    glEnd();
}

static void drawCircleOutline(float cx, float cy, float radius, int segments = 32)
{
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < segments; ++i)
    {
        float angle = 2.0f * PI * i / segments;
        glVertex2f(cx + radius * cosf(angle),
                   cy + radius * sinf(angle));
    }
    glEnd();
}

static Fish makeFish(float x, float y)
{
    Fish f;
    f.x = x;
    f.y = y;
    f.baseY = y;
    f.speed = randRange(1.0f, 2.0f);
    f.direction = (rand() % 2 == 0) ? 1.0f : -1.0f;
    f.size = randRange(0.8f, 1.9f);
    f.r = randRange(0.3f, 1.0f);
    f.g = randRange(0.2f, 1.0f);
    f.b = randRange(0.1f, 0.9f);
    f.time = randRange(0.0f, 2.5f * PI);
    return f;
}

void initFish()
{
    fishes.clear();

    struct
    {
        float x, y;
        float r, g, b;
        float size;
    } presets[] = {

        {150.0f, 400.0f, 1.0f, 0.55f, 0.0f, 2.5f},
        {500.0f, 300.0f, 0.2f, 0.8f, 1.0f, 2.1f},
        {300.0f, 200.0f, 1.0f, 0.2f, 0.4f, 1.5f},
        {650.0f, 450.0f, 0.6f, 1.0f, 0.3f, 1.4f},
        {250.0f, 500.0f, 1.0f, 0.55f, 0.0f, 2.4f},
        {400.0f, 400.0f, 0.2f, 0.8f, 1.0f, 1.2f},
        {700.0f, 300.0f, 1.0f, 0.2f, 0.4f, 2.0f},
        {750.0f, 550.0f, 0.6f, 1.0f, 0.3f, 1.66f},
    };

    for (auto &p : presets)
    {
        Fish f = makeFish(p.x, p.y);
        f.r = p.r;
        f.g = p.g;
        f.b = p.b;
        f.size = p.size;
        fishes.push_back(f);
    }
}

static Bubble makeBubble()
{
    Bubble b;
    b.x = randRange(30.0f, WIN_W - 30.0f);
    b.y = randRange(60.0f, 130.0f);
    b.speed = randRange(0.4f, 1.8f);
    b.radius = randRange(3.0f, 7.0f);
    return b;
}

void initBubbles()
{
    bubbles.clear();
    for (int i = 0; i < INIT_BUBBLES; ++i)
    {
        Bubble b = makeBubble();
        b.y = randRange(60.0f, WIN_H - 20.0f);
        bubbles.push_back(b);
    }
}
void drawRock(float x, float y, float scale)
{
    glColor3f(0.45f, 0.42f, 0.38f);

    glPushMatrix();
    glTranslatef(x, y, 0.0f);
    glScalef(scale, scale, 1.0f);

    glBegin(GL_POLYGON);
    glVertex2f(-40, 0);
    glVertex2f(40, 0);
    glVertex2f(60, 30);
    glVertex2f(20, 50);
    glVertex2f(-30, 35);
    glEnd();

    glColor3f(0.65f, 0.62f, 0.58f);
    glBegin(GL_POLYGON);
    glVertex2f(0, 20);
    glVertex2f(20, 30);
    glVertex2f(10, 40);
    glVertex2f(-10, 30);
    glEnd();

    glPopMatrix();
}

void drawBackground()
{
    glBegin(GL_QUADS);
    glColor3f(0.0f, 0.18f, 0.42f);
    glVertex2f(0, 0);
    glColor3f(0.0f, 0.18f, 0.42f);
    glVertex2f(WIN_W, 0);
    glColor3f(0.1f, 0.55f, 0.85f);
    glVertex2f(WIN_W, WIN_H);
    glColor3f(0.1f, 0.55f, 0.85f);
    glVertex2f(0, WIN_H);
    glEnd();

    glColor3f(0.76f, 0.70f, 0.50f);
    glBegin(GL_QUADS);
    glVertex2f(0, 0);
    glVertex2f(WIN_W, 0);
    glVertex2f(WIN_W, 80);
    glVertex2f(0, 80);
    glEnd();

    glColor3f(0.68f, 0.62f, 0.42f);
    for (int i = 0; i < 6; ++i)
    {
        float yy = 15.0f + i * 12.0f;
        glBegin(GL_QUADS);
        glVertex2f(0, yy);
        glVertex2f(WIN_W, yy);
        glVertex2f(WIN_W, yy + 4);
        glVertex2f(0, yy + 4);
        glEnd();
    }
    drawRock(150, 80, 1.5f);
    drawRock(400, 80, 1.3f);
    drawRock(750, 80, 1.4f);
    drawRock(1100, 80, 1.5f);
    drawRock(1300, 80, 1.2f);
    drawRock(1500, 80, 1.5f);
    drawRock(1800, 80, 1.7f);
    glEnd();
}

void drawFish(const Fish &f)
{

    glPushMatrix();
    glTranslatef(f.x, f.y, 0.0f);
    glScalef(f.direction * f.size, f.size, 1.0f);

    float tailAngle = sinf(f.time * 3.0f) * 25.0f;
    glPushMatrix();
    glTranslatef(-28.0f, 0.0f, 0.0f);
    glRotatef(tailAngle, 0.0f, 0.0f, 1.0f);
    glColor3f(f.r * 0.75f, f.g * 0.75f, f.b * 0.75f);
    glBegin(GL_TRIANGLES);
    glVertex2f(0.0f, 0.0f);
    glVertex2f(-22.0f, 16.0f);
    glVertex2f(-22.0f, -16.0f);
    glEnd();
    glPopMatrix();

    glColor3f(f.r, f.g, f.b);
    glBegin(GL_POLYGON);
    int segs = 24;
    for (int i = 0; i < segs; ++i)
    {
        float angle = 2.0f * PI * i / segs;
        glVertex2f(cosf(angle) * 28.0f,
                   sinf(angle) * 14.0f);
    }
    glEnd();

    glColor3f(fminf(f.r + 0.3f, 1.0f),
              fminf(f.g + 0.3f, 1.0f),
              fminf(f.b + 0.3f, 1.0f));
    glBegin(GL_POLYGON);
    for (int i = 0; i < segs; ++i)
    {
        float angle = 2.0f * PI * i / segs;
        glVertex2f(cosf(angle) * 16.0f,
                   sinf(angle) * 6.0f);
    }
    glEnd();

    glColor3f(f.r * 0.85f, f.g * 0.85f, f.b * 0.85f);
    glBegin(GL_TRIANGLES);
    glVertex2f(5.0f, 14.0f);
    glVertex2f(18.0f, 14.0f);
    glVertex2f(12.0f, 26.0f);
    glEnd();

    glColor3f(f.r * 0.80f, f.g * 0.80f, f.b * 0.80f);
    glBegin(GL_TRIANGLES);
    glVertex2f(5.0f, -6.0f);
    glVertex2f(18.0f, -6.0f);
    glVertex2f(10.0f, -18.0f);
    glEnd();

    glColor3f(1.0f, 1.0f, 1.0f);
    drawFilledCircle(14.0f, 4.0f, 5.5f);
    glColor3f(0.05f, 0.05f, 0.05f);
    drawFilledCircle(15.0f, 4.0f, 2.5f);
    glColor3f(1.0f, 1.0f, 1.0f);
    drawFilledCircle(16.0f, 5.5f, 1.0f);

    glColor3f(f.r * 0.6f, f.g * 0.5f, f.b * 0.5f);
    glLineWidth(1.5f);
    glBegin(GL_LINE_STRIP);
    glVertex2f(27.0f, 2.0f);
    glVertex2f(29.0f, 0.0f);
    glVertex2f(27.0f, -2.0f);
    glEnd();
    glLineWidth(1.0f);

    glPopMatrix();
}

void drawBubbles()
{
    for (const Bubble &b : bubbles)
    {
        glColor4f(0.85f, 0.95f, 1.0f, 0.18f);
        drawFilledCircle(b.x, b.y, b.radius);

        glColor4f(0.8f, 0.92f, 1.0f, 0.65f);
        glLineWidth(1.2f);
        drawCircleOutline(b.x, b.y, b.radius);
        glLineWidth(1.0f);

        glColor4f(1.0f, 1.0f, 1.0f, 0.75f);
        drawFilledCircle(b.x - b.radius * 0.35f,
                         b.y + b.radius * 0.35f,
                         b.radius * 0.22f, 8);
    }
}

void updateFish()
{
    for (Fish &f : fishes)
    {
        f.time += 0.09f;

        f.x += f.speed * f.direction;

        f.y = f.baseY + sinf(f.time * 1.5f) * 12.0f;

        float halfW = 32.0f * f.size;
        if (f.x > WIN_W + halfW)
        {
            f.direction = -1.0f;
            f.x = WIN_W + halfW;
        }
        else if (f.x < -halfW)
        {
            f.direction = 1.0f;
            f.x = -halfW;
        }

        if (f.y < 110.0f)
        {
            f.y = 110.0f;
            f.baseY = 120.0f;
        }
        if (f.y > WIN_H - 30.0f)
        {
            f.y = WIN_H - 30.0f;
            f.baseY = WIN_H - 40.0f;
        }
    }
}

void updateBubbles()
{
    for (Bubble &b : bubbles)
    {
        b.y += b.speed;
        b.x += sinf(globalTime * 1.2f + b.y * 0.05f) * 0.3f;

        if (b.y > WIN_H + b.radius)
        {
            b = makeBubble();
        }
    }
}

void display()
{
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    drawBackground();
    drawBubbles();

    for (const Fish &f : fishes)
        drawFish(f);

    glDisable(GL_BLEND);

    glutSwapBuffers();
}

void timer(int /*value*/)
{
    globalTime += 0.016f;
    updateFish();
    updateBubbles();
    glutPostRedisplay();
    glutTimerFunc(TIMER_MS, timer, 0);
}

void keyboard(unsigned char key, int /*x*/, int /*y*/)
{
    switch (key)
    {
    case 'f':
    case 'F':
        if ((int)fishes.size() < MAX_FISH)
        {
            float ny = randRange(120.0f, WIN_H - 50.0f);
            float nx = randRange(50.0f, WIN_W - 50.0f);
            fishes.push_back(makeFish(nx, ny));
        }
        break;

    case 'b':
    case 'B':
        for (int i = 0; i < 10; ++i)
            bubbles.push_back(makeBubble());
        break;

    case 'c':
    case 'C':
        for (Fish &f : fishes)
        {
            f.r = randRange(0.2f, 1.0f);
            f.g = randRange(0.2f, 1.0f);
            f.b = randRange(0.1f, 0.9f);
        }
        break;

    case 'r':
    case 'R':
        globalTime = 0.0f;
        initFish();
        initBubbles();
        break;

    case 27:
        exit(0);
        break;
    }
}

void reshape(int w, int h)
{
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0.0, WIN_W, 0.0, WIN_H);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

int main(int argc, char **argv)
{
    srand(static_cast<unsigned>(time(nullptr)));

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
    glutInitWindowSize(WIN_W, WIN_H);
    glutInitWindowPosition(0, 0);
    glutCreateWindow("232611000037");
    glClearColor(0.0f, 0.15f, 0.35f, 1.0f);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0.0, WIN_W, 0.0, WIN_H);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

    initFish();
    initBubbles();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutTimerFunc(TIMER_MS, timer, 0);

    glutMainLoop();
    return 0;
}