#include <catch.hpp>

#include <nod/nod.hpp>

#include <iostream>
#include <sstream>

SCENARIO( "Connection objects are forced disconnected when disconnecting all slots from a signal" ) {
	GIVEN("A signal") {
		std::ostringstream ss;
		nod::signal<void(std::string const&)> signal;
		WHEN("Two slots gets connected") {
			auto connection1 = signal.connect([&ss](std::string const& str){
				ss << "1: " << str << "\n";
			});
			auto connection2 = signal.connect([&ss](std::string const& str){
				ss << "2: " << str << "\n";
			});
			THEN("both connections are considered connected") {
				REQUIRE( connection1.connected() == true );
				REQUIRE( connection2.connected() == true );
			}
			AND_WHEN("the signal is triggered") {
				signal("testing");
				THEN("both slots get called in connection order") {
					REQUIRE( ss.str() == "1: testing\n2: testing\n" );
				}
			}
			AND_WHEN("we force disconnect all slots from the signal") {
				signal.disconnect_all_slots();
				THEN("both connections are considered disconnected") {
					REQUIRE( connection1.connected() == false );
					REQUIRE( connection2.connected() == false );
					AND_WHEN("we try to disconnect the slots") {
						connection1.disconnect();
						connection2.disconnect();
						THEN("nothing changes") {
							REQUIRE( connection1.connected() == false );
							REQUIRE( connection2.connected() == false );
						}
					}
				}
				AND_WHEN("the signal is triggered") {
					signal("again");
					THEN("no slots are called") {
						REQUIRE( ss.str() == "" );
					}
				}
				AND_WHEN("we connect a new slot to the signal") {
					auto connection3 = signal.connect([&ss](std::string const& str){
						ss << "3: " << str << "\n";
					});
					THEN("the connection is considered connected") {
						REQUIRE( connection3.connected() == true );
					}
					AND_WHEN("we trigger the signal") {
						signal("third time");
						THEN("only the new slot is called") {
							REQUIRE( ss.str() == "3: third time\n" );
						}
					}
				}
			}
		}
	}
}
