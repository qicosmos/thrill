/*******************************************************************************
 * thrill/core/losertree.hpp
 *
 * Many generic loser tree variants.
 *
 * Extracted from MCSTL
 * http://algo2.iti.uni-karlsruhe.de/singler/mcstl/
 * Copyright (C) 2007 Johannes Singler <singler@ira.uka.de>
 *
 * copied and modified from STXXL
 * https://github.com/stxxl/stxxl
 *
 * which are both distributed under the Boost Software License, Version 1.0.
 * (See http://www.boost.org/LICENSE_1_0.txt)
 *
 * Part of Project Thrill - http://project-thrill.org
 *
 * Copyright (C) 2014-2015 Timo Bingmann <tb@panthema.net>
 * Copyright (C) 2015 Huyen Chau Nguyen <hello@chau-nguyen.de>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#pragma once
#ifndef THRILL_CORE_LOSERTREE_HEADER
#define THRILL_CORE_LOSERTREE_HEADER

#include <thrill/common/defines.hpp>
#include <thrill/common/math.hpp>

#include <algorithm>
#include <functional>
#include <ostream>

namespace thrill {
namespace core {

/*!
 * Guarded loser tree/tournament tree, either copying the whole element into
 * the tree structure, or looking up the element via the index.
 *
 * This is a base class for the LoserTreeCopy\<true> and \<false> classes.
 *
 * Guarding is done explicitly through one flag sup per element, inf is not
 * needed due to a better initialization routine.  This is a well-performing
 * variant.
 *
 * \tparam ValueType the element type
 * \tparam Comparator comparator to use for binary comparisons.
 */
template <typename ValueType, typename Comparator = std::less<ValueType> >
class LoserTreeCopyBase
{
public:
    //! size of counters and array indexes
    using size_type = unsigned int;
    //! type of the source field
    using Source = int;

protected:
    //! Internal representation of a loser tree player/node
    struct Loser
    {
        //! flag, true iff is a virtual maximum sentinel
        bool      sup;
        //! index of source
        Source    source;
        //! copy of key value of the element in this node
        ValueType key;
    };

    //! number of nodes
    const size_type ik_;
    //! log_2(ik) next greater power of 2
    const size_type k_;
    //! array containing loser tree nodes
    Loser* losers_;
    //! the comparator object
    Comparator cmp_;
    //! still have to construct keys
    bool first_insert_;

public:
    LoserTreeCopyBase(size_type k,
                      const Comparator& cmp = std::less<ValueType>())
        : ik_(k),
          k_(common::RoundUpToPowerOfTwo(ik_)),
          cmp_(cmp),
          first_insert_(true) {
        // avoid default-constructing losers[].key
        losers_ = static_cast<Loser*>(operator new (2 * k_ * sizeof(Loser)));

        for (size_type i = ik_ - 1; i < k_; ++i)
        {
            losers_[i + k_].sup = true;
            losers_[i + k_].source = (Source)(-1);
        }
    }

    LoserTreeCopyBase(const LoserTreeCopyBase&) = delete;
    LoserTreeCopyBase& operator = (const LoserTreeCopyBase&) = delete;
    LoserTreeCopyBase(LoserTreeCopyBase&&) = default;
    LoserTreeCopyBase& operator = (LoserTreeCopyBase&&) = default;

    ~LoserTreeCopyBase() {
        for (size_type i = 0; i < (2 * k_); ++i)
            losers_[i].~Loser();

        operator delete (losers_);
    }

    void print(std::ostream& os) {
        for (size_type i = 0; i < (k_ * 2); i++)
            os << i << "    " << losers_[i].key
               << " from " << losers_[i].source << ",  " << losers_[i].sup
               << "\n";
    }

    //! return the index of the player with the smallest element.
    Source min_source() {
        return losers_[0].source;
    }

