/*****************************************************************************
 *  Simlib
 *
 *  Copyright (c) 2011  Jiri Simacek <isimacek@fit.vutbr.cz>
 *
 *  Description:
 *    Binary relation class.
 *
 *****************************************************************************/

#ifndef _SIMLIB_BINARY_RELATION_HH_
#define _SIMLIB_BINARY_RELATION_HH_

#include <mata/simlib/util/convert.hh>
#include <mata/simlib/util/simlib.hh>
#include <mata/simlib/util/transl_weak.hh>
#include <mata/simlib/util/two_way_dict.hh>

// Standard library headers
#include <vector>
#include <algorithm>
#include <unordered_map>

namespace Simlib
{
	namespace Util
	{
		class BinaryRelation;
		class Identity;
		class DiscontBinaryRelation;
	}
}


/**
 * @brief  A binary relation address continuously
 *
 * A binary relation addressed from indices from 0 to @p size_ - 1
 */
class Simlib::Util::BinaryRelation
{
	std::vector<bool> data_;
	size_t rowSize_;
	size_t size_;

protected:

	void realloc(size_t newRowSize, bool defVal)
	{
		// check for sane parameters
		assert(0 < newRowSize);

		std::vector<bool> tmp(newRowSize*newRowSize, defVal);
		std::vector<bool>::const_iterator src = data_.begin();
		std::vector<bool>::iterator dst = tmp.begin();
		for (size_t i = 0; i < size_; ++i) {
			std::copy(src, src + size_, dst);
			src += rowSize_;
			dst += newRowSize;
		}
		std::swap(data_, tmp);
		rowSize_ = newRowSize;
	}

	void grow(size_t newSize, bool defVal = false)
	{
		assert(rowSize_ <= newSize);

		size_t newRowSize = rowSize_;
		while (newRowSize <= newSize)
			newRowSize <<= 1;
		assert(newSize <= newRowSize);
		this->realloc(newRowSize, defVal);

	}

public:

	void reset(bool defVal)
	{
		std::fill(data_.begin(), data_.end(), defVal);
	}

	void resize(size_t size, bool defVal = false)
	{

		if (rowSize_ < size)
		{
			this->grow(size, defVal);
		}

		size_ = size;
	}

	size_t alloc()
	{
		if (size_ >= rowSize_)
		{
			this->grow(size_ + 1);
		}

		return size_++;
	}

	size_t split(size_t i, bool reflexive = true)
	{
		assert(i < size_);

		if (size_ >= rowSize_)
		{
			this->grow(size_ + 1);
		}

		assert((size_ + 1)*rowSize_ <= data_.size());

		// fill collumns
		auto src = data_.begin() + i;
		auto end = src + size_*rowSize_;
		auto dst = data_.begin() + size_;

		for ( ; src != end; src += rowSize_, dst += rowSize_)
		{
			*dst = *src;
		}

		// fill rows
		src = data_.begin() + i*rowSize_;
		dst = data_.begin() + size_*rowSize_;

		std::copy(src, src + size_, dst);

		// set the reflexive bit
		*(dst + size_) = reflexive;

		return size_++;
	}

	bool get(size_t r, size_t c) const
	{
		assert(r < size_ && c < size_);
		assert(r*rowSize_ + c < data_.size());

		return data_[r*rowSize_ + c];
	}

	void set(size_t r, size_t c, bool v)
	{
		assert(r < size_ && c < size_);
		assert(r*rowSize_ + c < data_.size());

		data_[r*rowSize_ + c] = v;
	}

	size_t size() const
	{
		return size_;
	}

public:

	using IndexType    = std::vector<std::vector<size_t>>;

	BinaryRelation(
		size_t         size = 0,
		bool           defVal = false,
		size_t         rowSize = 16) :
		data_(rowSize*rowSize, defVal),
		rowSize_(rowSize),
		size_(0)
	{
		this->resize(size, defVal);
	}

	BinaryRelation(const std::vector<std::vector<bool> >& rel) :
		data_(16*16, false),
		rowSize_(16),
		size_(0)
	{
		this->resize(rel.size(), false);
		for (size_t i = 0; i < rel.size(); ++i)
		{
			assert(rel[i].size() == rel.size());
			for (size_t j = 0; j < rel.size(); ++j)
			{
				this->set(i, j, rel[i][j]);
			}
		}
	}

	// TODO: does this do what is expected? What is expected this should do???
	// WHAT ABOUT SOME FREAKING DOCUMENTATION??????
	bool sym(size_t row, size_t column) const
	{
		return this->get(row, column) && this->get(column, row);
	}

