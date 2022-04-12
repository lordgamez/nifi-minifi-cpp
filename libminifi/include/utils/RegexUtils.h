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

#pragma once

#include <string>
#include <string_view>
#include <vector>

#if defined(__GNUC__)
#include <regex.h>
#else
#include <regex>
#define NO_MORE_REGFREEE
#endif

namespace org::apache::nifi::minifi::utils {

class Regex;

class SMatch {
 private:
#ifdef NO_MORE_REGFREEE
  std::smatch matches_;
#else
  struct Regmatch {
    operator std::string() const {
      return str();
    }

    std::string str() const {
      if (match.rm_so == -1) {
        return "";
      }
      return std::string(pattern.begin() + match.rm_so, pattern.begin() + match.rm_eo);
    }

    regmatch_t match;
    std::string_view pattern;
  };

  struct SuffixWrapper {
    operator std::string() const {
      return str();
    }

    std::string str() const {
      return suffix;
    }

    std::string suffix;
  };

  void clear() {
    matches_.clear();
    pattern_.clear();
  }

  std::vector<Regmatch> matches_;
  std::string pattern_;
#endif

  friend bool regexMatch(const std::string &pattern, SMatch& match, const Regex& regex);
  friend bool regexSearch(const std::string &pattern, SMatch& match, const Regex& regex);
  friend utils::SMatch getLastRegexMatch(const std::string& str, const utils::Regex& pattern);

 public:
#ifdef NO_MORE_REGFREEE
  const decltype(matches_.suffix())& suffix() const;
  const decltype(matches_[0])& operator[](std::size_t index) const;

  typedef Iterator std::smatch::iterator;
  Iterator begin() { return matches_.begin(); }
  Iterator end() { return matches_.end(); }
#else
  struct Iterator {
    using iterator_category = std::forward_iterator_tag;
    using difference_type   = std::ptrdiff_t;
    using value_type        = Regmatch;
    using pointer           = value_type*;
    using reference         = value_type&;

    Iterator() : regmatch_(nullptr) {
    }

    explicit Iterator(Regmatch* regmatch)
      : regmatch_(regmatch) {
    }

    reference operator*() const { return *regmatch_; }
    pointer operator->() { return regmatch_; }

    Iterator& operator++() { regmatch_++; return *this; }
    Iterator operator++(int) { Iterator tmp = *this; ++(*this); return tmp; }

    friend bool operator== (const Iterator& a, const Iterator& b) { return a.regmatch_ == b.regmatch_; }
    friend bool operator!= (const Iterator& a, const Iterator& b) { return a.regmatch_ != b.regmatch_; }

   private:
    pointer regmatch_;
  };

  SuffixWrapper suffix() const;
  const Regmatch& operator[](std::size_t index) const;
  Iterator begin() { return Iterator(&matches_[0]); }
  Iterator end() { return Iterator(&matches_[matches_.size()]); }
#endif

  std::size_t size() const;
  bool ready() const {
#ifdef NO_MORE_REGFREEE
    return matches_.ready();
#else
    return !matches_.empty();
#endif
  }

  std::size_t position(std::size_t index) const {
#ifdef NO_MORE_REGFREEE
    return matches_.position(index);
#else
    return matches_[index].match.rm_so;
#endif
  }

  std::size_t length(std::size_t index) const {
#ifdef NO_MORE_REGFREEE
    return matches_.length(index);
#else
    return matches_.at(index).match.rm_eo - matches_.at(index).match.rm_so;
#endif
  }
};

class Regex {
 public:
  enum class Mode { ICASE };

  Regex();
  explicit Regex(const std::string &value);
  explicit Regex(const std::string &value,
                const std::vector<Mode> &mode);
  Regex(const Regex &);
  Regex& operator=(const Regex &);
  Regex(Regex&& other);
  Regex& operator=(Regex&& other);
  ~Regex();

 private:
  std::string regex_str_;
  bool valid_;

#ifdef NO_MORE_REGFREEE
  std::regex compiled_regex_;
  std::regex_constants::syntax_option_type regex_mode_;
#else
  regex_t compiled_regex_;
  regex_t compiled_full_input_regex_;
  int regex_mode_;
#endif

  friend class SMatch;
  friend bool regexMatch(const std::string &pattern, const Regex& regex);
  friend bool regexMatch(const std::string &pattern, SMatch& match, const Regex& regex);
  friend bool regexSearch(const std::string &pattern, const Regex& regex);
  friend bool regexSearch(const std::string &pattern, SMatch& match, const Regex& regex);
  friend SMatch getLastRegexMatch(const std::string& pattern, const utils::Regex& regex);
};

bool regexMatch(const std::string &pattern, const Regex& regex);
bool regexMatch(const std::string &pattern, SMatch& match, const Regex& regex);

bool regexSearch(const std::string &pattern, const Regex& regex);
bool regexSearch(const std::string &pattern, SMatch& match, const Regex& regex);

/**
 * Returns the last match of a regular expression within the given string
 * @param pattern incoming string
 * @param regex the regex to be matched
 * @return the last valid SMatch or a default constructed SMatch (ready() != true) if no matches have been found
 */
SMatch getLastRegexMatch(const std::string& pattern, const utils::Regex& regex);

}  // namespace org::apache::nifi::minifi::utils
