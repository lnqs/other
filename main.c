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

static void clear_terminal()
{
    printf("\033[1J");
}

static void goto_xy(int x, int y)
{
    printf("\033[%i;%iH", y, x);
}

static void set_color(int color)
{
    printf("\033[%im", 30 + color);
}

static void draw_pixel(int x, int y, int color, int weight)
{
    if (x > 0 && y > 0 && x <= terminal_width && y <= terminal_height)
    {
        goto_xy(x, y);
        set_color(color);
        printf("\xe2\x96%c", 0x91 + weight); // gets one of ░, ▒ or ▓
                                             // for weight of 0, 1 or 2
    }
}

static void draw_circle(int x, int y, int radius, int color, int weight)
{
    // Yep, this wastes incredible amounts of cpu-time for calculating
    // the same pixels over and over again. But it's less code than Bresenham's,
    // so it's kept :o)
    for (float angle = 0; angle < 2 * pi; angle += 0.05f)
    {
        float x2 = roundf(cos(angle) * radius) + x;
        float y2 = roundf(sin(angle) * radius) + y;
        draw_pixel(x2, y2, color, weight);
    }
}

static void draw_smooth_circle(int x, int y, int radius, int color)
{
    for (int i = -2; i <= 2; i++)
    {
        draw_circle(x, y, radius + i, color, abs(i));
    }
}

static void update_graphics(unsigned int position)
{
    for (int i = 0; i < 3; i++)
    {
        int radius = (position >> i | position) % 20;
        int x = (((position / (radius + 1)) >> i) | i) % terminal_width;
        int y = (((position / (radius + 1)) >> i) | i) % terminal_height;
        int color = (position + i) / 12 % 7 + 1;

        draw_smooth_circle(x, y, radius, color);
    }
}

int main(int argc, char** argv)
{
    clear_terminal();
    snd_pcm_t* alsa_handle = init_alsa();

    for (unsigned int position = 0; position < 1024; position++)
    {
        update_graphics(position);
        update_sound(alsa_handle, position);
    }
}
