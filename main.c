#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <SDL2/SDL.h>

#define retoss(ptr, t, n) (ptr = (t*) realloc((ptr), (n) * sizeof(t)))

#define toss(t, n) ((t*) malloc((n) * sizeof(t)))

#define zero(a) (memset(&(a), 0, sizeof(a)))

#define len(a) ((int) (sizeof(a) / sizeof(*a)))

typedef struct
{
    float x;
    float y;
}
Point;

typedef struct
{
    int y; // Notice y and x are in reverse
    int x; // here, contrary to the Point type.
    float val;
}
Atom;

typedef struct
{
    float** mesh;
    int rows;
    int cols;
    int res;
    int aura;
    float hscent;
    float wscent;
    float sscent;
}
Field;

typedef struct
{
    const char** walling;
    int rows;
    int cols;
}
Map;

typedef struct
{
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* texture;
}
Sdl;

typedef struct
{
    Point where;
    Point last;
    Point velocity;
    float speed;
    float acceleration;
}
Sprite;

typedef struct
{
    Sprite* sprite;
    int count;
    int max;
}
Sprites;

static Point sub(const Point a, const Point b)
{
    Point out;
    out.x = a.x - b.x;
    out.y = a.y - b.y;
    return out;
}

static Point add(const Point a, const Point b)
{
    Point out;
    out.x = a.x + b.x;
    out.y = a.y + b.y;
    return out;
}

static float mag(const Point a)
{
    return sqrtf(a.x * a.x + a.y * a.y);
}

static Point mul(const Point a, const float n)
{
    Point out;
    out.x = a.x * n;
    out.y = a.y * n;
    return out;
}

static Point unt(const Point a)
{
    return mul(a, 1.0f / mag(a));
}

static Map build()
{
    static const char* m[] = {
        "######################################################",
        "#                                                    #",
        "#                                                    #",
        "#        ############# # # #                         #",
        "#                    # # # #                         #",
        "#        ########### # # # #                         #",
        "#                  # # # # #                         #",
        "#                  # # # # #                         #",
        "#                  # # # # #                         #",
        "#                  # # # # #                         #",
        "#                  # # # # #                         #",
        "#                  # # # # #                         #",
        "#                  # # # # #                         #",
        "#                  # # # # #                         #",
        "#                  # # # # #          # # # # #      #",
        "#                  # # # # #          # # # # #      #",
        "#                  # # # # #         ## # # # ##     #",
        "#                  # # # # #         #         #     #",
        "#                  # # # # #         #         #     #",
        "#                  # # # # #         #         #     #",
        "#     ###########  # # # # #         #         #     #",
        "#                  # # # # #         #         #     #",
        "#                  # # # # #         #         #     #",
        "#                  # # # # #         #         #     #",
        "#                  # # # # #         ###########     #",
        "#    ############### # # # #                         #",
        "#                                                    #",
        "#                                                    #",
        "######################################################",
    };
    Map map;
    zero(map);
    map.rows = len(m);
    map.cols = strlen(m[0]);
    map.walling = m;
    return map;
}

static int on(const Field field, const int y, const int x)
{
    return y >= 0 && x >= 0 && y < field.rows && x < field.cols;
}

static float average(const Field field, const int y, const int x)
{
    float sum = 0.0f;
    int sums = 0;
    for(int j = y - 1; j <= y + 1; j++)
    for(int i = x - 1; i <= x + 1; i++)
    {
        if(!on(field, j, i))
            continue;
        if(j == y && i == x)
            continue;
        sum += field.mesh[j][i];
        sums++;
    }
    return sum / sums;
}

static Atom materialize(const Field field, const int y, const int x)
{
    const Atom atom = { y, x, average(field, y, x) };
    return atom;
}

static Field prepare(const Map map, const int aura)
{
    Field field;
    field.res = 10;
    field.rows = field.res * map.rows;
    field.cols = field.res * map.cols;
    field.mesh = toss(float*, field.rows);
    field.aura = field.res * aura;
    for(int j = 0; j < field.rows; j++)
        field.mesh[j] = toss(float, field.cols);
    field.hscent = +1e30;
    field.wscent = -5.0;
    field.sscent = -1.0;
    return field;
}

static void box(const Field field, const int y, const int x, const int w)
{
    Atom* const atoms = toss(Atom, 8 * w);
    int count = 0;
    const int t = y - w; // Top.
    const int b = y + w; // Bottom.
    const int l = x - w; // Left.
    const int r = x + w; // Right.
    for(int j = t; j <= b; j++)
    for(int i = l; i <= r; i++)
        if((i == l || j == t || i == r || j == b)
        && on(field, j, i)
        && field.mesh[j][i] == 0.0f)
            atoms[count++] = materialize(field, j, i);
    for(int a = 0; a < count; a++)
        field.mesh[atoms[a].y][atoms[a].x] = atoms[a].val;
    free(atoms);
}

static int largest(float* gradients, const int size)
{
    float max = -FLT_MAX;
    int index = 0;
    for(int i = 0; i < size; i++)
        if(gradients[i] > max)
            max = gradients[i], index = i;
    return index;
}