    /*!
     * Initializes the player source with the element key.
     *
     * \param keyp the element to insert
     * \param source index of the player
     * \param sup flag that determines whether the value to insert is an
     *   explicit supremum sentinel.
     */
    void insert_start(const ValueType* keyp, Source source, bool sup) {
        size_type pos = k_ + source;
        assert(sup == (keyp == nullptr));

        losers_[pos].sup = sup;
        losers_[pos].source = source;

        if (THRILL_UNLIKELY(first_insert_))
        {
            // copy construct all keys from this first key
            for (size_type i = 0; i < 2 * k_; ++i) {
                if (keyp)
                    new (&(losers_[i].key))ValueType(*keyp);
                else
                    losers_[pos].key = ValueType();
            }
            first_insert_ = false;
        }
        else {
            losers_[pos].key = keyp ? *keyp : ValueType();
        }
    }

    /*!
     * Computes the winner of the competition at player root.  Called
     * recursively (starting at 0) to build the initial tree.
     *
     * \param root index of the game to start.
     */
    size_type init_winner(size_type root) {
        if (root >= k_)
            return root;

        size_type left = init_winner(2 * root);
        size_type right = init_winner(2 * root + 1);
        if (losers_[right].sup ||
            (!losers_[left].sup && !cmp_(losers_[right].key, losers_[left].key)))
        {
            // left one is less or equal
            losers_[root] = losers_[right];
            return left;
        }
        else
        {
            // right one is less
            losers_[root] = losers_[left];
            return right;
        }
    }

    void init() {
        losers_[0] = losers_[init_winner(1)];
    }
};

/*!
 * Guarded loser tree/tournament tree, either copying the whole element into
 * the tree structure, or looking up the element via the index.
 *
 * Unstable specialization of LoserTreeCopyBase.
 *
 * Guarding is done explicitly through one flag sup per element, inf is not
 * needed due to a better initialization routine.  This is a well-performing
 * variant.
 *
 * \tparam ValueType the element type
 * \tparam Comparator comparator to use for binary comparisons.
 */
template <bool Stable /* == false */,
          typename ValueType, typename Comparator = std::less<ValueType> >
class LoserTreeCopy : public LoserTreeCopyBase<ValueType, Comparator>
{
public:
    using Super = LoserTreeCopyBase<ValueType, Comparator>;

    using size_type = typename Super::size_type;
    using Source = typename Super::Source;
    using Super::k_;
    using Super::losers_;
    using Super::cmp_;

    explicit LoserTreeCopy(size_type k,
                           const Comparator& cmp = std::less<ValueType>())
        : Super(k, cmp) { }

    // do not pass const reference since key will be used as local variable
    void delete_min_insert(const ValueType* keyp, bool sup) {
        using std::swap;
        assert(sup == (keyp == nullptr));

        Source source = losers_[0].source;
        ValueType key = keyp ? *keyp : ValueType();
        for (size_type pos = (k_ + source) / 2; pos > 0; pos /= 2)
        {
            // the smaller one gets promoted
            if (sup ||
                (!losers_[pos].sup && cmp_(losers_[pos].key, key)))
            {
                // the other one is smaller
                swap(losers_[pos].sup, sup);
                swap(losers_[pos].source, source);
                swap(losers_[pos].key, key);
            }
        }

        losers_[0].sup = sup;
        losers_[0].source = source;
        losers_[0].key = key;
    }
};

/*!
 * Guarded loser tree/tournament tree, either copying the whole element into
 * the tree structure, or looking up the element via the index.
 *
 * Stable specialization of LoserTreeCopyBase.
 *
 * Guarding is done explicitly through one flag sup per element, inf is not
 * needed due to a better initialization routine.  This is a well-performing
 * variant.
 *
 * \tparam ValueType the element type
 * \tparam Comparator comparator to use for binary comparisons.
 */
