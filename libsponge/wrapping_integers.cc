#include "wrapping_integers.hh"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <ostream>
#include <vector>

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    uint64_t seqno = n + isn.raw_value();
    seqno %= (1ul << 32);
    return WrappingInt32{static_cast<uint32_t>(seqno)};
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    WrappingInt32 offset(static_cast<uint32_t>((static_cast<uint64_t>(isn.raw_value()) + checkpoint) % (1ul << 32)));

    int64_t diff = offset - n;
    int64_t other_diff = 0;
    if (diff >= 0) {
        other_diff = static_cast<int64_t>(1ul << 32) - diff;
    } else {
        other_diff = static_cast<int64_t>(1ul << 32) + diff;
    }
    std::cout << "diff: " << diff << std::endl;
    std::cout << "other_diff: " << other_diff << std::endl;
    //
    // at most these possibilities
    // checkpoint + diff
    // checkpoint - diff
    // checkpoint + other_diff
    // checkpoint - other_diff
    //
    int64_t test1 = static_cast<int64_t>(checkpoint) - diff;
    int64_t test2 = 0;
    if (diff < 0) {
        test2 = static_cast<int64_t>(checkpoint) - other_diff;
    } else {
        test2 = static_cast<int64_t>(checkpoint) + other_diff;
    }
    if (test1 < 0) {
        return test2;
    }
    if (test2 < 0) {
        return test1;
    }
    if (std::abs(test1 - static_cast<int64_t>(checkpoint)) < std::abs(test2 - static_cast<int64_t>(checkpoint))) {
        return test1;
    }
    return test2;
}
