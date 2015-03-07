## librtcdc is a tiny WebRTC DataChannel implementation that works everywhere (Alpha).

#### Prerequisites

Python & Cython (only for python binding)

ICE/UDP - [libnice](http://nice.freedesktop.org/wiki/)

DTLS - [OpenSSL](https://www.openssl.org/)

SCTP - [usrsctp](https://sctp-refimpl.googlecode.com/svn/trunk/KERN/usrsctp/)

#### Python demo

```
import pyrtcdc
from pyrtcdc import PeerConnection

def on_message(channel, datatype, data):
    print 'received data from channel %s: %s' %(channel.label, data)
    channel.send_message(pyrtcdc.RTCDC_DATATYPE_STRING, 'Roger')

def on_channel(channel):
    print 'channel %s created' %(channel.label)
    channel.on_message = on_message

peer = PeerConnection(on_channel)

# offer/answer signalling here
# ...

peer.loop()
```
