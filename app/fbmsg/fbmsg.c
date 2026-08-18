/*
 * fbmsg - scrive un messaggio a tutto schermo sul framebuffer.
 *
 * Serve durante l'espansione della microSD al primo avvio: quel lavoro dura
 * parecchio e prima non si vedeva niente, quindi chi guardava uno schermo
 * nero poteva pensare che la console fosse bloccata e spegnerla a meta'.
 *
 * Perche' non basta un echo: in questo kernel la console testuale sul
 * framebuffer (CONFIG_FRAMEBUFFER_CONSOLE) NON e' compilata, quindi scrivere
 * su /dev/tty1 non produce niente a video. Qui si disegna direttamente.
 *
 * Uso:  fbmsg "PRIMA RIGA" "SECONDA RIGA" ...
 *       fbmsg -q ...              non lamentarsi se il framebuffer non c'e'
 *       fbmsg -t L A file.ppm ... disegna su file invece che sullo schermo,
 *                                 per poter collaudare a occhio il risultato
 *
 * Le righe vengono centrate. Minuscole e accenti non esistono: si scrive in
 * maiuscolo, e quello che non e' nel carattere diventa uno spazio.
 *
 * Se il framebuffer non si apre, il programma esce in silenzio: un avviso
 * mancato non deve impedire l'espansione.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>

#include "font8x8.h"

#define FB_DEVICE "/dev/fb0"

/* Colori: sfondo scuro, testo chiaro, prima riga in risalto. */
static const uint8_t BG[3]      = {  0,   0,  32 };
static const uint8_t FG[3]      = {220, 220, 220 };
static const uint8_t WARN[3]    = {255, 200,  40 };

static struct fb_var_screeninfo vinfo;
static struct fb_fix_screeninfo finfo;
static uint8_t* fbmem = NULL;

/* Modalita' di collaudo: si disegna in memoria e si salva un'immagine, cosi'
   il risultato si guarda invece di darlo per buono. */
static int  test_mode = 0;
static const char* test_file = NULL;

static void write_ppm(void)
{
  FILE* f = fopen(test_file, "wb");
  if(!f) return;

  fprintf(f, "P6\n%u %u\n255\n", vinfo.xres, vinfo.yres);

  for(unsigned y = 0; y < vinfo.yres; ++y)
    for(unsigned x = 0; x < vinfo.xres; ++x)
    {
      const uint8_t* p = fbmem + (long)y * finfo.line_length + (long)x * 4;
      const uint8_t rgb[3] = { p[vinfo.red.offset   / 8],
                               p[vinfo.green.offset / 8],
                               p[vinfo.blue.offset  / 8] };
      fwrite(rgb, 1, 3, f);
    }

  fclose(f);
}

static void put_pixel(int x, int y, const uint8_t* rgb)
{
  if(x < 0 || y < 0 || x >= (int)vinfo.xres || y >= (int)vinfo.yres)
    return;

  uint8_t* p = fbmem + (long)y * finfo.line_length
                     + (long)x * (vinfo.bits_per_pixel / 8);

  if(vinfo.bits_per_pixel == 32 || vinfo.bits_per_pixel == 24)
  {
    /* Si rispettano le posizioni dichiarate dal driver invece di dare per
       scontato ARGB: su questi chip capita di trovare BGR. */
    p[vinfo.red.offset   / 8] = rgb[0];
    p[vinfo.green.offset / 8] = rgb[1];
    p[vinfo.blue.offset  / 8] = rgb[2];
    if(vinfo.bits_per_pixel == 32 && vinfo.transp.length)
      p[vinfo.transp.offset / 8] = 0xff;
  }
  else if(vinfo.bits_per_pixel == 16)
  {
    const uint16_t v = (uint16_t)(((rgb[0] >> 3) << 11) |
                                  ((rgb[1] >> 2) <<  5) |
                                   (rgb[2] >> 3));
    *(uint16_t*)p = v;
  }
}

static void clear_screen(void)
{
  for(unsigned y = 0; y < vinfo.yres; ++y)
    for(unsigned x = 0; x < vinfo.xres; ++x)
      put_pixel((int)x, (int)y, BG);
}

