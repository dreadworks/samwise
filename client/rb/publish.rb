#!/usr/bin/env ruby

$LOAD_PATH.unshift(File.dirname(__FILE__) + '/lib')

require 'samwise'

sam = Samwise::Client.new 'ipc://../../sam_ipc'

if sam.ping
  puts 'Samwise is reachable'
else
  puts 'Samwise is not reachable'
end


sam.close!

