
lib = File.expand_path("../lib", __FILE__)
$LOAD_PATH.unshift(lib) unless $LOAD_PATH.include?(lib)
require "png/version"

Gem::Specification.new do |spec|
  spec.name          = "libpng-ruby"
  spec.version       = PNG::VERSION
  spec.authors       = ["Hiroshi Kuwagata"]
  spec.email         = ["kgt9221@gmail.com"]

  spec.summary       = %q{libpng interface for ruby}
  spec.description   = %q{libpng interface for ruby}
  spec.homepage      = "https://github.com/kwgt/libpng-ruby"
  spec.license       = "MIT"

  if spec.respond_to?(:metadata)
    spec.metadata["homepage_uri"] = spec.homepage
  else
    raise "RubyGems 2.0 or newer is required to protect against " \
      "public gem pushes."
  end

  spec.files         = Dir.chdir(File.expand_path('..', __FILE__)) do
    `git ls-files -z`.split("\x0").reject { |f|
      f.match(%r{^(test|spec|features)/})
    }
  end

  spec.bindir        = "exe"
  spec.executables   = spec.files.grep(%r{^bin/}) { |f| File.basename(f) }
  spec.extensions    = ["ext/png/extconf.rb"]
  spec.require_paths = ["lib"]

  spec.required_ruby_version = ">= 2.4.0"

  spec.add_development_dependency "bundler", ">= 2.3"
  spec.add_development_dependency "rake", ">= 13.0"
  spec.add_development_dependency "rake-compiler", "~> 1.1.0"
end