static int glyph_index(char c)
{
  if(c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');

  const char* p = strchr(FONT_CHARS, c);
  return p ? (int)(p - FONT_CHARS) : 0;   /* sconosciuto -> spazio */
}

static void draw_char(char c, int x0, int y0, int scale, const uint8_t* rgb)
{
  const int g = glyph_index(c);

  for(int row = 0; row < GLYPH_H; ++row)
  {
    const char* bits = FONT_ROWS[g * GLYPH_H + row];

    for(int col = 0; col < GLYPH_W; ++col)
    {
      if(bits[col] == ' ') continue;

      for(int dy = 0; dy < scale; ++dy)
        for(int dx = 0; dx < scale; ++dx)
          put_pixel(x0 + col * scale + dx, y0 + row * scale + dy, rgb);
    }
  }
}

static void draw_line(const char* text, int y0, int scale, const uint8_t* rgb)
{
  const int len = (int)strlen(text);
  const int w   = len * GLYPH_W * scale;
  int x0 = ((int)vinfo.xres - w) / 2;

  if(x0 < 0) x0 = 0;

  for(int i = 0; i < len; ++i)
    draw_char(text[i], x0 + i * GLYPH_W * scale, y0, scale, rgb);
}

int main(int argc, char** argv)
{
  int quiet = 0, first = 1;
  /* dichiarati qui, non dopo il salto: altrimenti il compilatore avvisa che
     potrebbero essere usati senza valore */
  int    fd     = -1;
  size_t fbsize = 0;

  if(argc > 1 && strcmp(argv[1], "-q") == 0) { quiet = 1; first = 2; }

  if(argc > 4 && strcmp(argv[1], "-t") == 0)
  {
    test_mode = 1;
    vinfo.xres = (unsigned)atoi(argv[2]);
    vinfo.yres = (unsigned)atoi(argv[3]);
    test_file  = argv[4];
    first      = 5;
  }

  if(first >= argc) return 0;   /* niente da scrivere */

  if(test_mode)
  {
    vinfo.bits_per_pixel = 32;
    vinfo.red.offset = 16; vinfo.green.offset = 8; vinfo.blue.offset = 0;
    vinfo.transp.length = 0;
    finfo.line_length = vinfo.xres * 4;
    fbsize = (size_t)finfo.line_length * vinfo.yres;
    fbmem = calloc(fbsize, 1);
    if(!fbmem) return 1;
    goto disegna;
  }

  fd = open(FB_DEVICE, O_RDWR);
  if(fd < 0)
  {
    if(!quiet)
      fprintf(stderr, "fbmsg: %s: %s\n", FB_DEVICE, strerror(errno));
    return 1;
  }

  if(ioctl(fd, FBIOGET_VSCREENINFO, &vinfo) != 0 ||
     ioctl(fd, FBIOGET_FSCREENINFO, &finfo) != 0)
  {
    if(!quiet) fprintf(stderr, "fbmsg: non leggo i parametri dello schermo\n");
    close(fd);
    return 1;
  }

  if(vinfo.bits_per_pixel != 16 && vinfo.bits_per_pixel != 24 &&
     vinfo.bits_per_pixel != 32)
  {
    if(!quiet) fprintf(stderr, "fbmsg: %u bit per pixel non gestiti\n",
                       vinfo.bits_per_pixel);
    close(fd);
    return 1;
  }

  fbsize = (size_t)finfo.line_length * vinfo.yres;
  fbmem = mmap(NULL, fbsize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

  if(fbmem == MAP_FAILED)
  {
    if(!quiet) fprintf(stderr, "fbmsg: mmap: %s\n", strerror(errno));
    close(fd);
    return 1;
  }

disegna:
  ;  /* un'etichetta vuole una istruzione, non una dichiarazione: il
        compilatore del PC lo perdona, quello ARM no */

  /* La dimensione del carattere si adatta allo schermo: alla riga piu' lunga
     si lasciano due caratteri di margine per parte. */
  int longest = 1;
  for(int i = first; i < argc; ++i)
  {
    const int l = (int)strlen(argv[i]);
    if(l > longest) longest = l;
  }

  int scale = (int)vinfo.xres / ((longest + 4) * GLYPH_W);
  if(scale < 1) scale = 1;
  if(scale > 8) scale = 8;

  const int nlines    = argc - first;
  const int lineStep  = GLYPH_H * scale * 2;      /* una riga vuota fra le righe */
  /* l'ultima riga non ha lo spazio sotto: contarlo sposterebbe tutto in alto */
  const int blockH    = (nlines - 1) * lineStep + GLYPH_H * scale;
  int y = ((int)vinfo.yres - blockH) / 2;
  if(y < 0) y = 0;

  clear_screen();

  for(int i = first; i < argc; ++i, y += lineStep)
    draw_line(argv[i], y, scale, (i == first) ? WARN : FG);

  if(test_mode)
  {
    write_ppm();
    free(fbmem);
    return 0;
  }

  munmap(fbmem, fbsize);
  close(fd);
  return 0;
}
