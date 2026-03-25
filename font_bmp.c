/*
 * font_bmp.c — bidirectional font8x8 C header <-> 1bpp BMP converter
 *
 * Usage:
 *   font_bmp export font.h  font.bmp   (C header -> BMP for editing)
 *   font_bmp import font.bmp font.h    (BMP -> C header)
 *
 * Layout: 16 glyphs wide x 8 glyphs tall = 128x64 pixels, 1bpp BMP.
 * Compile: cc -o font_bmp font_bmp.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

#define COLS     16
#define ROWS      8
#define GLYPH_W   8
#define GLYPH_H   8
#define IMG_W   128   /* COLS * GLYPH_W */
#define IMG_H    64   /* ROWS * GLYPH_H */
#define N_GLYPHS 128

/* -------------------------------------------------------------------------
 * Minimal 1bpp BMP writer/reader — no external dependencies
 * -------------------------------------------------------------------------*/

/* 1bpp rows are padded to a multiple of 4 bytes */
#define BMP_STRIDE  ((IMG_W / 8 + 3) & ~3)   /* = 20 bytes for 128px wide */

/* File layout: BITMAPFILEHEADER(14) + BITMAPINFOHEADER(40) + palette(8) */
#define BMP_HDR_SIZE  62
#define BMP_FILE_SIZE (BMP_HDR_SIZE + BMP_STRIDE * IMG_H)

static void write_u16le(uint8_t *p, uint16_t v)
{
    p[0] = v & 0xFF; p[1] = v >> 8;
}
static void write_u32le(uint8_t *p, uint32_t v)
{
    p[0] = v & 0xFF; p[1] = (v >> 8) & 0xFF;
    p[2] = (v >> 16) & 0xFF; p[3] = v >> 24;
}
static uint16_t read_u16le(const uint8_t *p)
{
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}
static uint32_t read_u32le(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static int32_t read_i32le(const uint8_t *p)
{
    return (int32_t)read_u32le(p);
}

static uint8_t reverse_byte(uint8_t b)
{
    b = (uint8_t)(((b & 0xF0) >> 4) | ((b & 0x0F) << 4));
    b = (uint8_t)(((b & 0xCC) >> 2) | ((b & 0x33) << 2));
    b = (uint8_t)(((b & 0xAA) >> 1) | ((b & 0x55) << 1));
    return b;
}

/*
 * pixels[row][col] — row 0 = top of image, 1 = ink/black, 0 = paper/white.
 * BMP stores rows bottom-up, so we reverse on write.
 */
static int bmp_write(const char *path,
                     const uint8_t pixels[IMG_H][IMG_W])
{
    uint8_t hdr[BMP_HDR_SIZE];
    memset(hdr, 0, sizeof(hdr));

    /* BITMAPFILEHEADER */
    hdr[0] = 'B'; hdr[1] = 'M';
    write_u32le(hdr + 2,  BMP_FILE_SIZE);
    write_u32le(hdr + 10, BMP_HDR_SIZE);   /* pixel data offset */

    /* BITMAPINFOHEADER */
    write_u32le(hdr + 14, 40);             /* header size       */
    write_u32le(hdr + 18, (uint32_t)IMG_W);
    write_u32le(hdr + 22, (uint32_t)IMG_H);
    write_u16le(hdr + 26, 1);              /* color planes      */
    write_u16le(hdr + 28, 1);              /* bits per pixel    */
    /* compression=0, image size, pels/m x2, clrUsed=0, clrImportant=0 */
    write_u32le(hdr + 34, (uint32_t)(BMP_STRIDE * IMG_H));
    write_u32le(hdr + 38, 2835); write_u32le(hdr + 42, 2835); /* ~72 DPI */

    /* Color table: entry 0 = white, entry 1 = black (BGRx quads) */
    hdr[54] = 0xFF; hdr[55] = 0xFF; hdr[56] = 0xFF; /* white */
    hdr[58] = 0x00; hdr[59] = 0x00; hdr[60] = 0x00; /* black */

    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return -1; }
    if (fwrite(hdr, 1, BMP_HDR_SIZE, f) != BMP_HDR_SIZE) goto err;

    /* Write rows bottom-up */
    for (int r = IMG_H - 1; r >= 0; r--) {
        uint8_t row[BMP_STRIDE];
        memset(row, 0, BMP_STRIDE);
        for (int c = 0; c < IMG_W; c++)
            if (pixels[r][c])
                row[c >> 3] |= (uint8_t)(0x80 >> (c & 7));
        if (fwrite(row, 1, BMP_STRIDE, f) != BMP_STRIDE) goto err;
    }
    fclose(f);
    return 0;
err:
    perror(path); fclose(f); return -1;
}

