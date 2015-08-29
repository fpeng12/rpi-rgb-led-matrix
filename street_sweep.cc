// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
// Small example how write text.
//
// This code is public domain
// (but note, that the led-matrix library this depends on is GPL v2)

#include "led-matrix.h"
#include "graphics.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
//#include <dos.h>
#include <ctime>


using namespace rgb_matrix;

static int usage(const char *progname) {
  fprintf(stderr, "usage: %s [options]\n", progname);
  fprintf(stderr, "Reads text from stdin and displays it. "
          "Empty string: clear screen\n");
  fprintf(stderr, "Options:\n"
          "\t-f <font-file>: Use given font.\n"
          "\t-r <rows>     : Display rows. 16 for 16x32, 32 for 32x32. "
          "Default: 32\n"
          "\t-P <parallel> : For Plus-models or RPi2: parallel chains. 1..3. "
          "Default: 1\n"
          "\t-c <chained>  : Daisy-chained boards. Default: 1.\n"
          "\t-x <x-origin> : X-Origin of displaying text (Default: 0)\n"
          "\t-y <y-origin> : Y-Origin of displaying text (Default: 0)\n"
          "\t-C <r,g,b>    : Color. Default 255,255,0\n");
  return 1;
}

static bool parseColor(Color *c, const char *str) {
  return sscanf(str, "%hhu,%hhu,%hhu", &c->r, &c->g, &c->b) == 3;
}

int main(int argc, char *argv[]) {
  Color color(255, 255, 0);
  const char *bdf_font_file = NULL;
  int rows = 32;
  int chain = 1;
  int parallel = 1;
  int x_orig = 0;
  int y_orig = -1;

  int opt;
  while ((opt = getopt(argc, argv, "r:P:c:x:y:f:C:")) != -1) {
    switch (opt) {
    case 'r': rows = atoi(optarg); break;
    case 'P': parallel = atoi(optarg); break;
    case 'c': chain = atoi(optarg); break;
    case 'x': x_orig = atoi(optarg); break;
    case 'y': y_orig = atoi(optarg); break;
    case 'f': bdf_font_file = strdup(optarg); break;
    case 'C':
      if (!parseColor(&color, optarg)) {
        fprintf(stderr, "Invalid color spec.\n");
        return usage(argv[0]);
      }
      break;
    default:
      return usage(argv[0]);
    }
  }

  if (bdf_font_file == NULL) {
    fprintf(stderr, "Need to specify BDF font-file with -f\n");
    return usage(argv[0]);
  }

  /*
   * Load font. This needs to be a filename with a bdf bitmap font.
   */
  rgb_matrix::Font font;
  if (!font.LoadFont(bdf_font_file)) {
    fprintf(stderr, "Couldn't load font '%s'\n", bdf_font_file);
    return usage(argv[0]);
  }

  if (rows != 16 && rows != 32) {
    fprintf(stderr, "Rows can either be 16 or 32\n");
    return 1;
  }

  if (chain < 1) {
    fprintf(stderr, "Chain outside usable range\n");
    return 1;
  }
  if (chain > 8) {
    fprintf(stderr, "That is a long chain. Expect some flicker.\n");
  }
  if (parallel < 1 || parallel > 3) {
    fprintf(stderr, "Parallel outside usable range.\n");
    return 1;
  }

  /*
   * Set up GPIO pins. This fails when not running as root.
   */
  GPIO io;
  if (!io.Init())
    return 1;
    
  /*
   * Set up the RGBMatrix. It implements a 'Canvas' interface.
   */
  RGBMatrix *canvas = new RGBMatrix(&io, rows, chain, parallel);

  bool all_extreme_colors = true;
  all_extreme_colors &= color.r == 0 || color.r == 255;
  all_extreme_colors &= color.g == 0 || color.g == 255;
  all_extreme_colors &= color.b == 0 || color.b == 255;
  if (all_extreme_colors)
    canvas->SetPWMBits(1);

  const int x = x_orig;
  const int usable_x = 32 - x;
  int y = y_orig;

  if (isatty(STDIN_FILENO)) {
    // Only give a message if we are interactive. If connected via pipe, be quiet
    printf("Enter lines. Full screen or empty line clears screen.\n"
           "Supports UTF-8. CTRL-D for exit.\n");
  }

  if ((y + font.height() > canvas->height())) {
    canvas->Clear();
    y = y_orig;
  }

  while (1) {
    char line[1024] = "Yooo! No street sweeping this week!!";
    if (wkday == 1) {
      line = "Take the trash out!";
    }
    
    time_t t = time(0);
    struct tm * now = localtime(&t);

    int day = now->tm_mday; // day in month 1--31
    int wkday = no+w->tm_wday; // day of week 0--6, 0==Sunday
    int hr = now->tm_hour;
    bool sweep_wed = false, sweep_thur = false;
    if (day-wkday>=19 && day-wkday<=25) { // Wed falls in this week
      sweep_wed = true;
      sweep_thur = true;
    }
    if (sweep_thur == false) { // if true already, skip this
      if (day-wkday>=18 && day-wkday<=24) {
        sweep_thur = true;
      }
    }
    if (sweep_wed || sweep_thur) {
      if (sweep_thur) {
        if ((day==3 && hr>=15) || (day==4 && hr<=15)) {
          line = "Move your car to the other side of the street RIGHT NOW!!" // before 2:30pm
        }
      } else {
        if (day==2) {
          line = "Street sweep tomorrow, move your car to our side of the street!!"
        }
        if (day==3 && hr<=15) {
          line = "Move your car to our side of the street RIGHT NOW!!" // before 2:30pm
        }
      }
      if (day==1) {
        line = "Street sweep this week, get ready! And take the trash out."
      }
    }

    int ct = 0;
    int refresh_int = 0.3;
    int length = 0;
    for (int i = 0; i < strlen(line); i++) {
      const uint32_t cp = utf8_next_codepoint(line[i]);
      length += font.CharacterWidth(cp);
    } 
    while (ct < 60 * 5) { // switch every 5 mins
      int shift = 0;
      while (shift + usable_x < length) {
        rgb_matrix::DrawText(canvas, font, x, y + font.baseline(), color, line, shift);
      //   y += font.height();
      }
      sleep(refresh_int);
      ct += refresh_int;
    }
  }

  // Finished. Shut down the RGB matrix.
  canvas->Clear();
  delete canvas;

  return 0;
}