static Point force(const Field field, const Point from, const Point to, const Map map)
{
    const Point dead = { 0.0f, 0.0f };
    const float dist = mag(sub(from, to));
    if(dist < 1.33f || dist > (field.aura / field.res) - 1)
        return dead;
    const Point v[] = {
        { +1.0f, -0.0f }, // E
        { +1.0f, +1.0f }, // SE
        { +0.0f, +1.0f }, // S
        { -1.0f, +1.0f }, // SW
        { -1.0f, +0.0f }, // W
        { -1.0f, -1.0f }, // NW
        { +0.0f, -1.0f }, // N
        { +1.0f, -1.0f }, // NE
    };
    float grads[len(v)];
    zero(grads);
    for(int i = 0; i < len(v); i++)
    {
        const Point dir = add(v[i], from);
        const int y = field.res * from.y, yy = field.res * dir.y;
        const int x = field.res * from.x, xx = field.res * dir.x;
        if(on(field, yy, xx)) grads[i] = field.mesh[yy][xx] - field.mesh[y][x];
    }
    const Point grad = v[largest(grads, len(v))];
    const Point where = add(grad, from);
    const int xx = where.x;
    const int yy = where.y;
    return map.walling[yy][xx] != ' ' ? dead : grad;
}

static void diffuse(const Field field, const Point where)
{
    const int y = field.res * where.y;
    const int x = field.res * where.x;
    for(int w = 1; w <= field.aura; w++)
        box(field, y, x, w);
}

static void fclear(const Field field)
{
    for(int j = 0; j < field.rows; j++)
    for(int i = 0; i < field.cols; i++)
        field.mesh[j][i] = 0.0f;
}

static void swall(const Field field, const Map map)
{
    for(int j = 0; j < field.rows; j++)
    for(int i = 0; i < field.cols; i++)
    {
        const int x = i / field.res;
        const int y = j / field.res;
        if(map.walling[y][x] != ' ')
            field.mesh[j][i] = field.wscent;
    }
}

static void ssprt(const Field field, const Sprites sprites)
{
    for(int s = 0; s < sprites.count; s++)
    {
        Sprite* const sprite = &sprites.sprite[s];
        const int j = field.res * sprite->where.y;
        const int i = field.res * sprite->where.x;
        for(int a = -field.res / 2; a <= field.res / 2; a++)
        for(int b = -field.res / 2; b <= field.res / 2; b++)
            // Sprite scents stack.
            if(on(field, j + a, i + b))
                field.mesh[j + a][i + b] += field.sscent;
    }
}

static void shero(const Field field, const Point where)
{
    const int j = field.res * where.y;
    const int i = field.res * where.x;
    field.mesh[j][i] = field.hscent;
}

static void ddot(uint32_t* pixels, const int width, const Point where, const int size, const uint32_t in, const uint32_t out)
{
    for(int y = -size; y <= size; y++)
    for(int x = -size; x <= size; x++)
    {
        const int xx = x + where.x;
        const int yy = y + where.y;
        pixels[xx + yy * width] = in;
        if(x == -size) pixels[xx + yy * width] = out;
        if(x == +size) pixels[xx + yy * width] = out;
        if(y == -size) pixels[xx + yy * width] = out;
        if(y == +size) pixels[xx + yy * width] = out;
    }
}

static void dfield(const Sdl sdl, const Field field, const Map map, const Sprites sprites)
{
    void* screen;
    int pitch;
    SDL_LockTexture(sdl.texture, NULL, &screen, &pitch);
    const int width = pitch / sizeof(uint32_t);
    uint32_t* pixels = (uint32_t*) screen;
    const uint32_t wht = 0XFFDFEFD7;
    const uint32_t blk = 0XFF000000;
    const uint32_t red = 0XFFD34549;
    // Field and map.
    for(int y = 0; y < field.rows; y++)
    for(int x = 0; x < field.cols; x++)
    {
        const float scent = field.mesh[y][x];
        pixels[x + y * width] = scent > 0.0f ? red : blk;
        const int yy = y / field.res;
        const int xx = x / field.res;
        if(map.walling[yy][xx] == '#')
            pixels[x + y * width] = wht;
    }
    // Sprites.
    for(int s = 0; s < sprites.count; s++)
    {
        const Point where = {
            field.res * sprites.sprite[s].where.x,
            field.res * sprites.sprite[s].where.y,
        };
        ddot(pixels, width, where, 3, wht, blk);
    }
    SDL_UnlockTexture(sdl.texture);
    SDL_RenderCopy(sdl.renderer, sdl.texture, NULL, NULL);
    SDL_RenderPresent(sdl.renderer);
}

static Sdl init(const int xres, const int yres)
{
    SDL_Init(SDL_INIT_VIDEO);
    Sdl sdl;
    zero(sdl);
    sdl.window = SDL_CreateWindow(
        "Pather",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        xres,
        yres,
        SDL_WINDOW_SHOWN);
    sdl.renderer = SDL_CreateRenderer(
        sdl.window,
        -1,
        SDL_RENDERER_ACCELERATED);
    sdl.texture = SDL_CreateTexture(
        sdl.renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        xres,
        yres);
    return sdl;
}

