#include "tcp_receiver.hh"

#include <cassert>

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    const TCPHeader &header = seg.header();
    WrappingInt32 seqno = header.seqno;
    if (!_isn && header.syn) {
        _isn = header.seqno;
        seqno = seqno + 1;
    }
    if (!_isn) {
        return;
    }
    _reassembler.push_substring(seg.payload().copy(), unwrap(seqno, *_isn, get_checkpoint()) - 1, /*eof=*/header.fin);
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!_isn) {
        return nullopt;
    }
    // If FIN has been received and counted toward, then +2
    return wrap(get_checkpoint() + (stream_out().input_ended() ? 2 : 1), *_isn);
}

size_t TCPReceiver::window_size() const { return _capacity - stream_out().buffer_size(); }

uint64_t TCPReceiver::get_checkpoint() const {
    assert(_isn.has_value());
    const ByteStream &bs = stream_out();
    // first unassembled offset
    return bs.bytes_read() + bs.buffer_size();
}
