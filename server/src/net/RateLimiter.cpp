#include "RateLimiter.h"

#include <algorithm>

namespace caw {
namespace {
constexpr std::size_t kForgetAbove = 10000;
}

RateLimiter::RateLimiter(double perMinute, double burst)
    : perMillisecond_(perMinute / 60000.0), burst_(burst) {}

bool RateLimiter::allow(const std::string& key, std::uint32_t now) {
  if (buckets_.size() > kForgetAbove) forgetFullBuckets(now);
  auto [found, fresh] = buckets_.try_emplace(key, Bucket{burst_, now});
  Bucket& bucket = found->second;
  if (!fresh) {
    bucket.tokens = std::min(burst_, bucket.tokens + (now - bucket.at) * perMillisecond_);
    bucket.at = now;
  }
  if (bucket.tokens < 1) return false;
  bucket.tokens -= 1;
  return true;
}

void RateLimiter::forgetFullBuckets(std::uint32_t now) {
  for (auto bucket = buckets_.begin(); bucket != buckets_.end();) {
    const double tokens = bucket->second.tokens + (now - bucket->second.at) * perMillisecond_;
    if (tokens >= burst_)
      bucket = buckets_.erase(bucket);
    else
      ++bucket;
  }
}

}  // namespace caw
