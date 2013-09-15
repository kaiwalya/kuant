#include <assert.h>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <map>

#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <boost/property_tree/json_parser.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>


#include <korelate/korelate.hpp>

namespace korelate {
	using Clock = std::chrono::system_clock;
	using TimePoint = std::chrono::time_point<Clock>;
		
	struct quotes {
		std::vector<TimePoint> & timepoints;
		float * const values;
		size_t const nSymbols;
		size_t const nDays;
		size_t iDay;
		size_t nValidDays;
		size_t iSymbol;
		quotes(std::vector<TimePoint> & timepoints, float * values, size_t nDays, size_t nSymbols)
		:timepoints(timepoints),
		values(values),
		nSymbols(nSymbols),
		nDays(nDays),
		iDay(-1), nValidDays(timepoints.size()), iSymbol(-1){
			
		}
		
		const TimePoint & timepoint() const {
			return timepoints.at(iDay);
		}
		
		TimePoint & timepoint() {
			while (timepoints.size() <= iDay) {
				nValidDays++;
				timepoints.push_back(Clock::now());
			}
			return timepoints.at(iDay);
		}
		
		size_t tickerIndex() const {
			return iSymbol;
		}
		
		const float (&current() const)[5]{
			float * location = values + ((iDay * nSymbols + iSymbol)* 5);
			return *(float (*) [5])(location);
		}
		
		float (&current())[5]{
			float * location = values + ((iDay * nSymbols + iSymbol)* 5);
			return *(float (*) [5])(location);
		}
		
		const float & open() const { return current()[0]; }
		float & open() { return current()[0]; }
		
		const float & close() const { return current()[1]; }
		float & close() { return current()[1]; }
		
		const float & high() const { return current()[2]; }
		float & high() { return current()[2]; }
		
		const float & low() const { return current()[3]; }
		float & low() { return current()[3]; }
		
		const float & volume() const { return current()[4]; }
		float & volume() { return current()[4]; }
		
		
		bool nextDay(){
			iDay++;
			return iDay < nDays;
		}
		
		bool isValidDay() {
			return iDay < nValidDays;
		}
		
		bool nextSymbol(){
			iSymbol++;
			return iSymbol < nSymbols;
		}
		
		bool prevDay() {
			if (iDay) {
				iDay--;
				return true;
			}
			return false;
		}
		
		bool prevSymbol() {
			if (iSymbol) {
				iSymbol--;
				return true;
			}
			return false;
		}
		
		void resetSymbol(){
			iSymbol = -1;
		}
		
		void resetDay(){
			iDay = -1;
		}
	};
		
	class quandl_ticker_source {
		
	private:
		std::string m_apiKey;
		CacheConnection m_cacheConn;
		TimePoint tStart, tEnd;
		int nDays;
		std::vector<equity> symbols;
		
		std::vector<TimePoint> fetchedDays;
		float * fetchedValues;
		
	public:
		quandl_ticker_source(std::string && apiKey, CacheConnection conn = nullptr)
		:m_apiKey(apiKey),
		m_cacheConn(conn),
		tStart(Clock::now()),
		tEnd(tStart),
		nDays(0),
		fetchedValues(nullptr) {
		}
		~quandl_ticker_source() { delete [] fetchedValues; }
		
		size_t equityCount() const{
			return symbols.size();
		}
		
		size_t dayCount() const{
			return nDays;
		}
		
		quotes values() {
			return quotes(fetchedDays, fetchedValues, dayCount(), equityCount());
		}
		
		void setWindow(TimePoint start, int nDays) {
			tStart = start;
			tEnd = start + std::chrono::hours(24*nDays);
			this->nDays = nDays;
		}
		void setSymbols(std::vector<equity> & symbols) {
			this->symbols = symbols;
		};
		
		bool fetch();
		
			
	};
	
	
	class ctx_impl {
		CacheConnection conn;
	public:
		ctx_impl(CacheConnection & conn);
		void analyize(std::vector<equity> & hedges);
		void dumpTree(boost::property_tree::ptree & tree);
	};
	
}


