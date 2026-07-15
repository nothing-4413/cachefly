#include "cachefly/storage/kv_store.h"

#include <algorithm>
#include <charconv>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <system_error>
#include <utility>

#include "cachefly/metrics/metrics.h"

namespace cachefly::storage {

EvictionPolicy ParseEvictionPolicy(const std::string& name) {
    if (name == "lru") return EvictionPolicy::kLru;
    if (name == "lfu") return EvictionPolicy::kLfu;
    if (name == "random") return EvictionPolicy::kRandom;
    if (name == "noeviction") return EvictionPolicy::kNoEviction;
    throw std::invalid_argument("invalid eviction policy: " + name);
}

KvStore::KvStore(ClockFunction clock, std::size_t maxmemory,
                 EvictionPolicy policy, metrics::Metrics* metrics)
    : clock_(std::move(clock)), maxmemory_(maxmemory), policy_(policy), metrics_(metrics) {}

std::optional<std::string> KvStore::Get(const std::string& key) {
    auto found = entries_.find(key);
    if (found == entries_.end() || RemoveIfExpired(found, clock_())) {
        if (metrics_ != nullptr) metrics_->CacheMiss();
        return std::nullopt;
    }
    if (metrics_ != nullptr) metrics_->CacheHit();
    found->second.last_access = ++access_clock_;
    if (found->second.frequency < std::numeric_limits<std::uint64_t>::max()) ++found->second.frequency;
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

    const std::size_t old_bytes = exists ? EntryBytes(found->first, found->second.value) : 0;
    const std::size_t new_bytes = EntryBytes(request.key, request.value);
    if (new_bytes > maxmemory_ ||
        !MakeRoom(memory_usage_ - old_bytes + new_bytes, request.key)) {
        return command::WriteResult::kNoMemory;
    }
    std::optional<Clock::time_point> expires_at;
    if (request.ttl.has_value()) expires_at = now + *request.ttl;
    if (exists) memory_usage_ -= old_bytes;
    entries_.insert_or_assign(std::move(request.key),
                              Entry{std::move(request.value), expires_at, ++access_clock_, 1});
    memory_usage_ += new_bytes;
    return command::WriteResult::kOk;
}

command::WriteResult KvStore::MSet(std::vector<command::Database::KeyValue> values) {
    KvStore trial = *this;
    trial.metrics_ = nullptr;
    std::unordered_map<std::string, std::string> updates;
    for (auto& [key, value] : values) {
        updates.insert_or_assign(std::move(key), std::move(value));
    }
    for (const auto& [key, value] : updates) {
        static_cast<void>(value);
        const auto found = trial.entries_.find(key);
        if (found != trial.entries_.end()) {
            trial.memory_usage_ -= trial.EntryBytes(found->first, found->second.value);
            trial.entries_.erase(found);
        }
    }
    std::size_t required = 0;
    for (const auto& [key, value] : updates) {
        const std::size_t bytes = trial.EntryBytes(key, value);
        if (bytes > trial.maxmemory_ - required) return command::WriteResult::kNoMemory;
        required += bytes;
    }
    std::size_t projected = trial.memory_usage_ + required;
    if (projected < trial.memory_usage_) return command::WriteResult::kNoMemory;
    if (trial.policy_ == EvictionPolicy::kNoEviction && projected > trial.maxmemory_) {
        return command::WriteResult::kNoMemory;
    }
    std::size_t evicted = 0;
    while (projected > trial.maxmemory_) {
        auto victim = trial.SelectVictim();
        if (victim == trial.entries_.end()) return command::WriteResult::kNoMemory;
        const std::size_t bytes = trial.EntryBytes(victim->first, victim->second.value);
        trial.memory_usage_ -= bytes;
        projected -= bytes;
        trial.entries_.erase(victim);
        ++evicted;
    }
    for (auto& [key, value] : updates) {
        trial.entries_.emplace(std::move(key),
                               Entry{std::move(value), std::nullopt,
                                     ++trial.access_clock_, 1});
    }
    trial.memory_usage_ += required;
    entries_ = std::move(trial.entries_);
    memory_usage_ = trial.memory_usage_;
    access_clock_ = trial.access_clock_;
    random_state_ = trial.random_state_;
    expire_scan_offset_ = trial.expire_scan_offset_;
    if (metrics_ != nullptr) {
        for (std::size_t index = 0; index < evicted; ++index) metrics_->KeyEvicted();
    }
    return command::WriteResult::kOk;
}

std::int64_t KvStore::Delete(const std::vector<std::string>& keys) {
    std::int64_t removed = 0;
    const auto now = clock_();
    for (const std::string& key : keys) {
        auto found = entries_.find(key);
        if (found != entries_.end() && !RemoveIfExpired(found, now)) {
            memory_usage_ -= EntryBytes(found->first, found->second.value);
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
    if (ttl <= std::chrono::milliseconds::zero()) {
        memory_usage_ -= EntryBytes(found->first, found->second.value);
        entries_.erase(found);
    }
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
    command::SetRequest request{key, std::to_string(result), std::nullopt,
                                command::SetCondition::kNone};
    if (found != entries_.end() && found->second.expires_at.has_value()) {
        request.ttl = std::chrono::duration_cast<std::chrono::milliseconds>(
            *found->second.expires_at - now);
    }
    if (Set(std::move(request)) == command::WriteResult::kNoMemory) {
        return {command::IncrementStatus::kNoMemory, 0};
    }
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
            memory_usage_ -= EntryBytes(current->first, current->second.value);
            entries_.erase(current);
            ++removed;
            if (metrics_ != nullptr) metrics_->KeyExpired();
        }
    }
    expire_scan_offset_ = entries_.empty() ? 0 : (skip + examined) % entries_.size();
    return removed;
}

std::size_t KvStore::Size() const noexcept { return entries_.size(); }
std::size_t KvStore::MemoryUsage() const noexcept { return memory_usage_; }

std::vector<SnapshotEntry> KvStore::Snapshot() {
    ActiveExpire(entries_.size());
    const auto now = clock_();
    std::vector<SnapshotEntry> snapshot;
    snapshot.reserve(entries_.size());
    for (const auto& [key, entry] : entries_) {
        std::optional<std::chrono::milliseconds> ttl;
        if (entry.expires_at.has_value()) {
            ttl = std::max(std::chrono::milliseconds::zero(),
                           std::chrono::duration_cast<std::chrono::milliseconds>(
                               *entry.expires_at - now));
        }
        snapshot.push_back({key, entry.value, ttl});
    }
    return snapshot;
}

void KvStore::Clear() noexcept { entries_.clear(); memory_usage_ = 0; expire_scan_offset_ = 0; }

bool KvStore::IsExpired(const Entry& entry, Clock::time_point now) const {
    return entry.expires_at.has_value() && *entry.expires_at <= now;
}

bool KvStore::RemoveIfExpired(Map::iterator iterator, Clock::time_point now) {
    if (!IsExpired(iterator->second, now)) return false;
    memory_usage_ -= EntryBytes(iterator->first, iterator->second.value);
    entries_.erase(iterator);
    if (metrics_ != nullptr) metrics_->KeyExpired();
    return true;
}

std::size_t KvStore::EntryBytes(const std::string& key, const std::string& value) const noexcept {
    return sizeof(Entry) + key.size() + value.size();
}

KvStore::Map::iterator KvStore::SelectVictim(const std::string& protected_key) {
    auto best = entries_.end();
    if (policy_ == EvictionPolicy::kRandom && entries_.size() > 1) {
        random_state_ ^= random_state_ << 13;
        random_state_ ^= random_state_ >> 7;
        random_state_ ^= random_state_ << 17;
        auto candidate = entries_.begin();
        std::advance(candidate, static_cast<std::ptrdiff_t>(random_state_ % entries_.size()));
        if (candidate->first != protected_key) return candidate;
    }
    for (auto iterator = entries_.begin(); iterator != entries_.end(); ++iterator) {
        if (iterator->first == protected_key) continue;
        if (best == entries_.end() || policy_ == EvictionPolicy::kRandom ||
            (policy_ == EvictionPolicy::kLru && iterator->second.last_access < best->second.last_access) ||
            (policy_ == EvictionPolicy::kLfu && iterator->second.frequency < best->second.frequency)) best = iterator;
    }
    return best;
}

KvStore::Map::iterator KvStore::SelectVictim() {
    if (policy_ == EvictionPolicy::kRandom && !entries_.empty()) {
        random_state_ ^= random_state_ << 13;
        random_state_ ^= random_state_ >> 7;
        random_state_ ^= random_state_ << 17;
        auto candidate = entries_.begin();
        std::advance(candidate, static_cast<std::ptrdiff_t>(random_state_ % entries_.size()));
        return candidate;
    }
    auto best = entries_.end();
    for (auto iterator = entries_.begin(); iterator != entries_.end(); ++iterator) {
        if (best == entries_.end() ||
            (policy_ == EvictionPolicy::kLru && iterator->second.last_access < best->second.last_access) ||
            (policy_ == EvictionPolicy::kLfu && iterator->second.frequency < best->second.frequency)) {
            best = iterator;
        }
    }
    return best;
}

bool KvStore::MakeRoom(std::size_t projected, const std::string& protected_key) {
    if (projected <= maxmemory_) return true;
    if (policy_ == EvictionPolicy::kNoEviction) return false;
    while (projected > maxmemory_) {
        auto victim = SelectVictim(protected_key);
        if (victim == entries_.end()) return false;
        const std::size_t bytes = EntryBytes(victim->first, victim->second.value);
        memory_usage_ -= bytes;
        projected -= bytes;
        entries_.erase(victim);
        if (metrics_ != nullptr) metrics_->KeyEvicted();
    }
    return true;
}

}  // namespace cachefly::storage
