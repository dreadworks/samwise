#!/usr/bin/env ruby

$LOAD_PATH.unshift(File.dirname(__FILE__) + '/lib')

require 'ostruct'
require 'optparse'
require 'samwise'


endpoint = "ipc://../../sam_ipc"
Samwise::Connection.connect endpoint


def publish (opts)
  puts "publishing #{opts.n} messages (#{opts.t})" unless opts.quiet

  time_start = Time.now
  opts.n.times do |i|
    puts "publishing message #{i}" if opts.verbose

    amqp_args = { :exchange => "amq.direct" }
    amqp_opts = { :headers => { :header_key => "header_val" }}
    msg = "#{opts.t} publishing request"

    if opts.t == "redundant"
      Samwise::RabbitMQ.publish_redundant opts.d, amqp_args, amqp_opts, msg
    end

    if opts.t == "round robin"
      Samwise::RabbitMQ.publish_roundrobin amqp_args, amqp_opts, msg
    end

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

      opts.on("-t TYPE", String, "Distribution type [redundant, round robin]") do |t|
        options.t = t
      end

      # optional
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

      opts.on("-d count", Integer, "For -t redundant") do |d|
        if not options.t == "redundant"
          raise "You must provide '-t redundant' to use -d"
        end
        options.d = d
      end
    end

    rc = opt_parser.parse!(args)
    raise OptionParser::MissingArgument, "-t" if options.t.nil?
    if options.t == "redundant"
      raise OptionParser::MissingArgument, "-d" if options.d.nil?
    end

    options
  end

end


options = OptionParser.parse(ARGV)
publish(options)

Samwise::Connection.close!
