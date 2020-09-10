require 'winevt'

@session = Winevt::EventLog::Session.new("127.0.0.1") # Or remote box ip
# @session.domain = "<EXAMPLEGROUP>"
# @session.username = "<username>"
# @session.password = "<password>"
@query = Winevt::EventLog::Query.new("Application", "*[System[(Level <= 4) and TimeCreated[timediff(@SystemTime) <= 86400000]]]", @session)

@query.render_as_xml = true
@query.preserve_qualifiers = true
@query.each do |eventlog, message, string_inserts|
  puts ({eventlog: eventlog, data: message})
end
