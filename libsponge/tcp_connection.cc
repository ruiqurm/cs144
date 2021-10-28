#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { 
    return _sender.remaining_outbound_capacity();
}

size_t TCPConnection::bytes_in_flight() const {
    return _sender.bytes_in_flight();
}

size_t TCPConnection::unassembled_bytes() const {
    return _receiver.unassembled_bytes();
}

size_t TCPConnection::time_since_last_segment_received() const {
    return _time_since_last_segment_received;
}

void TCPConnection::segment_received(const TCPSegment &seg) {
    //! if this is a keep-alive segement
    if(seg.header().rst){
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
        _active = false;
        cout<<"segment_received 11111"<<endl;
        return;
    }
    //! update next_seq and window
    _sender.ack_received(seg.header().ackno,seg.header().win);
    //! flush clock
    _time_since_last_segment_received = 0;
    if (_receiver.ackno().has_value() and (seg.length_in_sequence_space() == 0)
        and seg.header().seqno == _receiver.ackno().value() - 1) {
        _sender.send_empty_segment();
        return;
    }

    //! get data from segment; update ack
    _receiver.segment_received(seg); 

    // piggybacking
    if(seg.header().fin&&_sender.stream_in().eof()&& _sender.bytes_in_flight()==0){
         _linger_after_streams_finish = true;
        if(_receiver.stream_out().eof()){
            //! a special case of passively close
            //! should send two packets
            _sender.send_empty_segment();
            _sender.send_empty_segment();
            _sender.segments_out().back().header().fin = true;
        }
    }else if(_sender.stream_in().buffer_empty()){
        _sender.send_empty_segment();
    }else{
        _sender.fill_window(); 
    }
    move_to_outer_queue();
}

bool TCPConnection::active() const {
    return _active;
}

size_t TCPConnection::write(const string &data) {
    auto sz = _sender.stream_in().write(data);
    _sender.fill_window();
    move_to_outer_queue();
    return sz;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _sender.tick(ms_since_last_tick);
    _time_since_last_segment_received += ms_since_last_tick;
    if(_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS){
        _sender.send_empty_segment();
        _sender.segments_out().back().header().rst = true;
        _active = false;
    }
    // if(_sender.stream_in().eof() && _sender.bytes_in_flight()==0 && _receiver.stream_out().eof()){
    //     _linger_after_streams_finish = true;
    // }
    if(_linger_after_streams_finish && _time_since_last_segment_received>= 10 * _cfg.rt_timeout){
        // option A
        cout<<"option A"<<endl;
        _active = false;
        return;
    }
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    move_to_outer_queue();
}

void TCPConnection::connect() {
    if(_sender.next_seqno_absolute()==0) {
        _sender.fill_window();
        move_to_outer_queue();
        _active= true;
    }
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            _sender.send_empty_segment();
            _sender.segments_out().back().header().rst = true;
            _active = false;
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
void TCPConnection::move_to_outer_queue(){
    auto has_ack = _receiver.ackno().has_value();
    while(!_sender.segments_out().empty()){
        auto front = _sender.segments_out().front();
        front.header().ack = has_ack;
        if(has_ack){
            front.header().ackno = _receiver.ackno().value();
        }
        _segments_out.push(front);
        _sender.segments_out().pop();
    }
}
