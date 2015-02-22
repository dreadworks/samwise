
module Samwise::Connection
  extend self # singleton

  # Create a new connection to a samwise endpoint. This creates a new
  # ZeroMQ Context and establishes a connection to this endpoint. If
  # this method gets called multiple times it destroys the former
  # context and closes any existing connection and connects to the new
  # endpoint.
  #
  # @param endpoint [String] a valid ZeroMQ endpoint specifier
  # @return [Connection] freshly instantiated connection instance
  #
  def connect endpoint
    if connected?
      close!
    end

    @ctx ||= ZMQ::Context.new
    @req = @ctx.socket(:REQ)
    raise Samwise::ConnectionFailure, "create socket" unless @req

    begin
      rc = @req.connect(endpoint)
      raise "connection failed" unless rc
    rescue StandardError => e
      raise Samwise::ConnectionFailure, e.inspect
    end

    connected?
  end


  # Destroy a ZeroMQ context explicitly
  #
  def close!
    @ctx.destroy if @ctx
    @ctx = nil
  end


  def send msg
    raise Samwise::ConnectionFailure, "socket not connected" unless connected?

    @req.send_message msg
    raise Samwise::ConnectionFailure, "message was not sent" unless msg.gone?

    rcode = @req.recv_frame().data.to_i
    if rcode == -1
      raise Samwise::ResponseError, @req.recv_frame().data
    end

    raise Samwise::ResponseMalformed unless rcode == 0
    true
  end


  # Test if the context is still alive and the connection to the
  # endpoint still exists.
  #
  # @return [Boolean] true if connected
  #
  def connected?
    begin
      return true if @ctx and @req and @req.state & ZMQ::Socket::CONNECTED
    rescue ZMQ::Error => e
      raise Samwise::ConnectionFailure, e.inspect
    end

    false
  end

end
