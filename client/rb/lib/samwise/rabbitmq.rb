  module Samwise::RabbitMQ

    PUBLISHING_ARGS = [
      :echange,
      :routing_key,
      :mandatory,
      :immediate
    ]

    PUBLISHING_OPTS = [
      :content_type,
      :content_encoding,
      :delivery_mode,      # beetle // either :persistent or not
      :priority,
      :correlation_id,
      :reply_to,
      :expiration,         # beetle // as :expires_at
      :message_id,
      :type,
      :user_id,
      :app_id,
      :cluster_id
    ]


    def self.publish_roundrobin args, opts, payload
      msg = Samwise::Message.new
      msg.add "publish", "round robin"
      self.publish msg, args, opts, payload
    end


    def self.publish_redundant n, args, opts, payload
      msg = Samwise::Message.new
      msg.add "publish", "redundant", n.inspect
      self.publish msg, args, opts, payload
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

    private


    def self.publish msg, args, opts, payload

      # add arguments
      msg.add *(args.values_at :exchange, :routing_key, :mandatory, :immediate)

      # add opts (retain order)
      msg.add PUBLISHING_OPTS.size
      PUBLISHING_OPTS.each do |key|
        msg.add opts[key]
      end

      # add headers
      if opts[:headers]
        flat_headers = opts[:headers].collect { |k, v| [k, v] }.flatten
        msg.add flat_headers.size, *flat_headers
      else
        msg.add 0
      end

      # add payload
      msg.add payload

      msg.send
    end


  end
