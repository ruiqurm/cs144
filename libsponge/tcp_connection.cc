#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { 
    return _cfg.send_capacity - _sender.bytes_in_flight();
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
    // if this is a keep-alive segement
    if(seg.header().rst){
        //! rst have been set
        //!
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
        _active = false;
        return;
    }
    if (_receiver.ackno().has_value() and (seg.length_in_sequence_space() == 0)
        and seg.header().seqno == _receiver.ackno().value() - 1) {
        _sender.send_empty_segment();
    }else {
        _receiver.segment_received(seg);  // receiver from sender;update ack no
        if (seg.header().ack) {
            _sender.ack_received(seg.header().ackno, seg.header().win);
        } else {
            // SYN
        }
    }
    _time_since_last_segment_received = 0;
    // TODO:设定一个时间，超时则返回报文
    _sender.fill_window(); // piggybacking
    _sender.segments_out().
}

bool TCPConnection::active() const {
    return _active;
}

size_t TCPConnection::write(const string &data) {
    auto sz = _sender.stream_in().write(data);
    _sender.fill_window();
    return sz;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _sender.tick(ms_since_last_tick);
    _time_since_last_segment_received += ms_since_last_tick;
    if(_sender.stream_in().eof() && _sender.bytes_in_flight()==0){
        // option A
    }
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
}

void TCPConnection::connect() {
    if(_sender.next_seqno_absolute()==0)
        _sender.fill_window();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            _sender.send_empty_segment();
            _sender.segments_out().front().header().rst = true;
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
