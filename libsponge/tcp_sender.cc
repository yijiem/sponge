#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <algorithm>
#include <iostream>
#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

void PrintTCPSegment(const TCPSegment &segment) {
    std::cout << "segment: seqno=" << segment.header().seqno << ", length=" << segment.length_in_sequence_space()
              << std::endl;
}

RetransmissionTimer::RetransmissionTimer(unsigned int timeout_ms) : _timeout_ms(timeout_ms) {}
void RetransmissionTimer::tick(size_t ms_since_last_tick) { _tick += ms_since_last_tick; }
void RetransmissionTimer::start() {
    if (!_started) {
        _tick = 0;
        _started = true;
    }
}
bool RetransmissionTimer::expired() const { return _tick >= _timeout_ms; }
unsigned int RetransmissionTimer::timeout_ms() const { return _timeout_ms; }
void RetransmissionTimer::reset(unsigned int timeout_ms) {
    _timeout_ms = timeout_ms;
    _tick = 0;
    _started = false;
}

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _timer(_initial_retransmission_timeout) {}

uint64_t TCPSender::bytes_in_flight() const {
    uint64_t res = 0;
    for (const TCPSegment &segment : _outstanding_queue) {
        res += segment.length_in_sequence_space();
    }
    return res;
}

void TCPSender::fill_window() {
    int window_size = _receiver_window_size - bytes_in_flight();
    // Tricky: when _receiver_window_size is 0 and there is already bytes in
    // flight, do nothing. Otherwise, act like window size is 1 to provoke
    // receiver to ack again when it can.
    if (_receiver_window_size == 0 && bytes_in_flight() == 0) {
        window_size = 1;
    }
    while (window_size > 0) {
        bool has_something_to_send = false;
        TCPSegment next_segment;
        next_segment.header().seqno = wrap(_next_seqno, _isn);
        if (_next_seqno == 0) {
            // handle SYN
            has_something_to_send = true;
            next_segment.header().syn = true;
            _next_seqno++;
            window_size--;
        }
        if (!_stream.eof()) {
            // handle data
            int64_t data_read_size = std::min<int64_t>(
                {static_cast<int64_t>(_stream.buffer_size()), window_size, TCPConfig::MAX_PAYLOAD_SIZE});
            if (data_read_size > 0) {
                next_segment.payload() = Buffer(_stream.read(data_read_size));
                _next_seqno += data_read_size;
                window_size -= data_read_size;
                has_something_to_send = true;
            }
        }
        // Tricky: FIN and SYN does not count toward payload size, they are
        // headers. But they count toward window size.
        if (_stream.eof() && (window_size > 0) && !_finned) {
            // handle FIN
            next_segment.header().fin = true;
            _next_seqno++;
            window_size--;
            has_something_to_send = true;
            _finned = true;
        }
        if (has_something_to_send) {
            // send!
            _segments_out.push(next_segment);
            _outstanding_queue.push_back(next_segment);
        } else {
            break;
        }
    }
    _timer.start();
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    uint64_t unwrapped_ackno = unwrap(ackno, _isn, /* checkpoint = */ _next_seqno);
    if (unwrapped_ackno > _next_seqno) {
        // Tricky: ignore impossible ackno (beyond next seqno)
        return;
    }
    _receiver_window_size = window_size;
    bool make_progress = false;
    do {
        const TCPSegment &segment = _outstanding_queue.front();
        uint64_t next_segment =
            unwrap(segment.header().seqno, _isn, /* checkpoint= */ _next_seqno) + segment.length_in_sequence_space();
        if (next_segment <= unwrapped_ackno) {
            // acknowleged this segment
            _outstanding_queue.pop_front();
            make_progress = true;
        } else {
            break;
        }
    } while (!_outstanding_queue.empty());
    if (make_progress) {
        _timer.reset(_initial_retransmission_timeout);
        if (!_outstanding_queue.empty()) {
            _timer.start();
        }
        _consecutive_retx = 0;
    }
    // try to fill window again if space has opened up
    fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _timer.tick(ms_since_last_tick);
    if (_timer.expired() && !_outstanding_queue.empty()) {
        _segments_out.push(_outstanding_queue.front());
        unsigned int timeout_ms = _timer.timeout_ms();
        if (_receiver_window_size > 0) {
            _consecutive_retx++;
            // exponential backoff
            timeout_ms *= 2;
        }
        _timer.reset(timeout_ms);
        _timer.start();
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retx; }

void TCPSender::send_empty_segment() {
    TCPSegment segment;
    segment.header().seqno = wrap(_next_seqno, _isn);
    _segments_out.push(segment);
}