static int bmp_read(const char *path,
                    uint8_t pixels[IMG_H][IMG_W])
{
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); return -1; }

    /* Read full file into memory — it's tiny (< 2 KB) */
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    rewind(f);
    uint8_t *buf = malloc((size_t)fsize);
    if (!buf) { fclose(f); return -1; }
    if ((long)fread(buf, 1, (size_t)fsize, f) != fsize)
        { free(buf); fclose(f); return -1; }
    fclose(f);

    if (buf[0] != 'B' || buf[1] != 'M') {
        fprintf(stderr, "%s: not a BMP file\n", path); goto err;
    }

    uint32_t pixel_offset = read_u32le(buf + 10);
    uint32_t hdr_size     = read_u32le(buf + 14);
    int32_t  width        = read_i32le(buf + 18);
    int32_t  height       = read_i32le(buf + 22);
    uint16_t bpp          = read_u16le(buf + 28);

    if (bpp != 1) {
        fprintf(stderr, "%s: expected 1bpp BMP, got %ubpp\n", path, bpp);
        goto err;
    }
    if (abs((int)width) != IMG_W || abs((int)height) != IMG_H) {
        fprintf(stderr, "%s: expected %dx%d, got %dx%d\n",
                path, IMG_W, IMG_H, (int)width, (int)height);
        goto err;
    }

    /*
     * Determine which palette index is ink (darker entry).
     * Palette starts at offset 14 + hdr_size, each entry is 4 bytes (BGRx).
     */
    uint32_t ct = 14 + hdr_size;
    unsigned lum0 = buf[ct+0] + buf[ct+1] + buf[ct+2]; /* entry 0 */
    unsigned lum1 = buf[ct+4] + buf[ct+5] + buf[ct+6]; /* entry 1 */
    uint8_t  ink  = (lum0 < lum1) ? 0 : 1;             /* darker = ink */

    int top_down = (height < 0);
    uint32_t stride = ((uint32_t)(abs((int)width)) / 8u + 3u) & ~3u;

    for (int r = 0; r < IMG_H; r++) {
        /* BMP default is bottom-up; flip unless top-down flag set */
        int src_row = top_down ? r : (IMG_H - 1 - r);
        const uint8_t *row = buf + pixel_offset + (uint32_t)src_row * stride;
        for (int c = 0; c < IMG_W; c++) {
            uint8_t bit = (row[c >> 3] >> (7 - (c & 7))) & 1;
            pixels[r][c] = (bit == ink) ? 1 : 0;
        }
    }
    free(buf);
    return 0;
err:
    free(buf);
    return -1;
}

/* -------------------------------------------------------------------------
 * Font C header parser
 *
 * Streams through the file looking for { 0xHH, 0xHH, ... } groups with
 * exactly 8 hex values. Does not attempt to understand C syntax beyond that.
 * -------------------------------------------------------------------------*/
