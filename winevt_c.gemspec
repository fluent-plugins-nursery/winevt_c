
lib = File.expand_path("../lib", __FILE__)
$LOAD_PATH.unshift(lib) unless $LOAD_PATH.include?(lib)
require "winevt/version"

Gem::Specification.new do |spec|
  spec.name          = "winevt_c"
  spec.version       = Winevt::VERSION
  spec.authors       = ["Hiroshi Hatake"]
  spec.email         = ["cosmo0920.wp@gmail.com"]

  spec.summary       = %q{Windows Event Log API bindings from winevt.h.}
  spec.description   = spec.summary
  spec.homepage      = "https://github.com/cosmo0920/winevt_c"

  # Specify which files should be added to the gem when it is released.
  # The `git ls-files -z` loads the files in the RubyGem that have been added into git.
  spec.files         = Dir.chdir(File.expand_path('..', __FILE__)) do
    `git ls-files -z`.split("\x0").reject { |f| f.match(%r{^(test|spec|features)/}) }
  end
  spec.bindir        = "exe"
  spec.executables   = spec.files.grep(%r{^exe/}) { |f| File.basename(f) }
  spec.require_paths = ["lib"]
  spec.extensions = ["ext/winevt/extconf.rb"]
  spec.license = "Apache-2.0"
  spec.required_ruby_version = Gem::Requirement.new(">= 2.4".freeze)

  spec.add_development_dependency "bundler", [">= 1.16", "< 3"]
  spec.add_development_dependency "rake", "~> 11.0"
  spec.add_development_dependency "rake-compiler"
  spec.add_development_dependency "test-unit", "~> 3.2"
end