////////////////
////////////////

static std::shared_ptr<sqlite3_stmt> sqlPrepare(sqlite3 * sql, const char * q, size_t sz) {
	sqlite3_stmt * stmt;
	auto error = sqlite3_prepare_v2(sql, q, sz, &stmt, nullptr);
	if (error != SQLITE_OK)
		return nullptr;
	return {stmt, [](sqlite3_stmt * stmt){
		sqlite3_finalize(stmt);
	}};
};

static bool installSchema(sqlite3 * sql, sqlite3_stmt * stmt) {
	return false;
}


using namespace korelate;
ctx_impl::ctx_impl(CacheConnection & conn):conn(conn) {
	{
		auto sql = conn;
		{
			char qCreateSchemaDay[] = "CREATE TABLE day (date integer, exchange text, symbol text, open real, close real, low real, high real, primary key (date, exchange, symbol));";
			
			std::string schemaDayExistsError = "table day already exists";
			
			auto createCommand = sqlPrepare(sql.get(), qCreateSchemaDay, sizeof(qCreateSchemaDay)/sizeof(qCreateSchemaDay[0]));
			std::string error(sqlite3_errmsg(sql.get()));
			if (!createCommand) {
				if (error != schemaDayExistsError) {
					std::cout <<"Error " << sqlite3_errcode(sql.get()) << ":" << sqlite3_errstr(sqlite3_errcode(sql.get())) << ":" << sqlite3_errmsg(sql.get()) << std::endl;
				}
			}
			else if (sqlite3_step(createCommand.get()) != SQLITE_DONE) {
				std::cout <<"Error " << sqlite3_errcode(sql.get()) << ":" << sqlite3_errstr(sqlite3_errcode(sql.get())) << ":" << sqlite3_errmsg(sql.get()) << std::endl;
			}
		}
		
		{
			char qCreateSchemaHttpCache [] = "CREATE TABLE httpcache (path text, json text, primary key(path));";
			std::string schemaCacheExistsError = "table httpcache already exists";
			
			auto createCommand = sqlPrepare(sql.get(), qCreateSchemaHttpCache, sizeof(qCreateSchemaHttpCache)/sizeof(qCreateSchemaHttpCache[0]));
			std::string error(sqlite3_errmsg(sql.get()));
			if (!createCommand) {
				if (error != schemaCacheExistsError) {
					std::cout <<"Error " << sqlite3_errcode(sql.get()) << ":" << sqlite3_errstr(sqlite3_errcode(sql.get())) << ":" << sqlite3_errmsg(sql.get()) << std::endl;
				}
			}
			else if (sqlite3_step(createCommand.get()) != SQLITE_DONE) {
				std::cout <<"Error " << sqlite3_errcode(sql.get()) << ":" << sqlite3_errstr(sqlite3_errcode(sql.get())) << ":" << sqlite3_errmsg(sql.get()) << std::endl;
			}
		}
		
	}
}

void ctx_impl::analyize(std::vector<equity> &hedges) {
	
	const int nMaxDays = 14;
	
	quandl_ticker_source quandl("mzTwizqpbUxtKqUEZKqs&columns", conn);
	quandl.setWindow(Clock::now() - std::chrono::hours(24*nMaxDays), nMaxDays);
	quandl.setSymbols(hedges);
	if (!quandl.fetch()) {
		return;
	}
	
	auto vals = quandl.values();
	while(vals.nextDay() && vals.isValidDay()) {
		{
			auto timet = Clock::to_time_t(vals.timepoint());
			auto tm = *std::gmtime(&timet);
			auto d = boost::gregorian::date_from_tm(tm);
			std::cout << boost::gregorian::to_sql_string(d) << std::endl;
		}
		while(vals.nextSymbol()) {
			std::cout
			<< hedges[vals.tickerIndex()].symbol
			<< " " << vals.open()
			<< " " << vals.high()
			<< " " << vals.low()
			<< " " << vals.close()
			//<< " " << vals.volume()
			<< std::endl;
		}
		vals.resetSymbol();
	}
}