	// build equivalence classes
	void build_equivalence_classes(std::vector<size_t>& headIndex) const
	{
		headIndex.resize(size_);
		std::vector<size_t> head;
		for (size_t i = 0; i < size_; ++i)
		{
			size_t j = 0;
			while ((j < head.size()) && !this->sym(i, head[j]))
			{
				++j;
			}

			if (j < head.size())
			{
				headIndex[i] = head[j];
			}
			else
			{
				headIndex[i] = i;
				head.push_back(i);
			}
		}
	}

	// build equivalence classes
	void build_equivalence_classes(
		std::vector<size_t>&       index,
		std::vector<size_t>&       head) const
	{
		index.resize(size_);
		head.clear();

		for (size_t i = 0; i < size_; ++i)
		{
			size_t j = 0;
			while ((j < head.size()) && !this->sym(i, head[j]))
			{
				++j;
			}

			if (j < head.size())
			{
				index[i] = j;
			}
			else
			{
				index[i] = head.size();
				head.push_back(i);
			}
		}
	}

	// and composition
	BinaryRelation& operator&=(const BinaryRelation& rhs)
	{
		assert(size_ == rhs.size_);

		for (size_t i = 0; i < size_; ++i)
		{
			for (size_t j = 0; j < size_; ++j)
			{
				this->set(i, j, this->get(i, j) && rhs.get(i,j));
			}
		}

		return *this;
	}

	// transposition
	BinaryRelation& transposed(BinaryRelation& dst) const
	{
		dst.resize(size_);
		for (size_t i = 0; i < size_; ++i)
		{
			for (size_t j = 0; j < size_; ++j)
			{
				dst.set(j, i, this->get(i, j));
			}
		}

		return dst;
	}


	/**
	 * @brief  Creates a mapping from elements to their images
	 *
	 * This method maps elements to their images. Formally, it creates for each
	 * element 'x' the mapping 'x -> Y' where 'Y = {y | xRy}'
	 *
	 * @param[out]  dst  The result mapping every element to its images
	 *
	 * @see  build_inv_index
	 */
	void build_index(IndexType& dst) const
	{
		dst.resize(size_);

		auto rowStart = data_.begin();
		auto src = rowStart;
		auto end = src + size_*rowSize_;
		auto dstIter = dst.begin();

		for (; src != end; src = rowStart, ++dstIter)
		{
			auto rowEnd = src + size_;
			rowStart = src + rowSize_;

			for (size_t i = 0; src != rowEnd; ++src, ++i)
			{
				if (*src)
				{
					dstIter->push_back(i);
				}
			}
		}
	}

	/**
	 * @brief  Creates a mapping from elements to their co-images
	 *
	 * This method maps elements to their co-images. Formally, it creates for each
	 * element 'x' the mapping 'x -> Y' where 'Y = {y | yRx}'
	 *
	 * @param[out]  dst  The result mapping every element to its co-images
	 *
	 * @see build_index
	 */
	void build_inv_index(IndexType& dst) const
	{
		dst.resize(size_);

		for (size_t i = 0; i < size_; ++i)
		{
			for (size_t j = 0; j < size_; ++j)
			{
				if (this->get(i, j))
				{
					dst[j].push_back(i);
				}
			}
		}
	}

	/**
	 * @brief  Creates a mapping from elements to their images and co-images
	 *
	 * This method the call to build_index and build_inv_index, i.e. it maps
	 * elements to their images (in @p ind) and their co-images (in @p inv).
	 *
	 * @param[out]  ind  The result mapping every element to its images
	 * @param[out]  inv  The result mapping every element to its co-images
	 *
	 * @see build_index build_inv_index
	 */
	void build_index(IndexType& ind, IndexType& inv) const
	{
		ind.resize(size_);
		inv.resize(size_);

		for (size_t i = 0; i < size_; ++i)
		{
			for (size_t j = 0; j < size_; ++j)
			{
				if (this->get(i, j))
				{
					ind[i].push_back(j);
					inv[j].push_back(i);
				}
			}
		}
	}

	/**
	 * @brief  Restricts the relation to its symmetric fragment
	 */
	void restrict_to_symmetric()
	{
		for (size_t row = 0; row < this->size(); ++row)
		{
			for (size_t col = row + 1; col < this->size(); ++col)
			{	// traverse the matrix (we do just the part above the diagonal)
				bool res = this->get(row, col) && this->get(col, row);
				this->set(row, col, res);
				this->set(col, row, res);
			}
		}
	}


