#define ALSA_PCM_NEW_HW_PARAMS_API
#include <alsa/asoundlib.h>
#include <math.h>
#include <stdbool.h>
#include <alloca.h>
#include <limits.h>

#define print(...) fprintf(stderr, __VA_ARGS__)

static const float pi = 3.14159265358979323846f;
static const int terminal_width = 80;
static const int terminal_height = 25;

snd_pcm_t* init_alsa()
{
    // Goodbye, portability.
    // The following buffer is used for almost everything in this function so
    // save a few bytes of code. The main problem with this is
    // that it has to be at least as big as snd_pcm_hw_params_t which is 604 on
    // my system and which i believe to be implementation defined.
    // Just using a sizeof() isn't possible, too. The declaration of the struct
    // is somewhere hidden in the implementation.
    // I feel dirty now. I'll go take a shower. Crying. :o(
    char buffer[604] = { 0, };

    // We really should add error-handling to the following calls,
    // but this would be a lot of additional code :/
    snd_pcm_t* handle;
    snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
    snd_pcm_hw_params_t* params = (snd_pcm_hw_params_t*)buffer;
    snd_pcm_hw_params_any(handle, params);
    *(unsigned int*)buffer = 44100;
    snd_pcm_hw_params_set_rate_near(handle, params, (unsigned int*)buffer, NULL);
    *(snd_pcm_uframes_t*)buffer = 32688;
    snd_pcm_hw_params_set_buffer_size_max(handle, params, (snd_pcm_uframes_t*)buffer);
    snd_pcm_hw_params(handle, params);

    // It may look strange that we fill alsa's internal buffer with zeros, but
    // there's a good reason to do it. Since the call to snd_pcm_writei() in
    // update_sound() handles our timing, it has to be full when we enter it
    // initially. Otherwise, it would return too fast and the first frames would
    // be way shorter. This is ugly, yeah, but I don't see an approach that wouldn't
    // require lots of additional code.
    while(snd_pcm_writei(handle, buffer, sizeof(buffer)) == sizeof(buffer));
    snd_pcm_nonblock(handle, 0);

    return handle;
}

static void fill_sound_buffer(unsigned short position, char* buffer, size_t size)
{
    for (size_t i = 0; i < size; i++)
    {
        size_t t = position * size + i;
        buffer[i] = sinf(2.0f * pi * 1.0f / (46 * (t & (t >> 12))) * t) * CHAR_MAX;
    }
}

static void update_sound(snd_pcm_t* handle, unsigned short position)
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
    // initially, I disabled buffering of stdout, but this created a lot of code.
    // Therefore, printing to the per-default non-buffered stderr should be fine, too
    print("\033[1J");
}

static void goto_xy(int x, int y)
{
    // terminal-coordinates start from (1, 1), what sucks.
    print("\033[%i;%iH", y + 1, x + 1);
}

static void set_color(int color)
{
    print("\033[%im", 30 + color);
}

static void draw_pixel(int x, int y, int color, int weight)
{
    if (x >= 0 && y >= 0 && x <= terminal_width && y <= terminal_height)
    {
        goto_xy(x, y);
        set_color(color);
        print("\xe2\x96%c", 0x91 + weight); // becomes one of ░, ▒ or ▓
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

// the compiler refuses to inline these, even when using the force_inline attribute.
// Since they're only used once, I implemented them as macros to avoid the calling
// overhead
#define draw_chessboard(_position) \
    do { \
        static const int width = 8; \
        static const int height = 4; \
        \
        int x = _position % (terminal_width / width) * width; \
        int y = _position % (terminal_height / height + 1) * height; \
        int color = (_position >> 2 | _position) % 8; \
        \
        draw_rectangle(x, y, width, height, color); \
    } while (false)

#define draw_circles(_position) \
    do { \
        for (int i = 0; i < 3; i++) \
        { \
            int radius_x = (_position >> i | _position) % 20; \
            int radius_y = radius_x * 0.75f; \
            int x = (((_position / (radius_x + 1)) >> i) | i) % terminal_width; \
            int y = (((_position / (radius_y + 1)) >> i) | i) % terminal_height; \
            int color = (_position + i) / 12 % 8; \
            \
            draw_smooth_circle(x, y, radius_x, radius_y, color); \
        } \
    } while (false)

// Who needs main()? :o)
void _start()
{
    init_terminal();
    snd_pcm_t* alsa_handle = init_alsa();

    for (unsigned short position = 0; position < 512; position++)
    {
        update_sound(alsa_handle, position);
        if ((position / 128) % 2 == 0)
        {
            draw_chessboard(position);
        }
        else
        {
            draw_circles(position);
        }
    }

    exit(0);
}
