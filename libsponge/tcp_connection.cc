#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { 
    return _sender.stream_in().remaining_capacity();
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
    // cerr<<"\n----------------------------\n";
    // switch (_state) {
    //     case State::LISTEN:
    //         cerr<<"Listen"<<endl;
    //         break;
    //     case State::SYN_SENT:
    //         cerr<<"SYN_SENT"<<endl;
    //         break;
    //     case State::SYN_RECEIVED:
    //         cerr<<"SYN_RECEIVED"<<endl;
    //         break;
    //     case State::ESTABLISHED:
    //         cerr<<"ESTABLISHED"<<endl;
    //         break;
    //     case State::FIN_WAIT1:
    //         cerr<<"FIN_WAIT1"<<endl;
    //         break;
    //     case State::FIN_WAIT2:
    //         cerr<<"FIN_WAIT2"<<endl;
    //         break;
    //     case State::CLOSING:
    //         cerr<<"CLOSING"<<endl;
    //         break;
    //     case State::LAST_ACK:
    //         cerr<<"LAST_ACK"<<endl;
    //         break;
    //     case State::CLOSE_WAIT:
    //         cerr<<"CLOSE_WAIT"<<endl;
    //         break;
    //     case State::TIME_WAIT:
    //         cerr<<"TIME_WAIT"<<endl;
    //         break;
    //     case State::CLOSED:
    //         cerr<<"CLOSED"<<endl;
    //         break;
    // }
    // cerr<<seg.header().to_string();
    // cerr<<"size: "<<seg.length_in_sequence_space()<<endl;
    // cerr<<" _sender.bytes_in_flight() "<< _sender.bytes_in_flight()<<endl;
    // cerr<<" _sender.buffer "<< _sender.stream_in().buffer_size()<<endl;
    // cerr<<"----------------------------\n";
    if(seg.header().rst){
        set_error();
        return;
    }
    //! update next_seq and window_linger_after_streams_finish
    // bool should_response_in_established = _sender.check_ack(seg.header().ackno);
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
    switch(_state){
        case State::LISTEN:
            if(seg.header().syn){
                _sender.fill_window();
                _state = State::SYN_RECEIVED;
            }
            break;
        case State::SYN_SENT:
            if(seg.header().syn){
                _state = seg.header().ack?State::ESTABLISHED:State::SYN_RECEIVED;
                bool should_clean = !seg.header().ack;
                _sender.send_sized_one_segment(should_clean);
            }
            break;
        case State::SYN_RECEIVED:
            if(seg.header().ack){
                _state = State::ESTABLISHED;
            }
            break;
        case State::ESTABLISHED:
            if(_receiver.stream_out().input_ended()){
                _state = State::CLOSE_WAIT;
                _linger_after_streams_finish = false;
                if(!_sender.stream_in().input_ended() && _sender.stream_in().buffer_empty()){
                    _sender.send_empty_segment();
                    if(!_sender.stream_in().input_ended()){
                        _sender.segments_out().back().header().fin = true;
                        _state = State::LAST_ACK;
                    }
                }else {
                    _sender.fill_window();
                }
            }else if(!_sender.stream_in().buffer_empty()){
                _sender.fill_window();
                if( _sender.segments_out().back().header().fin)_state = State::FIN_WAIT1;
            }else if((_sender.stream_in().input_ended() && _sender.bytes_in_flight()==0)){
                // 单窗口eof的情况
                _sender.fill_window();
                _state = State::FIN_WAIT1;
            }else if(_receiver.should_ack()){
                _sender.send_empty_segment();
            }
            
            break;
        case State::FIN_WAIT1:
            if(seg.header().fin){
                _state = State::CLOSING;
                _sender.send_empty_segment();
            }else if(seg.header().ack && _sender.bytes_in_flight()==0){
                _state = State::FIN_WAIT2;
            }else if(_receiver.should_ack()){
                _sender.send_empty_segment();
            }
            break;
        case State::FIN_WAIT2:
            if(_receiver.stream_out().input_ended()){
                _state = State::TIME_WAIT;
                _sender.send_empty_segment();
            }else {
                _sender.send_empty_segment();
            }
            break;
        case State::CLOSING:
            if(seg.header().ack && _sender.bytes_in_flight()==0){
                _state = State::TIME_WAIT;
            }
            break;
        case State::CLOSE_WAIT:
            if(seg.header().fin)_sender.send_empty_segment();
            else{
                _sender.fill_window();
                if(_sender.segments_out().back().header().fin){
                    _state = State::LAST_ACK;
                }
            }
            break;
        case State::LAST_ACK:
            if(seg.header().ack && _sender.bytes_in_flight()==0){
                _state = State::CLOSED;
                _active = false;
            }
            break;
        case State::TIME_WAIT:
            if(seg.header().fin)_sender.send_empty_segment();
            break;
        default:
            break;
    }
    // report_state();
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
        set_error();
    }
    move_to_outer_queue();
    if((_sender.bytes_in_flight()==0 && _sender.stream_in().input_ended() &&_receiver.stream_out().input_ended()) && _time_since_last_segment_received>= 10 * _cfg.rt_timeout){
        // option A
        _active = false;
        _state = State::CLOSED;
        return;
    }
}

