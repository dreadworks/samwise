Gem::Specification.new do |s|

  s.name = "samwise"

  # the first two parts of the version number
  # are equal to the supported sam protocol
  # specification, the last number gets iterated
  # for new versions of the gem
  s.version       = "0.1.0"
  s.date          = "2015-02-21"
  s.summary       = "Samwise client"
  s.description   = "A client implementation to communicate with samd"
  s.authors       = ["Felix Hamann"]
  s.email         = "felix.hamann@xing.com"
  s.files         = [
    "lib/samwise.rb",
    'lib/samwise/connection.rb',
    "lib/samwise/message.rb",
    "lib/samwise/rabbitmq.rb"
  ]
  s.homepage      = "source.xing.com/felix-hamann/samwise"
  s.license       = "MIT"

end
