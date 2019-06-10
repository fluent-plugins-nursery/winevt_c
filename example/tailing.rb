require 'winevt'

@subscribe = Winevt::EventLog::Subscribe.new
@subscribe.tail = true
@subscribe.subscribe("Application", "*[System[(Level <= 4) and TimeCreated[timediff(@SystemTime) <= 86400000]]]")
while (1) do
  if @subscribe.next
    puts @subscribe.render
  else
    printf(".")
    sleep(1)
  end
end
puts @subscribe.bookmark
