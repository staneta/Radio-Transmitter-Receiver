all: sikradio-sender sikradio-receiver

sikradio-sender: sikradio-sender.cc controller.cc transmitter.cc err.cc
	g++ -Wall -pthread -O2 -std=c++17 sikradio-sender.cc controller.cc transmitter.cc err.cc -o sikradio-sender

sikradio-receiver: sikradio-receiver.cc discoverer.cc menu.cc player.cc reexmiter.cc err.cc
	g++ -Wall -pthread -O2 -std=c++17 sikradio-receiver.cc discoverer.cc menu.cc player.cc reexmiter.cc err.cc -o sikradio-receiver

.PHONY: clean TARGET
clean:
	rm -f sikradio-sender *.o *~ *.bak sikradio-sender sikradio-receiver

