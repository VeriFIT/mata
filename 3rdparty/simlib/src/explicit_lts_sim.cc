/*****************************************************************************
 *  Simlib
 *
 *  Copyright (c) 2011  Jiri Simacek <isimacek@fit.vutbr.cz>
 *
 *  Description:
 *    Source for explicit LTS simulation algorithm.
 *
 *****************************************************************************/

// Standard library headers
#include <cmath>
#include <ostream>
#include <vector>
#include <algorithm>
#include <memory>


// simlib headers
#include <mata/simlib/explicit_lts.hh>
#include <mata/simlib/util/binary_relation.hh>
#include <mata/simlib/util/convert.hh>
#include <mata/simlib/util/smart_set.hh>
#include <mata/simlib/util/simlib.hh>

#include <mata/simlib/util/caching_allocator.hh>
#include <mata/simlib/util/shared_counter.hh>
#include <mata/simlib/util/shared_list.hh>
#include <mata/simlib/util/splitting_relation.hh>


using Simlib::Util::BinaryRelation;
using Simlib::Util::SplittingRelation;
using Simlib::Util::SmartSet;
using Simlib::Util::CachingAllocator;
using Simlib::Util::SharedList;
using Simlib::Util::SharedCounter;
using Simlib::Util::Convert;

typedef CachingAllocator<std::vector<size_t>> VectorAllocator;

struct SharedListInitF
{
	VectorAllocator& allocator_;

	explicit SharedListInitF(VectorAllocator& allocator) : allocator_(allocator) {}

	void operator()(SharedList<std::vector<size_t>>* list)
	{
		auto sublist = this->allocator_();

		sublist->clear();

		list->init(sublist);
	}

};

struct StateListElem
{
	size_t index_;
	struct Block* block_;
	StateListElem* next_;
	StateListElem* prev_;

	static void link(
		StateListElem*   elem1,
		StateListElem*   elem2)
	{
		elem1->next_ = elem2;
		elem2->prev_ = elem1;
	}

};

typedef SharedList<std::vector<size_t>> RemoveList;
typedef CachingAllocator<RemoveList, SharedListInitF> RemoveAllocator;

typedef std::pair<struct Block*, size_t> RemoveQueueElement;
typedef std::vector<RemoveQueueElement> RemoveQueue;

struct Block
{
	size_t index_;
	StateListElem* states_;
	size_t size_;
	std::vector<RemoveList*> remove_;
	SharedCounter counter_;
	SmartSet inset_;
	std::vector<StateListElem*> tmp_;

public:

	Block(
		const Simlib::ExplicitLTS&         lts,
		size_t                           index,
		StateListElem*                   states,
		size_t                           size,
		const SharedCounter::Key&        key,
		const SharedCounter::LabelMap&   labelMap,
		const size_t&                    rowSize,
		SharedCounter::Allocator& allocator) :
		index_(index),
		states_(states),
		size_(size),
		remove_(lts.labels()),
		counter_(key, lts.states(), labelMap, rowSize, allocator),
		inset_(lts.labels()), tmp_()
	{
		do
		{
			assert(nullptr != states);

			for (auto& a : lts.bw_labels(states->index_))
			{
				this->inset_.add(a);
			}

			states->block_ = this;
			states = states->next_;
		} while (states != this->states_);
	}

	Block(
		const Simlib::ExplicitLTS&   lts,
		Block&                     parent,
		StateListElem*             states,
		size_t                     size,
		size_t                     index) :
		index_(index),
		states_(states),
		size_(size),
		remove_(lts.labels()),
		counter_(parent.counter_),
		inset_(lts.labels()),
		tmp_()
	{
		do
		{
			assert(nullptr != states);

			for (auto& a : lts.bw_labels(states->index_))
			{
				parent.inset_.remove_strict(a);
				this->inset_.add(a);
			}

			states->block_ = this;
			states = states->next_;
		} while (states != this->states_);
	}

	void move_to_tmp(StateListElem* elem)
	{
		this->tmp_.push_back(elem);
	}

