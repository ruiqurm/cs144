#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

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
            auto index = unwrap(header.seqno,isn,_reassembler.head_index());
            _reassembler.push_substring("",index,eof);
            ack = wrap(2,isn);
        }
    }else{
        auto index = unwrap(header.seqno,isn,_reassembler.head_index());
        ack = wrap(_reassembler.head_index(),isn);
        _reassembler.push_substring(seg.payload().copy(),index,eof);
    }
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if(valid)return { ack };
    else return nullopt;
}

size_t TCPReceiver::window_size() const { 
    return _capacity - _reassembler.unassembled_bytes();
}