	/**
	 * @brief  Gets the projection of elements to their representatives
	 *
	 * This method relies on the fact that @p *this represents an equivalance
	 * relation (the result is undefined otherwise). The method creates the
	 * projection of elements to their representatives in the quotient set
	 * induced by the equivalence relation represented by @p *this.
	 *
	 * @param[out]  quotProj  The vector mapping elements to their
	 *                        representatives in the quotient set, quotProj[i] is
	 *                        the number that the i-th guy maps to
	 */
	void get_quotient_projection(
		std::vector<size_t>&             quotProj)
	{
		const size_t UNDEF_PROJ = static_cast<size_t>(-1);

		quotProj.resize(this->size());
		for (size_t& elem : quotProj)
		{
			elem = UNDEF_PROJ;
		}

		for (size_t row = 0; row < this->size(); ++row)
		{	// traverse the matrix above the diagonal, only
			assert(row < quotProj.size());
			if (UNDEF_PROJ != quotProj[row])
			{	// in the case 'row' is already in some equivalence class
				continue;
			}

			quotProj[row] = row;
			for (size_t col = row + 1; col < this->size(); ++col)
			{
				if (this->get(row, col))
				{	// if 'col' is equivalent (w.r.t. the relation) to 'row'
					assert(UNDEF_PROJ == quotProj[col]);

					quotProj[col] = row;
				}
			}
		}
	}


	friend std::ostream& operator<<(
		std::ostream&             os,
		const BinaryRelation&     v)
	{
		for (size_t i = 0; i < v.size_; ++i)
		{
			for (size_t j = 0; j < v.size_; ++j)
			{
				os << v.get(i, j);
			}

			os << std::endl;
		}

		return os;
	}
};

class Simlib::Util::Identity
{
private:  // data members

	size_t size_;

public:   // methods

	bool get(size_t r, size_t c) const
	{
		assert(r < size_);
		assert(c < size_);

		return r == c;
	}

	size_t size() const
	{
		return size_;
	}

public:

	using IndexType = std::unordered_map<size_t, std::vector<size_t>>;

	explicit Identity(size_t size) :
		size_(size)
	{ }

	bool sym(size_t r, size_t c) const
	{
		assert(r < size_);
		assert(c < size_);

		return r == c;
	}

	// build equivalence classes
	void build_equivalence_classes(std::vector<size_t>& headIndex) const
	{
		headIndex.resize(size_);
		std::vector<size_t> head;

		for (size_t i = 0; i < size_; ++i)
		{
			size_t j = 0;
			while ((j < head.size()) && !this->sym(i, head[j]))
			{
				++j;
			}

			if (j < head.size())
			{
				headIndex[i] = head[j];
			}
			else
			{
				headIndex[i] = i;
				head.push_back(i);
			}
		}
	}

	// build equivalence classes
	void build_equivalence_classes(
		std::vector<size_t>& index,
		std::vector<size_t>& head) const
	{
		index.resize(size_);
		head.clear();
		for (size_t i = 0; i < size_; ++i)
		{
			size_t j = 0;
			while ((j < head.size()) && !this->sym(i, head[j]))
			{
				++j;
			}

			if (j < head.size())
			{
				index[i] = j;
			}
			else
			{
				index[i] = head.size();
				head.push_back(i);
			}
		}
	}

	// relation index
	void build_index(IndexType& dst) const
	{
		assert(dst.empty());

		for (size_t i = 0; i < size_; ++i)
		{
			bool inserted = dst.insert(std::make_pair(i, std::vector<size_t>({i}))).second;
			if (!inserted)  assert(false);
		}
	}

	// inverted relation index
	void build_inv_index(IndexType& dst) const
	{
		assert(dst.empty());
		this->build_index(dst);
	}

	// relation index
	void build_index(IndexType& ind, IndexType& inv) const
	{
		assert(ind.empty());
		assert(inv.empty());

        this->build_index(ind);
		inv = ind;
	}

	friend std::ostream& operator<<(
		std::ostream&        os,
		const Identity&      v)
	{
		for (size_t i = 0; i < v.size(); ++i)
		{
			for (size_t j = 0; j < v.size(); ++j)
			{
				os << v.get(i, j);
			}
			os << std::endl;
		}

		return os;
	}
};

/**
 * @brief  A binary relation with discontinuous indexing
 */
