#include <nod/nod.hpp>
#include <catch.hpp>
#include <iostream>

namespace
{
	// Function to output a endline to stdout
	void endline() {
		std::cout << std::endl;
	}

	// Function that prints the sum calculation of two integers
	void print_sum( int x, int y ) {
		std::cout << x << "+" << y << "=" << (x+y) << std::endl;
	}

	// Function that prints the product calculation of two integers
	void print_product( int x, int y ) {
		std::cout << x << "*" << y << "=" << (x*y) << std::endl;
	}
}

SCENARIO( "Example usage") {
	SECTION( "Simple usage" ) {
		// Create a signal which accepts slots with no arguments and void return value.
		nod::signal<void()> signal;
		// Connect a lambda slot that writes "Hello, World!" to stdout
		signal.connect([](){
				std::cout << "Hello, World!" << std::endl;
			});
		// Call the slots
		signal();
	}

	SECTION( "Connecting multiple slots" ) {
		// Create a signal
		nod::signal<void()> signal;
		// Connect a lambda that prints a message
		signal.connect([](){
				std::cout << "Message without endline!";
			});
		// Connect a function that prints a endline
		signal.connect(endline);

		// Call the slots
		signal();
	}

	SECTION( "Slot arguments" ) {
		// We create a signal with two integer arguments.
		nod::signal<void(int,int)> signal;
		// Let's connect our slot
		signal.connect( print_sum );
		signal.connect( print_product );

		// Call the slots
		signal( 10, 15 );
		signal(-5, 7);
	}

	SECTION( "Disconnecting slots" ) {
		// Let's create a signal
		nod::signal<void()> signal;
		// Connect a slot, and save the connection
		nod::connection connection = signal.connect([](){
										std::cout << "I'm connected!" << std::endl;
		                             });
		// Triggering the signal will call the slot
		signal();
		// Now we disconnect the slot
		connection.disconnect();
		// Triggering the signal will no longer call the slot
		signal();
	}

	SECTION( "Scoped connections" ) {
		// We create a signal
		nod::signal<void()> signal;
		// Let's use a scope to control lifetime
		{
			// Let's save the connection in a scoped_connection
			nod::scoped_connection connection =
				signal.connect([](){
					std::cout << "This message should only be emitted once!" << std::endl;
				});
			// If we trigger the signal, the slot will be called
			signal();
		} // Our scoped connection is destructed, and disconnects the slot
		// Triggering the signal now will not call the slot
		signal();
	}
	SECTION( "Slot return values (accumulate)" ) {
		// We create a singal with slots that return a value
		nod::signal<int(int, int)> signal;
		// Then we connect some signals
		signal.connect( std::plus<int>{} );
		signal.connect( std::multiplies<int>{} );
		signal.connect( std::minus<int>{} );
		// Let's say we want to calculate the sum of all the slot return values
		// when triggering the singal with the parameters 10 and 100.
		// We do this by accumulating the return values with the initial value 0
		// and a plus function object, like so:
		std::cout << "Sum: " << signal.accumulate(0, std::plus<int>{})(10,100) << std::endl;
		// Or accumulate by multiplying (this needs 1 as initial value):
		std::cout << "Product: " << signal.accumulate(1, std::multiplies<int>{})(10,100) << std::endl;
		// If we instead want to build a vector with all the return values
		// we can accumulate them this way (start with a empty vector and add each value):
		auto vec = signal.accumulate( std::vector<int>{}, []( std::vector<int> result, int value ) {
				result.push_back( value );
				return result;
			})(10,100);

		std::cout << "Vector: ";
		for( auto const& element : vec ) {
			std::cout << element << " ";
		}
		std::cout << std::endl;

		// These checks are not part of the usage example, but we can just as well
		// turn this example into a verifying test when we have the chance.
		REQUIRE( signal.accumulate(0, std::plus<int>{})(10,100) == 1020 );
		REQUIRE( signal.accumulate(1, std::multiplies<int>{})(10,100) == -9900000 );
		REQUIRE( vec == (std::vector<int>{110, 1000, -90}) );
	}
	SECTION( "Slot return values (aggregate)" ) {
		// We create a singal
		nod::signal<int(int, int)> signal;
		// Let's connect some slots
		signal.connect( std::plus<int>{} );
		signal.connect( std::multiplies<int>{} );
		signal.connect( std::minus<int>{} );
		// We can now trigger the signal and aggregate the slot return values
		auto vec = signal.aggregate<std::vector<int>>(10,100);

		std::cout << "Result: ";
		for( auto const& element : vec ) {
			std::cout << element << " ";
		}
		std::cout << std::endl;
		REQUIRE( vec == (std::vector<int>{110, 1000, -90}) );
	}
}