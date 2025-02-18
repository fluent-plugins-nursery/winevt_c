# winevt_c

[![Test](https://github.com/fluent-plugins-nursery/winevt_c/actions/workflows/test.yml/badge.svg)](https://github.com/fluent-plugins-nursery/winevt_c/actions/workflows/test.yml)

## Prerequisites

* Windows Vista/Windows Server 2008 or later.
* gcc and g++ from MSYS2 for building C/C++ extension.
* Ruby 2.4 or later with MSYS2.

## Installation

Add this line to your application's Gemfile:

```ruby
gem 'winevt_c'
```

And then execute:

    $ ridk exec bundle

Or install it yourself as:

    $ ridk exec gem install winevt_c

## Fat gems building

* Docker is needed to build fat gem due to rake-compiler-dock uses docker container.

## Usage

Usage examples are found in [example directory](example).

### Multilingual description

Currently, the following locales should be supported to output description:

locale    | language
---------:|:--------
bg\_BG    | Bulgarian
zh\_CN    | Simplified Chinese
zh\_TW    | Traditional Chinese
zh\_HK    | Chinese (Hong Kong)
zh\_SG    | Chinese (Singapore)
hr\_HR    | Croatian
cz\_CZ    | Czech
da\_DK    | Danish
nl\_NL    | Dutch
nl\_BG    | Dutch (Belgium)
en\_US    | English (United States)
en\_GB    | English (UK)
en\_AU    | English (Australia)
en\_CA    | English (Canada)
en\_NZ    | English (New Zealand)
en\_IE    | English (Ireland)
fi\_FI    | Finnish
fr\_FR    | French
fr\_BE    | French (Belgium)
fr\_CA    | French (Canada)
fr\_CH    | French (Swiss)
de\_DE    | German
de\_CH    | German (Swiss)
de\_AT    | German (Austria)
el\_GR    | Greek (Ελληνικά)
hu\_HU    | Hungarian
is\_IS    | Icelandic
it\_IT    | Italian (Italy)
it\_CH    | Italian (Swiss)
ja\_JP    | Japanese
ko\_KO    | Korean
no\_NO    | Norwegian (Bokmål)
nb\_NO    | Norwegian (Bokmål)
nn\_NO    | Norwegian (Nynorsk)
pl\_PL    | Polish (Poland)
pt\_PT    | Portuguese
pt\_BR    | Portuguese (Brazil)
ro\_RO    | Romanian
ru\_RU    | Russian (русский язык)
sk\_SK    | Slovak
sl\_SI    | Slovenian
es\_ES    | Spanish
es\_ES\_T | Spanish (Traditional)
es\_MX    | Spanish (Mexico)
es\_ES\_M | Spanish (Modern)
sv\_SE    | Swedish
tr\_TR    | Turkish

## Development

After checking out the repo, run `bin/setup` to install dependencies. You can also run `bin/console` for an interactive prompt that will allow you to experiment.

To install this gem onto your local machine, run `bundle exec rake install`. To release a new version, update the version number in `version.rb`, and then run `bundle exec rake release`, which will create a git tag for the version, push git commits and tags, and push the `.gem` file to [rubygems.org](https://rubygems.org).

## Contributing

Bug reports and pull requests are welcome on GitHub at https://github.com/fluent-plugins-nursery/winevt_c.
