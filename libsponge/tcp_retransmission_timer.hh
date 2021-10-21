#ifndef SPONGE_LIBSPONGE_TCP_RETRANSMISSION_TIMER_HH
#define SPONGE_LIBSPONGE_TCP_RETRANSMISSION_TIMER_HH
#include "wrapping_integers.hh"
#include "buffer.hh"

using namespace std;

//! \brief This a simple implementation of tcp retransmission _timer.
class TCPRetransmissionTimer {
  private:
    uint32_t _timer {0};
    inline static uint32_t _init_rto {0};
    inline static uint32_t _rto {0};
    bool _is_valid {false};
    bool _is_expired {false};
    string _buf {};
    bool _is_eof {false};
    bool _is_syn {false};
    uint64_t _seqno {0};


  public:
    TCPRetransmissionTimer(){}

    //! for empty
    void start_timer(uint64_t seq) {
        _timer = 0;
        _is_valid = true;
        _is_expired = false;
        _seqno = seq;
    }
    void set_str(const string &str){
        _buf = str;
    }
    void stop_timer() {
        _is_expired = false;
        _is_valid = false;
        _buf.clear();
        _is_eof = false;
        _is_syn = false;
    }

    //! return false if the _timer is stop
    void increase_tick(uint32_t tick) {
        if (_is_valid) {
            _timer += tick;
            if (_timer >= _rto) {
                _is_expired = true;
                _is_valid = false;
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
        _rto *= 2;
    }

    uint64_t get_seq() const { return _seqno; }
    const string &get_buf() const { return _buf; }
    void set_eof() { _is_eof = true; }
    void set_syn() { _is_syn = true; }
    bool is_eof() const { return _is_eof; }
    bool is_syn() const { return _is_syn; }
    void reset_timer() { _timer = 0; }

    //! static method 

    static void init_rto(uint32_t rto) { _rto = _init_rto = rto;}
    static void reset_rto() { _rto = _init_rto; }
};
#endif