#include <korelate/korelate.hpp>

using namespace korelate;
int main() {
	korelate::ctx ctx;
	//{"NASDAQ_AMZN", /*"NASDAQ_QQQ",*/ "NYSE_PSQ", "NYSE_REW"}
	ctx.analyize({{Exchange::NASDAQ, "AMZN"}, {Exchange::NYSE, "PSQ"}, {Exchange::NYSE, "REW"}});
}