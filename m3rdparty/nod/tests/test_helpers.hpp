#ifndef IG_TEST_HELPERS_HPP
#define IG_TEST_HELPERS_HPP

#include <memory>     // std::unique_ptr

namespace test
{
	// Implementation of the c++14 function `std::make_unique`
	template <class T, class...Args>
	std::unique_ptr<T> make_unique( Args&&... args ) {
		return std::unique_ptr<T>{ new T{std::forward<Args>(args)... } };
	}

}	// namespace test

#endif // IG_TEST_HELPERS_HPP