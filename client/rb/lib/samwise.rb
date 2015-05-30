require 'ffi'

require 'samwise/version'
require 'samwise/client'


##
#  Mapping-class to access libsamwise. All native functions are
#  prefixed by samwise_* and are invoked by the wrapper functions
#  provided by the Client class.
#

module Samwise
  extend FFI::Library
  ffi_lib 'samwise'

  attach_function :samwise_new, [:string], :pointer
  attach_function :samwise_destroy, [:pointer], :void

  attach_function :samwise_ping, [:pointer], :int


end