class Simlib::Util::DiscontBinaryRelation
{
public:   // data types

	using IndexType    = std::unordered_map<size_t, std::vector<size_t>>;
	// using DictType     = std::unordered_map<size_t, size_t>;
	using DictType     = Simlib::Util::TwoWayDict<size_t, size_t,
		std::unordered_map<size_t, size_t>, std::unordered_map<size_t, size_t>>;
	using TranslType   = Simlib::Util::TranslatorWeak<DictType>;

private:  // data members

	/// The underlying binary relation, indexed from 0
	BinaryRelation rel_;

	/// The counter of allocated indices
	size_t index_cnt_;

	/// The mapping of inputs to the range 0..size-1
	DictType dict_;

	/// The translator for indices
	TranslType transl_;

private:  // methods

	/**
	 * @brief  Translates index of internal relation to the discontinuous
	 *
	 * @param[in,out]  inner_index  The index for the internal continuous relation
	 *
	 * @returns  The index in the discontinuous relation
	 *
	 * @note  Destroys @p inner_index
	 *
	 * @note  Is this optimal?
	 */
	IndexType translate_index_to_discont(
		BinaryRelation::IndexType&      inner_index) const
	{
		IndexType result;

		for (size_t x = 0; x < inner_index.size(); ++x)
		{
			std::vector<size_t>& images = inner_index[x];

			// 'std::transform' is C++ for 'map' in Haskell
			std::transform(
				/* where to start */ images.cbegin(),
				/* where to finish */ images.cend(),
				/* where to insert the results */ images.begin(),
				/* what to compute */ [this](size_t img) { return dict_.translate_bwd(img);});

			bool inserted = result.insert(
				std::make_pair(dict_.translate_bwd(x), std::move(images))).second;
			if (!inserted)  assert(false);
		}

		return result;
	}

public:   // methods

	/**
	 * @brief  The constructor
	 */
	DiscontBinaryRelation(
		size_t         size = 0,
		bool           defVal = false,
		size_t         rowSize = 16) :
			rel_(size, defVal, rowSize),
			index_cnt_(0),
			dict_(),
			transl_(dict_, [this](const size_t&){return index_cnt_++;})
	{ }


	/**
	 * @brief  Constructor from a binary relation and a dictionary with copy semantics
	 *
	 * @note This allows a conversion from TDict to DictType
	 */
	template <
		class TDict>
	DiscontBinaryRelation(
		const BinaryRelation&      rel,
		const TDict&               dict) :
			rel_(rel),
			index_cnt_(0),
			dict_(dict),
			transl_(dict_, [this](const size_t&){return index_cnt_++;})
	{ }


	/**
	 * @brief  Constructor from a binary relation and a dictionary with copy semantics
	 */
	DiscontBinaryRelation(
		const BinaryRelation&      rel,
		const DictType&            dict) :
			rel_(rel),
			index_cnt_(0),
			dict_(dict),
			transl_(dict_, [this](const size_t&){return index_cnt_++;})
	{ }


	/**
	 * @brief  Constructor from a binary relation and a dictionary with move semantics
	 *
	 * @note This allows a conversion from TDict to DictType
	 */
	template <
		class TDict>
	DiscontBinaryRelation(
		BinaryRelation&&      rel,
		TDict&&               dict) :
		rel_(rel),
		index_cnt_(0),
		dict_(dict),
		transl_(dict_, [this](const size_t&){return index_cnt_++;})
	{ }

	/**
	 * @brief  Constructor from a binary relation and a dictionary with move semantics
	 */
	DiscontBinaryRelation(
		BinaryRelation&&      rel,
		DictType&&            dict) :
			rel_(rel),
			index_cnt_(0),
			dict_(dict),
			transl_(dict_, [this](const size_t&){return index_cnt_++;})
	{ }


	/**
	 * @brief  Copy constructor
	 */
	DiscontBinaryRelation(const DiscontBinaryRelation& rhs) :
		rel_(rhs.rel_),
		index_cnt_(rhs.index_cnt_),
		dict_(rhs.dict_),
		transl_(dict_, [this](const size_t&){return index_cnt_++;})
	{ }


	/**
	 * @brief  Move constructor
	 */
	DiscontBinaryRelation(DiscontBinaryRelation&& rhs) :
		rel_(rhs.rel_),
		index_cnt_(rhs.index_cnt_),
		dict_(rhs.dict_),
		transl_(dict_, [this](const size_t&){return index_cnt_++;})
	{ }


