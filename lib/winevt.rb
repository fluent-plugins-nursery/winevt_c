begin
  require "winevt/#{RUBY_VERSION[/\d+.\d+/]}/winevt"
rescue LoadError
  require "winevt/winevt"
end
require "winevt/bookmark"
require "winevt/query"
require "winevt/subscribe"
require "winevt/version"

module Winevt
  # Your code goes here...
end