void TCPConnection::end_input_stream() {
    if(_state == State::ESTABLISHED || _state == State::CLOSE_WAIT){
        cerr<<"关闭"<<_sender.stream_in().buffer_size()<<endl;
        _sender.stream_in().end_input();
        _sender.fill_window();
        // cerr<<"!!!!!!!!!!!!!!!!!!!!\n"
        //     <<_sender.segments_out().back().header().to_string()
        //     <<"!!!!!!!!!!!!!!!!!!!!\n";
        // cerr<<_sender.bytes_in_flight()<<"in flight "<<_sender.stream_in().eof()<<"\n";
        move_to_outer_queue();
        _linger_after_streams_finish = _state ==State::ESTABLISHED?true:false;
        if(_state == State::ESTABLISHED){
            _state = _sender.segments_out().back().header().fin?State::FIN_WAIT1:State::ESTABLISHED;
        }else{
            _state = _sender.segments_out().back().header().fin?State::LAST_ACK:State::CLOSE_WAIT;

        }
    }
}

void TCPConnection::connect() {
    if(_sender.next_seqno_absolute()==0) {
        _sender.fill_window();
        move_to_outer_queue();
        _state = State::SYN_SENT;
    }
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";
            // Your code here: need to send a RST segment to the peer
            _sender.send_empty_segment();
            _sender.segments_out().back().header().rst = true;
            set_error();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
void TCPConnection::move_to_outer_queue(){
    // 在这里加个删掉empty
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

void TCPConnection::set_error(){
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    _state = State::CLOSED;
    _active = false;
    _linger_after_streams_finish = false;
}
void TCPConnection::report_state(){
    cerr<<"\033[36m state:";
    switch (_state) {
        case State::LISTEN:
            cerr<<"Listen";
            break;
        case State::SYN_SENT:
            cerr<<"SYN_SENT";
            break;
        case State::SYN_RECEIVED:
            cerr<<"SYN_RECEIVED";
            break;
        case State::ESTABLISHED:
            cerr<<"ESTABLISHED";
            break;
        case State::FIN_WAIT1:
            cerr<<"FIN_WAIT1";
            break;
        case State::FIN_WAIT2:
            cerr<<"FIN_WAIT2";
            break;
        case State::CLOSING:
            cerr<<"CLOSING";
            break;
        case State::LAST_ACK:
            cerr<<"LAST_ACK";
            break;
        case State::CLOSE_WAIT:
            cerr<<"CLOSE_WAIT";
            break;
        case State::TIME_WAIT:
            cerr<<"TIME_WAIT";
            break;
        case State::CLOSED:
            cerr<<"CLOSED";
            break;
    }
    cerr<<"\nactive："<<_active
        <<"\nlinger: "<<_linger_after_streams_finish
        <<"\nreceiver,end:"<<_receiver.stream_out().eof()
        <<"\nsender end:"<<_sender.stream_in().eof()
        <<"\033[0m"
        <<endl;
}