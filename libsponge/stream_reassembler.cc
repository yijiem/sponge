#include "stream_reassembler.hh"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <ostream>

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

namespace {

inline std::ostream &operator<<(std::ostream &os, const Range &range) {
    return os << "data: " << range.data() << " index: " << range.index();
}

}  // namespace

Range::Range(const string &data, uint64_t index) : _data(data), _index(index) {}

bool Range::operator<(const Range &rhs) const {
    uint64_t rl = rhs._index;
    return (_index + _data.size()) < rl;
}

string Range::data() const { return _data; }

uint64_t Range::index() const { return _index; }

size_t Range::size() const { return _data.size(); }

void Range::consume(const Range *other) {
    uint64_t ll = _index;
    uint64_t lr = ll + _data.size();
    uint64_t rl = other->_index;
    uint64_t rr = rl + other->_data.size();
    // [ll, lr)
    //   [rl, rr)
    // [ll, lr)
    //        [rl, rr)
    //
    //   [ll, lr)
    // [rl, rr)
    //        [ll, lr)
    // [rl, rr)
    assert((rl >= ll && rl <= lr) || (ll >= rl && ll <= rr));

    cout << "consume: "
         << "range: " << *this << " range: " << *other << endl
         << flush;

    // "defg"
    // ll: 3
    // lr: 7
    //
    // "ghi"
    // rl: 6
    // rr: 9
    if (ll > rl) {
        _data = other->_data.substr(0, ll - rl) + _data;
    }
    if (lr < rr) {
        _data = _data + other->_data.substr(lr - rl);
    }
    _index = std::min(_index, other->_index);
    cout << "consume: " << *this << endl << flush;
}

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {}

StreamReassembler::~StreamReassembler() {
    Range *curr = _head;
    while (curr != NULL) {
        Range *freeup = curr;
        curr = curr->_next;
        delete freeup;
    }
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    if (eof) {
        _eof = eof;
    }
    debug();
    //
    //                 unassembled_offset
    //                         |
    // | stream start | unread | unassembled | unacceptable ...
    //                |                      |
    //            unread_offset      unacceptable_offset
    //
    size_t unread_offset = _output.bytes_read();
    size_t unassembled_offset = unread_offset + _output.buffer_size();
    size_t unacceptable_offset = unread_offset + _capacity;

    size_t l = index;
    size_t r = l + data.size();
    string actual_data = data;
    size_t actual_index = index;
    if (r <= unassembled_offset || l >= unacceptable_offset) {
        if (_head == NULL && _eof) {
            _output.end_input();
        }
        return;
    }
    if (l < unassembled_offset) {
        // clip front
        actual_data = actual_data.substr(unassembled_offset - l);
        actual_index = unassembled_offset;
    }
    if (r > unacceptable_offset) {
        // clip back
        actual_data = actual_data.substr(0, actual_data.size() - r + unacceptable_offset);
        if (eof) {
            // This is pretty confusing, but apparently if we reject some last
            // bytes from `data`, we also need to throw away `eof`.
            _eof = false;
        }
    }
    if (actual_data.size() > 0) {
        Range *new_range = new Range(actual_data, actual_index);
        range_put(new_range);
        // If _head can be assembled, assemble it right away
        if (_head->index() == unassembled_offset) {
            _output.write(_head->data());
            Range *prev_head = _head;
            _head = _head->_next;
            delete prev_head;
        }
    }
    if (_head == NULL && _eof) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const {
    size_t res = 0;
    for (Range *curr = _head; curr != NULL; curr = curr->_next) {
        res += curr->size();
    }
    return res;
}

void StreamReassembler::debug() const {
    for (Range *curr = _head; curr != NULL; curr = curr->_next) {
        cout << "debug: " << *curr << endl << flush;
    }
}

bool StreamReassembler::empty() const { return _head == NULL; }

void StreamReassembler::range_put(Range *new_range) {
    if (!_head) {
        _head = new_range;
        return;
    }
    // slide and merge
    // slide, if can insert, insert and return
    // if can merge, merge and potentially merge forward, until cant and return
    Range *curr = _head;
    Range *prev = NULL;
    while (curr != NULL) {
        if (*new_range < *curr) {
            // insert range in front of curr
            if (!prev) {
                // new head!
                curr->_prev = new_range;
                new_range->_next = curr;
                _head = new_range;
            } else {
                prev->_next = new_range;
                new_range->_prev = prev;
                curr->_prev = new_range;
                new_range->_next = curr;
            }
            return;
        }
        if (*curr < *new_range) {
            prev = curr;
            curr = curr->_next;
            continue;
        }
        // overlap
        new_range->consume(curr);
        if (prev != NULL) {
            prev->_next = curr->_next;
        }
        if (curr->_next != NULL) {
            curr->_next->_prev = prev;
        }
        // here we don't move prev
        Range *freeup = curr;
        curr = curr->_next;
        delete freeup;
    }
    // insert at tail
    if (!prev) {
        // we didn't move prev at all, so new_range must have consumed all the way
        // through
        _head = new_range;
    } else {
        prev->_next = new_range;
        new_range->_prev = prev;
    }
}
