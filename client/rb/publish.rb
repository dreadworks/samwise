#!/usr/bin/env ruby

require 'ostruct'
require 'optparse'
require 'rbczmq'


class Publisher

  def initialize (endpoint, opts)
    @opts = opts
    @ctx = ZMQ::Context.new

    puts "connecting to #{endpoint}" unless @opts.quiet
    @req = @ctx.socket(:REQ)
    raise "could not create socket" unless @req

    @req.connect(endpoint)
    raise "could not connect" unless @req.state & ZMQ::Socket::CONNECTED

  end

  def destroy!
    @ctx.destroy
  end

  def publish (distribution, exchange, routing_key, message)
    msg = ZMQ::Message.new

    # msg.push ZMQ::Frame("rabbitmq")
    msg.push ZMQ::Frame(message)
    msg.push ZMQ::Frame(routing_key)
    msg.push ZMQ::Frame(exchange)
    msg.push ZMQ::Frame(distribution)

    puts "sending message" if @opts.verbose
    @req.send_message msg
    raise "message was not sent" unless msg.gone?

    # apply back pressure and comply with
    # the strict REQ/REP cycle
    reply = @req.recv
    puts "received #{reply}" if @opts.verbose

  end

end


def publish (opts)
  puts "publishing #{opts.n} messages" unless opts.quiet
  pub = Publisher.new("ipc://../../sam_ipc", opts)

  time_start = Time.now
  opts.n.times do |i|
    puts "publishing message #{i}" if opts.verbose
    pub.publish("redundant", "amq.direct", "", "hi!")
  end
  time_end = Time.now

  puts "done in #{time_end - time_start}, exiting" unless opts.quiet
  pub.destroy!
end


#
#  parse command line options
#
class OptionParser

  def self.parse (args)
    options = OpenStruct.new
    options.n = 1
    options.quiet = false
    options.verbose = false

    opt_parser = OptionParser.new do |opts|
      opts.banner = "Usage: publish.rb [options]"
      opts.separator ""
      opts.separator "Specific options:"


      # mandatory
      opts.on("-n NUMBER", Integer, "Number of messages to publish") do |n|
        options.n = n
      end

      opts.separator ""
      opts.separator "Common options:"

      opts.on_tail("-h", "--help", "Show this message") do
        puts opts
        exit
      end

      opts.on("-v", "--[no-]verbose", "Run verbosely") do |v|
        options.verbose = v
      end

      opts.on("-q", "--quiet", "Completely suppress output") do |q|
        options.quiet = q
        options.verbose = false
      end
    end

    opt_parser.parse!(args)
    options
  end

end


options = OptionParser.parse(ARGV)
publish(options)
