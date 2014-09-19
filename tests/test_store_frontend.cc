#include "broker/broker.hh"
#include "broker/endpoint.hh"
#include "broker/store/master.hh"
#include "broker/store/frontend.hh"
#include "broker/store/response_queue.hh"
#include "testsuite.hh"
#include <map>
#include <vector>
#include <unistd.h>
#include <poll.h>

using namespace std;
using dataset = map<broker::data, broker::data>;

bool compare_contents(const broker::store::frontend& store, const dataset& ds)
	{
	dataset actual;

	for ( const auto& key : broker::store::keys(store) )
		{
		auto val = broker::store::lookup(store, key);
		if ( val ) actual.insert(make_pair(key, *val.get()));
		}

	return actual == ds;
	}

void wait_for(const broker::store::frontend& f, broker::data k,
              bool exists = true)
	{
	while ( broker::store::exists(f, k) != exists ) usleep(1000);
	}

int main()
	{
	broker::init();
	broker::endpoint node("node0");
	broker::store::master m(node, "mystore");

	dataset ds0 = { make_pair("1", "one"),
	                make_pair("2", "two"),
	                make_pair(3, "three") };
	for ( const auto& p : ds0 ) m.insert(p.first, p.second);

	broker::store::frontend f(node, "mystore");
	f.insert("4", "four");
	wait_for(f, "4");

	ds0.insert(make_pair("4", "four"));
	BROKER_TEST(compare_contents(f, ds0));

	m.insert("5", "five");
	wait_for(f, "5");
	ds0.insert(make_pair("5", "five"));
	BROKER_TEST(compare_contents(f, ds0));

	f.erase("5");
	wait_for(f, "5", false);
	ds0.erase("5");
	BROKER_TEST(compare_contents(f, ds0));
	BROKER_TEST(broker::store::size(f) == ds0.size());
	BROKER_TEST(*broker::store::lookup(f, 3) == "three");

	pollfd pfd{f.responses().fd(), POLLIN, 0};
	vector<string> cookies { "exists", "lookup", "size", "timeout", "keys" };
	f.exists("1", chrono::seconds(10), &cookies[0]);
	f.lookup("2", chrono::seconds(10), &cookies[1]);
	f.size(chrono::seconds(10), &cookies[2]);
	f.size(chrono::seconds(0), &cookies[3]);
	f.keys(chrono::seconds(10), &cookies[4]);
	vector<broker::store::response> responses;

	while ( responses.size() < cookies.size() )
		{
		poll(&pfd, 1, -1);

		for ( auto& msg : f.responses().want_pop() )
			responses.push_back(move(msg));
		}

	for ( const auto& msg : responses )
		{
		using broker::store::result;
		if ( msg.cookie == &cookies[3] )
			{
			BROKER_TEST(msg.reply.stat == result::status::timeout);
			continue;
			}

		BROKER_TEST(msg.reply.stat == result::status::success);

		if ( msg.cookie == &cookies[0] )
			BROKER_TEST(msg.reply.exists == true);
		else if ( msg.cookie == &cookies[1] )
			BROKER_TEST(msg.reply.val == "two");
		else if ( msg.cookie == &cookies[2] )
			BROKER_TEST(msg.reply.size == ds0.size());
		else if ( msg.cookie == &cookies[4] )
			{
			unordered_set<broker::data> expected;
			for ( const auto& p : ds0 ) expected.insert(p.first);
			BROKER_TEST(msg.reply.keys == expected);
			}
		}

	f.clear();
	wait_for(f, "1", false);
	BROKER_TEST(broker::store::size(f) == 0);

	return BROKER_TEST_RESULT();
	}