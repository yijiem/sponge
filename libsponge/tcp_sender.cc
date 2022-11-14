#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <algorithm>
#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

RetransmissionTimer::RetransmissionTimer(unsigned int timeout_ms) : _timeout_ms(timeout_ms) {}
void RetransmissionTimer::tick(size_t ms_since_last_tick) {
  _tick += ms_since_last_tick;
}
void RetransmissionTimer::start() {
  if (!_started) {
    _tick = 0;
    _started = true;
  }
}
bool RetransmissionTimer::expired() const {
  return _tick >= _timeout_ms;
}
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

uint64_t TCPSender::bytes_in_flight() const { return {}; }

void TCPSender::fill_window() {
  unsigned int window_size = _receiver_window_size;
  while (window_size > 0) {
    // next_send <= _stream.buffer_size()
    // next_send <= window_size
    // next_send <= TCPConfig::MAX_PAYLOAD_SIZE
    size_t next_send = std::min<size_t>({_stream.buffer_size(), window_size, TCPConfig::MAX_PAYLOAD_SIZE});
    if (next_send == 0) {
      break;
    }
    TCPSegment next_segment;
    next_segment.header().seqno = wrap(_next_seqno, _isn);
    if (_next_seqno == 0) {
      // handle SYN
      next_segment.header().syn = true;
      _next_seqno++;
      next_send--;
    }
    if (next_send > 0) {
      // handle data
      // next_read <= _stream.buffer_size()
      // next_read <= next_send
      size_t next_read = std::min(_stream.buffer_size(), next_send);
      next_segment.payload() = Buffer(_stream.read(next_read));
      next_send -= next_read;
      _next_seqno += next_read;
    }
    if (next_send > 0 && _stream.eof()) {
      // handle FIN
      next_segment.header().fin = true;
      _next_seqno++;
    }
    // send!
    _segments_out.push(next_segment);
    _outstanding_queue.push(next_segment);

    _timer.start();

    window_size -= next_segment.length_in_sequence_space();
  }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
  _receiver_window_size = window_size;
  if (_receiver_window_size == 0) {
    _receiver_window_size = 1;
  }
  DUMMY_CODE(ackno, window_size);
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) { DUMMY_CODE(ms_since_last_tick); }

unsigned int TCPSender::consecutive_retransmissions() const { return {}; }

void TCPSender::send_empty_segment() {
  TCPSegment segment;
  segment.header().seqno = wrap(_next_seqno, _isn);
  _segments_out.push(segment);
}
