require 'winevt'

@channels = Winevt::EventLog::Channel.new
@channels.force_enumerate = false
result = []
@channels.each do |channel|
  result << channel
end

puts "length of channels: #{result.length}"
result.each do |r|
  puts r
end
