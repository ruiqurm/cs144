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
    bool eof = header.fin;
    if(header.syn){
        isn = header.seqno;
        valid = true;
        ack = wrap(1,isn);
        if(eof){
            // auto index = unwrap(header.seqno,isn,_reassembler.head_index());
            _reassembler.push_substring("",0,eof);
            ack = wrap(2,isn);
        }
    }else{
        auto index = unwrap(header.seqno,isn,_reassembler.head_index())-1;
        _reassembler.push_substring(seg.payload().copy(),index,eof);
        ack = wrap(_reassembler.head_index()+1,isn);
        // cout<<"index"<<index<<"\n";
        // cout<<"ack"<<ack<<"\n";
    }
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if(valid)return { ack };
    else return nullopt;
}

size_t TCPReceiver::window_size() const { 
    return _capacity - _reassembler.unassembled_bytes();
}
