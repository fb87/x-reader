#include "inflate.hpp"

#include <string.h>

namespace drivers::inflate {
namespace {

struct reader_t {
    const uint8_t* data;
    size_t size;
    size_t offset;
    uint32_t bits;
    uint8_t count;
};

struct tree_t {
    uint16_t count;
    uint16_t code[288];
    uint8_t length[288];
};

static const uint16_t length_base[] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27,
    31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258,
};
static const uint8_t length_extra[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
    2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0,
};
static const uint16_t distance_base[] = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129,
    193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
    8193, 12289, 16385, 24577,
};
static const uint8_t distance_extra[] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6,
    6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13,
};
static const uint8_t order[] = {
    16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15,
};

static esp_err_t read_bits(reader_t* reader, uint8_t count, uint32_t* value)
{
    if (count > 16) return ESP_ERR_INVALID_ARG;
    while (reader->count < count) {
        if (reader->offset >= reader->size) return ESP_ERR_INVALID_SIZE;
        reader->bits |= static_cast<uint32_t>(reader->data[reader->offset++]) << reader->count;
        reader->count = static_cast<uint8_t>(reader->count + 8);
    }
    *value = reader->bits & ((1U << count) - 1U);
    reader->bits >>= count;
    reader->count = static_cast<uint8_t>(reader->count - count);
    return ESP_OK;
}

static uint16_t reverse_bits(uint16_t value, uint8_t count)
{
    uint16_t output = 0;
    while (count--) {
        output = static_cast<uint16_t>((output << 1) | (value & 1));
        value >>= 1;
    }
    return output;
}

static esp_err_t build_tree(tree_t* tree, const uint8_t* lengths, uint16_t count)
{
    uint16_t used[16]{};
    uint16_t next[16]{};
    uint16_t code = 0;
    if (count > 288) return ESP_ERR_INVALID_ARG;
    for (uint16_t i = 0; i < count; ++i) {
        if (lengths[i] > 15) return ESP_ERR_INVALID_RESPONSE;
        ++used[lengths[i]];
    }
    for (uint8_t length = 1; length <= 15; ++length) {
        code = static_cast<uint16_t>((code + used[length - 1]) << 1);
        next[length] = code;
    }
    tree->count = count;
    for (uint16_t i = 0; i < count; ++i) {
        tree->length[i] = lengths[i];
        tree->code[i] = lengths[i] ? reverse_bits(next[lengths[i]]++, lengths[i]) : 0;
    }
    return ESP_OK;
}

static esp_err_t read_symbol(reader_t* reader, const tree_t* tree, uint16_t* output)
{
    uint16_t code = 0;
    for (uint8_t length = 1; length <= 15; ++length) {
        uint32_t bit;
        esp_err_t error = read_bits(reader, 1, &bit);
        if (error != ESP_OK) return error;
        code = static_cast<uint16_t>(code | (bit << (length - 1)));
        for (uint16_t i = 0; i < tree->count; ++i) {
            if (tree->length[i] == length && tree->code[i] == code) {
                *output = i;
                return ESP_OK;
            }
        }
    }
    return ESP_ERR_INVALID_RESPONSE;
}

static esp_err_t fixed_trees(tree_t* literals, tree_t* distances)
{
    uint8_t literal_lengths[288]{};
    uint8_t distance_lengths[32]{};
    for (int i = 0; i <= 143; ++i) literal_lengths[i] = 8;
    for (int i = 144; i <= 255; ++i) literal_lengths[i] = 9;
    for (int i = 256; i <= 279; ++i) literal_lengths[i] = 7;
    for (int i = 280; i < 288; ++i) literal_lengths[i] = 8;
    for (int i = 0; i < 32; ++i) distance_lengths[i] = 5;
    esp_err_t error = build_tree(literals, literal_lengths, 288);
    return error == ESP_OK ? build_tree(distances, distance_lengths, 32) : error;
}

