#!/usr/bin/env python

import pyrtcdc
import base64

def on_channel(channel):
  print 'new channel %s created' %(channel.label)
  channel.on_message = on_message

def on_candidate(candidate):
  print 'local candidate sdp:\n%s' %(candidate)

def on_message(channel, datatype, data):
  print 'received data from channel %s: %s' %(channel.label, data)
  channel.send_message(pyrtcdc.DATATYPE_STRING, 'hi')

peer = pyrtcdc.PeerConnection(on_channel, on_candidate, stun_server='stun.services.mozilla.com')

offer = peer.generate_offer()
print 'base64 encoded local offer sdp:\n%s\n' %(base64.b64encode(offer))

print 'enter base64 encoded remote offer sdp:'
while True:
  roffer64 = raw_input('> ')
  roffer = base64.b64decode(roffer64)
  print 'remote offer sdp:\n%s' %(roffer)

  res = peer.parse_offer(roffer)
  if res >= 0:
    offer = peer.generate_offer()
    print 'new base64 encoded local offer sdp:\n%s\n' %(base64.b64encode(offer))
    break
  print 'invalid remote offer sdp'
  print 'enter base64 encoded remote offer sdp:'

print 'enter remote candidate sdp:'
while True:
  rcand = raw_input('> ')
  if peer.parse_candidates(rcand) > 0:
    break
  print 'invalid remote candidate sdp'
  print 'enter remote candidate sdp:'

peer.loop()