template <typename ValueType, typename Comparator>
class LoserTreeCopy</* Stable == */ true, ValueType, Comparator>
    : public LoserTreeCopyBase<ValueType, Comparator>
{
public:
    using Super = LoserTreeCopyBase<ValueType, Comparator>;

    using size_type = typename Super::size_type;
    using Source = typename Super::Source;
    using Super::k_;
    using Super::losers_;
    using Super::cmp_;

    explicit LoserTreeCopy(size_type k,
                           const Comparator& cmp = std::less<ValueType>())
        : Super(k, cmp)
    { }

    // do not pass const reference since key will be used as local variable
    void delete_min_insert(const ValueType* keyp, bool sup) {
        using std::swap;
        assert(sup == (keyp == nullptr));

        Source source = losers_[0].source;
        ValueType key = keyp ? *keyp : ValueType();
        for (size_type pos = (k_ + source) / 2; pos > 0; pos /= 2)
        {
            if ((sup && (!losers_[pos].sup || losers_[pos].source < source)) ||
                (!sup && !losers_[pos].sup &&
                 ((cmp_(losers_[pos].key, key)) ||
                  (!cmp_(key, losers_[pos].key) && losers_[pos].source < source))))
            {
                // the other one is smaller
                swap(losers_[pos].sup, sup);
                swap(losers_[pos].source, source);
                swap(losers_[pos].key, key);
            }
        }

        losers_[0].sup = sup;
        losers_[0].source = source;
        losers_[0].key = key;
    }
};

/*!
 * Guarded loser tree, using pointers to the elements instead of copying them
 * into the tree nodes.
 *
 * This is a base class for the LoserTreePointer\<true> and \<false> classes.
 *
 * Guarding is done explicitly through one flag sup per element, inf is not
 * needed due to a better initialization routine.  This is a well-performing
 * variant.
 */
template <typename ValueType, typename Comparator = std::less<ValueType> >
class LoserTreePointerBase
{
protected:
    //! size of counters and array indexes
    using size_type = typename LoserTreeCopyBase<ValueType, Comparator>
                      ::size_type;
    //! type of the source field
    using Source = typename LoserTreeCopyBase<ValueType, Comparator>::Source;

protected:
    //! Internal representation of a loser tree player/node
    struct Loser
    {
        //! index of source
        Source         source;
        //! pointer to key value of the element in this node
        const ValueType* keyp;
    };

    //! number of nodes
    const size_type ik_;
    //! log_2(ik) next greater power of 2
    const size_type k_;
    //! array containing loser tree nodes
    Loser* losers_;
    //! the comparator object
    Comparator cmp_;

public:
    LoserTreePointerBase(size_type k,
                         const Comparator& cmp = std::less<ValueType>())
        : ik_(k),
          k_(common::RoundUpToPowerOfTwo(ik_)),
          losers_(new Loser[k_ * 2]),
          cmp_(cmp) {
        for (size_type i = ik_ - 1; i < k_; i++)
        {
            losers_[i + k_].keyp = nullptr;
            losers_[i + k_].source = (Source)(-1);
        }
    }

    LoserTreePointerBase(const LoserTreePointerBase&) = delete;
    LoserTreePointerBase& operator = (const LoserTreePointerBase&) = delete;
    LoserTreePointerBase(LoserTreePointerBase&&) = default;
    LoserTreePointerBase& operator = (LoserTreePointerBase&&) = default;

    ~LoserTreePointerBase() {
        delete[] losers_;
    }

    void print(std::ostream& os) {
        for (size_type i = 0; i < (k_ * 2); i++)
            os << i << "    " << losers_[i].keyp
               << " from " << losers_[i].source << ",  " << losers_[i].keyp
               << "\n";
    }

    //! return the index of the player with the smallest element.
    Source min_source() {
        return losers_[0].source;
    }

    /*!
     * Initializes the player source with the element key.
     *
     * \param keyp the element to insert
     * \param source index of the player
     * \param sup flag that determines whether the value to insert is an
     *   explicit supremum sentinel.
     */
    void insert_start(const ValueType* keyp, Source source, bool sup) {
        size_type pos = k_ + source;

        assert(sup == (keyp == nullptr));
        losers_[pos].source = source;
        losers_[pos].keyp = keyp;
        common::UNUSED(sup);
    }