static int parse_header(const char *path,
                        uint8_t glyphs[N_GLYPHS][GLYPH_H])
{
    FILE *f = fopen(path, "r");
    if (!f) { perror(path); return -1; }

    int n = 0;
    int ch;
    while (n < N_GLYPHS && (ch = fgetc(f)) != EOF) {
        if (ch != '{') continue;
        uint16_t vals[GLYPH_H];
        int count = 0;
        while (count < GLYPH_H) {
            int c2;
            while ((c2 = fgetc(f)) != EOF && c2 != '}' && c2 != '0')
                ;
            if (c2 == '}' || c2 == EOF) break;
            int c3 = fgetc(f);
            if (c3 == EOF) break;
            if (c3 != 'x' && c3 != 'X') { ungetc(c3, f); continue; }
            unsigned v = 0;
            int digits = 0, c4;
            while ((c4 = fgetc(f)) != EOF) {
                if      (c4 >= '0' && c4 <= '9') v = v*16 + (unsigned)(c4-'0');
                else if (c4 >= 'a' && c4 <= 'f') v = v*16 + (unsigned)(c4-'a'+10);
                else if (c4 >= 'A' && c4 <= 'F') v = v*16 + (unsigned)(c4-'A'+10);
                else { ungetc(c4, f); break; }
                digits++;
            }
            if (digits > 0) vals[count++] = (uint16_t)(v & 0xFFFF);
        }
        if (count == GLYPH_H) {
            /* undo the transform: un-reverse bits then un-reverse row order */
            for (int r = 0; r < GLYPH_H; r++)
                glyphs[n][GLYPH_H - 1 - r] = reverse_byte((uint8_t)(vals[r] & 0xFF));
            n++;
        }
    }
    fclose(f);

    if (n != N_GLYPHS) {
        fprintf(stderr, "%s: expected %d glyphs, found %d\n", path, N_GLYPHS, n);
        return -1;
    }
    return 0;
}

/* -------------------------------------------------------------------------
 * Font C header emitter
 * -------------------------------------------------------------------------*/
static const char *glyph_label(int i)
{
    static char buf[8];
    static const char *ctrl[] = {
        "NUL","SOH","STX","ETX","EOT","ENQ","ACK","BEL",
        "BS", "HT", "LF", "VT", "FF", "CR", "SO", "SI",
        "DLE","DC1","DC2","DC3","DC4","NAK","SYN","ETB",
        "CAN","EM", "SUB","ESC","FS", "GS", "RS", "US"
    };
    if (i < 0x20)             return ctrl[i];
    if (i == 0x7F)            return "DEL";
    if (i >= 0x20 && i < 0x7F) {
        buf[0] = '\'';
        buf[1] = (char)i;
        buf[2] = '\'';
        buf[3] = '\0';
        return buf;
    }
    snprintf(buf, sizeof(buf), "0x%02X", i);
    return buf;
}

static int emit_header(const char *path,
                       const uint8_t glyphs[N_GLYPHS][GLYPH_H])
{
    FILE *f = fopen(path, "w");
    if (!f) { perror(path); return -1; }

    fprintf(f, "/* AUTO-GENERATED by font_bmp bmp2h — do not edit by hand */\n");
    fprintf(f, "/* Precomputed HD63484 pattern RAM glyph cache.             */\n");
    fprintf(f, "/* Edit font.bmp instead, then run: make font.h             */\n");
    fprintf(f, "#ifndef FONT_H\n#define FONT_H\n#include <stdint.h>\n\n");
    fprintf(f, "static const uint16_t font8x8[%d][%d] = {\n",
            N_GLYPHS, GLYPH_H);

    for (int i = 0; i < N_GLYPHS; i++) {
        fprintf(f, "    { ");
        for (int r = 0; r < GLYPH_H; r++) {
            uint16_t v = reverse_byte(glyphs[i][GLYPH_H - 1 - r]);
            fprintf(f, "0x%04X%s", v, r < GLYPH_H - 1 ? ", " : "");
        }
        fprintf(f, " },  /* 0x%02X %s */\n", i, glyph_label(i));
    }

    fprintf(f, "};\n\n#endif /* FONT_H */\n");
    fclose(f);
    return 0;
}

