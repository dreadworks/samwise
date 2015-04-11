require 'forwardable'


module Samwise
  class Message
    extend Forwardable

    def initialize
      @frames = []
    end

    def_delegators :@frames, :size, :empty?


    # Add a variable number of frames to the message.
    #
    # @param frames Variadic number of frames
    #
    def add *frames
      frames.each do |m|
        @frames.insert 0, m
      end
    end


    # Send the message to samwise.
    #
    def send
      if @frames.empty?
        raise Samwise::RequestMalformed, "At least one frame required"
      end

      @frames << Samwise::VERSION.inspect
      zmsg = ZMQ::Message.new

      # puts "message: #{@frames.reverse.to_s}"
      @frames.map { |m| ZMQ::Frame(m.to_s) }.each do |f|
        zmsg.push f
      end

      Samwise::Connection.send zmsg
    end

  end

end
