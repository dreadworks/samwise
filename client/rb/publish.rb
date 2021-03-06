#!/usr/bin/env ruby

$LOAD_PATH.unshift File.dirname(__FILE__) + '/lib'


require 'ostruct'
require 'optparse'
require "samwise"


def publish sam, opts
  puts "publishing #{opts.n} messages (#{opts.t})" unless opts.quiet
  time_start = Time.now

  opts.n.times do |i|

    # create a new message to send
    if opts.t == "redundant"
      msg = Samwise::Message.new "hi!", :redundant, opts.d
    else
      msg = Samwise::Message.new "hi!"
    end

    msg.exchange = "amq.direct"
    # msg.options = { user_id: "1", app_id: "2" }
    # msg.headers = { prop_1: "prop1", prop_2: "prop2" }

    raise "could not publish" unless sam.publish msg

  end

  unless opts.quiet
    time_end = Time.now
    puts "done in #{time_end - time_start}, exiting"
  end
end


#
#  parse command line options
#
class OptionParser

  def self.parse args
    options = OpenStruct.new
    options.n = 1
    options.quiet = false
    options.verbose = false

    opt_parser = OptionParser.new do |opts|
      opts.banner = "Usage: publish.rb [options]"
      opts.separator ""
      opts.separator "Specific options:"


      # mandatory
      opts.on "-n NUMBER", Integer, "Number of messages to publish"  do |n|
        options.n = n
      end

      opts.on "-t TYPE", String, "Distribution type [redundant, roundrobin]" do |t|
        options.t = t
      end

      # optional
      opts.separator ""
      opts.separator "Common options:"

      opts.on_tail "-h", "--help", "Show this message" do
        puts opts
        exit
      end

      opts.on "-v", "--[no-]verbose", "Run verbosely" do |v|
        options.verbose = v
      end

      opts.on "-q", "--quiet", "Completely suppress output" do |q|
        options.quiet = q
        options.verbose = false
      end

      opts.on "-d count", Integer, "For -t redundant" do |d|
        if not options.t == "redundant"
          raise "You must provide '-t redundant' to use -d"
        end
        options.d = d
      end
    end

    rc = opt_parser.parse! args
    raise OptionParser::MissingArgument, "-t" if options.t.nil?
    if options.t == "redundant"
      raise OptionParser::MissingArgument, "-d" if options.d.nil?
    end

    options
  end

end


# create a new client that connects to a running samwise instance
options = OptionParser.parse ARGV
sam = Samwise::Client.new "ipc://../../sam_ipc"

# test if the connection was established correctly
raise "Samwise not reachable" unless sam.ping
publish sam, options

# close the connection
sam.close!
