#include <jvmti.h>

enum GroupVarintIteratorError {
    ERR_OK = 0, ERR_NO_DATA = 1, ERR_BITMAP_IDX = 2, ERR_DATA_IDX = 3
};

class GroupVarintIterator {
  private:
    jbyte* _data;
    jbyte* _bitmap;
    size_t _pos;
    size_t _data_size;
    size_t _bitmap_size;
    size_t _bitmap_pos;
    int _bitmap_bit;
    bool _valid;
    unsigned int _current_size_mask;

    static u32 _bitmap_bitmask[];

    static u64 readVarint();

  public:
    GroupVarintIterator(jbyte* data_chunk, size_t size);
    bool hasNext();
    GroupVarintIteratorError next(u64* value);
};

u64 readVarint(jbyte* data, size_t* pos, const size_t limit);