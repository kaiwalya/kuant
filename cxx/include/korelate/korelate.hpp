#include <memory>
#include <string>
#include <vector>
#include <sqlite3.h>

namespace korelate {
	class ctx_impl;
	
	class Exchange {
	public:
		enum Type {
			NASDAQ, NYSE,
		};
	private:
		Type mType;
	public:
		Exchange(Type t): mType(t) {}
		
		std::string toString() const{
			switch(mType) {
				case NASDAQ:
					return "NASDAQ";
				case NYSE:
					return "NYSE";
			};
		}
	};
	
	using CacheConnection = std::shared_ptr<sqlite3>;
		
	struct equity {
	public:
		Exchange exchange;
		std::string symbol;
	public:
		equity(Exchange exchange, std::string && symbol): exchange(exchange), symbol(symbol){}
	};
	class ctx {
	private:
		std::unique_ptr<ctx_impl> m_impl;
	public:
		ctx(CacheConnection conn);
		~ctx();
		void analyize(std::vector<equity> && hedges);
	};
}


extern std::ostream& operator<<(std::ostream& os, const korelate::Exchange & ex);
