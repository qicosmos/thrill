/*******************************************************************************
 * c7a/api/reduce_to_index.hpp
 *
 * DIANode for a reduce operation. Performs the actual reduce operation
 *
 * Part of Project c7a.
 *
 * Copyright (C) 2015 Alexander Noe <aleexnoe@gmail.com>
 *
 * This file has no license. Only Chuck Norris can compile it.
 ******************************************************************************/

#pragma once
#ifndef C7A_API_REDUCE_TO_INDEX_HEADER
#define C7A_API_REDUCE_TO_INDEX_HEADER

#include <c7a/api/dop_node.hpp>
#include <c7a/common/logger.hpp>
#include <c7a/core/reduce_post_table.hpp>
#include <c7a/core/reduce_pre_table.hpp>

#include <cmath>
#include <functional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace c7a {
namespace api {

//! \addtogroup api Interface
//! \{

/*!
 * A DIANode which performs a ReduceToIndex operation. ReduceToIndex groups the elements in a
 * DIA by their key and reduces every key bucket to a single element each. The
 * ReduceToIndexNode stores the key_extractor and the reduce_function UDFs. The
 * chainable LOps ahead of the Reduce operation are stored in the Stack. The
 * ReduceToIndexNode has the type ValueType, which is the result type of the
 * reduce_function. The key type is an unsigned integer and the output DIA will have element
 * with key K at index K.
 *
 * \tparam ParentType Input type of the Reduce operation
 * \tparam ValueType Output type of the Reduce operation
 * \tparam ParentStack Function stack, which contains the chained lambdas between the last and this DIANode.
 * \tparam KeyExtractor Type of the key_extractor function.
 * \tparam ReduceFunction Type of the reduce_function
 */
template <typename ValueType, typename ParentDIARef,
          typename KeyExtractor, typename ReduceFunction,
		  bool PreservesKey>
class ReduceToIndexNode : public DOpNode<ValueType>
{
    static const bool debug = false;

    using Super = DOpNode<ValueType>;

    using Key = typename common::FunctionTraits<KeyExtractor>::result_type;

    static_assert(std::is_same<Key, size_t>::value,
                  "Key must be an unsigned integer");

    using Value = typename common::FunctionTraits<ReduceFunction>::result_type;

    typedef std::pair<Key, Value> KeyValuePair;

    using Super::context_;
    using Super::result_file_;

public:
    using Emitter = data::BlockWriter;
    using PreHashTable = typename c7a::core::ReducePreTable<
              KeyExtractor, ReduceFunction>;

    /*!
     * Constructor for a ReduceToIndexNode. Sets the DataManager, parent, stack,
     * key_extractor and reduce_function.
     *
     * \param parent Parent DIARef.
     * \param key_extractor Key extractor function
     * \param reduce_function Reduce function
     * \param max_index maximum index returned by reduce_function.
     * \param neutral_element Item value with which to start the reduction in
     * each array cell.
     */
    ReduceToIndexNode(const ParentDIARef& parent,
                      KeyExtractor key_extractor,
                      ReduceFunction reduce_function,
                      size_t max_index,
                      Value neutral_element)
        : DOpNode<ValueType>(parent.ctx(), { parent.node() }, "ReduceToIndex"),
          key_extractor_(key_extractor),
          reduce_function_(reduce_function),
          channel_(parent.ctx().data_manager().GetNewChannel()),
          emitters_(channel_->OpenWriters()),
          reduce_pre_table_(parent.ctx().number_worker(), key_extractor,
                            reduce_function_, emitters_,
                            [=](size_t key, PreHashTable* ht) {
                                size_t global_index = key * ht->NumBuckets() /
                                                      (max_index + 1);
                                size_t partition_id = key *
                                                      ht->NumPartitions() / (max_index + 1);
                                size_t partition_offset = global_index -
                                                          partition_id * ht->NumBucketsPerPartition();
                                return typename PreHashTable::
                                hash_result(partition_id,
                                            partition_offset,
                                            global_index);
                            }),
          max_index_(max_index),
          neutral_element_(neutral_element)
    {
        // Hook PreOp
        auto pre_op_fn = [=](Value input) {
                             PreOp(input);
                         };
        // close the function stack with our pre op and register it at parent
        // node for output
        auto lop_chain = parent.stack().push(pre_op_fn).emit();
        parent.node()->RegisterChild(lop_chain);
    }

