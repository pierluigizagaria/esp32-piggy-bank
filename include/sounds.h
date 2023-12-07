#define SOUNDS                            \
  SOUND(HORN, "horn.mp3")                 \
  SOUND(KIDS, "kids.mp3")                 \
  SOUND(HAPPY_WHEELS, "happy-wheels.mp3") \
  SOUND(SAD_HORN, "sad-horn.mp3")         \
  SOUND(FART_1, "fart1.mp3")              \
  SOUND(FART_2, "fart2.mp3")

enum sounds
{
#define SOUND(x, y) x,
  SOUNDS
#undef SOUND
};

const char *const sounds[] = {
#define SOUND(x, y) y,
    SOUNDS
#undef SOUND
};