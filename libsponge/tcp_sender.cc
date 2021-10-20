#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}
#include <iostream>
using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _receiver_window(65535)
    , _timer(retx_timeout) {}

uint64_t TCPSender::bytes_in_flight() const { return !_timer.is_running() ? 0 : _next_seqno - _timer.get_seq(); }
#include <iostream>
using namespace std;
void TCPSender::fill_window() {
    if (!_stream.buffer_empty() || _next_seqno == 0) {
        _segments_out.emplace();
        auto &back = _segments_out.back();
        back.header().syn = _next_seqno == 0;  // if  _next_seqno==0,syn = true;
        back.header().seqno = wrap(_next_seqno, _isn);
        // 没用到ack
        // back.header().ackno =
        back.header().fin = _stream.eof() && _next_seqno;
        if (_next_seqno) {
            auto str = _stream.read(_receiver_window);
            // cout << str.size() << " window:" << _receiver_window << " "
                //  << " " << str << endl;
            _timer.start_timer(_next_seqno, str, back.header().fin);
            back.payload() = string(_timer.get_buf());
            _next_seqno = _next_seqno + str.size();
        } else {
            _timer.start_timer(_next_seqno);
            _next_seqno = _next_seqno + 1;
        }
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    // auto next_seqno = unwrap(ackno, _isn, _next_seqno);
    // if(next_seqno < _next_seqno || next_seqno > _next_seqno+_receiver_window)return;
    // _next_seqno = next_seqno;
    auto old_seq = _timer.get_seq();
    auto recv_seq = unwrap(ackno, _isn, _next_seqno);
    // if not in valid range,abort
    // cout<<"111 "<<old_seq<<" "<<recv_seq<<endl;
    auto valid_window = (old_seq == 0)?1:_timer.get_buf().size();
    if (old_seq < recv_seq && old_seq + valid_window  >= recv_seq) {
        _receiver_window = window_size == 0 ? 1 :  window_size;
        _timer.stop_timer();
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _timer.increase_tick(ms_since_last_tick);
    if (_timer.is_expired()) {
        _segments_out.emplace();
        auto &back = _segments_out.back();
        back.header().seqno = wrap(_next_seqno, _isn);
        back.payload() = string(_timer.get_buf());
        back.header().fin = _timer.get_eof();
        _timer.restart_timer();
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _timer.get_retransmissions_times(); }

void TCPSender::send_empty_segment() {
    _segments_out.emplace();
    auto &back = _segments_out.back();
    back.header().seqno = wrap(_next_seqno, _isn);
    _timer.start_timer(_next_seqno);
}
