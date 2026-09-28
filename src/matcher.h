/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _MATCHER_H
#define _MATCHER_H

enum MatchType {
    MATCH_EQUALS,
    MATCH_CONTAINS,
    MATCH_STARTS_WITH,
    MATCH_ENDS_WITH
};


class Matcher {
  private:
    MatchType _type;
    char* _pattern;
    int _len;

  public:
    Matcher(const char* pattern);
    ~Matcher();

    Matcher(const Matcher& m);
    Matcher& operator=(const Matcher& m);

    bool matches(const char* s) const;
};

#endif // _MATCHER_H
