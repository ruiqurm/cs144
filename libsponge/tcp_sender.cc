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
    , _timers {}
    , _retransmission_times{0}{}

uint64_t TCPSender::bytes_in_flight() const { 
    return _bytes_in_flight; 
}

void TCPSender::fill_window() {
    if (_next_seqno) {
        if ( (_stream.buffer_empty() && !_stream.eof()) || (_stream.eof() && sended_eof) )
            return;//nothing to send
        auto max_allow_sending_data_count = (_receiver_window > _bytes_in_flight)?_receiver_window - _bytes_in_flight:0;
        if(max_allow_sending_data_count==0)
            return;

        _segments_out.emplace();
        auto &back = _segments_out.back();
        back.header().syn = _next_seqno == 0;  // if  _next_seqno==0,syn = true;
        back.header().seqno = wrap(_next_seqno, _isn);
        auto str = _stream.read(max_allow_sending_data_count);
        back.header().fin = _stream.eof() && _next_seqno && max_allow_sending_data_count >= str.size()+1;
        _timers.emplace_back(_initial_retransmission_timeout);

        // last timer start
        _timers.back().start_timer(_next_seqno);
        _timers.back().set_str(str);
        if(back.header().fin)_timers.back().set_eof();

        // set up payload
        back.payload() = string(str);

        // add increment
        auto increment =str.size() + static_cast<int>(back.header().fin);
        _bytes_in_flight += increment;
        _next_seqno += increment;
        sended_eof = back.header().fin;
    } else {
        _segments_out.emplace();
        auto &back = _segments_out.back();
        back.header().syn = true;  
        back.header().seqno = wrap(0, _isn);
        _timers.emplace_back(_initial_retransmission_timeout);
        _timers.back().start_timer(0);
        _timers.back().set_syn();
        _next_seqno =  1;
        _bytes_in_flight += 1;
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    auto recv_seq = unwrap(ackno, _isn, _next_seqno);
    // if not in valid range,abort
    // 确认是哪个片段接到了
    _receiver_window = window_size == 0 ? 1 :  window_size;
    auto timer = _timers.begin();
    while(timer != _timers.end()){
        auto seg_start = timer->get_seq();
        auto size = timer->get_buf().size() + static_cast<int>(timer->is_eof());
        auto seg_end = seg_start + size;
        if(recv_seq < seg_end){
            // if(){
            //     // 部分接收
            //     auto recv_part_count = recv_seq - seg_start;      
            //     _bytes_in_flight -= recv_part_count;  
            // }
            return;
        }
        _bytes_in_flight -= (recv_seq==1)?1:size;
        timer->stop_timer();
        timer = _timers.erase(timer);
        // 不需要自增
        // cout<<"byte: "<<_bytes_in_flight<<" seq: "<<_next_seqno<<" eof"<<_stream.eof()<<" writen"<<_stream.bytes_written()<<endl;
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    auto first_expired = true;
    for(auto&i:_timers){
        i.increase_tick(ms_since_last_tick);
        if (first_expired && i.is_expired()) {
            first_expired = false;
            if(last_retransmission_seq == (i.get_seq()&0xffffffff)){
                _retransmission_times++;
            }else{
                last_retransmission_seq = (i.get_seq()&0xffffffff);
                _retransmission_times = 1;
            }
            
            _segments_out.emplace();
            auto &back = _segments_out.back();
            back.header().seqno = wrap(i.get_seq(), _isn);
            back.payload() = string(i.get_buf());
            back.header().fin = i.is_eof();
            back.header().syn = i.is_syn();
            i.restart_timer();
        }
    }

}

unsigned int TCPSender::consecutive_retransmissions() const { return _retransmission_times; }

void TCPSender::send_empty_segment() {
    _segments_out.emplace();
    auto &back = _segments_out.back();
    back.header().seqno = wrap(_next_seqno, _isn);
    _timers.emplace_back(_initial_retransmission_timeout);
    _timers.back().start_timer(_next_seqno);
}
