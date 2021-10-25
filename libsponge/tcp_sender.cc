#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>
#include <algorithm>
#include<cmath>
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
    , _retransmission_times{0}{
        TCPRetransmissionTimer::init_rto(retx_timeout);
    }

uint64_t TCPSender::bytes_in_flight() const { 
    return _bytes_in_flight; 
}

void TCPSender::fill_window() {
    if (_next_seqno) {
        if(!_is_syn)
            return;
        if ( (_stream.buffer_empty() && !_stream.eof()) || (_stream.eof() && sended_eof) )
            return;//nothing to send
        decltype(_bytes_in_flight) receiver_window = (_receiver_window==0)?1:_receiver_window;
        auto current_window = (receiver_window > _bytes_in_flight)?receiver_window - _bytes_in_flight:0;
        
        // if the window is full,or there is no more input string,then return
        if(current_window==0)return; 

        // how many window actually need?
        auto number_of_window_need = _stream.buffer_size();
        auto number_of_window_need_include_fin = number_of_window_need+static_cast<int>(_stream.input_ended());

        // the maximum size of a segment 
        auto max_sender_window = static_cast<uint16_t>(TCPConfig::MAX_PAYLOAD_SIZE); 
        // can/should this time send with a fin flag?
        bool can_carry_fin = current_window >= number_of_window_need_include_fin;
        auto number_of_window_used = min(current_window,number_of_window_need);

        int number_of_segment = static_cast<int>(number_of_window_used==0?1:ceil(static_cast<double>(number_of_window_used) / max_sender_window));
        for(int i=0;i<number_of_segment;i++){
            auto size_of_segment = (number_of_window_used <= max_sender_window)?number_of_window_used:max_sender_window;
            _segments_out.emplace();
            auto &back = _segments_out.back();
            back.header().syn = _next_seqno == 0;  // if  _next_seqno==0,syn = true;
            back.header().seqno = wrap(_next_seqno, _isn);
            auto str = _stream.read(size_of_segment);
            if (i==number_of_segment - 1){// last segment may be the final segment
                // if size_of_segment==TCPConfig::MAX_PAYLOAD_SIZE
                // back.header().fin = _stream.eof(), because MAX_PAYLOAD_SIZE limits payload only
                // else back.header().fin = _stream.eof() && size_of_segment >= str.size()+1;
                bool a = (size_of_segment==TCPConfig::MAX_PAYLOAD_SIZE);
                back.header().fin = _stream.eof() && (a? true : can_carry_fin);
            }
            _timers.emplace_back();

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

            number_of_window_used -= max_sender_window;
        }
    } else {
        _segments_out.emplace();
        auto &back = _segments_out.back();
        back.header().syn = true;  
        back.header().seqno = wrap(0, _isn);
        _timers.emplace_back();
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
    _receiver_window = window_size;
    auto timer = _timers.begin();
    bool is_new_data = false;
    if(_timers.empty())return;
    auto back = _timers.back();
    auto max_ack = back.get_seq()+back.get_buf().size() +  static_cast<int>(back.is_eof()) + static_cast<int>(back.is_syn());
    if(recv_seq>max_ack)return;
    while(timer != _timers.end()){
        auto seg_start = timer->get_seq();
        auto size = timer->get_buf().size() + static_cast<int>(timer->is_eof()) + static_cast<int>(timer->is_syn());
        auto seg_end = seg_start + size;
        if(recv_seq < seg_end){
            // if(){
            //     // 部分接收
            //     auto recv_part_count = recv_seq - seg_start;      
            //     _bytes_in_flight -= recv_part_count;  
            // }
            break;
        }
        if(recv_seq >0 )_is_syn = true;
        is_new_data = true;
        _bytes_in_flight -= (recv_seq==1)?1:size;
        timer->stop_timer();
        timer = _timers.erase(timer);
        // 不需要自增
        // cout<<"byte: "<<_bytes_in_flight<<" seq: "<<_next_seqno<<" eof"<<_stream.eof()<<" writen"<<_stream.bytes_written()<<endl;
    }
    if(is_new_data){
        _retransmission_times = 0;
        for(auto&i:_timers){
            i.reset_timer();
        }
        TCPRetransmissionTimer::reset_rto();
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    auto first_expired = true;
    for(auto&i:_timers){
        i.increase_tick(ms_since_last_tick);
        if (first_expired && i.is_expired()) {
            first_expired = false;
            _retransmission_times++;
            
            _segments_out.emplace();
            auto &back = _segments_out.back();
            back.header().seqno = wrap(i.get_seq(), _isn);
            back.payload() = string(i.get_buf());
            back.header().fin = i.is_eof();
            back.header().syn = i.is_syn();
            i.restart_timer(_receiver_window!=0);
        }
    }
    if(!first_expired){
        for(auto&i:_timers){
            i.reset_timer();
        }
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _retransmission_times; }

void TCPSender::send_empty_segment() {
    // _segments_out.emplace();
    // auto &back = _segments_out.back();
    // back.header().seqno = wrap(_next_seqno, _isn);
    // _timers.emplace_back(_initial_retransmission_timeout);
    // _timers.back().start_timer(_next_seqno);
}
