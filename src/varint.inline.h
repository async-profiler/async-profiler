#include "log.h"
#include "varint.h"

u32 GroupVarintIterator::_bitmap_bitmask[] = {
        0b111000000000000000000000,
        0b000111000000000000000000,
        0b000000111000000000000000,
        0b000000000111000000000000,
        0b000000000000111000000000,
        0b000000000000000111000000,
        0b000000000000000000111000,
        0b000000000000000000000111,
};

GroupVarintIterator::GroupVarintIterator(jbyte *data_chunk, size_t size) {
    u32 bitmap_offset = (((((data_chunk[0] & 0xff) * 256) + (data_chunk[1] & 0xff)) * 256) + (data_chunk[2] & 0xff)) * 256 + (data_chunk[3] & 0xff);

    if (bitmap_offset >= size) {
        // corrupted data - make sure we are not going to read from random memory
        Log::debug("Invalid bitmap offset %u>%lu", bitmap_offset, size);
        _pos = 0;
        _data_size = 0;
        _bitmap_size = 0;
        _valid = false;
        return;
    }

    _pos = 0;
    _data = data_chunk + 4;
    _bitmap = data_chunk + bitmap_offset;

    _data_size = _bitmap - _data;
    _bitmap_size = size - _data_size;

    _bitmap_pos = 0;
    _bitmap_bit = -1;
    _current_size_mask = 0;
    _valid = true;
}

bool GroupVarintIterator::hasNext() {
    return _valid && _pos < _data_size;
}

GroupVarintIteratorError GroupVarintIterator::next(u64 *value) {
    if (!hasNext()) {
        // no more data
        return ERR_NO_DATA;
    }
    if (_bitmap_bit == -1) {
        if (_bitmap_pos >= _bitmap_size) {
            Log::debug("Out-of-bounds bitmap access %lu>%lu", _bitmap_pos, _bitmap_size);
            return ERR_BITMAP_IDX; // out-of-bounds bitmap index
        }
        _bitmap_bit = 0;
        _current_size_mask = (_bitmap[_bitmap_pos++] & 0xff) * 256 * 256;
        if (_bitmap_pos < _bitmap_size) {
            _current_size_mask += (_bitmap[_bitmap_pos++] & 0xff) * 256;
        }
        if (_bitmap_pos < _bitmap_size) {
            _current_size_mask += (_bitmap[_bitmap_pos++] & 0xff);
        }
    }
    unsigned int size = ((_current_size_mask & _bitmap_bitmask[_bitmap_bit]) >> (7 - _bitmap_bit) * 3) + 1;
    if (++_bitmap_bit > 7) {
        _bitmap_pos += 3;
        _bitmap_bit = -1;
        if (_bitmap_pos >= _bitmap_size) {
            Log::debug("Out-of-bounds bitmap access %lu>%lu", _bitmap_pos, _bitmap_size);
            return ERR_BITMAP_IDX; // out-of-bounds bitmap index
        }
    }
    *value = 0;
    for (int i = 0; i < size; i++) {
        if (_pos > _data_size) {
            Log::debug("Out-of-bounds data access %lu>%lu", _pos, _data_size);
            return ERR_DATA_IDX;
        }
        *value = (*value) * 256 + (_data[_pos++] & 0xff);
    }
    return ERR_OK;
}

u64 readVarint(jbyte* data, size_t* pos, const size_t limit) {
    u64 result = 0;
    // 0
    u64 value = ((u64)(*(data + (*pos)++))) & 0xff;
    result += (value & 0x7f);
    if ((value & 0x80) == 0 || *pos >= limit) {
        return result;
    }
    // 1
    value = *(data + (*pos)++) & 0xff;
    result |= ((value & 0x7f) << 7);
    if ((value & 0x80) == 0 || *pos >= limit) {
        return result;
    }
    // 2
    value = *(data + (*pos)++) & 0xff;
    result |= ((value & 0x7f) << 14);
    if ((value & 0x80) == 0 || *pos >= limit) {
        return result;
    }
    // 3
    value = *(data + (*pos)++) & 0xff;
    result |= ((value & 0x7f) << 21);
    if ((value & 0x80) == 0 || *pos >= limit) {
        return result;
    }
    // 4
    value = *(data + (*pos)++) & 0xff;
    result |= ((value & 0x7f) << 28);
    if ((value & 0x80) == 0 || *pos >= limit) {
        return result;
    }
    // 5
    value = *(data + (*pos)++) & 0xff;
    result |= ((value & 0x7f) << 35);
    if ((value & 0x80) == 0 || *pos >= limit) {
        return result;
    }
    // 6
    value = *(data + (*pos)++) & 0xff;
    result |= ((value & 0x7f) << 42);
    if ((value & 0x80) == 0 || *pos >= limit) {
        return result;
    }
    // 7
    value = *(data + (*pos)++) & 0xff;
    result |= ((value & 0x7f) << 49);
    if ((value & 0x80) == 0 || *pos >= limit) {
        return result;
    }
    // 8
    value =*(data + (*pos)++) & 0xff;
    result |= ((value & 0x7f) << 56);
    return result;
}