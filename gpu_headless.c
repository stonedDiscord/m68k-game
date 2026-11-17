/* Headless GPU backend: no hardware access, waits or rendering.
 * Reads return zero so absent video hardware cannot pass diagnostics. */
#include "gpu.h"

/* RAM backing also keeps legacy direct register access off the bus. */
static volatile uint16_t acrtc_register;
static volatile uint8_t ramdac_register;
volatile uint16_t *hd63484_addr = &acrtc_register;
volatile uint16_t *hd63484_control = &acrtc_register;
volatile uint8_t *ramdac_index = &ramdac_register;
volatile uint8_t *ramdac_palette = &ramdac_register;
volatile uint8_t *ramdac_mask = &ramdac_register;
int16_t hd63484_screen_height = 0;
uint16_t stringx = 8;
uint16_t stringy = 8;

void hd63484_write_ar(uint8_t reg_addr) { (void)reg_addr; }
uint16_t hd63484_read_sr(void) { return 0; }
void hd63484_write_reg(uint8_t reg_addr, uint16_t value) { (void)reg_addr; (void)value; }
uint16_t hd63484_read_reg(uint8_t reg_addr) { (void)reg_addr; return 0; }
void hd63484_wait_wfr(void) { }
void hd63484_wait_wfe(void) { }
void hd63484_wait_rfr(void) { }
void hd63484_wait_ced(void) { }
void hd63484_fifo_write(uint16_t word) { (void)word; }
uint16_t hd63484_fifo_read(void) { return 0; }
void hd63484_reset(void) { }
void hd63484_abort(void) { }
void hd63484_start(void) { }
void hd63484_stop(void) { }
void hd63484_init(void) { }
void hd63484_wpr(uint8_t pr_addr, uint16_t value) { (void)pr_addr; (void)value; }
uint16_t hd63484_rpr(uint8_t pr_addr) { (void)pr_addr; return 0; }
void hd63484_wptn(uint8_t pra, uint8_t n, const uint16_t *data) { (void)pra; (void)n; (void)data; }
void hd63484_rptn(uint8_t pra, uint8_t n, uint16_t *data) { (void)pra; (void)n; (void)data; }
void hd63484_set_origin(uint8_t dn, uint32_t addr, uint8_t dot) { (void)dn; (void)addr; (void)dot; }
void hd63484_disable_screen(uint8_t screen) { (void)screen; }
void hd63484_enable_screen(uint8_t screen, uint32_t vram_addr, uint16_t mem_width) { (void)screen; (void)vram_addr; (void)mem_width; }
void hd63484_set_color_bg(uint8_t color) { (void)color; }
void hd63484_set_color_fg(uint8_t color) { (void)color; }
void hd63484_set_edge_color(uint8_t color) { (void)color; }
void hd63484_set_mask(uint16_t mask) { (void)mask; }
void hd63484_set_compare_color(uint8_t color) { (void)color; }
void hd63484_set_area(int16_t xmin, int16_t ymin, int16_t xmax, int16_t ymax) { (void)xmin; (void)ymin; (void)xmax; (void)ymax; }
void hd63484_set_rwp(uint8_t dn, uint32_t addr) { (void)dn; (void)addr; }
void hd63484_set_dp(uint8_t dn, uint32_t addr, uint8_t dot) { (void)dn; (void)addr; (void)dot; }
void hd63484_set_solid_pattern(void) { }
void hd63484_amove(int16_t x, int16_t y) { (void)x; (void)y; }
void hd63484_rmove(int16_t dx, int16_t dy) { (void)dx; (void)dy; }
void hd63484_aline(int16_t x, int16_t y, uint8_t area, uint8_t col, uint8_t opm) { (void)x; (void)y; (void)area; (void)col; (void)opm; }
void hd63484_rline(int16_t dx, int16_t dy, uint8_t area, uint8_t col, uint8_t opm) { (void)dx; (void)dy; (void)area; (void)col; (void)opm; }
void hd63484_arct(int16_t x, int16_t y, uint8_t area, uint8_t col, uint8_t opm) { (void)x; (void)y; (void)area; (void)col; (void)opm; }
void hd63484_rrct(int16_t dx, int16_t dy, uint8_t area, uint8_t col, uint8_t opm) { (void)dx; (void)dy; (void)area; (void)col; (void)opm; }
void hd63484_afrct(int16_t x, int16_t y, uint8_t area, uint8_t col, uint8_t opm) { (void)x; (void)y; (void)area; (void)col; (void)opm; }
void hd63484_rfrct(int16_t dx, int16_t dy, uint8_t area, uint8_t col, uint8_t opm) { (void)dx; (void)dy; (void)area; (void)col; (void)opm; }
void hd63484_crcl(uint16_t radius, int clockwise, uint8_t area, uint8_t col, uint8_t opm) { (void)radius; (void)clockwise; (void)area; (void)col; (void)opm; }
void hd63484_elps(uint16_t a, uint16_t b, int16_t dx, int16_t dy, int clockwise, uint8_t area, uint8_t col, uint8_t opm) { (void)a; (void)b; (void)dx; (void)dy; (void)clockwise; (void)area; (void)col; (void)opm; }
void hd63484_dot(uint8_t area, uint8_t col, uint8_t opm) { (void)area; (void)col; (void)opm; }
void hd63484_paint(int stop_at_edge, uint8_t area, uint8_t col, uint8_t opm) { (void)stop_at_edge; (void)area; (void)col; (void)opm; }
void hd63484_ptn(int16_t sz_x, int16_t sz_y, uint8_t sl, uint8_t so, uint8_t area, uint8_t col, uint8_t opm) { (void)sz_x; (void)sz_y; (void)sl; (void)so; (void)area; (void)col; (void)opm; }
uint16_t hd63484_rd(void) { return 0; }
void hd63484_wt(uint16_t data) { (void)data; }
void hd63484_mod(uint16_t data, uint8_t mm) { (void)data; (void)mm; }
void hd63484_clr(uint16_t fill, int16_t ax, int16_t ay) { (void)fill; (void)ax; (void)ay; }
void hd63484_cpy(uint16_t sah, uint16_t sal, int16_t ax, int16_t ay) { (void)sah; (void)sal; (void)ax; (void)ay; }
void hd63484_apll(uint8_t n, const int16_t *xy, uint8_t area, uint8_t col, uint8_t opm) { (void)n; (void)xy; (void)area; (void)col; (void)opm; }
void hd63484_aplg(uint8_t n, const int16_t *xy, uint8_t area, uint8_t col, uint8_t opm) { (void)n; (void)xy; (void)area; (void)col; (void)opm; }
void hd63484_clear_screen(uint8_t color, int16_t w, int16_t h) { (void)color; (void)w; (void)h; }
void hd63484_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color) { (void)x; (void)y; (void)w; (void)h; (void)color; }
void hd63484_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color) { (void)x0; (void)y0; (void)x1; (void)y1; (void)color; }
void hd63484_set_font(const uint8_t (*font)[8]) { (void)font; }
void hd63484_draw_char(int16_t sx, int16_t sy, char c) { (void)sx; (void)sy; (void)c; }
void hd63484_draw_string(int16_t sx, int16_t sy, const char *str) { (void)sx; (void)sy; (void)str; }
void print_string(const char *str) { (void)str; }
void println(const char *str) { (void)str; }
uint8_t ramdac_read_mask(void) { return 0; }
void ramdac_reset(void) { }
