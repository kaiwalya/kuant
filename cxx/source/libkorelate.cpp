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
#include <sqlite3.h>

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
		
	public:
		using CacheConnection = std::shared_ptr<sqlite3>;
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
		fetchedValues(nullptr) {}
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
	public:
		void analyize(std::vector<equity> & hedges);
		void dumpTree(boost::property_tree::ptree & tree);
	};
	
}


////////////////
////////////////


using namespace korelate;

static std::shared_ptr<sqlite3> sqlOpen() {
	sqlite3 * lite;
	auto openError = sqlite3_open_v2("prices.db", &lite, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
	if (openError != SQLITE_OK)
		return nullptr;
	
	return {lite, [](sqlite3 * lite){
		sqlite3_close_v2(lite);
	}};
}

static std::shared_ptr<sqlite3_stmt> sqlPrepare(sqlite3 * sql, const char * q, size_t sz) {
	sqlite3_stmt * stmt;
	auto error = sqlite3_prepare_v2(sql, q, sz, &stmt, nullptr);
	if (error != SQLITE_OK)
		return nullptr;
	
	return {stmt, [](sqlite3_stmt * stmt){
		sqlite3_finalize(stmt);
	}};
};

void ctx_impl::analyize(std::vector<equity> &hedges) {
	
	const int nMaxDays = 7;
	
	quandl_ticker_source quandl("mzTwizqpbUxtKqUEZKqs&columns");
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
	{
		auto sql = sqlOpen();
		
		char qCreateSchema[] = "create table day (date integer, exchange text, symbol text, open real, close real, low real, high real, primary key (date, exchange, symbol));";
		auto createCommand = sqlPrepare(sql.get(), qCreateSchema, sizeof(qCreateSchema)/sizeof(qCreateSchema[0]));
		if (createCommand && (sqlite3_step(createCommand.get()) != SQLITE_OK)){
			std::cout <<"Error " << sqlite3_errcode(sql.get()) << ":" << sqlite3_errstr(sqlite3_errcode(sql.get())) << ":" << sqlite3_errmsg(sql.get()) << std::endl;
		}
		
	}
	/*
	struct Data {
		float open;
		float close;
		float percentIncrease;
	};
	
	const size_t arrayElements = nMaxDays * hedges.size();
	std::unique_ptr<Data []> arrData(new Data[arrayElements]);
	std::string dates[nMaxDays];
	Data * pData = arrData.get();
	memset(pData, 0, sizeof(pData[0]) * arrayElements);
	
	auto rows = tree.get_child("data");
	int iDay = 0;
	for (auto & row: rows) {
		auto it = row.second.begin();
		
		//Date
		dates[iDay] = it->second.data();
		it++;
		
		for (int iHedge = 0; iHedge < hedges.size(); iHedge++) {
			assert(it != row.second.end());
			Data & data = pData[iDay * hedges.size() + iHedge];
			
			//Open
			data.open = it->second.get_value<float>();
			it++;
			
			//High
			it++;
			
			//Low;
			it++;
			
			//Close
			data.close = it->second.get_value<float>();
			it++;
			
			//Volume
			it++;
			
			
			data.percentIncrease = (data.close - data.open)/data.open * 100.0;
		}
		assert(it == row.second.end());
		iDay++;
	}
	
	int nDay = iDay;
	iDay = 0;
	const int printWidth = 9;
	const int printPrecesion = 4;
	
	while(iDay < nDay) {
		std::cout << dates[iDay] << std::endl;
		for (int iOuterHedge = 0; iOuterHedge < hedges.size(); iOuterHedge++) {
			Data & outerData = pData[iDay * hedges.size() + iOuterHedge];
			std::cout
			<< std::setw(printWidth) << hedges[iOuterHedge].symbol
			<< std::setw(printWidth) << std::setprecision(printPrecesion) << outerData.open
			<< std::setw(printWidth) << std::setprecision(printPrecesion) << outerData.close
			<< std::setw(printWidth) << std::setprecision(printPrecesion) << outerData.percentIncrease
			<< std::endl;
		}
		for (int iOuterHedge = 0; iOuterHedge < hedges.size(); iOuterHedge++) {
			Data & outerData = pData[iDay * hedges.size() + iOuterHedge];
			
			for (int iInnerHedge = 0; iInnerHedge < hedges.size(); iInnerHedge++) {
				if (iInnerHedge <= iOuterHedge) continue;
				Data & innerData = pData[iDay * hedges.size() + iInnerHedge];
				std::cout
				<< std::setw(printWidth) << hedges[iInnerHedge].symbol << "/" << hedges[iOuterHedge].symbol << "\t"
				<< std::setw(printWidth) << std::setprecision(printPrecesion) << innerData.percentIncrease/outerData.percentIncrease
				<< std::endl;
			}

		}
		std::cout << std::endl;
		iDay++;
	}
	*/
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
	
	
	Poco::Net::HTTPClientSession session("www.quandl.com");
	Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, pathStream.str(), Poco::Net::HTTPMessage::HTTP_1_1);
	session.sendRequest(request);
	Poco::Net::HTTPResponse response;
	auto & data = session.receiveResponse(response);
	auto status = response.getStatus();
	assert(status == Poco::Net::HTTPResponse::HTTPStatus::HTTP_OK || status == 422);
	boost::property_tree::ptree tree;
	boost::property_tree::read_json(data, tree);
	
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
	
	quotes q(fetchedDays, fetchedValues, dayCount(), equityCount());
	while(q.nextDay() && dayIt != dayEnd) {
		auto it = dayIt->second.begin();
		auto itEnd = dayIt->second.end();

		TimePoint currentTime;
		boost::gregorian::date d(boost::gregorian::from_simple_string(it->second.data()));
		auto tm = boost::gregorian::to_tm(d);
		q.timepoint() = Clock::from_time_t(std::mktime(&tm));
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

ctx::ctx(): m_impl(new ctx_impl) {

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



