/*******************************************************************************
 * thrill/api/prefixsum.hpp
 *
 * Part of Project Thrill - http://project-thrill.org
 *
 * Copyright (C) 2015 Alexander Noe <aleexnoe@gmail.com>
 * Copyright (C) 2015 Timo Bingmann <tb@panthema.net>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#pragma once
#ifndef THRILL_API_PREFIXSUM_HEADER
#define THRILL_API_PREFIXSUM_HEADER

#include <thrill/api/dia.hpp>
#include <thrill/api/dop_node.hpp>
#include <thrill/common/logger.hpp>
#include <thrill/data/file.hpp>

namespace thrill {
namespace api {

/*!
 * \ingroup api_layer
 */
template <typename ValueType, typename ParentDIA, typename SumFunction>
class PrefixSumNode final : public DOpNode<ValueType>
{
    static constexpr bool debug = false;

    using Super = DOpNode<ValueType>;
    using Super::context_;

public:
    PrefixSumNode(const ParentDIA& parent,
                  const SumFunction& sum_function,
                  const ValueType& initial_element)
        : Super(parent.ctx(), "PrefixSum", { parent.id() }, { parent.node() }),
          sum_function_(sum_function),
          local_sum_(initial_element),
          initial_element_(initial_element)
    {
        // Hook PreOp(s)
        auto pre_op_fn = [this](const ValueType& input) {
                             PreOp(input);
                         };

        auto lop_chain = parent.stack().push(pre_op_fn).fold();
        parent.node()->AddChild(this, lop_chain);
    }

    //! PreOp: compute local prefixsum and store items.
    void PreOp(const ValueType& input) {
        LOG << "Input: " << input;
        local_sum_ = sum_function_(local_sum_, input);
        writer_.Put(input);
    }

    bool OnPreOpFile(const data::File& file, size_t /* parent_index */) final {
        if (!ParentDIA::stack_empty) return false;
        // copy complete Block references to writer_
        file_ = file.Copy();
        // read File for prefix sum.
        auto reader = file_.GetKeepReader();
        while (reader.HasNext()) {
            local_sum_ = sum_function_(
                local_sum_, reader.template Next<ValueType>());
        }
        return true;
    }

    void StopPreOp(size_t /* id */) final {
        writer_.Close();
    }

    //! Executes the prefixsum operation.
    void Execute() final {
        LOG << "MainOp processing";

        ValueType sum = context_.net.ExPrefixSum(
            local_sum_, initial_element_, sum_function_);

        if (context_.my_rank() == 0) {
            sum = initial_element_;
        }

        local_sum_ = sum;
    }

    void PushData(bool consume) final {
        data::File::Reader reader = file_.GetReader(consume);

        ValueType sum = local_sum_;

        for (size_t i = 0; i < file_.num_items(); ++i) {
            sum = sum_function_(sum, reader.Next<ValueType>());
            this->PushItem(sum);
        }
    }

    void Dispose() final {
        file_.Clear();
    }

private:
    //! The sum function which is applied to two elements.
    SumFunction sum_function_;
    //! Local sum to be used in all reduce operation.
    ValueType local_sum_;
    //! Initial element.
    ValueType initial_element_;

    //! Local data file
    data::File file_ { context_.GetFile(this) };
    //! Data writer to local file (only active in PreOp).
    data::File::Writer writer_ { file_.GetWriter() };
};

template <typename ValueType, typename Stack>
template <typename SumFunction>
auto DIA<ValueType, Stack>::PrefixSum(
    const SumFunction &sum_function, const ValueType &initial_element) const {
    assert(IsValid());

    using PrefixSumNode
              = api::PrefixSumNode<ValueType, DIA, SumFunction>;

    static_assert(
        std::is_convertible<
            ValueType,
            typename FunctionTraits<SumFunction>::template arg<0>
            >::value,
        "SumFunction has the wrong input type");

    static_assert(
        std::is_convertible<
            ValueType,
            typename FunctionTraits<SumFunction>::template arg<1> >::value,
        "SumFunction has the wrong input type");

    static_assert(
        std::is_convertible<
            typename FunctionTraits<SumFunction>::result_type,
            ValueType>::value,
        "SumFunction has the wrong input type");

    auto node = common::MakeCounting<PrefixSumNode>(
        *this, sum_function, initial_element);

    return DIA<ValueType>(node);
}

} // namespace api
} // namespace thrill

#endif // !THRILL_API_PREFIXSUM_HEADER

/******************************************************************************/
