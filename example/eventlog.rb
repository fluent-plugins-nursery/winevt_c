require 'winevt'

@query = Winevt::EventLog::Query.new("Application", "*[System[(Level <= 3) and TimeCreated[timediff(@SystemTime) <= 86400000]]]")

@query.render_as_xml = true
@query.preserve_qualifiers = true
@query.each do |eventlog, message, string_inserts|
  puts ({eventlog: eventlog, data: message})
end
