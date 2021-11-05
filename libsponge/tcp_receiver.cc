#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.
#include<iostream>
template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}
using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    auto& header = seg.header();
    if(header.syn){
        isn = header.seqno;
        valid = true;
    }
    // seg.seqno may be less than isn
    auto index = header.syn? 0:unwrap(header.seqno,isn,_reassembler.head_index())-1;
    if(seg.length_in_sequence_space()==0 && index == _reassembler.head_index())
        _should_ack=false;
    else
        _should_ack =  true;
    _reassembler.push_substring(seg.payload().copy(),index,header.fin);
    ack = wrap(_reassembler.head_index()+1,isn);
    ack = ack + (_reassembler.stream_out().input_ended()?1:0);

}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if(valid)return { ack };
    else return nullopt;
}

size_t TCPReceiver::window_size() const { 
    return _reassembler.stream_out().remaining_capacity();
}
