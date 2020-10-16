#include <catch.hpp>

#include <nod/nod.hpp>

#include <iostream>
#include <sstream>
#include <string>

namespace {

	// free function to output a string to a ostream
	void output( std::ostream& out, std::string const& str ) {
		out << str;
	}

	// free function that prints "one" to an ostream
	void outputOne( std::ostream& out ) {
		output( out, "one" );
	}

	// std::function<> that outputs "two" to an ostream
	std::function<void(std::ostream&)> outputTwo =
		[]( std::ostream& out ){
			output( out, "two" );
		};

	//test class
	struct test
	{
		test( std::string const& str ) :
			_str(str)
		{}

		void operator()( std::ostream& out ) const {
			out << _str;
		}

		std::string _str;
	};

	using output_signal = nod::signal<void(std::ostream&)>;

}	// anonymous namespace

TEST_CASE( "General tests", "[general]" ) {

	SECTION("Connecting lambda") {
		output_signal signal;
		signal.connect(
			[](std::ostream& out){
				out << "lambda";
			});
		std::stringstream ss;
		signal(ss);
		REQUIRE( ss.str() == "lambda" );
	}

	SECTION( "Connecting free function" ) {
		output_signal signal;
		signal.connect(outputOne);
		std::stringstream ss;
		signal(ss);
		REQUIRE( ss.str() == "one" );
	}

	SECTION( "Connecting std::function" ) {
		output_signal signal;
		signal.connect(outputTwo);
		std::stringstream ss;
		signal(ss);
		REQUIRE( ss.str() == "two" );
	}

	SECTION( "Connecting bind expression" ) {
		output_signal signal;
		signal.connect( std::bind(output, std::placeholders::_1, "three" ) );
		std::stringstream ss;
		signal(ss);
		REQUIRE( ss.str() == "three" );
	}

	SECTION( "Connecting functor object" ) {
		output_signal signal;
		test functor{ "four" };
		signal.connect( functor );
		std::stringstream ss;
		signal(ss);
		REQUIRE( ss.str() == "four" );
	}

	SECTION( "Multiple slots" ) {
		std::ostringstream ss;
		nod::signal<void(int)> signal;
		signal.connect(
			[&ss]( int value ){
				ss << "A: " << value << ",";
			});
		signal.connect(
			[&ss]( int value ){
				ss << "B: " << value << ",";
			});

		signal(12);
		signal(42);

		REQUIRE( ss.str() == "A: 12,B: 12,A: 42,B: 42," );
	}

	SECTION( "Slot's can remove themself from the signal" ) {
		auto x = 0;
		nod::signal<void(int)> signal;

		nod::connection c;
		c = signal.connect(
			[&]( int value ) {
				x += value;			
				c.disconnect();
			});

		signal(5); // slot disconnects here
		REQUIRE( x == 5 ); 
		signal(10); // the slot will not be called
		REQUIRE( x == 5 );
	}
}
