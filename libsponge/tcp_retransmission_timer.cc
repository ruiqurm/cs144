#ifndef SPONGE_LIBSPONGE_TCP_RETRANSMISSION_TIMER_HH
#define SPONGE_LIBSPONGE_TCP_RETRANSMISSION_TIMER_HH
#include "wrapping_integers.hh"

//! \brief This a simple implementation of tcp retransmission timer.
class TCPRetransmissionTimer{
	private:

	public:
		TCPRetransmissionTimer(){}

		void start_timer();

		void stop_timer();
		
		//! return false if the timer is stop
		bool increase_tick(uint32_t tick);

		//! stop the timer,double the RTO,restart the timer
		void expired_timer();

		//! check if the timer is running
		bool is_running();

		//! check if the timer is expired
		bool is_expired();

};
#endif