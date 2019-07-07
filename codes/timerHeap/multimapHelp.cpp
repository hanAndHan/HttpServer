// @Author Liu Han
// @Email  hancurrent@foxmail.com
#include"multimapHelp.h"
#include"../TCP/Connection.h"
void multimapDelete(multimap<T1, T2, compare>& mulMup, const T1 & key, const T3 & value)
{
	pair<multimap<T1, T2, compare >::iterator, multimap<T1, T2, compare >::iterator> pos = mulMup.equal_range(key);
	auto it = pos.first;
	auto end = pos.second;
	while (it != end)
	{
		if (it->second->name() == value)
		{
			it = mulMup.erase(it);
			break;
		}
		else
		{
			++it;
		}
	}
}
