#!/usr/bin/env ruby

$LOAD_PATH.unshift(File.dirname(__FILE__) + '/lib')


require "samwise"


# create a new client that connects to a running samwise instance
sam = Samwise::Client.new "ipc://../../sam_ipc"


# test if the connection was established correctly
if sam.ping
  puts "Samwise is reachable"
else
  puts "Samwise is not reachable"
end


# create a new message to send
msg = Samwise::Message.new "hi!"

msg.exchange = "amq.direct"
msg.options = { user_id: "1", app_id: "2" }
msg.headers = { prop_1: "prop1", prop_2: "prop2" }

raise "could not publish" unless sam.publish msg

# close the connection
sam.close!
