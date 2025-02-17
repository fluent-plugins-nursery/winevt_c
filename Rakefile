require "rake/testtask"
require 'rake/clean'

Rake::TestTask.new(:test) do |t|
  t.libs << "test"
  t.libs << "lib"
  t.test_files = FileList["test/**/test_*.rb"]
end

require "rake/extensiontask"

Rake::ExtensionTask.new("winevt") do |ext|
  ext.ext_dir = 'ext/winevt'
  ext.lib_dir = 'lib/winevt'
end

CLEAN.include('lib/winevt/winevt.*')

task :default => [:clobber, :compile, :test]