    //! Virtual destructor for a ReduceToIndexNode.
    virtual ~ReduceToIndexNode() { }

    /*!
     * Actually executes the reduce to index operation. Uses the member functions PreOp,
     * MainOp and PostOp.
     */
    void Execute() override {
        this->StartExecutionTimer();
        MainOp();
        this->StopExecutionTimer();
    }

    void PushData() override {
        // TODO(tb@ms): this is not what should happen: every thing is reduced again:

        using ReduceTable
                  = core::ReducePostTable<KeyExtractor,
                                          ReduceFunction,
                                          true>;

        size_t min_local_index =
            std::ceil(static_cast<double>(max_index_ + 1)
                      * static_cast<double>(context_.rank())
                      / static_cast<double>(context_.number_worker()));
        size_t max_local_index =
            std::ceil(static_cast<double>(max_index_ + 1)
                      * static_cast<double>(context_.rank() + 1)
                      / static_cast<double>(context_.number_worker())) - 1;

        if (context_.rank() == context_.number_worker() - 1) {
            max_local_index = max_index_;
        }
        if (context_.rank() == 0) {
            min_local_index = 0;
        }

        ReduceTable table(key_extractor_, reduce_function_,
                          DIANode<ValueType>::callbacks(),
                          [=](Key key, ReduceTable* ht) {
                              return (key - min_local_index) *
                              (ht->NumBuckets() - 1) /
                              (max_local_index - min_local_index + 1);
                          },
                          min_local_index,
                          max_local_index,
                          neutral_element_);

		if (PreservesKey) {
			//we actually want to wire up callbacks in the ctor and NOT use this blocking method
			auto reader = channel_->OpenReader();
			sLOG << "reading data from" << channel_->id() << "to push into post table which flushes to" << result_file_.ToString();
			while (reader.HasNext()) {
				table.Insert(reader.template Next<Value>());
			}

			table.Flush();
		} else {
			//we actually want to wire up callbacks in the ctor and NOT use this blocking method
			auto reader = channel_->OpenReader();
			sLOG << "reading data from" << channel_->id() << "to push into post table which flushes to" << result_file_.ToString();
			while (reader.HasNext()) {
				table.Insert(reader.template Next<KeyValuePair>());
			}
			table.Flush();
		}
    }

    void Dispose() override { }

    /*!
     * Produces a function stack, which only contains the PostOp function.
     * \return PostOp function stack
     */
    auto ProduceStack() {
        // Hook PostOp
        auto post_op_fn = [=](ValueType elem, auto emit_func) {
                              return this->PostOp(elem, emit_func);
                          };

        return MakeFunctionStack<ValueType>(post_op_fn);
    }

    /*!
     * Returns "[ReduceToIndexNode]" and its id as a string.
     * \return "[ReduceToIndexNode]"
     */
    std::string ToString() override {
        return "[ReduceToIndexNode] Id: " + result_file_.ToString();
    }

private:
    //!Key extractor function
    KeyExtractor key_extractor_;
    //!Reduce function
    ReduceFunction reduce_function_;

    data::ChannelPtr channel_;

    std::vector<data::BlockWriter> emitters_;

    core::ReducePreTable<KeyExtractor, ReduceFunction>
    reduce_pre_table_;

    size_t max_index_;

    Value neutral_element_;

    //! Locally hash elements of the current DIA onto buckets and reduce each
    //! bucket to a single value, afterwards send data to another worker given
    //! by the shuffle algorithm.
    void PreOp(Value input) {
        reduce_pre_table_.Insert(std::move(input));
    }

    //!Receive elements from other workers.
    auto MainOp() {
        LOG << ToString() << " running main op";
        //Flush hash table before the postOp
        reduce_pre_table_.Flush();
        reduce_pre_table_.CloseEmitter();
    }

