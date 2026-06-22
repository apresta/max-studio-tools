#pragma once

#include <utility>

#include "c74_min.h"

namespace attr {

template <typename T, typename Hook>
auto MakeNumeric(c74::min::object_base* owner, const char* name,
                  T default_value, T min, T max, const char* title,
                  const char* description, Hook hook) {
  return c74::min::attribute<T>{
      owner,
      name,
      default_value,
      c74::min::title{title},
      c74::min::description{description},
      c74::min::range{min, max},
      c74::min::setter{[min, max, hook = std::move(hook)](
                           const c74::min::atoms& args,
                           const int) mutable -> c74::min::atoms {
        const T v = c74::min::clamp(static_cast<T>(args[0]), min, max);
        hook(v);
        return {v};
      }}};
}

template <typename Hook>
auto MakeBool(c74::min::object_base* owner, const char* name,
               bool default_value, const char* title, const char* description,
               Hook hook) {
  return c74::min::attribute<bool>{
      owner,
      name,
      default_value,
      c74::min::title{title},
      c74::min::description{description},
      c74::min::setter{
          [hook = std::move(hook)](const c74::min::atoms& args,
                                   const int) mutable -> c74::min::atoms {
            const bool v = static_cast<bool>(args[0]);
            hook(v);
            return {v};
          }}};
}

}  // namespace attr

#define DECLARE_ATTR_DOUBLE(name_, title_, desc_, default_, min_, max_, ...) \
  c74::min::attribute<double> name_ {                                        \
    attr::MakeNumeric<double>(this, #name_, static_cast<double>(default_),  \
                               static_cast<double>(min_),                    \
                               static_cast<double>(max_), title_, desc_,     \
                               __VA_ARGS__)                                  \
  }

#define DECLARE_ATTR_INT(name_, title_, desc_, default_, min_, max_, ...)   \
  c74::min::attribute<int> name_ {                                          \
    attr::MakeNumeric<int>(this, #name_, static_cast<int>(default_),       \
                            static_cast<int>(min_), static_cast<int>(max_), \
                            title_, desc_, __VA_ARGS__)                     \
  }

#define DECLARE_ATTR_BOOL(name_, title_, desc_, default_, ...)                \
  c74::min::attribute<bool> name_ {                                           \
    attr::MakeBool(this, #name_, static_cast<bool>(default_), title_, desc_, \
                    __VA_ARGS__)                                              \
  }
