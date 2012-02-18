#define ALSA_PCM_NEW_HW_PARAMS_API
#include <alsa/asoundlib.h>
#include <math.h>
#include <stdbool.h>
#include <alloca.h>
#include <limits.h>

const float pi = 3.14159265358979323846f;
const int terminal_width = 80;
const int terminal_height = 25;

snd_pcm_t* init_alsa()
{
    // We really should add error-handling to the following calls,
    // but this would be a lot of additional code :/
    snd_pcm_t* handle;
    snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
    snd_pcm_hw_params_t* params;
    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(handle, params);
    unsigned int rate = 44100;
    snd_pcm_hw_params_set_rate_near(handle, params, &rate, NULL);
    snd_pcm_uframes_t size = 32688;
    snd_pcm_hw_params_set_buffer_size_max(handle, params, &size);
    snd_pcm_hw_params(handle, params);

    // It may look strange that we fill alsa's internal buffer with zeros, but
    // there's a good reason to do it. Since the call to snd_pcm_writei() in
    // update_sound() handles our timing, it has to be full when we enter it
    // initially. Otherwise, it would return too fast and the first frames would
    // be way shorter. This is ugly, yeah, but I don't see an approach that wouldn't
    // require lots of additional code.
    char buffer[512] = { 0, };
    snd_pcm_nonblock(handle, 1);
    while(snd_pcm_writei(handle, buffer, sizeof(buffer)) == sizeof(buffer));
    snd_pcm_nonblock(handle, 0);

    return handle;
}

static void fill_sound_buffer(unsigned int position, char* buffer, size_t size)
{
    for (size_t i = 0; i < size; i++)
    {
        unsigned int t = position * size + i;
        buffer[i] = sinf(2.0f * pi * 1.0f / (46 * (t & (t >> 12))) * t) * CHAR_MAX;
    }
}

static void update_sound(snd_pcm_t* handle, unsigned int position)
{
    char buffer[8172]; // this arrays size controls the frame-rate

    fill_sound_buffer(position, buffer, sizeof(buffer));

    // This call implicitly handles the timing. Since it doesn't return until
    // all data is written or an error occurs and the time it takes for the buffer
    // to get empty is fixed, it guarantees that we also have a fixed frame-rate
    snd_pcm_writei(handle, buffer, sizeof(buffer));
}

static void init_terminal()
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("\033[1J");
}

static void goto_xy(int x, int y)
{
    // terminal-coordinates start from (1, 1), what sucks.
    printf("\033[%i;%iH", y + 1, x + 1);
}

static void set_color(int color)
{
    printf("\033[%im", 30 + color);
}

static void draw_pixel(int x, int y, int color, int weight)
{
    if (x >= 0 && y >= 0 && x <= terminal_width && y <= terminal_height)
    {
        goto_xy(x, y);
        set_color(color);
        printf("\xe2\x96%c", 0x91 + weight); // becomes one of ░, ▒ or ▓
                                             // for weight of 0, 1 or 2
    }
}

static void draw_circle(int x, int y, int radius_x, int radius_y, int color, int weight)
{
    // Yep, this wastes incredible amounts of cpu-time for calculating
    // the same pixels over and over again. But it's less code than Bresenham's,
    // so it's kept :o)
    for (float angle = 0; angle < 2 * pi; angle += 0.05f)
    {
        float x2 = roundf(cos(angle) * radius_x) + x;
        float y2 = roundf(sin(angle) * radius_y) + y;
        draw_pixel(x2, y2, color, weight);
    }
}

static void draw_smooth_circle(int x, int y, int radius_x, int radius_y, int color)
{
    for (int i = -2; i <= 2; i++)
    {
        draw_circle(x, y, radius_x + i, radius_y + i, color, abs(i));
    }
}

static void draw_rectangle(int x, int y, int width, int height, int color)
{
    for (int x2 = 0; x2 < width; x2++)
    {
        for (int y2 = 0; y2 < height; y2++)
        {
            draw_pixel(x + x2, y + y2, color, 2);
        }
    }
}

// TODO: cleanup/refactor
static bool intro_scene(unsigned int position)
{
    static const char text1[] = "Lynx presents:";
    static const char text2[] = "O T H E R";

    if (position > 8)
    {
        goto_xy(terminal_width / 2 - sizeof(text1) / 2, terminal_height / 2 - 1);
        puts(text1);
    }

    if (position > 16)
    {
        goto_xy(terminal_width / 2 - sizeof(text2) / 2, terminal_height / 2 + 1);
        puts(text2);
    }

    return position < 32;
}

// TODO: cleanup/refactor
static bool chessboard_scene(unsigned int position)
{
    static const int width = 8;
    static const int height = 4;

    int x = position % (terminal_width / width) * width;
    int y = position % (terminal_height / height + 1) * height;
    int color = (position >> 2 | position) % 8;

    draw_rectangle(x, y, width, height, color);

    return position < 128;
}

// TODO: cleanup/refactor
static bool circle_scene(unsigned int position)
{
    for (int i = 0; i < 3; i++)
    {
        int radius_x = (position >> i | position) % 20;
        int radius_y = radius_x * 0.75f;
        int x = (((position / (radius_x + 1)) >> i) | i) % terminal_width;
        int y = (((position / (radius_y + 1)) >> i) | i) % terminal_height;
        int color = (position + i) / 12 % 8;

        draw_smooth_circle(x, y, radius_x, radius_y, color);
    }

    return position < 256;
}

// TODO: cleanup/refactor
static bool smilie_scene(unsigned int position)
{
    if (position == 256)
    {
        int eye_width = terminal_height / 3;
        int eye_height = position != 256;
        int eye_offset_x = terminal_width / 3.5f;
        int eye_offset_y = terminal_height / 4;

        draw_smooth_circle(eye_offset_x, eye_offset_y, eye_width, eye_height, 5);
        draw_smooth_circle(terminal_width - eye_offset_x, eye_offset_y, eye_width, eye_height, 5);
    }

    int mouth_width = terminal_height / 5 * ((position - 256) / 2);
    int mouth_height = position - 256;
    int mouth_offset_x = terminal_width / 2;
    int mouth_offset_y = terminal_height * 0.65f;

    draw_smooth_circle(mouth_offset_x, mouth_offset_y, mouth_width + 2, mouth_height + 2, 5);
    draw_smooth_circle(mouth_offset_x, mouth_offset_y, mouth_width, mouth_height, 0);

    return true;
}

// TODO: cleanup/refactor
int main(int argc, char** argv)
{
    typedef bool (*scene_fn)(unsigned int);
    static const scene_fn scenes[] = {
            &intro_scene,
            &chessboard_scene,
            &circle_scene,
            &smilie_scene,
            NULL
    };
    int current_scene = 0;

    init_terminal();
    snd_pcm_t* alsa_handle = init_alsa();

    unsigned int position = 0;
    while (scenes[current_scene])
    {
        update_sound(alsa_handle, position);

        if (!scenes[current_scene](position))
        {
            current_scene++;
        }

        position++;
    }
}

