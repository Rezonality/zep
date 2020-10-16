#include <catch.hpp>
#include <nod/nod.hpp>

SCENARIO( "signals can be queried for how many slots are connected to them" ) {
	GIVEN( "a signal without connected slots" ) {
		nod::signal<void()> sig;
		WHEN( "we query it for the number of slots" ) {
			auto count = sig.slot_count();
			THEN( "it has none" ) {
				REQUIRE( count == 0 );
			}
			AND_THEN( "it is considered empty" ) {
				REQUIRE( sig.empty() == true );
			}
		}
		WHEN( "we connect a slot" ) {
			auto conn = sig.connect( [](){} );
			THEN( "it has one connected slot" ) {
				REQUIRE( sig.slot_count() == 1 );
				AND_THEN( "it is not considered empty" ) {
					REQUIRE( sig.empty() == false );
				}
			}
			AND_WHEN( "we connect another slot" ) {
				auto conn2 = sig.connect( [](){} );
				THEN( "it has two connected slots" ) {
					REQUIRE( sig.slot_count() == 2 );
					AND_THEN( "it is not considered empty" ) {
						REQUIRE( sig.empty() == false );
					}
				}
				AND_WHEN( "we disconnect one of the slots" ) {
					conn.disconnect();
					THEN( "it has one connected slot" ) {
						REQUIRE( sig.slot_count() == 1 );
						AND_THEN( "it is not considered empty" ) {
							REQUIRE( sig.empty() == false );
						}
					}
					AND_WHEN( "we disconnect the last of the slots" ) {
						conn2.disconnect();
						THEN( "it has zero connected slot" ) {
							REQUIRE( sig.slot_count() == 0 );
							AND_THEN( "it is considered empty" ) {
								REQUIRE( sig.empty() == true );
							}
						}
					}
				}
			}
		}
	}
}

SCENARIO( "Slots are able to query their signal for slot count" ) {
	GIVEN(  "a signal with connected slots" ) {
		int x = 0;
		nod::signal<void()> sig;
		WHEN( "we connect a slot that counts slots of it's own signal" ) {
			sig.connect(
				[&](){
					x += sig.slot_count();
				});
			
			THEN( "Triggering the signal will get the result without a deadlock" ) {
				sig();
				REQUIRE( x == 1 );
			}
		}
	}
}