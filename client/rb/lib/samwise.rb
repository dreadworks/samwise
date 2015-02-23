require 'rbczmq'


module Samwise

  # protocol version
  VERSION = 1 * 100 + 0

  # abstract superclass
  class Error < StandardError; end

  # if no connection to samwise could be established
  class ConnectionFailure < StandardError; end

  # if a message request can not be sent because of errors
  class RequestMalformed < StandardError; end

  # if an answer can not be parsed
  class ResponseMalformed < StandardError; end

  # if samwise answered with an error code
  class ResponseError < StandardError; end

  # if either send or recv run into a timeout
  class Timeout < StandardError; end

end


require 'samwise/connection'
require 'samwise/message'
require 'samwise/rabbitmq'
