require 'forwardable'

class Samwise::Message
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
      @frames.push ZMQ::Frame(m)
    end
  end


  # Send the message to samwise.
  #
  def send
    if @frames.empty?
      raise Samwise::RequestMalformed, "At least one frame required"
    end

    add Samwise::VERSION.inspect
    zmsg = ZMQ::Message.new

    @frames.each do |f|
      zmsg.push f
    end

    Samwise::Connection.send zmsg
  end

end
