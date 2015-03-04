#!/usr/bin/env ruby

$LOAD_PATH.unshift(File.dirname(__FILE__) + '/lib')

require 'ostruct'
require 'optparse'
require 'samwise'


endpoint = "ipc://../../sam_ipc"
Samwise::Connection.connect endpoint


def publish (opts)
    puts "publishing #{opts.n} messages" unless opts.quiet

  time_start = Time.now
  opts.n.times do |i|
    puts "publishing message #{i}" if opts.verbose
    Samwise::RabbitMQ.publish_roundrobin "amq.direct", "", "hi!"
  end
  time_end = Time.now

  puts "done in #{time_end - time_start}, exiting" unless opts.quiet
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

Samwise::Connection.close!
