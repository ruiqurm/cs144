#ifndef SPONGE_LIBSPONGE_TCP_RETRANSMISSION_TIMER_HH
#define SPONGE_LIBSPONGE_TCP_RETRANSMISSION_TIMER_HH
#include "wrapping_integers.hh"

#include <memory>

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
    // WrappingInt32 tracked_ackno; //unused
    shared_ptr<Buffer> _buf;
    bool _is_eof;

  public:
    TCPRetransmissionTimer(uint32_t initrto)
        : _timer(0)
        , _init_rto(initrto)
        , _rto(initrto)
        , _is_valid(false)
        , _is_expired(false)
        , _retransmissions_times(0)
        , _buf(nullptr)
        , _is_eof(false) {}
    void start_timer(string &&s, bool is_eof) {
        _timer = 0;
        _is_valid = true;
        _is_expired = false;
        _rto = _init_rto;
        _retransmissions_times = 0;
        _buf = make_shared<Buffer>(std::forward<string>(s));
        _is_eof = is_eof;
    }

    void stop_timer() {
        _is_valid = false;
        _buf.reset();
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

    Buffer &get_buf() const { return *_buf; }
    bool get_eof() const { return _is_eof; }
};
#endif