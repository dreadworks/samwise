#!/usr/bin/env ruby

require 'ostruct'
require 'optparse'
require 'zmq'


class Publisher

  def initialize (endpoint)
    @ctx = ZMQ::Context.new
    @req = @ctx.bind(:REQ, endpoint)
  end

  def destroy!
    @ctx.destroy
  end

  def publish (distribution, exchange, routing_key, message)
    msg = ZMQ::Message("rabbitmq", distribution, exchange, routing_key, message)
    @req.send msg

    # apply back pressure and comply with
    # the strict REQ/REP cycle
    @req.recv
  end

end


def publish (opts)
  puts("publishing {opts.n} messages" ) unless opts.verbose or opts.quiet
  pub = Publisher.new

  n.times do |i|
    puts "publishing message {i}" unless opts.verbose
    pub.publish("redundant", "amq.direct", "", "hi!")
  end

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
    end

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
    end

    opt_parser.parse!(args)
    options
  end

end


options = OptionParser.parse(ARGV)
pp options
# publish(options)
