// @Author Liu Han
// @Email  hancurrent@foxmail.com
#pragma once
#include<map>
#include<memory>
#include<string>

using namespace std;
class Connection;

typedef int64_t T1;
typedef shared_ptr<Connection> T2;
typedef string T3;
struct compare
{
	bool operator()(const T1 & left, const T1 & right) const
	{
		return left < right;
	}
};
void multimapDelete(multimap<T1, T2, compare >& mulMup, const T1 & key, const T3 & value);


