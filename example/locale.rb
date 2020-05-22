require 'winevt'

@locale = Winevt::EventLog::Locale.new

header = <<EOC
locale    | language
---------:|:--------
EOC

print header
@locale.each do |code, desc|
  print "#{code.gsub("_", "\\_")}#{" "*(8 - code.size)}| #{desc}\n"
end
