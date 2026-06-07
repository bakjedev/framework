#pragma once
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace fwrk {
  template<typename Key, typename Value, typename Hash = std::hash<Key>>
  class flat_hash_map {
    static constexpr uint8_t STATE_EMPTY = 0x80; // 0b1000'0000
    static constexpr uint8_t STATE_DELETED = 0xFF; // 0b1111'1111
    static constexpr uint8_t STATE_OCCUPIED = 0x00; // 0b0000'0000

    using value_type = std::pair<const Key, Value>;

    struct bucket {
      uint8_t state = STATE_EMPTY;
      union {
        value_type slot_;
      };

      bucket() {}
      ~bucket() {}

      value_type& slot() { return slot_; }
      const value_type& slot() const { return slot_; }
      const Key& key() const { return slot_.first; }
      Value& value() { return slot_.second; }
      const Value& value() const { return slot_.second; }
    };

    std::vector<bucket> buckets_;
    size_t bucket_count_ = 0;
    size_t element_count_ = 0;
    size_t occupied_count_ = 0;
    size_t index_mask_ = 0;
    size_t growth_threshold_ = 0;
    Hash hasher_;

    template<bool Const>
    class iterator_impl {
      using value_ref = std::conditional_t<Const, const value_type&, value_type&>;
      using value_ptr = std::conditional_t<Const, const value_type*, value_type*>;
      using vector_ptr = std::conditional_t<Const, const std::vector<bucket>*, std::vector<bucket>*>;

      vector_ptr buckets_;
      size_t index_;
      size_t capacity_;

      friend class flat_hash_map;
      iterator_impl(vector_ptr buckets, const size_t index, const size_t capacity) :
          buckets_(buckets), index_(index), capacity_(capacity)
      {
      }

    public:
      value_ref operator*() const { return (*buckets_)[index_].slot(); }
      value_ptr operator->() const { return &**this; }

      iterator_impl& operator++()
      {
        do {
          index_++;
        } while (index_ < capacity_ && ((*buckets_)[index_].state & STATE_EMPTY) != 0);
        return *this;
      }

      bool operator==(const iterator_impl& other) const { return index_ == other.index_; }
    };

  public:
    using iterator = iterator_impl<false>;
    using const_iterator = iterator_impl<true>;

    flat_hash_map() = default;

    explicit flat_hash_map(const size_t count) { reserve(count); }

    ~flat_hash_map() { clear(); }

    flat_hash_map(const flat_hash_map& other) :
        bucket_count_(other.bucket_count_), element_count_(other.element_count_),
        occupied_count_(other.occupied_count_), index_mask_(other.index_mask_),
        growth_threshold_(other.growth_threshold_), hasher_(other.hasher_)
    {
      buckets_.resize(bucket_count_);

      for (size_t i = 0; i < bucket_count_; i++) {
        const bucket& other_slot = other.buckets_[i];
        if ((other_slot.state & STATE_EMPTY) == 0) {
          bucket& slot = buckets_[i];
          new (&slot.slot_) value_type(other_slot.slot_);
          slot.state = other_slot.state;
        }
      }
    }

    flat_hash_map& operator=(const flat_hash_map& other)
    {
      if (this != &other) {
        flat_hash_map temp(other);
        swap(temp);
      }
      return *this;
    }

    flat_hash_map(flat_hash_map&& other) noexcept :
        buckets_(std::move(other.buckets_)), bucket_count_(other.bucket_count_), element_count_(other.element_count_),
        occupied_count_(other.occupied_count_), index_mask_(other.index_mask_),
        growth_threshold_(other.growth_threshold_), hasher_(std::move(other.hasher_))
    {
      other.bucket_count_ = 0;
      other.element_count_ = 0;
      other.occupied_count_ = 0;
      other.index_mask_ = 0;
      other.growth_threshold_ = 0;
    }

    flat_hash_map& operator=(flat_hash_map&& other) noexcept
    {
      if (this != &other) {
        clear();

        buckets_ = std::move(other.buckets_);
        bucket_count_ = other.bucket_count_;
        element_count_ = other.element_count_;
        occupied_count_ = other.occupied_count_;
        index_mask_ = other.index_mask_;
        growth_threshold_ = other.growth_threshold_;
        hasher_ = std::move(other.hasher_);

        other.bucket_count_ = 0;
        other.element_count_ = 0;
        other.occupied_count_ = 0;
        other.index_mask_ = 0;
        other.growth_threshold_ = 0;
      }
      return *this;
    }

    std::pair<iterator, bool> insert(std::pair<Key, Value> pair)
    {
      return insert_internal(std::move(pair.first), std::move(pair.second));
    }

    std::pair<iterator, bool> insert_or_assign(Key&& key, Value&& value)
    {
      return insert_or_assign_internal(std::move(key), std::move(value));
    }

    std::pair<iterator, bool> insert_or_assign(const Key& key, const Value& value)
    {
      return insert_or_assign_internal(key, value);
    }

    Value& operator[](const Key& key)
    {
      auto [it, _] = insert_internal(key);
      return it->second;
    }

    const_iterator find(const Key& key) const
    {
      if (bucket_count_ == 0) return end();

      const size_t hash = get_hash(key);
      size_t index = get_index(hash);

      while (true) {
        const bucket* slot = &buckets_[index];
        if ((slot->state & STATE_EMPTY) == 0) {
          if (slot->key() == key) {
            return const_iterator(&buckets_, index, bucket_count_);
          }
        } else if (slot->state == STATE_EMPTY) {
          return end();
        }

        index = (index + 1) & index_mask_;
      }
    }

    bool contains(const Key& key) const { return find(key) != end(); }

    iterator find(const Key& key)
    {
      const_iterator const_it = std::as_const(*this).find(key);
      return iterator(&buckets_, const_it.index_, bucket_count_);
    }

    iterator erase(iterator pos)
    {
      bucket& slot = buckets_[pos.index_];

      slot.state = STATE_DELETED;
      slot.slot_.~pair();

      element_count_--;

      ++pos;
      return pos;
    }

    void clear()
    {
      for (bucket& slot: buckets_) {
        if ((slot.state & STATE_EMPTY) == 0) {
          slot.slot_.~pair();
          slot.state = STATE_EMPTY;
        }
      }

      element_count_ = 0;
      occupied_count_ = 0;
    }

    void reserve(const size_t count)
    {
      const size_t min_buckets = (count * 4) / 3;

      if (min_buckets <= bucket_count_) {
        return;
      }

      size_t new_bucket_count = 1;
      while (new_bucket_count < min_buckets) {
        new_bucket_count <<= 1;
      }

      if (new_bucket_count < 16) {
        new_bucket_count = 16;
      }

      allocate(new_bucket_count);
    }

    void swap(flat_hash_map& other) noexcept
    {
      buckets_.swap(other.buckets_);
      std::swap(bucket_count_, other.bucket_count_);
      std::swap(element_count_, other.element_count_);
      std::swap(occupied_count_, other.occupied_count_);
      std::swap(index_mask_, other.index_mask_);
      std::swap(growth_threshold_, other.growth_threshold_);
      std::swap(hasher_, other.hasher_);
    }

    [[nodiscard]] bool empty() const { return element_count_ == 0; }

    [[nodiscard]] size_t size() const { return element_count_; }

    [[nodiscard]] size_t bucket_count() const { return bucket_count_; }

    iterator begin() { return iterator(&buckets_, get_first_valid_index(), bucket_count_); }
    iterator end() { return iterator(&buckets_, bucket_count_, bucket_count_); }

    const_iterator begin() const { return const_iterator(&buckets_, get_first_valid_index(), bucket_count_); }
    const_iterator end() const { return const_iterator(&buckets_, bucket_count_, bucket_count_); }

    const_iterator cbegin() const { return begin(); }
    const_iterator cend() const { return end(); }

  private:
    [[nodiscard]] size_t get_hash(const Key& key) const { return hasher_(key); }

    [[nodiscard]] size_t get_index(const size_t hash) const { return hash & index_mask_; }

    void allocate(const size_t new_bucket_count)
    {
      std::vector<bucket> new_buckets(new_bucket_count);

      const size_t new_index_mask = new_bucket_count - 1;

      for (size_t i = 0; i < bucket_count_; i++) {
        bucket& slot = buckets_[i];
        if ((slot.state & STATE_EMPTY) == 0) {
          const size_t hash = get_hash(slot.key());
          size_t new_index = hash & new_index_mask;
          bucket* new_bucket = &new_buckets[new_index];

          while (new_bucket->state != STATE_EMPTY) {
            new_index = (new_index + 1) & new_index_mask;
            new_bucket = &new_buckets[new_index];
          }

          new (&new_bucket->slot_) value_type(std::move(const_cast<Key&>(slot.key())), std::move(slot.value()));
          new_bucket->state = STATE_OCCUPIED;

          slot.slot_.~pair();
        }
      }

      bucket_count_ = new_bucket_count;
      index_mask_ = bucket_count_ - 1;
      growth_threshold_ = (bucket_count_ * 3) >> 2;
      occupied_count_ = element_count_;

      buckets_.swap(new_buckets);
    }

    std::pair<size_t, bool> insert_lookup(const Key& key)
    {
      if (bucket_count_ == 0) {
        allocate(16);
      }

      if (occupied_count_ >= growth_threshold_) {
        allocate(bucket_count_ * 2);
      }

      const size_t hash = get_hash(key);
      size_t index = get_index(hash);
      std::optional<size_t> first_deleted;

      while (true) {
        bucket* slot = &buckets_[index];

        if ((slot->state & STATE_EMPTY) == 0) {
          if (slot->key() == key) {
            return {index, true};
          }
        } else if (slot->state == STATE_DELETED) {
          if (!first_deleted) {
            first_deleted = index;
          }
        } else { // STATE_EMPTY
          return {first_deleted ? *first_deleted : index, false};
        }
        index = (index + 1) & index_mask_;
      }
    }

    template<typename K, typename... Args>
    iterator construct_at(const size_t index, K&& key, Args&&... args)
    {
      bucket& slot = buckets_[index];
      new (&slot.slot_) value_type(std::piecewise_construct, std::forward_as_tuple(std::forward<K>(key)),
                                   std::forward_as_tuple(std::forward<Args>(args)...));
      slot.state = STATE_OCCUPIED;
      element_count_++;
      occupied_count_++;
      return iterator(&buckets_, index, bucket_count_);
    }

    template<typename K, typename V>
    std::pair<iterator, bool> insert_or_assign_internal(K&& key, V&& value)
    {
      const auto [index, found] = insert_lookup(key);
      if (found) {
        buckets_[index].value() = std::forward<V>(value);
        return {iterator(&buckets_, index, bucket_count_), false};
      }
      return {construct_at(index, std::forward<K>(key), std::forward<V>(value)), true};
    }

    template<typename K, typename... Args>
    std::pair<iterator, bool> insert_internal(K&& key, Args&&... args)
    {
      const auto [index, found] = insert_lookup(key);
      if (found) {
        return {iterator(&buckets_, index, bucket_count_), false};
      }
      return {construct_at(index, std::forward<K>(key), std::forward<Args>(args)...), true};
    }

    [[nodiscard]] size_t get_first_valid_index() const
    {
      if (element_count_ == 0) return bucket_count_;

      size_t index = 0;
      while (index < bucket_count_ && (buckets_[index].state & STATE_EMPTY) != 0) {
        index++;
      }
      return index;
    }
  };
} // namespace fwrk
