#pragma once

namespace gamesolver {

typedef uint64_t HashKey;

template <class _data>
class OpenAddressHashTable;

template <class _data>
class OpenAddressHashTableEntry {
public:
    OpenAddressHashTableEntry()
    {
        clear();
    }

    inline void clear()
    {
        data_.clear();
        is_free_ = true;
        key_ = 0;
    }

    inline void setFree(bool is_free) { is_free_ = is_free; }
    inline void setHashKey(HashKey key) { key_ = key; }
    inline void setData(const _data& data) { data_ = data; }
    inline bool isFree() const { return is_free_; }
    inline HashKey getHashKey() const { return key_; }
    inline _data& getData() { return data_; }
    inline const _data& getData() const { return data_; }

private:
    _data data_;
    bool is_free_;
    HashKey key_;
};

template <class _data>
class OpenAddressHashTable {
public:
    OpenAddressHashTable(int bit_size = 12)
        : kMask((1 << bit_size) - 1),
          kSize(1 << bit_size),
          count_(0),
          entry_(new OpenAddressHashTableEntry<_data>[1ULL << bit_size])
    {
        clear();
    }

    ~OpenAddressHashTable() { delete[] entry_; }

    void clear()
    {
        count_ = 0;
        for (unsigned int i = 0; i < kSize; ++i) { entry_[i].clear(); }
    }

    unsigned int lookup(const HashKey& key) const
    {
        unsigned int index = static_cast<unsigned int>(key) & kMask;

        while (true) {
            const OpenAddressHashTableEntry<_data>& entry = entry_[index];
            if (!entry.isFree()) {
                if (entry.getHashKey() == key) {
                    return index;
                } else {
                    index = (index + 1) & kMask;
                }
            } else {
                return -1;
            }
        }
        return -1;
    }

    void store(const HashKey& key, const _data& data)
    {
        unsigned int index = static_cast<unsigned int>(key) & kMask;
        while (true) {
            OpenAddressHashTableEntry<_data>& entry = entry_[index];
            if (entry.isFree()) {
                entry.setFree(false);
                entry.setHashKey(key);
                entry.setData(data);
                ++count_;
                break;
            } else {
                index = (index + 1) & kMask;
            }
        }
    }

    inline unsigned int getSize() const { return kSize; }
    inline unsigned int getCount() const { return count_; }
    inline OpenAddressHashTableEntry<_data>& getEntry(unsigned int index) { return entry_[index]; }
    inline const OpenAddressHashTableEntry<_data>& getEntry(unsigned int index) const { return entry_[index]; }
    inline bool isFull() const { return count_ >= kSize; }

protected:
    const unsigned int kMask;
    const unsigned int kSize;

    unsigned int count_;
    OpenAddressHashTableEntry<_data>* entry_;
};

} // namespace gamesolver
