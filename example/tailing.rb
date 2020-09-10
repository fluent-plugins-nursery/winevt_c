require 'winevt'

@session = Winevt::EventLog::Session.new("127.0.0.1") # Or remote box ip
# @session.domain = "<EXAMPLEGROUP>"
# @session.username = "<username>"
# @session.password = "<password>"
@bookmark = Winevt::EventLog::Bookmark.new
@subscribe = Winevt::EventLog::Subscribe.new
@subscribe.read_existing_events = true
@subscribe.preserve_qualifiers = true
@subscribe.render_as_xml = true
@subscribe.subscribe(
  "Security", "*[System[(Level <= 4) and TimeCreated[timediff(@SystemTime) <= 86400000]]]",
  @bookmark, @session
)
while true do
  @subscribe.each do |eventlog, message, string_inserts|
    puts ({eventlog: eventlog, data: message})
  end
  sleep(1)
end
