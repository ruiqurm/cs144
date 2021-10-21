#ifndef SPONGE_LIBSPONGE_TCP_RETRANSMISSION_TIMER_HH
#define SPONGE_LIBSPONGE_TCP_RETRANSMISSION_TIMER_HH
#include "wrapping_integers.hh"
#include "buffer.hh"

using namespace std;

//! \brief This a simple implementation of tcp retransmission _timer.
class TCPRetransmissionTimer {
  private:
    uint32_t _timer;
    uint32_t _init_rto;
    uint32_t _rto;
    bool _is_valid;
    bool _is_expired;
    uint8_t _retransmissions_times;
    string _buf;
    bool _is_eof;
    uint64_t _seqno; 


  public:
    TCPRetransmissionTimer(uint32_t initrto)
        : _timer(0)
        , _init_rto(initrto)
        , _rto(initrto)
        , _is_valid(false)
        , _is_expired(false)
        , _retransmissions_times(0)
        , _buf {}
        , _is_eof(false)
        , _seqno(0){}
    TCPRetransmissionTimer():TCPRetransmissionTimer(1000){}
    void start_timer(uint64_t seq,string &s, bool is_eof) {
        _timer = 0;
        _is_valid = true;
        _is_expired = false;
        _rto = _init_rto;
        _retransmissions_times = 0;
        _buf = s;
        _is_eof = is_eof;
        _seqno = seq;
    }
    //! for empty
    void start_timer(uint64_t seq) {
        _timer = 0;
        _is_valid = true;
        _is_expired = false;
        _rto = _init_rto;
        _retransmissions_times = 0;
        _is_eof = false;
        _seqno = seq;
    }
    void stop_timer() {
        _is_valid = false;
        _buf.clear();
    }

    //! return false if the _timer is stop
    void increase_tick(uint32_t tick) {
        if (_is_valid) {
            _timer += tick;
            if (tick >= _rto) {
                _is_expired = true;
                _is_valid = false;
                _rto *= 2;
            }
        }
    }

    //! check if the _timer is running
    bool is_running() const { return _is_valid; }

    //! check if the _timer is expired
    bool is_expired() const { return _is_expired; }

    void restart_timer() {
        _timer = 0;
        _is_valid = true;
        _is_expired = false;
        _retransmissions_times++;
    }
    unsigned int get_retransmissions_times() const { return _retransmissions_times; }

    uint64_t get_seq() const { return _seqno; }
    const string &get_buf() const { return _buf; }
    bool get_eof() const { return _is_eof; }
};
#endif