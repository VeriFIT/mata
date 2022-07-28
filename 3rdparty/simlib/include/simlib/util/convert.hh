/*****************************************************************************
 *  Simlib
 *
 *  Copyright (c) 2011  Ondra Lengal <ilengal@fit.vutbr.cz>
 *
 *  Description:
 *    Convert class declaration.
 *
 *****************************************************************************/

#ifndef _SIMLIB_CONVERT_HH_
#define _SIMLIB_CONVERT_HH_

// Standard library headers
#include <cassert>
#include <list>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>


// insert class to proper namespace
namespace Simlib { namespace Util {	class Convert; } }


/**
 *  @brief  A static class for conversions.
 *
 *  This is a static class that constains useful methods for various
 *  conversions.
 */
class Simlib::Util::Convert
{
private:

	/**
	 * @brief  Private default constructor
	 *
	 * Default constructor which is private to disable creating an instance
	 * of the class.
	 */
	Convert();


	/**
	 * @brief  Private copy constructor
	 *
	 * Copy constructor which is private to disable creating an instance
	 * of the class.
	 *
	 * @param[in]  convert  The instance to be copied
	 */
	Convert(const Convert& convert);


	/**
	 * @brief  Private assignment operator
	 *
	 * Assignment operator which is private to disable creating an instance
	 * of the class.
	 *
	 * @param[in]  convert  The instance to be copied
	 *
	 * @returns  The resulting object
	 */
	Convert& operator=(const Convert& convert);


	/**
	 * @brief  Private destructor
	 *
	 * Private destructor.
	 */
	~Convert();


public:

	/**
	 * @brief  Converts an object to string
	 *
	 * Static method for conversion of an object of any class with the << output
	 * operator into a string
	 *
	 * @param[in]  n  The object for the conversion
	 *
	 * @returns  The string representation of the object
	 */
	template <typename T>
	static std::string ToString(const T& n)
	{
		// the output stream for the string
		std::ostringstream oss;
		// insert the object into the stream
		oss << n;
		// return the string
		return oss.str();
	}


	/**
	 * @brief  Converts an object to string (pointer specialization)
	 *
	 * Static method for conversion of a pointer of an object of any class with
	 * the << output operator into a string
	 *
	 * @param[in]  ptr  Pointer for the conversion
	 *
	 * @returns  The string representation of the pointer
	 */
	template <typename T>
	static std::string ToString(T* ptr)
	{
		assert(ptr != nullptr);

		// the output stream for the string
		std::ostringstream oss;
		// insert the string of the underlying class into the stream
		oss << ToString(*ptr);
		// return the string
		return oss.str();
	}


	/**
	 * @brief  Converts an object to string (shared_ptr specialization)
	 *
	 * Static method for conversion of a shared pointer of an object of any class with
	 * the << output operator into a string
	 *
	 * @param[in]  ptr  Shared pointer for the conversion
	 *
	 * @returns  The string representation of the shared pointer
	 */
	template <typename T>
	static std::string ToString(const std::shared_ptr<T>& ptr)
	{
		assert(ptr != nullptr);

		// the output stream for the string
		std::ostringstream oss;
		// insert the string of the underlying class into the stream
		oss << ToString(*ptr);
		// return the string
		return oss.str();
	}

	/**
	 * @brief  Converts an object to string (weak_ptr specialization)
	 *
	 * Static method for conversion of a weak pointer of an object of any class with
	 * the << output operator into a string
	 *
	 * @param[in]  ptr  Weak pointer for the conversion
	 *
	 * @returns  The string representation of the weak pointer
	 */
	template <typename T>
	static std::string ToString(const std::weak_ptr<T>& ptr)
	{
		// the output stream for the string
		std::ostringstream oss;
		// insert the string of the underlying class into the stream
		oss << ptr.use_count();
		// return the string
		return oss.str();
	}

