# coding: utf-8
lib = File.expand_path('../lib', __FILE__)
$LOAD_PATH.unshift(lib) unless $LOAD_PATH.include?(lib)
require 'samwise/version'

Gem::Specification.new do |spec|
  spec.name          = "samwise"
  spec.version       = Samwise::VERSION
  spec.authors       = ["Felix Hamann"]
  spec.email         = ["nvri@dreadworks.de"]

  spec.summary       = %q{Samwise client library}
  spec.description   = %q{Client library speaking the samwise protocol to send messages to running samwise daemons}
  spec.homepage      = "https://github.com/dreadworks/samwise"
  spec.license       = "MIT"

  spec.files         = `git ls-files -z`.split("\x0").reject { |f| f.match(%r{^(test|spec|features)/}) }
  spec.bindir        = "exe"
  spec.executables   = spec.files.grep(%r{^exe/}) { |f| File.basename(f) }
  spec.require_paths = ["lib"]

  spec.add_development_dependency "bundler", "~> 1.9"
  spec.add_development_dependency "rake", "~> 10.0"

  spec.add_runtime_dependency "ffi"

end