    /*!
     * Computes the winner of the competition at player root.  Called
     * recursively (starting at 0) to build the initial tree.
     *
     * \param root index of the game to start.
     */
    size_type init_winner(size_type root) {
        if (root >= k_)
            return root;

        size_type left = init_winner(2 * root);
        size_type right = init_winner(2 * root + 1);
        if (!losers_[right].keyp ||
            (losers_[left].keyp && !cmp_(*losers_[right].keyp, *losers_[left].keyp)))
        {
            // left one is less or equal
            losers_[root] = losers_[right];
            return left;
        }
        else
        {
            // right one is less
            losers_[root] = losers_[left];
            return right;
        }
    }

    void init() {
        losers_[0] = losers_[init_winner(1)];
    }
};

/*!
 * Guarded loser tree, using pointers to the elements instead of copying them
 * into the tree nodes.
 *
 * Unstable specialization of LoserTreeCopyBase.
 *
 * Guarding is done explicitly through one flag sup per element, inf is not
 * needed due to a better initialization routine.  This is a well-performing
 * variant.
 */
template <bool Stable /* == false */,
          typename ValueType, typename Comparator = std::less<ValueType> >
class LoserTreePointer : public LoserTreePointerBase<ValueType, Comparator>
{
public:
    using Super = LoserTreePointerBase<ValueType, Comparator>;

    using size_type = typename Super::size_type;
    using Source = typename Super::Source;
    using Super::k_;
    using Super::losers_;
    using Super::cmp_;

    explicit LoserTreePointer(size_type k,
                              const Comparator& cmp = std::less<ValueType>())
        : Super(k, cmp)
    { }

    void delete_min_insert(const ValueType* keyp, bool sup) {
        using std::swap;
        assert(sup == (keyp == nullptr));

        Source source = losers_[0].source;
        for (size_type pos = (k_ + source) / 2; pos > 0; pos /= 2)
        {
            // the smaller one gets promoted
            if (!keyp ||
                (losers_[pos].keyp && cmp_(*losers_[pos].keyp, *keyp)))
            {                   //the other one is smaller
                swap(losers_[pos].source, source);
                swap(losers_[pos].keyp, keyp);
            }
        }

        losers_[0].source = source;
        losers_[0].keyp = keyp;
        common::UNUSED(sup);
    }
};

/*!
 * Guarded loser tree, using pointers to the elements instead of copying them
 * into the tree nodes.
 *
 * Unstable specialization of LoserTreeCopyBase.
 *
 * Guarding is done explicitly through one flag sup per element, inf is not
 * needed due to a better initialization routine.  This is a well-performing
 * variant.
 */
template <typename ValueType, typename Comparator>
class LoserTreePointer</* Stable == */ true, ValueType, Comparator>
    : public LoserTreePointerBase<ValueType, Comparator>
{
public:
    using Super = LoserTreePointerBase<ValueType, Comparator>;

    using size_type = typename Super::size_type;
    using Source = typename Super::Source;
    using Super::k_;
    using Super::losers_;
    using Super::cmp_;

    explicit LoserTreePointer(size_type k,
                              const Comparator& cmp = std::less<ValueType>())
        : Super(k, cmp)
    { }

    void delete_min_insert(const ValueType* keyp, bool sup) {
        using std::swap;
        assert(sup == (keyp == nullptr));

        Source source = losers_[0].source;
        for (size_type pos = (k_ + source) / 2; pos > 0; pos /= 2)
        {
            // the smaller one gets promoted, ties are broken by source
            if ((!keyp && (losers_[pos].keyp || losers_[pos].source < source)) ||
                (keyp && losers_[pos].keyp &&
                 ((cmp_(*losers_[pos].keyp, *keyp)) ||
                  (!cmp_(*keyp, *losers_[pos].keyp) && losers_[pos].source < source))))
            {
                // the other one is smaller
                swap(losers_[pos].source, source);
                swap(losers_[pos].keyp, keyp);
            }
        }

        losers_[0].source = source;
        losers_[0].keyp = keyp;
        common::UNUSED(sup);
    }
};

