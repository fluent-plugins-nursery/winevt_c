module Winevt
  class EventLog
    class Session
      alias_method :initialize_raw, :initialize

      def initialize(server, domain = nil, username = nil, password = nil)
        initialize_raw
        self.server = server
        self.domain = domain if domain.is_a?(String)
        self.username = username if username.is_a?(String)
        self.password = password if password.is_a?(String)
      end
    end
  end
end
