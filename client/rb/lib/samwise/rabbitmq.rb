  module Samwise::RabbitMQ

    def self.publish_roundrobin exch, rkey, payload
      msg = Samwise::Message.new
      msg.add "publish", "round robin", exch, rkey, payload
      msg.send
    end


    def self.publish_redundant n, exch, rkey, payload
      msg = Samwise::Message.new
      msg.add "publish", "redundant", n.inspect, exch, rkey, payload
      msg.send
    end


    def self.exchange_declare name, type
      msg = Samwise::Message.new
      msg.add "rpc", "", "exchange.declare", name, type
      msg.send
    end


    def self.exchange_delete name
      msg = Samwise::Message.new
      msg.add "rpc", "", "exchange.delete", name
      msg.send
    end

  end
