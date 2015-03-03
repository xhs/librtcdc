#!/usr/bin/env python

import pyrtcdc
import base64

def on_channel(channel):
  print 'new channel %s created' %(channel.label)
  channel.set_on_message(on_message)

def on_message(channel, datatype, data):
  print 'received data from channel %s: %s' %(channel.label, data)

peer = pyrtcdc.PeerConnection(on_channel)

print 'stun://%s:%d\n' %(peer.stun_server, peer.stun_port)

offer = peer.generate_offer()
print 'base64 encoded local offer sdp:\n%s\n' %(base64.b64encode(offer))

cand = peer.generate_candidates()
print 'local candidate sdp:\n%s' %(cand)

print 'enter base64 encoded remote offer sdp:'
while True:
  roffer64 = raw_input('> ')
  roffer = base64.b64decode(roffer64)
  print 'remote offer sdp:\n%s' %(roffer)

  res = peer.parse_offer(roffer)
  if res >= 0:
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
