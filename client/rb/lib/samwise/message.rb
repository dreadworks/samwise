
##
# Message to send via libsamwise. Used by Samwise::Client#publish

class Samwise::Message

  # distribution strategies
  DIST_T = [:redundant, :roundrobin]

  # offers access to the underlying FFI::Struct
  attr_reader :pub_t


  ##
  # RabbitMQ options

  class Pub_options_t < FFI::Struct
    layout :content_type,     :pointer,
           :content_encoding, :pointer,
           :delivery_mode,    :pointer,
           :priority,         :pointer,
           :correlation_id,   :pointer,
           :reply_to,         :pointer,
           :expiration,       :pointer,
           :message_id,       :pointer,
           :type,             :pointer,
           :user_id,          :pointer,
           :app_id,           :pointer,
           :cluster_id,       :pointer
  end


  ##
  # AMQP headers

  class Pub_headers_t < FFI::Struct
    layout :keys,   :pointer,
           :values, :pointer,
           :amount, :size_t
  end


  ##
  # Layout of the samwise_pub_t struct

  class Pub_t < FFI::Struct
    layout :disttype,    :int,
           :distcount,   :int,

           :exchange,    :pointer,
           :routing_key, :pointer,
           :mandatory,   :int,
           :immediate,   :int,

           :options,     Pub_options_t,
           :headers,     Pub_headers_t,

           :msg,         :pointer,
           :size,        :size_t
  end


  #
  #  generate getter/setter for first-level attributes
  #

  attr_reader :exchange, :routing_key, :mandatory, :immediate


  ["exchange=", "routing_key="].each do |meth|
    define_method meth do |val|
      key = meth[0..-2]
      set_ptr @pub_t, key.to_sym, val
      instance_variable_set "@#{key}", val
    end
  end


  ["mandatory=", "immediate="].each do |meth|
    define_method meth do |val|
      key = meth[0..-2]
      set_int @pub_t, key.to_sym, val
      instance_variable_set "@#{key}", val
    end
  end


  #
  #  headers and options
  #
  attr_reader :options, :headers


  ##
  # set AMQP options

  def options= options
    options.each do |key, val|
      set_ptr @pub_t[:options], key, val
    end

    @options = options
  end


  ##
  # set AMQP headers

  def headers= headers
    @pub_t[:headers][:amount] = size = headers.size

    keys = FFI::MemoryPointer.new :pointer, size
    vals = FFI::MemoryPointer.new :pointer, size

    # instead of using #keys and #values b/c I don't know ruby well
    # enough to be sure that the order is maintained
    pointer = { keys: [], vals: [] }
    headers.each do |key, val|
      pointer[:keys] << key
      pointer[:vals] << val
    end

    pointer[:keys] = pointer[:keys].collect do |key|
      FFI::MemoryPointer.from_string key.to_s
    end

    pointer[:vals] = pointer[:vals].collect do |val|
      FFI::MemoryPointer.from_string val
    end

    keys.write_array_of_pointer pointer[:keys]
    vals.write_array_of_pointer pointer[:vals]

    @pub_t[:headers][:keys] = keys
    @pub_t[:headers][:values] = vals

    @headers = headers
  end


  ##
  # Constructor accepts a string containing the message and the
  # distribution strategy

  def initialize msg, disttype=:roundrobin, distcount=nil

    raise "invalid disttype" unless DIST_T.include? disttype

    @pub_t = Pub_t.new

    # set payload
    set_ptr @pub_t, :msg, msg
    @pub_t[:size] = msg.size

    # set distribution strategy
    @pub_t[:disttype] = (disttype == :roundrobin)? 0: 1;
    @pub_t[:distcount] = distcount.nil?? 0: distcount;
  end


  private


  ##
  # Sets a pointer to a string as a structs property

  def set_ptr struct, key, val
    struct[key]= FFI::MemoryPointer.from_string val
  end


  ##
  # Sets an integer as a structs property

  def set_int struct, key, val
    struct[key] = val
  end

end
