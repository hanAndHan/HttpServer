// @Author Lin Han
// @Email  hancurrent@foxmail.com
// noncopyable为实现不可拷贝的类提供了简单清晰的解决方案
#pragma once

class noncopyable
{
protected:
    noncopyable() {}
    ~noncopyable() {}
private:
	noncopyable(const noncopyable&);
	const noncopyable& operator=(const noncopyable&);
};
