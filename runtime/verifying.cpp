#include "include/verifying.h"

#include <algorithm>

namespace ltest {

template <>
std::string toString<int>(const int &a) {
  return std::to_string(a);
}

template <>
std::string toString<std::shared_ptr<Token>>(
    const std::shared_ptr<Token> &token) {
  return "token";
}

std::string toLower(std::string str) {
  std::transform(str.begin(), str.end(), str.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return str;
}

std::vector<std::string> split(const std::string &s, char delim) {
  std::vector<std::string> res{""};
  for (char c : s) {
    if (c == delim) {
      res.push_back("");
    } else {
      res.back() += c;
    }
  }
  return res;
}

const std::string kRR = "rr";
const std::string kRandom = "random";
const std::string kTLA = "tla";
const std::string kPCT = "pct";

// Extracts required opts, returns the rest of args.
std::vector<std::string> parse_opts(std::vector<std::string> args, Opts &opts) {
  if (args.size() < 7) {
    throw std::invalid_argument("all required opts should be specified");
  }
  opts.threads = std::stoul(args[0]);  // Throws if can't transform.
  opts.tasks = std::stoul(args[1]);
  opts.switches = std::stoul(args[2]);
  opts.rounds = std::stoul(args[3]);
  opts.verbose = std::stoi(args[4]) == 1;
  std::string strategy_name = args[5];
  strategy_name = toLower(std::move(strategy_name));
  if (strategy_name == kRR) {
    opts.typ = RR;
  } else if (strategy_name == kRandom) {
    opts.typ = RND;
  } else if (strategy_name == kTLA) {
    opts.typ = TLA;
  } else if (strategy_name == kPCT) {
    opts.typ = PCT;
  } else {
    throw std::invalid_argument("unsupported strategy");
  }

  std::string weights_str = args[6];
  std::vector<int> thread_weights;
  if (weights_str != "") {
    auto splited = split(weights_str, ',');
    thread_weights.reserve(splited.size());
    for (auto &s : splited) {
      thread_weights.push_back(std::stoi(s));
    }
  }
  opts.thread_weights = std::move(thread_weights);

  args.erase(args.begin(), args.begin() + 7);
  return args;
}

}  // namespace ltest