	static bool check_list(StateListElem* elem, size_t size)
	{
		auto first = elem;

		while (size--)
		{
			assert(nullptr != elem);

			elem = elem->next_;
		}

		return elem == first;
	}

	std::pair<StateListElem*, size_t> try_split()
	{
		assert(!this->tmp_.empty());

		if (this->tmp_.size() == this->size_)
		{
			this->tmp_.clear();

			assert(Block::check_list(this->states_, this->size_));

			return std::make_pair(nullptr, 0);
		}

		auto last = this->tmp_.back();
		this->tmp_.pop_back();
		this->states_ = last->next_;
		StateListElem::link(last->prev_, last->next_);

		if (this->tmp_.empty())
		{
			StateListElem::link(last, last);

			assert(Block::check_list(last, 1));
			assert(Block::check_list(this->states_, this->size_ - 1));

			--this->size_;

			return std::make_pair(last, 1);
		}

		auto elem = last;

		for (auto& state : this->tmp_)
		{
			this->states_ = state->next_;

			StateListElem::link(state->prev_, state->next_);
			StateListElem::link(elem, state);

			elem = state;
		}

		StateListElem::link(elem, last);

		auto size = this->tmp_.size() + 1;
		this->tmp_.clear();

		assert(size < this->size_);

		this->size_ -= size;

		assert(Block::check_list(last, size));
		assert(Block::check_list(this->states_, this->size_));

		return std::make_pair(last, size);
	}

	SmartSet& inset()
	{
		return this->inset_;
	}

	size_t index() const
	{
		return this->index_;
	}

	friend std::ostream& operator<<(
		std::ostream&     os,
		const Block&      block)
	{
		assert(block.states_);

		os << block.index_ << " (";

		auto elem = block.states_;

		do
		{
			os << " " << elem->index_;
			elem = elem->next_;
		} while (elem != block.states_);

		return os << " )";
	}
};

class SimulationEngine
{
protected:

	template <class T>
	void make_block(
		const T&   states,
		size_t     blockIndex)
	{
		assert(states.size() > 0);

		auto list = &this->index_[states.back()];

		for (auto& q : states)
		{
			StateListElem::link(list, &this->index_[q]);

			list = list->next_;
			list->index_ = q;
		}

		this->partition_.push_back(
			new Block(
				this->lts_,
				blockIndex,
				list,
				states.size(),
				this->key_,
				this->label_map_,
				this->row_size_,
				this->counter_allocator_
			)
		);
	}

	void enqueue_to_remove(
		Block*   block,
		size_t   label,
		size_t   state)
	{
		if (RemoveList::append(block->remove_[label], state, this->remove_allocator_))
		{
			this->queue_.push_back(std::make_pair(block, label));
		}
	}

	template <class T>
	void build_pre(
		T&               pre,
		StateListElem*   states,
		size_t           label) const
	{
		std::vector<bool> blockMask(this->partition_.size(), false);

		auto elem = states;

		do
		{
			assert(elem);

			for (auto& q : this->lts_.pre(label)[elem->index_])
			{
				auto& block = this->index_[q].block_;

				assert(block);

				if (blockMask[block->index()])
				{
					continue;
				}

				blockMask[block->index()] = true;
				pre.push_back(block);
			}

			elem = elem->next_;

		} while (elem != states);
	}

	template <class T1, class T2>
	void internal_split(T1& modifiedBlocks, const T2& remove)
	{
		std::vector<bool> blockMask(this->partition_.size(), false);

		for (auto& q : remove)
		{
			assert(q < this->index_.size());

			auto& elem = this->index_[q];
			auto block = elem.block_;

			assert(block);

            block->move_to_tmp(&elem);

			assert(block->index() < this->partition_.size());

			if (blockMask[block->index()])
			{
				continue;
			}

			blockMask[block->index()] = true;
			modifiedBlocks.push_back(block);
		}
	}