    //! Hash recieved elements onto buckets and reduce each bucket to a single value.
    template <typename Emitter>
    void PostOp(ValueType input, Emitter emit_func) {
        emit_func(input);
    }
};

template <typename ValueType, typename Stack>
template <typename KeyExtractor, typename ReduceFunction>
auto DIARef<ValueType, Stack>::ReduceToIndexByKey(
    const KeyExtractor &key_extractor,
    const ReduceFunction &reduce_function,
    size_t max_index,
    ValueType neutral_element) const {

    using DOpResult
              = typename common::FunctionTraits<ReduceFunction>::result_type;

    static_assert(
        std::is_convertible<
            ValueType,
            typename common::FunctionTraits<ReduceFunction>::template arg<0>
            >::value,
        "ReduceFunction has the wrong input type");

    static_assert(
        std::is_convertible<
            ValueType,
            typename common::FunctionTraits<ReduceFunction>::template arg<1>
            >::value,
        "ReduceFunction has the wrong input type");

    static_assert(
        std::is_same<
            DOpResult,
            ValueType>::value,
        "ReduceFunction has the wrong output type");

    static_assert(
        std::is_same<
            typename std::decay<typename common::FunctionTraits<KeyExtractor>::template arg<0> >::type,
            ValueType>::value,
        "KeyExtractor has the wrong input type");

    static_assert(
        std::is_same<
            typename common::FunctionTraits<KeyExtractor>::result_type,
            size_t>::value,
        "The key has to be an unsigned long int (aka. size_t).");

	using ReduceResultNode
		= ReduceToIndexNode<DOpResult, DIARef,
							KeyExtractor, ReduceFunction, false>;

	auto shared_node
		= std::make_shared<ReduceResultNode>(*this,
											 key_extractor,
											 reduce_function,
											 max_index,
											 neutral_element);

	auto reduce_stack = shared_node->ProduceStack();

	return DIARef<DOpResult, decltype(reduce_stack)> (
		shared_node, 
		reduce_stack,
		{ AddChildStatsNode("ReduceToIndex", "DOp") });
}

template <typename ValueType, typename Stack>
template <typename KeyExtractor, typename ReduceFunction>
auto DIARef<ValueType, Stack>::ReduceToIndex(
    const KeyExtractor &key_extractor,
    const ReduceFunction &reduce_function,
    size_t max_index,
    ValueType neutral_element) const {

    using DOpResult
              = typename common::FunctionTraits<ReduceFunction>::result_type;

    static_assert(
        std::is_convertible<
            ValueType,
            typename common::FunctionTraits<ReduceFunction>::template arg<0>
            >::value,
        "ReduceFunction has the wrong input type");

    static_assert(
        std::is_convertible<
            ValueType,
            typename common::FunctionTraits<ReduceFunction>::template arg<1>
            >::value,
        "ReduceFunction has the wrong input type");

    static_assert(
        std::is_same<
            DOpResult,
            ValueType>::value,
        "ReduceFunction has the wrong output type");

    static_assert(
        std::is_same<
            typename std::decay<typename common::FunctionTraits<KeyExtractor>::template arg<0> >::type,
            ValueType>::value,
        "KeyExtractor has the wrong input type");

    static_assert(
        std::is_same<
            typename common::FunctionTraits<KeyExtractor>::result_type,
            size_t>::value,
        "The key has to be an unsigned long int (aka. size_t).");

	using ReduceResultNode
		= ReduceToIndexNode<DOpResult, DIARef,
							KeyExtractor, ReduceFunction, true>;

	auto shared_node
		= std::make_shared<ReduceResultNode>(*this,
											 key_extractor,
											 reduce_function,
											 max_index,
											 neutral_element);

	auto reduce_stack = shared_node->ProduceStack();

	return DIARef<DOpResult, decltype(reduce_stack)> (
		shared_node, 
		reduce_stack,
		{ AddChildStatsNode("ReduceToIndex", "DOp") });
}

//! \}

} // namespace api
} // namespace c7a

#endif // !C7A_API_REDUCE_TO_INDEX_HEADER

/******************************************************************************/