/*!
 * Unguarded loser tree, copying the whole element into the tree structure.
 *
 * This is a base class for the LoserTreeCopyUnguarded\<true> and \<false>
 * classes.
 *
 * No guarding is done, therefore not a single input sequence must run empty.
 * This is a very fast variant.
 */
template <typename ValueType, typename Comparator = std::less<ValueType> >
class LoserTreeCopyUnguardedBase
{
protected:
    //! Internal representation of a loser tree player/node
    struct Loser
    {
        //! index of source
        int       source;
        //! copy of key value of the element in this node
        ValueType key;
    };

    //! number of nodes
    unsigned int ik_;
    //! log_2(ik) next greater power of 2
    unsigned int k_;
    //! array containing loser tree nodes
    Loser* losers_;
    //! the comparator object
    Comparator cmp_;

public:
    LoserTreeCopyUnguardedBase(unsigned int k, const ValueType& sentinel,
                               const Comparator& cmp = std::less<ValueType>())
        : ik_(k),
          k_(common::RoundUpToPowerOfTwo(ik_)),
          losers_(new Loser[k_ * 2]),
          cmp_(cmp) {
        for (unsigned int i = 0; i < 2 * k_; i++)
        {
            losers_[i].source = -1;
            losers_[i].key = sentinel;
        }
    }

    ~LoserTreeCopyUnguardedBase() {
        delete[] losers_;
    }

    // non construction-copyable
    LoserTreeCopyUnguardedBase(const LoserTreeCopyUnguardedBase& other) = delete;
    // non copyable
    LoserTreeCopyUnguardedBase& operator = (const LoserTreeCopyUnguardedBase&) = delete;

    void print(std::ostream& os) {
        for (unsigned int i = 0; i < k_ + ik_; i++)
            os << i << "    " << losers_[i].key << " from " << losers_[i].source << "\n";
    }

    //! return the index of the player with the smallest element.
    int min_source() {
        assert(losers_[0].source != -1 && "Data underrun in unguarded merging.");
        return losers_[0].source;
    }

    void insert_start(const ValueType& key, int source) {
        unsigned int pos = k_ + source;

        losers_[pos].source = source;
        losers_[pos].key = key;
    }

    unsigned int init_winner(unsigned int root) {
        if (root >= k_)
            return root;

        unsigned int left = init_winner(2 * root);
        unsigned int right = init_winner(2 * root + 1);
        if (!cmp_(losers_[right].key, losers_[left].key))
        {                       //left one is less or equal
            losers_[root] = losers_[right];
            return left;
        }
        else
        {                       //right one is less
            losers_[root] = losers_[left];
            return right;
        }
    }

    void init() {
        losers_[0] = losers_[init_winner(1)];
    }
};

template <bool Stable /* == false */,
          typename ValueType, typename Comparator = std::less<ValueType> >
class LoserTreeCopyUnguarded : public LoserTreeCopyUnguardedBase<ValueType, Comparator>
{
private:
    using Super = LoserTreeCopyUnguardedBase<ValueType, Comparator>;

    using Super::k_;
    using Super::losers_;
    using Super::cmp_;

public:
    LoserTreeCopyUnguarded(unsigned int k, const ValueType& sentinel,
                           const Comparator& cmp = std::less<ValueType>())
        : Super(k, sentinel, cmp)
    { }