	template <class T>
	void fast_split(const T& remove)
	{
		std::vector<Block*> modifiedBlocks;
        this->internal_split(modifiedBlocks, remove);

		for (auto& block : modifiedBlocks)
		{
			assert(block);

			auto p = block->try_split();

			if (!p.first)
			{
				continue;
			}

			auto newBlock = new Block(
				this->lts_, *block, p.first, p.second, this->partition_.size()
			);

			this->partition_.push_back(newBlock);
			this->relation_.split(block->index_);
		}
	}

	template <class T>
	void split(
		std::vector<bool>&   removeMask,
		const T&             remove)
	{
		std::vector<Block*> modifiedBlocks;
        this->internal_split(modifiedBlocks, remove);

		for (auto& block : modifiedBlocks)
		{
			assert(block);

			auto p = block->try_split();

			if (!p.first)
			{
				removeMask[block->index_] = true;

				continue;
			}

			auto newBlock = new Block(
				this->lts_, *block, p.first, p.second, this->partition_.size()
			);

			this->partition_.push_back(newBlock);
			this->relation_.split(block->index_);
			removeMask[newBlock->index_] = true;
			newBlock->counter_.copy_labels(newBlock->inset_, block->counter_);

			for (auto& a : newBlock->inset_)
			{
				if (!block->remove_[a])
				{
					continue;
				}

				this->queue_.push_back(std::make_pair(newBlock, a));
				newBlock->remove_[a] = block->remove_[a]->copy();
			}
		}
	}

	void process_remove(
		Block*   block,
		size_t   label)
	{
		assert(nullptr != block);

		auto remove = block->remove_[label];
		block->remove_[label] = nullptr;

		assert(remove);

		std::vector<Block*> preList;
		std::vector<bool> removeMask(this->lts_.states());
		this->build_pre(preList, block->states_, label);
		this->split(removeMask, *remove);

		remove->unsafe_release(
		[this](RemoveList *list) {
					this->vector_allocator_.reclaim(list->sublist());
					this->remove_allocator_.reclaim(list);
				 }
        );

		for (auto& b1 : preList)
		{
			SplittingRelation::Row row = this->relation_.row(b1->index_);

			for (auto col = row.begin(); col != row.end(); ++col)
			{
				if (!removeMask[*col])
				{
					continue;
				}

				assert(b1->index_ != *col);
				this->relation_.erase(col);
				auto b2 = this->partition_[*col];

				for (auto a : b2->inset_)
				{
					if (!b1->inset_.contains(a))
					{
						continue;
					}

					auto elem = b2->states_;

					do
					{
						assert(elem);

						for (auto& pre : this->lts_.pre(a)[elem->index_])
						{
							if (!b1->counter_.decr(a, pre))
							{
                                this->enqueue_to_remove(b1, a, pre);
							}
						}

						elem = elem->next_;
					} while (elem != b2->states_);
				}
			}
		}
	}

	static bool is_partition(
		const std::vector<std::vector<size_t>>&    part,
		size_t                                     states)
	{
		std::vector<bool> mask(states, false);

		for (auto& cls : part)
		{
			for (auto& q : cls)
			{
				if (mask[q])
				{
					SIMLIB_INFO("state " << q << " appears in more than one block");

					return false;
				}

				mask[q] = true;
			}
		}

		for (size_t i = 0; i < mask.size(); ++i)
		{
			if (!mask[i])
			{
				SIMLIB_INFO("state " << i << " does not appear anywhere");

				return false;
			}
		}

		return true;
	}

	static bool is_consistent(
		const std::vector<std::vector<size_t>>&   part,
		const BinaryRelation&                     rel)
	{
		if (part.size() != rel.size())
		{
			SIMLIB_INFO("partition and relation sizes differ");

			return false;
		}

		for (size_t i = 0; i < rel.size(); ++i)
		{
			if (!rel.get(i, i))
			{
				SIMLIB_INFO("relation is not reflexive");

				return false;
			}
		}

		return true;
	}

private:

	const Simlib::ExplicitLTS& lts_;

	size_t row_size_;

	VectorAllocator vector_allocator_;
	RemoveAllocator remove_allocator_;
	SharedCounter::Allocator counter_allocator_;

	std::vector<Block*> partition_;
	SplittingRelation relation_;

