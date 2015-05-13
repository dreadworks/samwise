ENV['RC_ARCHS'] = '' if RUBY_PLATFORM =~ /darwin/

require 'rbconfig'
require 'mkmf'

$CFLAGS += ' -std=gnu99'


LIBDIR      = RbConfig::CONFIG['libdir']
INCLUDEDIR  = RbConfig::CONFIG['includedir']


HEADER_DIRS = [
  '/opt/local/include',
  '/usr/local/include',
  INCLUDEDIR,
  '/usr/include',
]

LIB_DIRS = [
  '/opt/local/lib',
  '/usr/local/lib',
  LIBDIR,
  '/usr/lib',
]


dir_config('samwise', HEADER_DIRS, LIB_DIRS)


unless have_header('samwise.h')
  abort 'samwise.h is missing'
end

unless find_library('samwise', 'samwise_new')
  abort 'libsamwise is missing'
end


create_makefile ('samwise/samwise')