    // do not pass const reference since key will be used as local variable
    void delete_min_insert(ValueType key) {
        using std::swap;

        int source = losers_[0].source;
        for (unsigned int pos = (k_ + source) / 2; pos > 0; pos /= 2)
        {
            // the smaller one gets promoted
            if (cmp_(losers_[pos].key, key))
            {
                // the other one is smaller
                swap(losers_[pos].source, source);
                swap(losers_[pos].key, key);
            }
        }

        losers_[0].source = source;
        losers_[0].key = key;
    }
};

template <typename ValueType, typename Comparator>
class LoserTreeCopyUnguarded</* Stable == */ true, ValueType, Comparator>
    : public LoserTreeCopyUnguardedBase<ValueType, Comparator>
{
private:
    using Super = LoserTreeCopyUnguardedBase<ValueType, Comparator>;

    using Super::k_;
    using Super::losers_;
    using Super::cmp_;

public:
    LoserTreeCopyUnguarded(unsigned int k, const ValueType& sentinel,
                           const Comparator& comp = std::less<ValueType>())
        : Super(k, sentinel, comp)
    { }

    // do not pass const reference since key will be used as local variable
    void delete_min_insert(ValueType key) {
        using std::swap;

        int source = losers_[0].source;
        for (unsigned int pos = (k_ + source) / 2; pos > 0; pos /= 2)
        {
            if (!cmp_(key, losers_[pos].key) && losers_[pos].source < source)
            {
                // the other one is smaller
                swap(losers_[pos].source, source);
                swap(losers_[pos].key, key);
            }
        }

        losers_[0].source = source;
        losers_[0].key = key;
    }
};

/*!
 * Unguarded loser tree, keeping only pointers to the elements in the tree
 * structure.
 *
 * This is a base class for the LoserTreePointerUnguarded\<true> and \<false>
 * classes.
 *
 * No guarding is done, therefore not a single input sequence must run empty.
 * This is a very fast variant.
 */
template <typename ValueType, typename Comparator = std::less<ValueType> >
class LoserTreePointerUnguardedBase
{
protected:
    //! Internal representation of a loser tree player/node
    struct Loser
    {
        //! index of source
        int            source;
        //! copy of key value of the element in this node
        const ValueType* keyp;
    };

    //! number of nodes
    unsigned int ik_;
    //! log_2(ik) next greater power of 2
    unsigned int k_;
    //! array containing loser tree nodes
    Loser* losers_;
    //! the comparator object
    Comparator cmp_;

protected:
    LoserTreePointerUnguardedBase(
        unsigned int k, const ValueType& sentinel,
        const Comparator& cmp = std::less<ValueType>())
        : ik_(k),
          k_(common::RoundUpToPowerOfTwo(ik_)),
          losers_(new Loser[k_ * 2]),
          cmp_(cmp) {
        for (unsigned int i = ik_ - 1; i < k_; i++)
        {
            losers_[i + k_].source = -1;
            losers_[i + k_].keyp = &sentinel;
        }
    }

    ~LoserTreePointerUnguardedBase() {
        delete[] losers_;
    }

    // non construction-copyable
    LoserTreePointerUnguardedBase(const LoserTreePointerUnguardedBase& other) = delete;
    // non copyable
    LoserTreePointerUnguardedBase& operator = (const LoserTreePointerUnguardedBase&) = delete;

    void print(std::ostream& os) {
        for (unsigned int i = 0; i < k_ + ik_; i++)
            os << i << "    " << *losers_[i].keyp << " from " << losers_[i].source << "\n";
    }

    int min_source() {
        return losers_[0].source;
    }

    void insert_start(const ValueType& key, int source) {
        unsigned int pos = k_ + source;

        losers_[pos].source = source;
        losers_[pos].keyp = &key;
    }

    unsigned int init_winner(unsigned int root) {
        if (root >= k_)
            return root;

        unsigned int left = init_winner(2 * root);
        unsigned int right = init_winner(2 * root + 1);
        if (!cmp_(*losers_[right].keyp, *losers_[left].keyp))
        {
            // left one is less or equal
            losers_[root] = losers_[right];
            return left;
        }
        else
        {
            // right one is less
            losers_[root] = losers_[left];
            return right;
        }
    }

