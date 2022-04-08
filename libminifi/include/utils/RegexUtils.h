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
  struct RegmatchWrapper {
    operator std::string() const {
      return str();
    }

    std::string str() const {
      if (match.rm_so == -1) {
        return "";
      }
      return std::string(pattern.begin() + match.rm_so, pattern.begin() + match.rm_eo);
    }

    const regmatch_t& match;
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
  std::vector<regmatch_t> matches_;
  std::string pattern_;
#endif

  friend bool regexMatch(const std::string &pattern, SMatch& match, const Regex& regex);
  friend bool regexSearch(const std::string &pattern, SMatch& match, const Regex& regex);

 public:
#ifdef NO_MORE_REGFREEE
  const decltype(matches_.suffix())& suffix() const;
  const decltype(matches_[0])& operator[](std::size_t index) const;
#else
  SuffixWrapper suffix() const;
  RegmatchWrapper operator[](std::size_t index) const;
#endif
  std::size_t size() const;
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
};

bool regexMatch(const std::string &pattern, const Regex& regex);
bool regexMatch(const std::string &pattern, SMatch& match, const Regex& regex);

bool regexSearch(const std::string &pattern, const Regex& regex);
bool regexSearch(const std::string &pattern, SMatch& match, const Regex& regex);

}  // namespace org::apache::nifi::minifi::utils
