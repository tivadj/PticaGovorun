#pragma once
#include <array>
#include <vector>
#include <string>
#include <sstream>
#include <functional>
#include <boost/utility/string_view.hpp>
#include "assertImpl.h"
#include "PticaGovorunCore.h"

// temporary implementation of a slice
namespace wv
{
	namespace detail {
		// detail meta functions {{{
		template<class Array>
		struct is_array_class {
			static bool const value = false;
		};
		template<class T, size_t N>
		struct is_array_class < std::array<T, N> > {
			static bool const value = true;
		};
		template<class T>
		struct is_array_class < std::vector<T> > {
			static bool const value = true;
		};
		template<class T>
		struct is_array_class < std::initializer_list<T> > {
			static bool const value = true;
		};
		//template<class T>
		//struct is_array_class < std::basic_string<T> > {
		//	static bool const value = true;
		//};
		template<>
		struct is_array_class < std::wstring > {
			static bool const value = true;
		};
		template<>
		struct is_array_class < std::string > {
			static bool const value = true;
		};
		// }}}
	}

	template <typename T>
	class PG_EXPORTS slice
	{
	public:
		typedef T value_type;
		typedef value_type* pointer;
		typedef value_type const* const_pointer;
		typedef value_type& reference;
		typedef value_type const& const_reference;
		typedef value_type* iterator;
		typedef value_type const* const_iterator;
		typedef size_t size_type;
		typedef ptrdiff_t difference_type;

	public:
		explicit slice()
			: data_(nullptr), size_(0)
		{}

		template <class Array>
		/*implicit*/ slice(Array& v)
			: data_((pointer)v.data()), size_(v.size())
		{}

		slice(T* start, T* last)
			: data_(start), size_(last - start)
		{
			PG_Assert(start <= last);
		}

		template<
			class InputIterator,
			class = typename std::enable_if<
				std::is_same<
				T,
				typename std::iterator_traits<InputIterator>::value_type
				>::value
			>::type
		>
		explicit slice(InputIterator start, InputIterator last)
		: data_(&*start._Unchecked()), size_(last - start)
		{
			PG_Assert(start <= last);
		}

			slice(std::initializer_list<T> const& l)
				: data_(std::begin(l)), size_(l.size())
			{}

			/*
			* iterator interfaces
			*/
			iterator begin()
			{
				return data_;
			}
			iterator end()
			{
				return data_ + size_;
			}
			const_iterator begin() const
			{
				return data_;
			}
			const_iterator end() const
			{
				return data_ + size_;
			}
			const_iterator cbegin() const
			{
				return begin();
			}
			const_iterator cend() const
			{
				return end();
			}

			/*
			* access
			*/
			size_type size() const
			{
				return size_;
			}

			bool empty() const
			{
				return size_ == 0;
			}
			const_reference operator[](size_type const n) const
			{
				return *(data_ + n);
			}
			reference operator[](size_type const n)
			{
				return *(data_ + n);
			}
			const_pointer data() const
			{
				return data_;
			}
			pointer data()
			{
				return data_;
			}
			const_reference front() const
			{
				return *data_;
			}
			const_reference back() const
			{
				return *(data_ + size_ - 1);
			}

	private:
		size_type size_;
		pointer data_;
	};

	// helpers to construct view {{{
	template<
		class Array,
		class = typename std::enable_if<
			detail::is_array_class<Array>::value
		>::type
	>
	inline
	auto make_view(Array& a)-> slice<typename Array::value_type>
		{
				//return{ a };
			return make_view(std::begin(a), std::end(a));
		}

		template<class T>
		inline slice<T> make_view(T* p, typename slice<T>::size_type const n)
		{
			return slice<T>{p, p + n};
		}

		template<class InputIterator, class Result = slice<typename std::iterator_traits<InputIterator>::value_type>>
		inline auto make_view(InputIterator begin, InputIterator end) -> Result
		{
			return Result{ begin, end };
		}
}


namespace PticaGovorun {

PG_EXPORTS void linearSpace(float min, float max, int numPoints, wv::slice<float> points);


// Performs the binary search to find the item with greatest associated item.FrameInd less than pivotFrameInd.
// The collection must be in ascending order.
// Numerical associated value is selected using frameIndSelector.
// T = long
// frameIndSelector :: *RandIt -> T  = the predicate to extract FrameInd from given item.
template <typename RandIt, typename ItemSelector>
auto binarySearch(const RandIt& begin, const RandIt& end, long frameIndValue, ItemSelector frameIndSelector) -> RandIt
{
	// the search finishes when (begin;end) are the two consecutive items; the result is the lesser value
	if (end - begin == 1)
		return begin;

	RandIt midIt = begin + (end - begin) / 2;
	long midFrameInd = frameIndSelector(*midIt);

	RandIt left;
	RandIt right;
	if (frameIndValue< midFrameInd)
	{
		// continue to search in the left half of the range
		left = begin;
		right = midIt;
	}
	else
	{
		// continue to search in the right half of the range
		left = midIt;
		right = end;
	}

	auto result = binarySearch(left, right, frameIndValue, frameIndSelector);
	return result;
}

	/// Joins
	/// Example:
	/// std::wstring s1(L"abc");
	/// std::vector<boost::wstring_view> list2;
	/// list2.push_back(s1);
	/// list2.push_back(s1);
	/// std::wostringstream buf;
	/// join(std::begin(list2), std::end(list2), boost::wstring_view(L", "), buf);
	template <typename Iter, typename ElemT>
	void join(Iter begin, Iter end, 
		boost::basic_string_view<ElemT, std::char_traits<ElemT>> separator,
		std::basic_ostringstream<ElemT>& result)
	{
		if (begin == end)
			return;

		result << *begin;
		++begin;

		for (; begin != end; ++begin)
		{
			result << separator << *begin;
		}
	}

	//template <
	//	class C,
	//	class X1 = typename std::enable_if<
	//		std::is_void<
	//			decltype(C().push_back(C::value_type()))
	//		>::value
	//	>::type
	//>
	//auto put(C& cont) -> std::function < auto (typename C::value_type) -> void >
	//{
	//	return[&cont](typename C::value_type item) -> void
	//	{
	//		cont.push_back(item);
	//	};
	//}

	// Adaptor from container with push_back method (such as std::vector) into output range function.
	template <typename C>
	auto putBack(C& cont) -> std::function < auto (typename C::value_type) -> void >
	{
		return[&cont](typename C::value_type item) -> void
		{
			cont.push_back(item);
		};
	}

	// Adaptor from container with insert method (such as std::set) into output range function.
	template <class C>
	auto putInsert(C& setCont) -> std::function < auto (typename C::value_type) -> void >
	{
		return[&setCont](typename C::value_type item) -> void
		{
			setCont.insert(item);
		};
	}
}
      