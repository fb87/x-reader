#include "xr/xr_text.h"

#include <string.h>

static const xr_glyph_t *glyph_of(const xr_font_t *f, unsigned char ch)
{
    if (ch < f->first || ch > f->last) ch = '?';
    return &f->glyphs[ch - f->first];
}

int xr_text_width(const xr_font_t *f, const char *s, int len)
{
    if (len < 0) len = (int)strlen(s);
    int w = 0;
    for (int i = 0; i < len; i++) w += glyph_of(f, (unsigned char)s[i])->advance;
    return w;
}

size_t xr_text_wrap(const xr_font_t *f, const char *s, int max_w, size_t *next)
{
    size_t i = 0, last_space = 0;
    bool have_space = false;
    int w = 0;

    while (s[i] && s[i] != '\n') {
        int a = glyph_of(f, (unsigned char)s[i])->advance;
        if (w + a > max_w && i > 0) {
            size_t end = have_space ? last_space : i;
            size_t n = end;
            while (s[n] == ' ') n++;
            *next = n;
            while (end > 0 && s[end - 1] == ' ') end--;
            return end;
        }
        if (s[i] == ' ') { last_space = i; have_space = true; }
        w += a;
        i++;
    }
    *next = (s[i] == '\n') ? i + 1 : i;
    return i;
}

int xr_text_measure_height(const xr_font_t *f, const char *s, int max_w)
{
    int lines = 0;
    size_t off = 0, next;
    do {
        xr_text_wrap(f, s + off, max_w, &next);
        off += next;
        lines++;
    } while (s[off]);
    return lines * f->line_height;
}

int xr_canvas_draw_text(xr_canvas_t *c, const xr_font_t *f, int x, int y,
                        const char *s, int len, uint8_t gray)
{
    if (len < 0) len = (int)strlen(s);
    int baseline = y + f->ascent;
    for (int i = 0; i < len; i++) {
        const xr_glyph_t *g = glyph_of(f, (unsigned char)s[i]);
        if (g->w && g->h)
            xr_canvas_draw_mask(c, x + g->x_off, baseline + g->y_off, g->w, g->h,
                                f->bitmap + g->offset, g->w, gray);
        x += g->advance;
    }
    return x;
}

void xr_canvas_draw_text_in(xr_canvas_t *c, const xr_font_t *f, xr_rect_t r,
                            const char *s, xr_align_t align, uint8_t gray)
{
    int len = (int)strlen(s);
    int w = xr_text_width(f, s, len);
    bool ellipsis = false;
    if (w > r.w) {
        int ew = xr_text_width(f, "...", 3);
        while (len > 0 && w + ew > r.w) {
            len--;
            w -= glyph_of(f, (unsigned char)s[len])->advance;
        }
        while (len > 0 && s[len - 1] == ' ') {
            len--;
            w -= glyph_of(f, ' ')->advance;
        }
        w += ew;
        ellipsis = true;
    }
    int x = r.x;
    if (align == XR_ALIGN_CENTER) x = r.x + (r.w - w) / 2;
    else if (align == XR_ALIGN_RIGHT) x = r.x + r.w - w;
    int y = r.y + (r.h - (f->ascent + f->descent)) / 2;
    x = xr_canvas_draw_text(c, f, x, y, s, len, gray);
    if (ellipsis) xr_canvas_draw_text(c, f, x, y, "...", 3, gray);
}

int xr_canvas_draw_text_wrapped(xr_canvas_t *c, const xr_font_t *f, xr_rect_t r,
                                const char *s, xr_align_t align, uint8_t gray)
{
    int y = r.y;
    size_t off = 0, next;
    do {
        size_t n = xr_text_wrap(f, s + off, r.w, &next);
        int w = xr_text_width(f, s + off, (int)n);
        int x = r.x;
        if (align == XR_ALIGN_CENTER) x = r.x + (r.w - w) / 2;
        else if (align == XR_ALIGN_RIGHT) x = r.x + r.w - w;
        xr_canvas_draw_text(c, f, x, y, s + off, (int)n, gray);
        y += f->line_height;
        off += next;
    } while (s[off]);
    return y - r.y;
}
