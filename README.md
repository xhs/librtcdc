## librtcdc is a tiny WebRTC DataChannel implementation that works everywhere (Alpha).

#### Features

* Tiny <2k LOC, thus easy to understand/extend
* Talks with each other, Chrome(39/40), Firefox?, Opera?
* 'Official' Python binding
* Lack of docs
* Buggy (Aaaaaaaah, help!)

#### Prerequisites

* Python & Cython (only for Python binding)
* ICE - [libnice](http://nice.freedesktop.org/wiki/)
* DTLS - [OpenSSL](https://www.openssl.org/)
* SCTP - [usrsctp](https://sctp-refimpl.googlecode.com/svn/trunk/KERN/usrsctp/)

#### Python demo

```python
import pyrtcdc
from pyrtcdc import PeerConnection

# called when the channel received a message
def on_message(channel, datatype, data):
    print 'received data from channel %s: %s' %(channel.label, data)
    channel.send_message(pyrtcdc.RTCDC_DATATYPE_STRING, 'Roger')

# called when a channel is created by the remote peer
def on_channel(channel):
    print 'channel %s created' %(channel.label)
    channel.on_message = on_message

# called when a new local candidate is found
def on_candidate(candidate):
    print 'local candidate sdp:\n%s' %(candidate)

peer = PeerConnection(on_channel, on_candidate, stun_server='stun.services.mozilla.com')

# generate local offer sdp and start candidates gathering
offer = peer.generate_offer()

# offer/answer/candidates signalling here (or somewhere)
# ...

# running until the sun cools
peer.loop()
```

#### License

BSD
