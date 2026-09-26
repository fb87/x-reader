#include "inflate.hpp"

#include <string.h>

namespace xreader
{
namespace epub
{
namespace inflate
{
namespace
{

struct reader_t
{
    const uint8_t* data;
    size_t size, offset;
    uint32_t bits;
    uint8_t count;
};
struct tree_t
{
    uint16_t count;
    uint16_t code[288];
    uint8_t length[288];
};

static const uint16_t length_base[] = {3,  4,  5,  6,   7,   8,   9,   10,  11, 13,
                                       15, 17, 19, 23,  27,  31,  35,  43,  51, 59,
                                       67, 83, 99, 115, 131, 163, 195, 227, 258};
static const uint8_t length_extra[] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                       2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
static const uint16_t distance_base[] = {
    1,   2,   3,   4,   5,   7,    9,    13,   17,   25,   33,   49,   65,    97,    129,
    193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
static const uint8_t distance_extra[] = {0, 0, 0, 0, 1, 1, 2, 2,  3,  3,  4,  4,  5,  5,  6,
                                         6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
static const uint8_t order[] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};

static esp_err_t bits(reader_t* r, uint8_t n, uint32_t* v)
{
    if (n > 16)
        return ESP_ERR_INVALID_ARG;
    while (r->count < n)
    {
        if (r->offset >= r->size)
            return ESP_ERR_INVALID_SIZE;
        r->bits |= static_cast<uint32_t>(r->data[r->offset++]) << r->count;
        r->count = static_cast<uint8_t>(r->count + 8);
    }
    *v = r->bits & ((1U << n) - 1U);
    r->bits >>= n;
    r->count = static_cast<uint8_t>(r->count - n);
    return ESP_OK;
}
static uint16_t reverse(uint16_t v, uint8_t n)
{
    uint16_t o = 0;
    while (n--)
    {
        o = (o << 1) | (v & 1);
        v >>= 1;
    }
    return o;
}
static esp_err_t tree(tree_t* t, const uint8_t* lengths, uint16_t count)
{
    uint16_t used[16] = {}, next[16] = {}, code = 0;
    if (count > 288)
        return ESP_ERR_INVALID_ARG;
    for (uint16_t i = 0; i < count; ++i)
    {
        if (lengths[i] > 15)
            return ESP_ERR_INVALID_RESPONSE;
        ++used[lengths[i]];
    }
    for (uint8_t n = 1; n <= 15; ++n)
    {
        code = static_cast<uint16_t>((code + used[n - 1]) << 1);
        next[n] = code;
    }
    t->count = count;
    for (uint16_t i = 0; i < count; ++i)
    {
        t->length[i] = lengths[i];
        t->code[i] = lengths[i] ? reverse(next[lengths[i]]++, lengths[i]) : 0;
    }
    return ESP_OK;
}
static esp_err_t symbol(reader_t* r, const tree_t* t, uint16_t* out)
{
    uint16_t code = 0;
    for (uint8_t n = 1; n <= 15; ++n)
    {
        uint32_t b;
        esp_err_t e = bits(r, 1, &b);
        if (e != ESP_OK)
            return e;
        code |= static_cast<uint16_t>(b << (n - 1));
        for (uint16_t i = 0; i < t->count; ++i)
            if (t->length[i] == n && t->code[i] == code)
            {
                *out = i;
                return ESP_OK;
            }
    }
    return ESP_ERR_INVALID_RESPONSE;
}
static esp_err_t fixed(tree_t* l, tree_t* d)
{
    uint8_t ll[288] = {}, dd[32] = {};
    for (int i = 0; i <= 143; ++i)
        ll[i] = 8;
    for (int i = 144; i <= 255; ++i)
        ll[i] = 9;
    for (int i = 256; i <= 279; ++i)
        ll[i] = 7;
    for (int i = 280; i < 288; ++i)
        ll[i] = 8;
    for (int i = 0; i < 32; ++i)
        dd[i] = 5;
    esp_err_t e = tree(l, ll, 288);
    return e == ESP_OK ? tree(d, dd, 32) : e;
}
static esp_err_t dynamic(reader_t* r, tree_t* l, tree_t* d)
{
    uint32_t v;
    esp_err_t e = bits(r, 5, &v);
    if (e != ESP_OK)
        return e;
    uint16_t lc = static_cast<uint16_t>(v + 257);
    e = bits(r, 5, &v);
    if (e != ESP_OK)
        return e;
    uint16_t dc = static_cast<uint16_t>(v + 1);
    e = bits(r, 4, &v);
    if (e != ESP_OK)
        return e;
    uint8_t cc = static_cast<uint8_t>(v + 4);
    uint8_t cl[19] = {};
    for (uint8_t i = 0; i < cc; ++i)
    {
        e = bits(r, 3, &v);
        if (e != ESP_OK)
            return e;
        cl[order[i]] = static_cast<uint8_t>(v);
    }
    tree_t ct = {};
    e = tree(&ct, cl, 19);
    if (e != ESP_OK)
        return e;
    uint8_t lengths[320] = {};
    uint16_t n = 0;
    while (n < lc + dc)
    {
        uint16_t s;
        e = symbol(r, &ct, &s);
        if (e != ESP_OK)
            return e;
        if (s < 16)
        {
            lengths[n++] = static_cast<uint8_t>(s);
        }
        else if (s == 16)
        {
            if (!n)
                return ESP_ERR_INVALID_RESPONSE;
            e = bits(r, 2, &v);
            if (e != ESP_OK)
                return e;
            uint16_t rep = static_cast<uint16_t>(v + 3);
            if (n + rep > lc + dc)
                return ESP_ERR_INVALID_RESPONSE;
            const uint8_t previous = lengths[n - 1];
            while (rep--)
                lengths[n++] = previous;
        }
        else
        {
            e = bits(r, s == 17 ? 3 : 7, &v);
            if (e != ESP_OK)
                return e;
            uint16_t rep = static_cast<uint16_t>(v + (s == 17 ? 3 : 11));
            if (n + rep > lc + dc)
                return ESP_ERR_INVALID_RESPONSE;
            while (rep--)
                lengths[n++] = 0;
        }
    }
    e = tree(l, lengths, lc);
    return e == ESP_OK ? tree(d, lengths + lc, dc) : e;
}
static esp_err_t block(reader_t* r, const tree_t* l, const tree_t* d, uint8_t* out, size_t cap,
                       size_t* n)
{
    while (1)
    {
        uint16_t s;
        esp_err_t e = symbol(r, l, &s);
        if (e != ESP_OK)
            return e;
        if (s < 256)
        {
            if (*n >= cap)
                return ESP_ERR_INVALID_SIZE;
            out[*n] = static_cast<uint8_t>(s);
            ++(*n);
            continue;
        }
        if (s == 256)
            return ESP_OK;
        if (s < 257 || s > 285)
            return ESP_ERR_INVALID_RESPONSE;
        uint32_t v;
        e = bits(r, length_extra[s - 257], &v);
        if (e != ESP_OK)
            return e;
        size_t len = length_base[s - 257] + v;
        e = symbol(r, d, &s);
        if (e != ESP_OK || s >= 30)
            return e == ESP_OK ? ESP_ERR_INVALID_RESPONSE : e;
        e = bits(r, distance_extra[s], &v);
        if (e != ESP_OK)
            return e;
        size_t dist = distance_base[s] + v;
        if (dist > *n || len > cap - *n)
            return ESP_ERR_INVALID_SIZE;
        while (len--)
        {
            out[*n] = out[*n - dist];
            ++(*n);
        }
    }
}

} // namespace
esp_err_t decode(const uint8_t* input, size_t input_size, uint8_t* output, size_t output_capacity,
                 size_t* output_size)
{
    if (!input || !output || !output_size)
        return ESP_ERR_INVALID_ARG;
    *output_size = 0;
    reader_t r = {input, input_size, 0, 0, 0};
    bool final = false;
    while (!final)
    {
        uint32_t v;
        esp_err_t e = bits(&r, 1, &v);
        if (e != ESP_OK)
            return e;
        final = v;
        e = bits(&r, 2, &v);
        if (e != ESP_OK)
            return e;
        if (v == 0)
        {
            r.bits = 0;
            r.count = 0;
            e = bits(&r, 16, &v);
            if (e != ESP_OK)
                return e;
            uint16_t len = static_cast<uint16_t>(v);
            e = bits(&r, 16, &v);
            if (e != ESP_OK || static_cast<uint16_t>(v) != static_cast<uint16_t>(~len))
                return ESP_ERR_INVALID_RESPONSE;
            if (len > output_capacity - *output_size || r.offset + len > r.size)
                return ESP_ERR_INVALID_SIZE;
            memcpy(output + *output_size, r.data + r.offset, len);
            r.offset += len;
            *output_size += len;
        }
        else
        {
            tree_t l = {}, d = {};
            e = v == 1 ? fixed(&l, &d) : v == 2 ? dynamic(&r, &l, &d) : ESP_ERR_NOT_SUPPORTED;
            if (e != ESP_OK)
                return e;
            e = block(&r, &l, &d, output, output_capacity, output_size);
            if (e != ESP_OK)
                return e;
        }
    }
    return ESP_OK;
}
} // namespace inflate
} // namespace epub
} // namespace xreader
