#include "byte_stream.hh"

#include <algorithm>
#include <string.h>

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) : _storage() { _storage.reserve(capacity); }

size_t ByteStream::write(const string &data) {
    size_t actual_size = std::min(data.size(), _storage.capacity() - _size);
    if (actual_size > 0) {
        memcpy(_storage.data() + _tail, data.c_str(), actual_size);
    }
    _tail = (_tail + actual_size) % _storage.capacity();
    _size += actual_size;
    _bytes_written += actual_size;
    return actual_size;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    size_t actual_size = std::min(len, _size);
    string output;
    if (actual_size == 0) {
        return output;
    }

    if (_head + actual_size > _storage.capacity()) {
        // wraps around, 2 copies
        size_t first_copy_size = _storage.capacity() - _head;
        output += string(_storage.data() + _head, first_copy_size);
        size_t second_copy_size = actual_size - first_copy_size;
        output += string(_storage.data(), second_copy_size);
    } else {
        output = string(_storage.data() + _head, actual_size);
    }
    return output;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    size_t actual_size = std::min(len, _size);
    _size -= actual_size;
    _head = (_head + actual_size) % _storage.capacity();
    _bytes_read += actual_size;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    std::string output = peek_output(len);
    pop_output(len);
    return output;
}

void ByteStream::end_input() { _end_input = true; }

bool ByteStream::input_ended() const { return _end_input; }

size_t ByteStream::buffer_size() const { return _size; }

bool ByteStream::buffer_empty() const { return _size == 0; }

bool ByteStream::eof() const { return input_ended() && buffer_empty(); }

size_t ByteStream::bytes_written() const { return _bytes_written; }

size_t ByteStream::bytes_read() const { return _bytes_read; }

size_t ByteStream::remaining_capacity() const { return _storage.capacity() - _size; }
