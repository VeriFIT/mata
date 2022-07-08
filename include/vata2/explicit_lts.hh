/*****************************************************************************
 *  VATA Tree Automata Library
 *
 *  Copyright (c) 2011  Jiri Simacek <isimacek@fit.vutbr.cz>
 *
 *  Description:
 *    Header file for LTS class definition.
 *
 *****************************************************************************/

#ifndef _VATA2_EXPLICIT_LTS_HH_
#define _VATA2_EXPLICIT_LTS_HH_

#include <vector>

#include <vata2/simutil/binary_relation.hh>
#include <vata2/simutil/smart_set.hh>


namespace Vata2 { class ExplicitLTS; }

class Vata2::ExplicitLTS {

	size_t states_;
	size_t transitions_;
	std::vector<
		std::pair<
			std::vector<std::vector<size_t>>,
			std::vector<std::vector<size_t>>
		>
	> data_;
	std::vector<Util::SmartSet> bwLabels_;

public:

	/**
	 * @brief  The constructor
	 *
	 * @param[in]  states  The least number of states to consider
	 */
	ExplicitLTS(size_t states = 0) :
		states_(states),
		transitions_(0),
		data_(),
		bwLabels_()
	{ }

	void add_transition(size_t q, size_t a, size_t r);

	void init()
	{
		this->bwLabels_.resize(this->states_, Util::SmartSet(this->data_.size()));

		for (size_t a = 0; a < this->data_.size(); ++a)
		{
			this->data_[a].first.resize(this->states_);
			this->data_[a].second.resize(this->states_);

			for (size_t r = 0; r < this->states_; ++r)
			{
				this->bwLabels_[r].init(a, this->data_[a].second[r].size());
			}
		}
	}

	void clear()
	{
		this->data_.clear();
		this->bwLabels_.clear();
		this->states_ = 0;
		this->transitions_ = 0;
	}

	const std::vector<std::vector<size_t>>& post(size_t a) const
	{
		assert(a < this->data_.size());

		return this->data_[a].first;
	}

	const std::vector<std::vector<size_t>>& pre(size_t a) const
	{
		assert(a < this->data_.size());

		return this->data_[a].second;
	}

	const Util::SmartSet& bwLabels(size_t q) const
	{
		assert(q < this->bwLabels_.size());

		return this->bwLabels_[q];
	}

	void buildDelta1(std::vector<Util::SmartSet>& delta1) const
	{
		delta1.resize(this->data_.size(), Util::SmartSet(this->states_));

		for (size_t a = 0; a < this->data_.size(); ++a)
		{
			for (size_t q = 0; q < this->data_[a].first.size(); ++q)
			{
				delta1[a].init(q, delta1[a].count(q) + this->data_[a].first[q].size());
			}
		}
	}

	size_t labels() const { return this->data_.size(); }

	const size_t& states() const { return this->states_; }

	friend std::ostream& operator<<(std::ostream& os, const ExplicitLTS& lts)
	{
		for (size_t a = 0; a < lts.data_.size(); ++a)
		{
			for (size_t q = 0; q < lts.data_[a].second.size(); ++q)
			{
				for (auto& r : lts.data_[a].first[q])
				{
					os << q << " --" << a << "--> " << r << std::endl;
				}
			}
		}

		return os;
	}

public:

	Util::BinaryRelation computeSimulation(
		const std::vector<std::vector<size_t>>&   partition,
		const Util::BinaryRelation&               relation,
		size_t                                    outputSize
	);

	Util::BinaryRelation computeSimulation(
		size_t   outputSize);

	Util::BinaryRelation computeSimulation();
};

#endif
