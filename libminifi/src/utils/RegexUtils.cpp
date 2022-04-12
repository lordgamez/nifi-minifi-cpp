/**
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "utils/RegexUtils.h"
#include "Exception.h"
#include <iostream>
#include <vector>

namespace org::apache::nifi::minifi::utils {

#ifndef NO_MORE_REGFREEE
SMatch::SuffixWrapper SMatch::suffix() const {
  if ((size_t) matches_[0].match.rm_eo >= pattern_.size()) {
    return SuffixWrapper{std::string()};
  } else {
    return SuffixWrapper{pattern_.substr(matches_[0].match.rm_eo)};
  }
}

const SMatch::Regmatch& SMatch::operator[](std::size_t index) const {
  return matches_[index];
}

std::size_t SMatch::size() const {
  std::size_t count = 0;
  for (const auto &m : matches_) {
    if (m.match.rm_so == -1) {
      break;
    }
    ++count;
  }
  return count;
}
#endif

Regex::Regex() : Regex::Regex("") {}

Regex::Regex(const std::string &value) : Regex::Regex(value, {}) {}

Regex::Regex(const std::string &value,
             const std::vector<Regex::Mode> &mode)
    : regex_str_(value),
      valid_(false) {
  // Create regex mode
#ifdef NO_MORE_REGFREEE
  regex_mode_ = std::regex_constants::ECMAScript;
#else
  regex_mode_ = REG_EXTENDED;
#endif
  for (const auto m : mode) {
    switch (m) {
      case Mode::ICASE:
#ifdef NO_MORE_REGFREEE
        regex_mode_ |= std::regex_constants::icase;
#else
        regex_mode_ |= REG_ICASE;
#endif
        break;
    }
  }
  // Compile
#ifdef NO_MORE_REGFREEE
  try {
    compiled_regex_ = std::regex(regex_str_, regex_mode_);
    valid_ = true;
  } catch (const std::regex_error &e) {
    throw Exception(REGEX_EXCEPTION, e.what());
  }
#else
  int err_code = regcomp(&compiled_regex_, regex_str_.c_str(), regex_mode_);
  if (err_code) {
    const size_t sz = regerror(err_code, &compiled_regex_, nullptr, 0);
    std::vector<char> msg(sz);
    regerror(err_code, &compiled_regex_, msg.data(), msg.size());
    throw Exception(REGEX_EXCEPTION, std::string(msg.begin(), msg.end()));
  }
  err_code = regcomp(&compiled_full_input_regex_, ('^' + regex_str_ + '$').c_str(), regex_mode_);
  if (err_code) {
    const size_t sz = regerror(err_code, &compiled_full_input_regex_, nullptr, 0);
    std::vector<char> msg(sz);
    regerror(err_code, &compiled_full_input_regex_, msg.data(), msg.size());
    throw Exception(REGEX_EXCEPTION, std::string(msg.begin(), msg.end()));
  }
  valid_ = true;
#endif
}

Regex::Regex(const Regex& other)
#ifndef NO_MORE_REGFREEE
  : valid_(false),
    regex_mode_(REG_EXTENDED)
#endif
{
  *this = other;
}

Regex& Regex::operator=(const Regex& other) {
  if (this == &other) {
    return *this;
  }

  regex_str_ = other.regex_str_;
#ifdef NO_MORE_REGFREEE
  compiled_regex_ = other.compiled_regex_;
  regex_mode_ = other.regex_mode_;
#else
  if (valid_) {
    regfree(&compiled_regex_);
    regfree(&compiled_full_input_regex_);
  }
  regex_mode_ = other.regex_mode_;
  int err_code = regcomp(&compiled_regex_, regex_str_.c_str(), regex_mode_);
  if (err_code) {
    const size_t sz = regerror(err_code, &compiled_regex_, nullptr, 0);
    std::vector<char> msg(sz);
    regerror(err_code, &compiled_regex_, msg.data(), msg.size());
    throw Exception(REGEX_EXCEPTION, std::string(msg.begin(), msg.end()));
  }
  err_code = regcomp(&compiled_full_input_regex_, ('^' + regex_str_ + '$').c_str(), regex_mode_);
  if (err_code) {
    const size_t sz = regerror(err_code, &compiled_full_input_regex_, nullptr, 0);
    std::vector<char> msg(sz);
    regerror(err_code, &compiled_full_input_regex_, msg.data(), msg.size());
    throw Exception(REGEX_EXCEPTION, std::string(msg.begin(), msg.end()));
  }
#endif
  valid_ = other.valid_;
  return *this;
}

Regex::Regex(Regex&& other)
#ifndef NO_MORE_REGFREEE
  : valid_(false),
    regex_mode_(REG_EXTENDED)
#endif
{
  *this = std::move(other);
}

Regex& Regex::operator=(Regex&& other) {
  if (this == &other) {
    return *this;
  }

  regex_str_ = std::move(other.regex_str_);
#ifdef NO_MORE_REGFREEE
  compiled_regex_ = std::move(other.compiled_regex_);
  regex_mode_ = other.regex_mode_;
#else
  if (valid_) {
    regfree(&compiled_regex_);
    regfree(&compiled_full_input_regex_);
  }
  compiled_regex_ = other.compiled_regex_;
  compiled_full_input_regex_ = other.compiled_full_input_regex_;
  regex_mode_ = other.regex_mode_;
#endif
  valid_ = other.valid_;
  other.valid_ = false;
  return *this;
}

Regex::~Regex() {
#ifndef NO_MORE_REGFREEE
  if (valid_) {
    regfree(&compiled_regex_);
    regfree(&compiled_full_input_regex_);
  }
#endif
}

bool regexSearch(const std::string &pattern, const Regex& regex) {
  if (!regex.valid_) {
    return false;
  }
#ifdef NO_MORE_REGFREEE
  return std::regex_search(pattern, regex.compiled_regex_);
#else
  std::vector<regmatch_t> match;
  int maxGroups = std::count(regex.regex_str_.begin(), regex.regex_str_.end(), '(') + 1;
  match.resize(maxGroups);
  return regexec(&regex.compiled_regex_, pattern.c_str(), match.size(), match.data(), 0) == 0;
#endif
}

bool regexSearch(const std::string &pattern, SMatch& match, const Regex& regex) {
  if (!regex.valid_) {
    return false;
  }
#ifdef NO_MORE_REGFREEE
  return std::regex_search(pattern, match, regex.compiled_regex_);
#else
  match.clear();
  std::vector<regmatch_t> regmatches;
  int maxGroups = std::count(regex.regex_str_.begin(), regex.regex_str_.end(), '(') + 1;
  regmatches.resize(maxGroups);
  bool result = regexec(&regex.compiled_regex_, pattern.c_str(), regmatches.size(), regmatches.data(), 0) == 0;
  match.pattern_ = pattern;
  for (auto regmatch : regmatches) {
    match.matches_.push_back(SMatch::Regmatch{regmatch, match.pattern_});
  }
  return result;
#endif
}

bool regexMatch(const std::string &pattern, const Regex& regex) {
  if (!regex.valid_) {
    return false;
  }
#ifdef NO_MORE_REGFREEE
  return std::regex_match(pattern, regex.compiled_regex_);
#else
  std::vector<regmatch_t> match;
  int maxGroups = std::count(regex.regex_str_.begin(), regex.regex_str_.end(), '(') + 1;
  match.resize(maxGroups);
  return regexec(&regex.compiled_full_input_regex_, pattern.c_str(), match.size(), match.data(), 0) == 0;
#endif
}

bool regexMatch(const std::string &pattern, SMatch& match, const Regex& regex) {
  if (!regex.valid_) {
    return false;
  }
#ifdef NO_MORE_REGFREEE
  return std::regex_match(pattern, match, regex.compiled_regex_);
#else
  match.clear();
  std::vector<regmatch_t> regmatches;
  int maxGroups = std::count(regex.regex_str_.begin(), regex.regex_str_.end(), '(') + 1;
  regmatches.resize(maxGroups);
  bool result = regexec(&regex.compiled_full_input_regex_, pattern.c_str(), regmatches.size(), regmatches.data(), 0) == 0;
  match.pattern_ = pattern;
  for (auto regmatch : regmatches) {
    match.matches_.push_back(SMatch::Regmatch{regmatch, match.pattern_});
  }
  return result;
#endif
}

SMatch getLastRegexMatch(const std::string& pattern, const utils::Regex& regex) {
#ifdef NO_MORE_REGFREEE
  auto matches = std::sregex_iterator(pattern.begin(), pattern.end(), regex.compiled_regex_);
  std::smatch last_match;
  while (matches != std::sregex_iterator()) {
    last_match = *matches;
    matches = std::next(matches);
  }
  return last_match;
#else
  SMatch search_result;
  SMatch last_match;
  auto current_str = pattern;
  while (regexSearch(current_str, search_result, regex)) {
    last_match = search_result;
    current_str = search_result.suffix();
  }

  auto diff = pattern.size() - last_match.pattern_.size();
  last_match.pattern_ = pattern;
  for (auto& match : last_match.matches_) {
    if (match.match.rm_so >= 0) {
      match.match.rm_so += diff;
      match.match.rm_eo += diff;
    }
  }
  return last_match;
#endif
}

}  // namespace org::apache::nifi::minifi::utils
