#include "cachefly/storage/kv_store.h"

#include <algorithm>
#include <charconv>
#include <iterator>
#include <limits>
#include <system_error>
#include <utility>

namespace cachefly::storage {

KvStore::KvStore(ClockFunction clock) : clock_(std::move(clock)) {}

std::optional<std::string> KvStore::Get(const std::string& key) {
    auto found = entries_.find(key);
    if (found == entries_.end() || RemoveIfExpired(found, clock_())) return std::nullopt;
    return found->second.value;
}

command::WriteResult KvStore::Set(command::SetRequest request) {
    const auto now = clock_();
    auto found = entries_.find(request.key);
    if (found != entries_.end() && RemoveIfExpired(found, now)) found = entries_.end();
    const bool exists = found != entries_.end();
    if (request.condition == command::SetCondition::kIfAbsent && exists) {
        return command::WriteResult::kConditionFailed;
    }
    if (request.condition == command::SetCondition::kIfPresent && !exists) {
        return command::WriteResult::kConditionFailed;
    }

    std::optional<Clock::time_point> expires_at;
    if (request.ttl.has_value()) expires_at = now + *request.ttl;
    entries_.insert_or_assign(std::move(request.key),
                              Entry{std::move(request.value), expires_at});
    return command::WriteResult::kOk;
}

std::int64_t KvStore::Delete(const std::vector<std::string>& keys) {
    std::int64_t removed = 0;
    const auto now = clock_();
    for (const std::string& key : keys) {
        auto found = entries_.find(key);
        if (found != entries_.end() && !RemoveIfExpired(found, now)) {
            entries_.erase(found);
            ++removed;
        }
    }
    return removed;
}

std::int64_t KvStore::Exists(const std::vector<std::string>& keys) {
    std::int64_t count = 0;
    const auto now = clock_();
    for (const std::string& key : keys) {
        auto found = entries_.find(key);
        if (found != entries_.end() && !RemoveIfExpired(found, now)) ++count;
    }
    return count;
}

bool KvStore::Expire(const std::string& key, std::chrono::milliseconds ttl) {
    auto found = entries_.find(key);
    const auto now = clock_();
    if (found == entries_.end() || RemoveIfExpired(found, now)) return false;
    if (ttl <= std::chrono::milliseconds::zero()) entries_.erase(found);
    else found->second.expires_at = now + ttl;
    return true;
}

std::int64_t KvStore::TtlSeconds(const std::string& key) {
    auto found = entries_.find(key);
    const auto now = clock_();
    if (found == entries_.end() || RemoveIfExpired(found, now)) return -2;
    if (!found->second.expires_at.has_value()) return -1;
    const auto remaining = std::chrono::duration_cast<std::chrono::seconds>(
        *found->second.expires_at - now).count();
    return std::max<std::int64_t>(0, remaining);
}

command::IncrementResult KvStore::Increment(const std::string& key, std::int64_t delta) {
    auto found = entries_.find(key);
    const auto now = clock_();
    if (found != entries_.end() && RemoveIfExpired(found, now)) found = entries_.end();

    std::int64_t current = 0;
    if (found != entries_.end()) {
        const std::string& text = found->second.value;
        const auto parsed = std::from_chars(text.data(), text.data() + text.size(), current);
        if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) {
            return {command::IncrementStatus::kNotInteger, 0};
        }
    }
    if ((delta > 0 && current > std::numeric_limits<std::int64_t>::max() - delta) ||
        (delta < 0 && current < std::numeric_limits<std::int64_t>::min() - delta)) {
        return {command::IncrementStatus::kOverflow, 0};
    }
    const std::int64_t result = current + delta;
    if (found == entries_.end()) entries_.emplace(key, Entry{std::to_string(result), std::nullopt});
    else found->second.value = std::to_string(result);
    return {command::IncrementStatus::kOk, result};
}

std::size_t KvStore::ActiveExpire(std::size_t max_samples) {
    if (entries_.empty() || max_samples == 0) return 0;
    const auto now = clock_();
    const std::size_t initial_size = entries_.size();
    std::size_t skip = expire_scan_offset_ % initial_size;
    auto iterator = entries_.begin();
    std::advance(iterator, static_cast<std::ptrdiff_t>(skip));
    std::size_t examined = 0;
    std::size_t removed = 0;
    while (!entries_.empty() && examined < max_samples && examined < initial_size) {
        if (iterator == entries_.end()) iterator = entries_.begin();
        auto current = iterator++;
        ++examined;
        if (IsExpired(current->second, now)) {
            entries_.erase(current);
            ++removed;
        }
    }
    expire_scan_offset_ = entries_.empty() ? 0 : (skip + examined) % entries_.size();
    return removed;
}

std::size_t KvStore::Size() const noexcept { return entries_.size(); }
void KvStore::Clear() noexcept { entries_.clear(); expire_scan_offset_ = 0; }

bool KvStore::IsExpired(const Entry& entry, Clock::time_point now) const {
    return entry.expires_at.has_value() && *entry.expires_at <= now;
}

bool KvStore::RemoveIfExpired(Map::iterator iterator, Clock::time_point now) {
    if (!IsExpired(iterator->second, now)) return false;
    entries_.erase(iterator);
    return true;
}

}  // namespace cachefly::storage