    void init() {
        losers_[0] = losers_[init_winner(1)];
    }
};

template <bool Stable /* == false */,
          typename ValueType, typename Comparator = std::less<ValueType> >
class LoserTreePointerUnguarded
    : public LoserTreePointerUnguardedBase<ValueType, Comparator>
{
private:
    using Super = LoserTreePointerUnguardedBase<ValueType, Comparator>;

    using Super::k_;
    using Super::losers_;
    using Super::cmp_;

public:
    LoserTreePointerUnguarded(unsigned int k, const ValueType& sentinel,
                              const Comparator& cmp = std::less<ValueType>())
        : Super(k, sentinel, cmp)
    { }

    void delete_min_insert(const ValueType& key) {
        using std::swap;

        const ValueType* keyp = &key;
        int source = losers_[0].source;
        for (unsigned int pos = (k_ + source) / 2; pos > 0; pos /= 2)
        {
            // the smaller one gets promoted
            if (cmp_(*losers_[pos].keyp, *keyp))
            {
                // the other one is smaller
                swap(losers_[pos].source, source);
                swap(losers_[pos].keyp, keyp);
            }
        }

        losers_[0].source = source;
        losers_[0].keyp = keyp;
    }
};

template <typename ValueType, typename Comparator>
class LoserTreePointerUnguarded</* Stable == */ true, ValueType, Comparator>
    : public LoserTreePointerUnguardedBase<ValueType, Comparator>
{
private:
    using Super = LoserTreePointerUnguardedBase<ValueType, Comparator>;

    using Super::k_;
    using Super::losers_;
    using Super::cmp_;

public:
    LoserTreePointerUnguarded(unsigned int k, const ValueType& sentinel,
                              const Comparator& cmp = std::less<ValueType>())
        : Super(k, sentinel, cmp)
    { }

    void delete_min_insert(const ValueType& key) {
        using std::swap;

        const ValueType* keyp = &key;
        int source = losers_[0].source;
        for (unsigned int pos = (k_ + source) / 2; pos > 0; pos /= 2)
        {
            // the smaller one gets promoted, ties are broken by source
            if (cmp_(*losers_[pos].keyp, *keyp) ||
                (!cmp_(*keyp, *losers_[pos].keyp) && losers_[pos].source < source))
            {
                // the other one is smaller
                swap(losers_[pos].source, source);
                swap(losers_[pos].keyp, keyp);
            }
        }

        losers_[0].source = source;
        losers_[0].keyp = keyp;
    }
};

/******************************************************************************/
// LoserTreeTraits selects loser tree by size of value type

template <bool Stable, typename ValueType, typename Comparator,
          typename Enable = void>
struct LoserTreeTraits
{
public:
    using Type = LoserTreePointer<Stable, ValueType, Comparator>;
};

template <bool Stable, typename ValueType, typename Comparator>
struct LoserTreeTraits<
    Stable, ValueType, Comparator,
    typename std::enable_if<sizeof(ValueType) <= 2* sizeof(size_t)>::type>
{
    using Type = LoserTreeCopy<Stable, ValueType, Comparator>;
};

template <bool Stable, typename ValueType, typename Comparator,
          typename Enable = void>
class LoserTreeTraitsUnguarded
{
public:
    using Type = LoserTreePointerUnguarded<Stable, ValueType, Comparator>;
};

template <bool Stable, typename ValueType, typename Comparator>
struct LoserTreeTraitsUnguarded<
    Stable, ValueType, Comparator,
    typename std::enable_if<sizeof(ValueType) <= 2* sizeof(size_t)>::type>
{
    using Type = LoserTreeCopyUnguarded<Stable, ValueType, Comparator>;
};

} // namespace core
} // namespace thrill

#endif // !THRILL_CORE_LOSERTREE_HEADER

/******************************************************************************/
