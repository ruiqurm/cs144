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
    auto index = header.syn? 0:unwrap(header.seqno,isn,_reassembler.head_index())-1;
    _reassembler.push_substring(seg.payload().copy(),index,header.fin);
    ack = wrap(_reassembler.head_index()+1,isn);
    ack = ack + (_reassembler.input_ended()?1:0);
    cout<<"ack"<<ack<<"\n";
    // cout<<"ack"<<_reassembler.unassembled_bytes()<<"\n";
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if(valid)return { ack };
    else return nullopt;
}

size_t TCPReceiver::window_size() const { 
    return _reassembler.window();
}
