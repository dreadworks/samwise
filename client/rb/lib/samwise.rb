require 'rbczmq'


module Samwise

  # abstract superclass
  class Error < StandardError; end

  # if no connection to samwise could be established
  class ConnectionFailure < StandardError; end

  # if either send or recv run into a timeout
  class Timeout < StandardError; end

end


require 'samwise/connection'



# class Samwise

#   def publish (distribution, exchange, routing_key, message)
#     msg = ZMQ::Message.new

#     # msg.push ZMQ::Frame("rabbitmq")
#     msg.push ZMQ::Frame(message)
#     msg.push ZMQ::Frame(routing_key)
#     msg.push ZMQ::Frame(exchange)
#     msg.push ZMQ::Frame(distribution)

#     puts "sending message" if @opts.verbose
#     @req.send_message msg
#     raise "message was not sent" unless msg.gone?

#     # apply back pressure and comply with
#     # the strict REQ/REP cycle
#     reply = @req.recv
#     puts "received #{reply}" if @opts.verbose

#   end

# end