void ctx_impl::dumpTree(boost::property_tree::ptree &tree) {
	std::cout << "{";
	for (auto & treeNode: tree) {
		std::cout << treeNode.first << ":" << treeNode.second.data();
		//dumpTree(treeNode.second);
		std::cout << ",";
	}
	std::cout << "}";
}

bool quandl_ticker_source::fetch() {
	std::stringstream pathStream;
	pathStream << "/api/v1/multisets.json?auth_token=" << m_apiKey << "&columns=";
	for (auto it = symbols.begin(); it != symbols.end(); it++) {
		if (it != symbols.begin()) {
			pathStream << ",";
		}
		pathStream << "GOOG." << it->exchange << "_" << it->symbol;
	}
	//trim_start=2012-11-01&trim_end=2012-11-30
	{
		auto today = tEnd;
		auto lastmonth = tStart;
		auto tNow = Clock::to_time_t(today);
		auto tmNow = *std::localtime(&tNow);
		auto tLastMonth = Clock::to_time_t(lastmonth);
		auto tmLastMonth = *std::localtime(&tLastMonth);
		auto toString = [] (tm & tmval) -> std::string {
			std::stringstream ss;
			ss << (tmval.tm_year + 1900)
			<< "-"
			<< std::setw(2) << std::setfill('0') << tmval.tm_mon + 1
			<< "-"
			<< std::setw(2) << std::setfill('0') << tmval.tm_mday;
			return ss.str();
		};
		
		pathStream << "&trim_start=" << toString(tmLastMonth) << "&trim_end=" << toString(tmNow);
	}
	
	boost::property_tree::ptree tree;
	bool bCacheHit = false;
	{
		auto sql = m_cacheConn;
		static const char lookupQuery[] = "select json from httpcache where path=?1;";
		auto lookupStmt = sqlPrepare(m_cacheConn.get(), lookupQuery, sizeof(lookupQuery)/sizeof(lookupQuery[0]));
		if (!lookupStmt) {
			std::cout <<"Error " << sqlite3_errcode(sql.get()) << ":" << sqlite3_errstr(sqlite3_errcode(sql.get())) << ":" << sqlite3_errmsg(sql.get()) << std::endl;
			return false;
		}
		if (sqlite3_bind_text(lookupStmt.get(), 1, pathStream.str().c_str(), -1, nullptr) != SQLITE_OK){
			std::cout <<"Error " << sqlite3_errcode(sql.get()) << ":" << sqlite3_errstr(sqlite3_errcode(sql.get())) << ":" << sqlite3_errmsg(sql.get()) << std::endl;
			return false;
		}
		if (sqlite3_step(lookupStmt.get()) != SQLITE_ROW){
			std::cout <<"Error " << sqlite3_errcode(sql.get()) << ":" << sqlite3_errstr(sqlite3_errcode(sql.get())) << ":" << sqlite3_errmsg(sql.get()) << std::endl;
			return false;
		}
		auto chars = sqlite3_column_text(lookupStmt.get(), 0);
		if (!chars) {
			std::cout <<"Error " << sqlite3_errcode(sql.get()) << ":" << sqlite3_errstr(sqlite3_errcode(sql.get())) << ":" << sqlite3_errmsg(sql.get()) << std::endl;
			return false;
		}
		std::stringstream stream;
		stream << chars;
		boost::property_tree::read_json(stream, tree);
		bCacheHit = true;
	}
	
	if (!bCacheHit){
		Poco::Net::HTTPClientSession session("www.quandl.com");
		Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, pathStream.str(), Poco::Net::HTTPMessage::HTTP_1_1);
		session.sendRequest(request);
		Poco::Net::HTTPResponse response;
		auto & data = session.receiveResponse(response);
		auto status = response.getStatus();
		assert(status == Poco::Net::HTTPResponse::HTTPStatus::HTTP_OK || status == 422);
		
		boost::property_tree::read_json(data, tree);
		std::stringstream buff;
		//std::streambuf buff;
		boost::property_tree::write_json(buff, tree);
		std::string json(buff.str());
		
		{
			auto sql = m_cacheConn;
			static const char insertQuery [] = "insert into httpcache values (?1,?2);";
			auto insertStmt = sqlPrepare(m_cacheConn.get(), insertQuery, sizeof(insertQuery)/sizeof(insertQuery[0]));
			if (!insertStmt) {
				std::cout <<"Error " << sqlite3_errcode(sql.get()) << ":" << sqlite3_errstr(sqlite3_errcode(sql.get())) << ":" << sqlite3_errmsg(sql.get()) << std::endl;
				return false;
			}
			if (sqlite3_bind_text(insertStmt.get(), 1, pathStream.str().c_str(), -1, nullptr) != SQLITE_OK){
				std::cout <<"Error " << sqlite3_errcode(sql.get()) << ":" << sqlite3_errstr(sqlite3_errcode(sql.get())) << ":" << sqlite3_errmsg(sql.get()) << std::endl;
				return false;
			}
			if (sqlite3_bind_text(insertStmt.get(), 2, json.c_str(), -1, nullptr) != SQLITE_OK) {
				std::cout <<"Error " << sqlite3_errcode(sql.get()) << ":" << sqlite3_errstr(sqlite3_errcode(sql.get())) << ":" << sqlite3_errmsg(sql.get()) << std::endl;
				return false;
			}
			if (sqlite3_step(insertStmt.get()) != SQLITE_DONE){
				std::cout <<"Error " << sqlite3_errcode(sql.get()) << ":" << sqlite3_errstr(sqlite3_errcode(sql.get())) << ":" << sqlite3_errmsg(sql.get()) << std::endl;
				return false;
			}
		}
	}
	
	auto errors = tree.get_child("errors");
	if (!errors.empty() || errors.size()) {
		return false;
	}
	
	auto columns = tree.get_child("columns");
	if (columns.size() != symbols.size() * 5 + 1) {
		assert(false && "Some symbols were not found");
		return false;
	}
	
	auto datadata = tree.get_child("data");
	auto dayIt = datadata.begin();
	auto dayEnd = datadata.end();
	
	fetchedDays.clear();
	delete [] fetchedValues;
	fetchedValues = new float[dayCount() * equityCount() * 5];
	
	TimePoint lastTime;
	
	quotes q(fetchedDays, fetchedValues, dayCount(), equityCount());
	while(q.nextDay() && dayIt != dayEnd) {
		auto it = dayIt->second.begin();
		auto itEnd = dayIt->second.end();

		TimePoint currentTime;
		boost::gregorian::date d(boost::gregorian::from_simple_string(it->second.data()));
		auto tm = boost::gregorian::to_tm(d);
		q.timepoint() = lastTime = Clock::from_time_t(std::mktime(&tm));
		it++;

		int i = 0;
		q.resetSymbol();
		while(it != itEnd) {
			float fVal = it->second.get_value<float>();
			
			switch (i%5) {
				case 0:
					q.nextSymbol();
					q.open() = fVal;
					break;
				case 3:
					q.close() = fVal;
					break;
				case 1:
					q.high() = fVal;
					break;
				case 2:
					q.low() = fVal;
					break;
				case 4:
					q.volume() = fVal;
					break;
			}
			i++;
			it++;
		}
		dayIt++;
	}
	
	
	
	return true;
}

ctx::ctx(CacheConnection conn): m_impl(new ctx_impl(conn)) {

}

ctx::~ctx() {
	
}

void ctx::analyize(std::vector<equity> && hedges) {
	m_impl->analyize(hedges);
}

std::ostream& operator<<(std::ostream& os, const Exchange & ex)
{
	os << ex.toString();
	return os;
}