	std::vector<StateListElem> index_;
	RemoveQueue queue_;
	std::vector<size_t> key_;
	std::vector<std::pair<size_t, size_t>> label_map_;

	static size_t get_row_size(size_t states)
	{
		size_t treshold = static_cast<size_t>(std::sqrt(states)) >> 1;
		size_t rowSize_ = 32;

		while (rowSize_ <= treshold)
		{
			rowSize_ <<= 1;
		}

		// make room for reference counter
		return rowSize_ - 1;
	}

public:

	explicit SimulationEngine(
		const Simlib::ExplicitLTS& lts) :
            lts_(lts),
            row_size_(SimulationEngine::get_row_size(lts.states())),
            vector_allocator_(),
            remove_allocator_(SharedListInitF(vector_allocator_)),
            counter_allocator_(row_size_ + 1),
            partition_(),
            relation_(lts.states()),
            index_(lts.states()),
            queue_(),
            key_(),
            label_map_()
	{
		assert(!this->index_.empty());
	}

	~SimulationEngine()
	{
		for (auto& block : this->partition_)
		{
			delete block;
		}
	}

	void init(
		const std::vector<std::vector<size_t>>&   partition,
		const BinaryRelation&                     relation)
	{
		assert(SimulationEngine::is_partition(partition, this->lts_.states()));
		assert(SimulationEngine::is_consistent(partition, relation));

		// build counter maps
		std::vector<SmartSet> delta1;
        this->lts_.build_delta1(delta1);

		this->key_.resize(this->lts_.labels()*this->lts_.states(), static_cast<size_t>(-1));
		this->label_map_.resize(this->lts_.labels());

		size_t x = 0;

		for (size_t a = 0; a < this->lts_.labels(); ++a)
		{
			this->label_map_[a].first = x / this->row_size_;
			this->label_map_[a].second =
				(x + delta1[a].size() - 1) / this->row_size_ + ((!delta1[a].empty()) ? (1) : (0));

			for (auto& q : delta1[a])
			{
				this->key_[a*this->lts_.states() + q] = x++;
			}
		}

		// initilize patition-relation
		for (size_t i = 0; i < partition.size(); ++i)
		{
            this->make_block(partition[i], i);
		}

		BinaryRelation::IndexType index;
		relation.build_index(index);
		this->relation_.init(index);

		// make initial refinement
		for (size_t a = 0; a < this->lts_.labels(); ++a)
		{
            this->fast_split(delta1[a]);
		}

		assert(this->relation_.size() == this->partition_.size());

		// prune relation
		std::vector<std::vector<size_t>> pre(this->partition_.size());
		std::vector<std::vector<bool>> noPreMask(
			this->lts_.labels(), std::vector<bool>(this->partition_.size())
		);

		for (auto& block : this->partition_)
		{
			auto elem = block->states_;

			do
			{
				for (size_t a = 0; a < this->lts_.labels(); ++a)
				{
					delta1[a].contains(elem->index_)
						? (pre[block->index_].push_back(a), true)
						: (noPreMask[a][block->index_] = true);
				}

				elem = elem->next_;
			} while (elem != block->states_);
		}

		for (auto& b1 : this->partition_)
		{
			auto row = this->relation_.row(b1->index_);

			for (auto& a : pre[b1->index_])
			{
				for (auto col = row.begin(); col != row.end(); ++col)
				{
					assert(a < noPreMask.size());
					assert(*col < noPreMask[a].size());

					if (!noPreMask[a][*col])
					{
						continue;
					}

					assert(b1->index_ != *col);

					this->relation_.erase(col);
				}
			}
		}

		// initialize counters
		SmartSet s;

		for (auto& b1 : this->partition_)
		{
			auto row = this->relation_.row(b1->index_);
			std::vector<bool> relatedBlocks(this->partition_.size());

			for (auto& col : row)
			{
				relatedBlocks[col] = true;
			}

			size_t size = 0;

			for (auto& a : b1->inset())
			{
				size = std::max(size, this->label_map_[a].second);
			}

			b1->counter_.resize(size);

			for (auto& a : b1->inset())
			{
				for (auto q : delta1[a])
				{
					size_t count = 0;

					for (auto r : this->lts_.post(a)[q])
					{
						if (relatedBlocks[this->index_[r].block_->index_])
						{
							++count;
						}
					}

					if (count)
					{
						b1->counter_.set(a, q, count);
					}
				}

				s.assign_flat(delta1[a]);

				for (auto& col : row)
				{
					auto b2 = this->partition_[col];
					auto elem = b2->states_;

					do
					{
						for (auto& q : this->lts_.pre(a)[elem->index_])
						{
							s.remove(q);
						}

						elem = elem->next_;
					} while (elem != b2->states_);
				}

				if (s.empty())
				{
					continue;
				}

				b1->remove_[a] = new RemoveList(new std::vector<size_t>(s.begin(), s.end()));
				this->queue_.push_back(std::make_pair(b1, a));

				assert(s.size() == b1->remove_[a]->sublist()->size());
			}

			b1->counter_.init();
		}
	}

