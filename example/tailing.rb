require 'winevt'

@subscribe = Winevt::EventLog::Subscribe.new
@subscribe.tail = true
@subscribe.subscribe(
  "Security", "*[System[(Level <= 4) and TimeCreated[timediff(@SystemTime) <= 86400000]]]"
)
while true do
  @subscribe.each do |eventlog, message, string_inserts|
    puts ({eventlog: eventlog, data: message})
  end
  sleep(1)
end
