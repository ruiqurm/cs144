#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>
#include <algorithm>
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
    , _timers {}{}

uint64_t TCPSender::bytes_in_flight() const { return _bytes_in_flight; }

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
            auto send_data_count = (_receiver_window > _bytes_in_flight)?_receiver_window - _bytes_in_flight:0;
            if(send_data_count==0)return;
            auto str = _stream.read(send_data_count);
            auto next = _next_seqno + str.size();
            _timers.emplace_back(_initial_retransmission_timeout);

            // last timer start
            _timers.back().start_timer(_next_seqno, str, back.header().fin);
            back.payload() = string(str);
            _next_seqno = next;
            _bytes_in_flight += str.size();
        } else {
            _timers.emplace_back(_initial_retransmission_timeout);
            // _timers[1] = TCPRetransmissionTimer {_initial_retransmission_timeout};
            _timers.back().start_timer(_next_seqno);
            _next_seqno = _next_seqno + 1;
            _bytes_in_flight += 1;
        }
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    auto recv_seq = unwrap(ackno, _isn, _next_seqno);
    // if not in valid range,abort
    // 确认是哪个片段接到了
    auto timer = _timers.begin();
    while(timer != _timers.end()){
        auto last = timer->get_seq() + timer->get_buf().size();
        if(recv_seq < last) return;
        _receiver_window = window_size == 0 ? 1 :  window_size;
        // _first_unacked = min(_first_unacked,recv_seq);
        _bytes_in_flight -= (recv_seq==1)?1:timer->get_buf().size();
        timer->stop_timer();
        timer = _timers.erase(timer);
        // 不需要自增
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    auto first_expired = true;
    for(auto&i:_timers){
        i.increase_tick(ms_since_last_tick);
        if (first_expired && i.is_expired()) {
            first_expired = false;
            _segments_out.emplace();
            auto &back = _segments_out.back();
            back.header().seqno = wrap(_next_seqno, _isn);
            back.payload() = string(i.get_buf());
            back.header().fin = i.get_eof();
            i.restart_timer();
        }
    }

}

unsigned int TCPSender::consecutive_retransmissions() const { return 0; }

void TCPSender::send_empty_segment() {
    _segments_out.emplace();
    auto &back = _segments_out.back();
    back.header().seqno = wrap(_next_seqno, _isn);
    _timers.emplace_back(_initial_retransmission_timeout);
    _timers.back().start_timer(_next_seqno);
}
