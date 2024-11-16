#pragma once
#include <tuple>
#include <type_traits>

#include "lib.h"

namespace ltest {

namespace generators {

extern std::shared_ptr<Token> generated_token;

// Makes single argument from the value.
template <typename T>
auto makeSingleArg(T&& arg) {
  using arg_type = typename std::remove_reference<T>::type;
  return std::tuple<arg_type>{std::forward<arg_type>(arg)};
}

std::tuple<> genEmpty(size_t thread_num);

std::tuple<std::shared_ptr<Token>> genToken(size_t thread_num);

}  // namespace generators

}  // namespace ltest