static int emit_cache(const char *path,
                      const uint8_t glyphs[N_GLYPHS][GLYPH_H])
{
    FILE *f = fopen(path, "w");
    if (!f) { perror(path); return -1; }

    fprintf(f, "/* AUTO-GENERATED by font_bmp — do not edit by hand */\n");
    fprintf(f, "/* Precomputed HD63484 pattern RAM glyph cache.      */\n");
    fprintf(f, "/* Each entry: 8 x uint16_t, ready for hd63484_wptn  */\n");
    fprintf(f, "#ifndef FONT_CACHE_H\n#define FONT_CACHE_H\n#include <stdint.h>\n\n");
    fprintf(f, "static const uint16_t font8x8_cache[%d][%d] = {\n",
            N_GLYPHS, GLYPH_H);

    for (int i = 0; i < N_GLYPHS; i++) {
        fprintf(f, "    { ");
        for (int r = 0; r < GLYPH_H; r++) {
            /* reverse row order (chip Y-up) and bit order (PTN scan direction) */
            uint16_t v = reverse_byte(glyphs[i][GLYPH_H - 1 - r]);
            fprintf(f, "0x%04X%s", v, r < GLYPH_H - 1 ? ", " : "");
        }
        fprintf(f, " },  /* 0x%02X %s */\n", i, glyph_label(i));
    }

    fprintf(f, "};\n\n#endif /* FONT_CACHE_H */\n");
    fclose(f);
    return 0;
}

/* -------------------------------------------------------------------------
 * Pixel <-> glyph layout
 * -------------------------------------------------------------------------*/
static void glyphs_to_pixels(const uint8_t glyphs[N_GLYPHS][GLYPH_H],
                              uint8_t pixels[IMG_H][IMG_W])
{
    memset(pixels, 0, sizeof(uint8_t) * IMG_H * IMG_W);
    for (int idx = 0; idx < N_GLYPHS; idx++) {
        int gx = (idx % COLS) * GLYPH_W;
        int gy = (idx / COLS) * GLYPH_H;
        for (int row = 0; row < GLYPH_H; row++) {
            uint8_t byte = glyphs[idx][row];
            for (int col = 0; col < GLYPH_W; col++)
                pixels[gy + row][gx + col] = (byte >> (7 - col)) & 1;
        }
    }
}

static void pixels_to_glyphs(const uint8_t pixels[IMG_H][IMG_W],
                              uint8_t glyphs[N_GLYPHS][GLYPH_H])
{
    for (int idx = 0; idx < N_GLYPHS; idx++) {
        int gx = (idx % COLS) * GLYPH_W;
        int gy = (idx / COLS) * GLYPH_H;
        for (int row = 0; row < GLYPH_H; row++) {
            uint8_t byte = 0;
            for (int col = 0; col < GLYPH_W; col++)
                byte = (uint8_t)((byte << 1) | pixels[gy + row][gx + col]);
            glyphs[idx][row] = byte;
        }
    }
}

/* -------------------------------------------------------------------------
 * main
 * -------------------------------------------------------------------------*/
int main(int argc, char **argv)
{
    if (argc != 4 ||
        (strcmp(argv[1], "bmp2h") != 0 &&
         strcmp(argv[1], "h2bmp") != 0)) {
        fprintf(stderr,
            "Usage:\n"
            "  %s bmp2h font.bmp font.h   (BMP -> transformed C header)\n"
            "  %s h2bmp font.h  font.bmp  (C header -> BMP for editing)\n",
            argv[0], argv[0]);
        return 1;
    }

    static uint8_t glyphs[N_GLYPHS][GLYPH_H];
    static uint8_t pixels[IMG_H][IMG_W];

    if (strcmp(argv[1], "bmp2h") == 0) {
        if (bmp_read(argv[2], pixels) != 0) return 1;
        pixels_to_glyphs(pixels, glyphs);
        if (emit_header(argv[3], glyphs) != 0) return 1;
        printf("bmp2h: %s -> %s (%d glyphs)\n", argv[2], argv[3], N_GLYPHS);
    } else {
        if (parse_header(argv[2], glyphs) != 0) return 1;
        glyphs_to_pixels(glyphs, pixels);
        if (bmp_write(argv[3], pixels) != 0) return 1;
        printf("h2bmp: %s -> %s (%dx%d 1bpp BMP)\n",
               argv[2], argv[3], IMG_W, IMG_H);
    }
    return 0;
}
