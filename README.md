# winevt_c

[![Build status](https://ci.appveyor.com/api/projects/status/o5771b3cb6x3acq0/branch/master?svg=true)](https://ci.appveyor.com/project/cosmo0920/winevt-c-6145k/branch/master)

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
## Development

After checking out the repo, run `bin/setup` to install dependencies. You can also run `bin/console` for an interactive prompt that will allow you to experiment.

To install this gem onto your local machine, run `bundle exec rake install`. To release a new version, update the version number in `version.rb`, and then run `bundle exec rake release`, which will create a git tag for the version, push git commits and tags, and push the `.gem` file to [rubygems.org](https://rubygems.org).

## Contributing

Bug reports and pull requests are welcome on GitHub at https://github.com/fluent-plugins-nursery/winevt_c.
