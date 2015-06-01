
class Samwise::Client

  ##
  # Create a new samwise client instance. Connects to a running
  # samwise daemon. The endpoint must be a valid ZeroMQ string such as
  # "ipc://sam".

  def initialize endpoint
    @sam = Samwise.samwise_new endpoint
    raise "Could not connect" if @sam.nil?
  end



  ##
  # Destroy the samwise client instance. Maybe there is a way to set
  # the free funtion for the ruby garbage collection but I was not
  # able to find this.

  def close!
    samptr = FFI::MemoryPointer.new :pointer
    samptr.write_pointer @sam
    Samwise.samwise_destroy samptr
  end


  ##
  # Send a ping to samwise. Returns true if the daemon answered, false
  # otherwise.

  def ping
    rc = Samwise.samwise_ping @sam
    rc == 0? true: false
  end


  ##
  # Hands a message object over to libsamwise for publishing

  def publish msg
    rc = Samwise.samwise_publish @sam, msg.pub_t
    rc == 0? true: false
  end


end