static esp_err_t dynamic_trees(reader_t* reader, tree_t* literals, tree_t* distances)
{
    uint32_t value;
    esp_err_t error = read_bits(reader, 5, &value);
    if (error != ESP_OK) return error;
    uint16_t literal_count = static_cast<uint16_t>(value + 257);
    error = read_bits(reader, 5, &value);
    if (error != ESP_OK) return error;
    uint16_t distance_count = static_cast<uint16_t>(value + 1);
    error = read_bits(reader, 4, &value);
    if (error != ESP_OK) return error;
    uint8_t code_count = static_cast<uint8_t>(value + 4);

    uint8_t code_lengths[19]{};
    for (uint8_t i = 0; i < code_count; ++i) {
        error = read_bits(reader, 3, &value);
        if (error != ESP_OK) return error;
        code_lengths[order[i]] = static_cast<uint8_t>(value);
    }
    tree_t code_tree{};
    error = build_tree(&code_tree, code_lengths, 19);
    if (error != ESP_OK) return error;

    uint8_t lengths[320]{};
    uint16_t count = 0;
    const uint16_t total = static_cast<uint16_t>(literal_count + distance_count);
    while (count < total) {
        uint16_t symbol;
        error = read_symbol(reader, &code_tree, &symbol);
        if (error != ESP_OK) return error;
        if (symbol < 16) {
            lengths[count++] = static_cast<uint8_t>(symbol);
            continue;
        }
        if (symbol == 16) {
            if (count == 0) return ESP_ERR_INVALID_RESPONSE;
            error = read_bits(reader, 2, &value);
            if (error != ESP_OK) return error;
            uint16_t repeat = static_cast<uint16_t>(value + 3);
            if (count + repeat > total) return ESP_ERR_INVALID_RESPONSE;
            const uint8_t previous = lengths[count - 1];
            while (repeat--) lengths[count++] = previous;
            continue;
        }
        if (symbol != 17 && symbol != 18) return ESP_ERR_INVALID_RESPONSE;
        error = read_bits(reader, symbol == 17 ? 3 : 7, &value);
        if (error != ESP_OK) return error;
        uint16_t repeat = static_cast<uint16_t>(value + (symbol == 17 ? 3 : 11));
        if (count + repeat > total) return ESP_ERR_INVALID_RESPONSE;
        while (repeat--) lengths[count++] = 0;
    }
    error = build_tree(literals, lengths, literal_count);
    return error == ESP_OK
        ? build_tree(distances, lengths + literal_count, distance_count)
        : error;
}

static esp_err_t decode_block(reader_t* reader, const tree_t* literals,
                              const tree_t* distances, uint8_t* output,
                              size_t capacity, size_t* output_size)
{
    for (;;) {
        uint16_t symbol;
        esp_err_t error = read_symbol(reader, literals, &symbol);
        if (error != ESP_OK) return error;
        if (symbol < 256) {
            if (*output_size >= capacity) return ESP_ERR_INVALID_SIZE;
            output[(*output_size)++] = static_cast<uint8_t>(symbol);
            continue;
        }
        if (symbol == 256) return ESP_OK;
        if (symbol > 285) return ESP_ERR_INVALID_RESPONSE;

        uint32_t value;
        error = read_bits(reader, length_extra[symbol - 257], &value);
        if (error != ESP_OK) return error;
        size_t length = length_base[symbol - 257] + value;
        error = read_symbol(reader, distances, &symbol);
        if (error != ESP_OK || symbol >= 30)
            return error == ESP_OK ? ESP_ERR_INVALID_RESPONSE : error;
        error = read_bits(reader, distance_extra[symbol], &value);
        if (error != ESP_OK) return error;
        size_t distance = distance_base[symbol] + value;
        if (distance > *output_size || length > capacity - *output_size)
            return ESP_ERR_INVALID_SIZE;
        while (length--) {
            output[*output_size] = output[*output_size - distance];
            ++(*output_size);
        }
    }
}

} // namespace

esp_err_t decode(const uint8_t* input, size_t input_size, uint8_t* output,
                 size_t output_capacity, size_t* output_size)
{
    if (!input || !output || !output_size) return ESP_ERR_INVALID_ARG;
    *output_size = 0;
    reader_t reader{input, input_size, 0, 0, 0};
    bool final = false;
    while (!final) {
        uint32_t value;
        esp_err_t error = read_bits(&reader, 1, &value);
        if (error != ESP_OK) return error;
        final = value != 0;
        error = read_bits(&reader, 2, &value);
        if (error != ESP_OK) return error;
        if (value == 0) {
            reader.bits = 0;
            reader.count = 0;
            error = read_bits(&reader, 16, &value);
            if (error != ESP_OK) return error;
            uint16_t length = static_cast<uint16_t>(value);
            error = read_bits(&reader, 16, &value);
            if (error != ESP_OK || static_cast<uint16_t>(value) != static_cast<uint16_t>(~length))
                return ESP_ERR_INVALID_RESPONSE;
            if (length > output_capacity - *output_size || reader.offset + length > reader.size)
                return ESP_ERR_INVALID_SIZE;
            memcpy(output + *output_size, input + reader.offset, length);
            reader.offset += length;
            *output_size += length;
            continue;
        }

        tree_t literals{};
        tree_t distances{};
        error = value == 1 ? fixed_trees(&literals, &distances)
                           : value == 2 ? dynamic_trees(&reader, &literals, &distances)
                                        : ESP_ERR_NOT_SUPPORTED;
        if (error != ESP_OK) return error;
        error = decode_block(&reader, &literals, &distances, output,
                             output_capacity, output_size);
        if (error != ESP_OK) return error;
    }
    return ESP_OK;
}

}  // namespace drivers::inflate
