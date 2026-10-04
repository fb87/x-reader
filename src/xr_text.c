#include "xr/xr_text.h"

#include <string.h>

static const xr_glyph_t *glyph_of(const xr_font_t *f, uint32_t codepoint)
{
    if (f->codepoints && f->glyph_count) {
        int lo = 0, hi = f->glyph_count - 1;
        while (lo <= hi) {
            int mid = lo + (hi - lo) / 2;
            if (f->codepoints[mid] == codepoint) return &f->glyphs[mid];
            if (f->codepoints[mid] < codepoint) lo = mid + 1; else hi = mid - 1;
        }
        codepoint = '?';
    }
    if (codepoint < f->first || codepoint > f->last) codepoint = '?';
    return &f->glyphs[codepoint - f->first];
}

static size_t utf8_next(const char *s, size_t len, uint32_t *codepoint)
{
    unsigned char c = (unsigned char)s[0];
    if (c < 0x80) { *codepoint = c; return 1; }
    if (c >= 0xc2 && c <= 0xdf && len >= 2 && ((unsigned char)s[1] & 0xc0) == 0x80) {
        *codepoint = ((uint32_t)(c & 0x1f) << 6) | ((unsigned char)s[1] & 0x3f); return 2;
    }
    if (c >= 0xe0 && c <= 0xef && len >= 3 && ((unsigned char)s[1] & 0xc0) == 0x80 &&
        ((unsigned char)s[2] & 0xc0) == 0x80) {
        *codepoint = ((uint32_t)(c & 0x0f) << 12) | (((unsigned char)s[1] & 0x3f) << 6) |
                     ((unsigned char)s[2] & 0x3f); return 3;
    }
    if (c >= 0xf0 && c <= 0xf4 && len >= 4 && ((unsigned char)s[1] & 0xc0) == 0x80 &&
        ((unsigned char)s[2] & 0xc0) == 0x80 && ((unsigned char)s[3] & 0xc0) == 0x80) {
        *codepoint = ((uint32_t)(c & 7) << 18) | (((unsigned char)s[1] & 0x3f) << 12) |
                     (((unsigned char)s[2] & 0x3f) << 6) | ((unsigned char)s[3] & 0x3f); return 4;
    }
    *codepoint = '?'; return 1;
}

int xr_text_width(const xr_font_t *f, const char *s, int len)
{
    if (len < 0) len = (int)strlen(s);
    int w = 0;
    for (size_t i = 0; i < (size_t)len;) {
        uint32_t cp; size_t n = utf8_next(s + i, (size_t)len - i, &cp);
        w += glyph_of(f, cp)->advance; i += n;
    }
    return w;
}

size_t xr_text_wrap(const xr_font_t *f, const char *s, int max_w, size_t *next)
{
    size_t i = 0, last_space = 0;
    bool have_space = false;
    int w = 0;

    while (s[i] && s[i] != '\n') {
        uint32_t cp; size_t n = utf8_next(s + i, strlen(s + i), &cp);
        int a = glyph_of(f, cp)->advance;
        if (w + a > max_w && i > 0) {
            size_t end = have_space ? last_space : i;
            size_t consumed = end;
            while (s[consumed] == ' ') consumed++;
            *next = consumed;
            while (end > 0 && s[end - 1] == ' ') end--;
            return end;
        }
        if (s[i] == ' ') { last_space = i; have_space = true; }
        w += a;
        i += n;
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
        uint32_t cp; size_t n = utf8_next(s + i, (size_t)len - i, &cp);
        const xr_glyph_t *g = glyph_of(f, cp);
        if (g->w && g->h)
            xr_canvas_draw_mask(c, x + g->x_off, baseline + g->y_off, g->w, g->h,
                                f->bitmap + g->offset, g->w, gray);
        x += g->advance; i += (int)n - 1;
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
