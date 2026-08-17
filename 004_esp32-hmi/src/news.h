#pragma once

#include <Arduino.h>

static const size_t NEWS_HEADLINE_COUNT = 3;

struct NewsResult
{
  bool ok;
  String headlines[NEWS_HEADLINE_COUNT];
  size_t count;
};

NewsResult fetchTopHeadlines();
