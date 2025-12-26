#pragma once

#include <atomic>
#include <unordered_map>
namespace Bess {
    // USING FROM:
    // https://gpfault.net/posts/mapping-types-to-values.txt.html

    template <class ValueType>
    class TypeMap {
        // Internally, we'll use a hash table to store mapping from type
        // IDs to the values.
        typedef std::unordered_map<int, ValueType> InternalMap;

      public:
        typedef typename InternalMap::iterator iterator;
        typedef typename InternalMap::const_iterator const_iterator;
        typedef typename InternalMap::value_type value_type;

        const_iterator begin() const { return m_map.begin(); }
        const_iterator end() const { return m_map.end(); }
        iterator begin() { return m_map.begin(); }
        iterator end() { return m_map.end(); }

        // Finds the value associated with the type "Key" in the type map.
        template <class Key>
        iterator find() { return m_map.find(getTypeId<Key>()); }

        // Same as above, const version
        template <class Key>
        const_iterator find() const { return m_map.find(getTypeId<Key>()); }

        // Associates a value with the type "Key"
        template <class Key>
        void put(ValueType &&value) {
            m_map[getTypeId<Key>()] = std::forward<ValueType>(value);
        }

      private:
        template <class Key>
        inline static int getTypeId() {
            static const int id = LastTypeId++;
            return id;
        }

        static std::atomic_int LastTypeId;
        InternalMap m_map;
    };

} // namespace Bess