	/**
	 * @brief  Converts an object to string (std::vector specialization)
	 *
	 * Static method for conversion of a vector of objects of any class with the
	 * << output operator into a string
	 *
	 * @param[in]  vec  The vector for the conversion
	 *
	 * @returns  The string representation of the vector
	 */
	template <typename T>
	static std::string ToString(const std::vector<T>& vec)
	{
		// the output stream for the string
		std::ostringstream oss;

		oss << "(";		// opening tag
		for (auto it = vec.cbegin(); it != vec.cend(); ++it)
		{	// for each element of the vector
			if (it != vec.cbegin())
			{	// if we are not at the first element
				oss << ", ";
			}

			// the string of the element
			oss << ToString(*it);
		}

		oss << ")";		// closing tag

		// return the string
		return oss.str();
	}


	/**
	 * @brief  Converts an object to string (std::set specialization)
	 *
	 * Static method for conversion of a set of objects of any class with the
	 * << output operator into a string
	 *
	 * @param[in]  st  The set for the conversion
	 *
	 * @returns  The string representation of the set
	 */
	template <typename T>
	static std::string ToString(const std::set<T>& st)
	{
		// the output stream for the string
		std::ostringstream oss;

		oss << "{";		// opening tag
		for (auto it = st.cbegin(); it != st.cend(); ++it)
		{	// for each element of the set
			if (it != st.cbegin())
			{	// if we are not at the first element
				oss << ", ";
			}

			// the string of the element
			oss << ToString(*it);
		}

		oss << "}";		// closing tag

		// return the string
		return oss.str();
	}


	/**
	 * @brief  Converts an object to string (std::unordered_set specialization)
	 *
	 * Static method for conversion of an unordered set of objects of any class
	 * with the << output operator into a string
	 *
	 * @param[in]  un_set  The unordered set for the conversion
	 *
	 * @returns  The string representation of the unordered set
	 */
	template <typename T>
	static std::string ToString(const std::unordered_set<T>& un_set)
	{
		// the output stream for the string
		std::ostringstream oss;

		oss << "{";		// opening tag
		for (auto it = un_set.cbegin(); it != un_set.cend(); ++it)
		{	// for each element of the set
			if (it != un_set.cbegin())
			{	// if we are not at the first element
				oss << ", ";
			}

			// the string of the element
			oss << ToString(*it);
		}

		oss << "}";		// closing tag

		// return the string
		return oss.str();
	}


	/**
	 * @brief  Converts an object to string (std::map specialization)
	 *
	 * Static method for conversion of a map of objects of any class with the
	 * << output operator into a string
	 *
	 * @param[in]  mp  The map for the conversion
	 *
	 * @returns  The string representation of the map
	 */
	template <typename T, typename U>
	static std::string ToString(const std::map<T, U>& mp)
	{
		// the output stream for the string
		std::ostringstream oss;

		oss << "[";		// opening tag
		for (auto it = mp.cbegin(); it != mp.cend(); ++it)
		{	// for each element of the map
			if (it != mp.cbegin())
			{	// if we are not at the first element
				oss << ", ";
			}

			// the string of the element
			oss << ToString(it->first) << " -> " << ToString(it->second);
		}

		oss << "]";		// closing tag

		// return the string
		return oss.str();
	}


	/**
	 * @brief  Converts an object to string (std::unordered_map specialization)
	 *
	 * Static method for conversion of an unordered map of objects of any class with the
	 * << output operator into a string
	 *
	 * @param[in]  unmap  The unordered map for the conversion
	 *
	 * @returns  The string representation of the unordered map
	 */
	template <typename T, typename U, class H>
	static std::string ToString(const std::unordered_map<T, U, H>& unmap)
	{
		// the output stream for the string
		std::ostringstream oss;

		oss << "[";		// opening tag
		for (auto it = unmap.cbegin(); it != unmap.cend(); ++it)
		{	// for each element of the unordered map
			if (it != unmap.cbegin())
			{	// if we are not at the first element
				oss << ", ";
			}

			// the string of the element
			oss << ToString(it->first) << " -> " << ToString(it->second);
		}

		oss << "]";		// closing tag

		// return the string
		return oss.str();
	}


