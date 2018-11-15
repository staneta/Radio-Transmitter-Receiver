# Radio-Transmitter-Receiver

### Receiver:
* Sends LOOKUP message on [DISCOVER_ADDR] and [CTRL_PORT] every 5 seconds and based on the responses creates a list of radio stations.
* Receives data from specified station and saves it to buffer. When buffer is 3/4 full, the playback starts.
* If a byte wasn't received, the receiver asks station for a reexmit of the missing byte.
* Allows to connect to it with TELNET on [UI_PORT] and provides simple GUI for changing stations

### Transmitter:
* Receives LOOKUP messages and replies with its [MCAST ADDRESS] and [DATA_PORT].
* Receives REXMIT messages and reexmits bytes.
* Sends data on [MCAST_ADDR] in packets of [PSIZE] bytes.



#### Options
* Receiver:

|                				|							  |
|-------------------------------|-----------------------------|
|`-d`            				|sets _DISCOVER_ADDR_         |
|`-P`            				|sets _DATA_PORT_             |
|`-C`            				|sets _CTRL_PORT_             |
|`-U`							|sets _UI_PORT_				  |
|`-b`							|sets buffer size			  |
|`-R`							|sets time after which we ask for reexmit |


* Transmitter:

|||
|-------------------------------|-----------------------------|
|`-a`            				|sets _MCAST_ADDR_            |
|`-P`            				|sets _DATA_PORT_             |
|`-C`            				|sets _CTRL_PORT_             |
|`-p`          					|sets _PSIZE_ 	              |
|`-f`							|sets buffer size			  |
|`-R`							|sets time between reexmits	  |
|`-n`							|sets name of the station	  |




#### Example Usage
* Receiver:
```sh
$ ./sikradio-receiver -b 500000 | play -t raw -c 2 -r 44100 -b 16 -e signed-integer --buffer 32768 -
```


* Transmitter:
```sh
$ sox -S "yoursong.mp3" -r 44100 -b 16 -e signed-integer -c 2 -t raw - | pv -q -L $((44100*4)) | ./sikradio-sender -a 239.10.11.12 -n "Radio Name"
```
