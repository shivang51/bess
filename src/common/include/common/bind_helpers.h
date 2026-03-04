#pragma once

#define BIND_FN(fn) std::bind(&fn, this)

#define BIND_FN_1(fn) std::bind(&fn, this, std::placeholders::_1)

#define BIND_FN_2(fn) std::bind(&fn, this, std::placeholders::_1, std::placeholders::_2)

#define BIND_FN_L(fn) [this](auto &&...args) { return fn(std::forward<decltype(args)>(args)...); }