static Sprites snew(const int max)
{
    const Sprites sprites = { toss(Sprite, max), 0, max };
    return sprites;
}

static Sprite born(const Point where)
{
    Sprite s;
    zero(s);
    s.where = where;
    s.last = where;
    s.speed = 0.3;
    s.acceleration = 0.02;
    return s;
}

static Sprites append(Sprites sprites, const Sprite sprite)
{
    if(sprites.max == 0)
        retoss(sprites.sprite, Sprite, sprites.max = 1);
    if(sprites.count >= sprites.max)
        retoss(sprites.sprite, Sprite, sprites.max *= 2);
    sprites.sprite[sprites.count++] = sprite;
    return sprites;
}

static int blok(const Point a, const char** const blocks)
{
    const int y = a.y;
    const int x = a.x;
    return blocks[y][x];
}

static int tile(const Point a, const char** const blocks)
{
    return blok(a, blocks) - ' ';
}

static void place(Sprite* const sprite, const Point to)
{
    sprite->last = sprite->where;
    sprite->where = to;
}

static int fl(const float x)
{
    return (int) x - (x < (int) x);
}

static Point mid(const Point a)
{
    Point out;
    out.x = fl(a.x) + 0.5f;
    out.y = fl(a.y) + 0.5f;
    return out;
}

static void bound(const Sprites sprites, const Map map)
{
    for(int i = 0; i < sprites.count; i++)
    {
        Sprite* const sprite = &sprites.sprite[i];
        // Stuck in a wall.
        if(tile(sprite->where, map.walling))
        {
            place(sprite, mid(sprite->last));
            zero(sprite->velocity);
        }
    }
}

static void move(const Sprites sprites, const Field field, const Point to, const Map map)
{
    for(int i = 0; i < sprites.count; i++)
    {
        Sprite* const sprite = &sprites.sprite[i];
        const Point dir = force(field, sprite->where, to, map);
        // No force of direction...
        if(dir.x == 0.0f && dir.y == 0.0f)
        {
            // Some sprites do not move.
            if(sprite->speed == 0.0f)
                continue;
            // Then slow down.
            sprite->velocity = mul(sprite->velocity, 1.0f - sprite->acceleration / sprite->speed);
        }
        // Otherwise speed up.
        else
        {
            const Point acc = mul(dir, sprite->acceleration);
            sprite->velocity = add(sprite->velocity, acc);
            // And then check top speed...
            if(mag(sprite->velocity) > sprite->speed)
                // And cap speed if the top speed is surpassed.
                sprite->velocity = mul(unt(sprite->velocity), sprite->speed);
        }
        place(sprite, add(sprite->where, sprite->velocity));
    }
}

static void route(const Field field, const Map map, const Point where, const Sprites sprites)
{
    fclear(field);
    swall(field, map);
    ssprt(field, sprites);
    shero(field, where);
    diffuse(field, where);
    move(sprites, field, where, map);
    bound(sprites, map);
}

static Sprites create()
{
    Sprites sprites = snew(10);
    const Point wheres[] = {
        {  2.5f,  2.5f },
        {  3.5f,  4.5f },
        {  4.5f,  2.5f },
        {  7.5f,  4.5f },
        {  8.5f,  3.5f },
        {  8.5f,  3.5f },
        { 24.5f,  3.5f },
        { 40.5f, 20.5f },
        { 43.5f, 22.5f },
        { 42.5f, 23.5f },
        { 41.5f, 22.5f },
        { 45.5f, 23.5f },
        { 43.5f, 22.5f },
    };
    for(int s = 0; s < len(wheres); s++)
        sprites = append(sprites, born(wheres[s]));
    return sprites;
}

static int done(const uint8_t* key, const SDL_Event e)
{
    return key[SDL_SCANCODE_ESCAPE] || key[SDL_SCANCODE_END] || e.type == SDL_QUIT;
}

static Point mouse(const int res)
{
    int x;
    int y;
    SDL_GetMouseState(&x, &y);
    const Point where = {
        (float) x / res,
        (float) y / res,
    };
    return where;
}

int main(int argc, char* argv[])
{
    (void) argc;
    (void) argv;
    const Map map = build();
    const Field field = prepare(map, 10);
    const Sdl sdl = init(field.cols, field.rows);
    const Sprites sprites = create();
    SDL_WarpMouseInWindow(sdl.window, field.cols / 2, field.rows / 2);
    SDL_Event e;
    for(const uint8_t* key = SDL_GetKeyboardState(NULL); !done(key, e); SDL_PollEvent(&e))
    {
        const int t0 = SDL_GetTicks();
        const Point cursor = mouse(field.res);
        route(field, map, cursor, sprites);
        dfield(sdl, field, map, sprites);
        const int t1 = SDL_GetTicks();
        const int ms = 1000.0f / 60.0f - (t1 - t0);
        SDL_Delay(ms < 0 ? 0 : ms);
    }
    return 0;
}
