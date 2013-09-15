#include <korelate/korelate.hpp>

using namespace korelate;

static std::shared_ptr<sqlite3> sqlOpen() {
	sqlite3 * lite;
	auto openError = sqlite3_open_v2("test.db", &lite, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
	if (openError != SQLITE_OK)
		return nullptr;
	
	return {lite, [](sqlite3 * lite){
		sqlite3_close_v2(lite);
	}};
}

int main() {
	auto sql = sqlOpen();
	korelate::ctx ctx(sql);
	//{"NASDAQ_AMZN", /*"NASDAQ_QQQ",*/ "NYSE_PSQ", "NYSE_REW"}
	ctx.analyize({{Exchange::NASDAQ, "AMZN"}, {Exchange::NYSE, "PSQ"}, {Exchange::NYSE, "REW"}});
}