	void run()
	{
		while (!this->queue_.empty())
		{
			std::pair<Block*, size_t> tmp(this->queue_.back());
			this->queue_.pop_back();
            this->process_remove(tmp.first, tmp.second);
		}
	}

	void build_result(
		BinaryRelation&    result,
		size_t             size) const
	{
		result.resize(size);

		std::vector<std::vector<size_t>> tmp(this->partition_.size());

		for (size_t i = 0; i < this->partition_.size(); ++i)
		{
			auto elem = this->partition_[i]->states_;

			do
			{
				assert(elem);

				if (elem->index_ < size)
				{
					tmp[i].push_back(elem->index_);
				}

				elem = elem->next_;

			} while (elem != this->partition_[i]->states_);
		}

		for (size_t i = 0; i < this->relation_.size(); ++i)
		{
			for (auto j : const_cast<SplittingRelation*>(&this->relation_)->row(i))
			{
				for (auto& r : tmp[i])
				{
					for (auto& s : tmp[j])
					{
						result.set(r, s, true);
					}
				}
			}
		}
	}

	friend std::ostream& operator<<(
		std::ostream&              os,
		const SimulationEngine&    engine)
	{
		os << "partition: " << std::endl;

		for (auto& block : engine.partition_)
		{
			os << *block;
		}

		BinaryRelation relation;

        engine.build_result(relation, engine.partition_.size());

		return os << "relation:" << std::endl << relation;
	}
};

BinaryRelation Simlib::ExplicitLTS::compute_simulation(
	const std::vector<std::vector<size_t>>&   partition,
	const BinaryRelation&                     relation,
	size_t                                    outputSize)
{
	if (0 == outputSize)
	{
		return BinaryRelation{};
	}

	SimulationEngine engine(*this);

	engine.init(partition, relation);
	engine.run();

	BinaryRelation result;

    engine.build_result(result, outputSize);

	return result;
}


BinaryRelation Simlib::ExplicitLTS::compute_simulation(
	size_t   outputSize)
{
	std::vector<std::vector<size_t>> partition(1);

	for (size_t i = 0; i < this->states_; ++i)
	{
		partition[0].push_back(i);
	}

	return this->compute_simulation(
            partition, Util::BinaryRelation(1, true), outputSize
    );
}


BinaryRelation Simlib::ExplicitLTS::compute_simulation()
{
	return this->compute_simulation(this->states_);
}


void Simlib::ExplicitLTS::add_transition(
	size_t   q,
	size_t   a,
	size_t   r)
{
	if (a >= this->data_.size())
	{
		this->data_.resize(a + 1);
	}

	if (q >= this->data_[a].first.size())
	{
		if (q >= this->states_)
		{
			this->states_ = q + 1;
		}

		this->data_[a].first.resize(q + 1);
	}

	if (r >= this->data_[a].second.size())
	{
		if (r >= this->states_)
		{
			this->states_ = r + 1;
		}

		this->data_[a].second.resize(r + 1);
	}

	this->data_[a].first[q].push_back(r);
	this->data_[a].second[r].push_back(q);

	++this->transitions_;
}
