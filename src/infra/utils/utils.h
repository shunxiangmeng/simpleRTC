#pragma once

//禁止拷贝基类
class noncopyable {
protected:
    noncopyable() {}
    ~noncopyable() {}
private:
    noncopyable(const noncopyable &that) = delete;             //拷贝构造函数
    noncopyable(noncopyable &&that) = delete;                  //移动构造函数
    noncopyable &operator=(const noncopyable &that) = delete;  //拷贝赋值运算符
    noncopyable &operator=(noncopyable &&that) = delete;       //移动赋值运算符
};