	/**
	 * @brief  Assignment operator
	 */
	DiscontBinaryRelation& operator=(const DiscontBinaryRelation& rhs)
	{
		if (this != &rhs)
		{
			rel_ = rhs.rel_;
			dict_ = rhs.dict_;
			index_cnt_ = rhs.index_cnt_;

			// the following (or a similar thing because this does not work) should
			// not be necessary because dict_ is used as a reference
			//
			// transl_ = TranslType(dict_, [this](const size_t&){return index_cnt_++;});
		}

		return *this;
	}


	/**
	 * @brief  Move assignment operator
	 */
	DiscontBinaryRelation& operator=(DiscontBinaryRelation&& rhs)
	{
		assert(this != &rhs);

		rel_ = std::move(rhs.rel_);
		dict_ = std::move(rhs.dict_);
		index_cnt_ = std::move(rhs.index_cnt_);

		// the following (or a similar thing because this does not work) should
		// not be necessary because dict_ is used as a reference
		//
		// transl_ = TranslType(dict_, [this](const size_t&){return index_cnt_++;});

		return *this;
	}

	std::string to_string() const
	{
		std::ostringstream str;

		str << "{";
		bool placeComma = false;
		for (auto firstStateToMappedPair : dict_)
		{	// iterate over all pairs
			for (auto secondStateToMappedPair : dict_)
			{
				if (this->get(firstStateToMappedPair.first, secondStateToMappedPair.first))
				{	// if the pair is in the relation print
					str << (placeComma? ", " : "");
					str << "(" << firstStateToMappedPair.first << ", " <<
						secondStateToMappedPair.first << ")";
					placeComma = true;
				}
			}
		}
		str << "}";

		return str.str();
	}

	/**
	 * @brief  Output stream operator
	 */
	friend std::ostream& operator<<(
		std::ostream&                     os,
		const DiscontBinaryRelation&      rel)
	{
		os << rel.to_string();

		return os;
	}


	/**
	 * @brief  Creates a mapping from elements to their images
	 *
	 * This method maps elements to their images. Formally, it creates for each
	 * element 'x' the mapping 'x -> Y' where 'Y = {y | xRy}'
	 *
	 * @param[out]  dst  The result mapping every element to its images
	 */
	void build_index(IndexType& dst) const
	{
		assert(dst.empty());

		std::vector<std::vector<size_t>> innerIndex;

		rel_.build_index(innerIndex);
		dst = this->translate_index_to_discont(innerIndex);
	}

	// relation index
	void build_index(
		IndexType&           ind,
		IndexType&           inv) const
	{
		assert(ind.empty());
		assert(inv.empty());

		std::vector<std::vector<size_t>> innerInd;
		std::vector<std::vector<size_t>> innerInv;

		rel_.build_index(innerInd, innerInv);
		assert(innerInd.size() == innerInv.size());
		ind = translate_index_to_discont(innerInd);
		inv = translate_index_to_discont(innerInv);
	}

	bool get(size_t row, size_t column) const
	{
		size_t rowTransl = transl_.at(row);
		size_t colTransl = transl_.at(column);
		return rel_.get(rowTransl, colTransl);
	}

	void set(size_t row, size_t column, bool value)
	{
		rel_.set(transl_[row], transl_[column], value);
	}

	size_t size() const
	{
		return rel_.size();
	}

	/**
	 * @brief  Restricts the relation to its symmetric fragment
	 */
	void restrict_to_symmetric()
	{
		rel_.restrict_to_symmetric();
	}


	/**
	 * @brief  Gets the projection of elements to their representatives
	 *
	 * This method relies on the fact that @p *this represents an equivalance
	 * relation (the result is undefined otherwise). The method creates the
	 * projection of elements to their representatives in the quotient set
	 * induced by the equivalence relation represented by @p *this.
	 *
	 * @param[out]  quot_proj  The projection of elements to their representatives
	 *                        in the quotient set
	 */
	template <
		class MapType>
	void get_quotient_projection(
		MapType&       quot_proj)
	{
		assert(quot_proj.empty());

		std::vector<size_t> innerProj;

		// get the vector for continuous values
		rel_.get_quotient_projection(innerProj);

		for (size_t i = 0; i < innerProj.size(); ++i)
		{	// go over all elements in the vector
			bool inserted = quot_proj.insert(
				std::make_pair(dict_.translate_bwd(i), dict_.translate_bwd(innerProj[i]))).second;
			if (!inserted)  assert(false);
		}
	}
};

#endif