	/**
	 * @brief  Converts an object to string (std::multimap specialization)
	 *
	 * Static method for conversion of a multimap of objects of any class with the
	 * << output operator into a string
	 *
	 * @param[in]  mm  The multimap for the conversion
	 *
	 * @returns  The string representation of the multimap
	 */
	template <typename T, typename U>
	static std::string ToString(const std::multimap<T, U>& mm)
	{
		// the output stream for the string
		std::ostringstream oss;

		oss << "{";		// opening tag
		for (auto it = mm.cbegin(); it != mm.cend(); )
		{	// for each element of the set
			if (it != mm.cbegin())
			{	// if we are not at the first element
				oss << ", ";
			}

			oss << Convert::ToString(it->first);
			oss << " -> [";

			auto findRes = mm.equal_range(it->first);

			while (it != findRes.second)
			{
				if (it != findRes.first)
				{	// if we are not at the first element
					oss << "; ";
				}

				// the string of the element
				oss << ToString(it->second);

				++it;
			}

			oss << "]";
		}

		oss << "}";		// closing tag

		// return the string
		return oss.str();
	}

	/**
	 * @brief  Converts an object to string (std::unordered_multimap specialization)
	 *
	 * Static method for conversion of an unordered multimap of objects of any
	 * class with the << output operator into a string
	 *
	 * @param[in]  mm  The unordered multimap for the conversion
	 *
	 * @returns  The string representation of the unordered multimap
	 */
	template <typename T, typename U>
	static std::string ToString(const std::unordered_multimap<T, U>& mm)
	{
		// the output stream for the string
		std::ostringstream oss;

		oss << "{";		// opening tag
		for (auto it = mm.cbegin(); it != mm.cend(); )
		{	// for each element of the set
			if (it != mm.cbegin())
			{	// if we are not at the first element
				oss << ", ";
			}

			oss << Convert::ToString(it->first);
			oss << " -> [";

			auto findRes = mm.equal_range(it->first);

			while (it != findRes.second)
			{
				if (it != findRes.first)
				{	// if we are not at the first element
					oss << "; ";
				}

				// the string of the element
				oss << ToString(it->second);

				++it;
			}

			oss << "]";
		}

		oss << "}";		// closing tag

		// return the string
		return oss.str();
	}


	/**
	 * @brief  Converts an object to string (std::pair specialization)
	 *
	 * Static method for conversion of a pair of objects of any class with the
	 * << output operator into a string
	 *
	 * @param[in]  pr  The pair for the conversion
	 *
	 * @returns  The string representation of the pair
	 */
	template <typename T, typename U>
	static std::string ToString(const std::pair<T, U>& pr)
	{
		// the output stream for the string
		std::ostringstream oss;

		oss << "(" << ToString(pr.first) << ", " << ToString(pr.second) << ")";

		// return the string
		return oss.str();
	}


	/**
	 * @brief  Converts an object to string (std::list specialization)
	 *
	 * Static method for conversion of a list of objects of any class with the
	 * << output operator into a string
	 *
	 * @param[in]  lst  The list for the conversion
	 *
	 * @returns  The string representation of the list
	 */
	template <typename T>
	static std::string ToString(const std::list<T>& lst)
	{
		// the output stream for the string
		std::ostringstream oss;

		oss << "(";		// opening tag
		for (auto it = lst.cbegin(); it != lst.cend(); ++it)
		{	// for each element of the vector
			if (it != lst.cbegin())
			{	// if we are not at the first element
				oss << ", ";
			}

			// the string of the element
			oss << ToString(*it);
		}

		oss << ")";		// closing tag

		// return the string
		return oss.str();
	}


	/**
	 * @brief  Converts a string to an object 
	 *
	 * Static method for conversion of a string to an object of any class with
	 * the >> input operator
	 *
	 * @param[in]  str  The string for the conversion
	 *
	 * @returns  The object that corresponds to the string
	 */
	template <typename T>
	static T FromString(const std::string& str)
	{
		T result;

		// the input stream for the string
		std::istringstream iss(str);
		if (!(iss >> result))
		{	// if there was an error
			throw std::invalid_argument(__func__ + std::string(": invalid argument"));
		}

		return result;
	}

};


namespace Simlib
{
	namespace Util
	{
		/**
		 * @brief  Converts an object to string (unsigned char specialization)
		 *
		 * Static method for conversion of unsigned char into a string
		 *
		 * @param[in]  n  The unsigned char for the conversion
		 *
		 * @returns  The string representation of the unsigned char
		 */
		template <>
		std::string Convert::ToString<unsigned char>(const unsigned char& n);
	}
}

#endif
