## librtcdc is a tiny WebRTC DataChannel implementation that works everywhere (Beta).

#### Features

* Tiny <2k LOC, easy to understand/bind/extend
* Talks with each other, Chrome(39/40), Firefox?, Opera?
* 'Official' Python binding
* Lack of docs

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
    channel.send(pyrtcdc.DATATYPE_STRING, 'Roger')

# called when a channel is created by the remote peer
def on_channel(peer, channel):
    print 'channel %s created' %(channel.label)
    channel.on_message = on_message

# called when a new local candidate is found
def on_candidate(peer, candidate):
    print 'local candidate sdp:\n%s' %(candidate)

# called when connected to remote peer
def on_connect(peer):
  peer.create_data_channel('demo', on_open=on_open)

# called when channel is opened
def on_open(channel):
  channel.on_message = on_message
  channel.send(pyrtcdc.DATATYPE_STRING, 'Hi')

peer = PeerConnection(on_channel, on_candidate, stun_server='stun.services.mozilla.com')

# generate local offer sdp and start candidates gathering
offer = peer.generate_offer()

# offer/answer/candidates signalling here (or somewhere)
# ...

# running until the sun cools
peer.loop()
```

#### License

BSD 2-Clause
