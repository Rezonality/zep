#include "../test_helpers.hpp"
#include <nod/nod.hpp>

#include <catch.hpp>

#include <sstream>

SCENARIO( "Connection objects are default constructible" ) {
	GIVEN( "a default constructed connection" ) {
		nod::connection connection;
		THEN( "the connection will not be considered connected" ) {
			REQUIRE( connection.connected() == false );
		}
	}
}

SCENARIO( "Slots can get connected and disconnected to signals" ) {
	GIVEN( "A singnal" ) {
		nod::signal<void()> signal;
		WHEN( "a slot is connected" ) {
			auto connection = signal.connect( [](){} );
			THEN( "the resulting connection is considered connected" ) {
				REQUIRE( connection.connected() == true );
			}
			AND_WHEN( "the connection is disconnected" ) {
				connection.disconnect();
				THEN( "the connetion is considered disconnected" ) {
					REQUIRE( connection.connected() == false );
				}
			}
		}
	}
}

SCENARIO( "Signals can be invoked and will trigger connected slots" ) {
	GIVEN( "A signal" ) {
		std::ostringstream ss;
		nod::signal<void(double)> signal;
		WHEN( "A slot gets connected to that signal" ) {
			auto connection = signal.connect(
				[&ss]( double x ){
					ss << "signaled!";
				});
			THEN( "the slot will get called when invoking the signal" ) {
				signal(12.5);
				REQUIRE( ss.str() == "signaled!" );
			}
			AND_WHEN( "we disconnect that slot with the connection object" ) {
				connection.disconnect();
				THEN( "the slot will no longer be called when invoking the signal" ) {
					signal(12.5);
					REQUIRE( ss.str().empty() );
				}
			}
		}
	}
}

SCENARIO( "Slots should be called in the order they where connected" ) {
	GIVEN( "A signal" ) {
		nod::signal<void(std::ostream&)> signal;
		WHEN( "we connect three slots to the signal" ) {
			auto c1 = signal.connect([](std::ostream& o){ o<<"one"; });
			auto c2 = signal.connect([](std::ostream& o){ o<<"two"; });
			auto c3 = signal.connect([](std::ostream& o){ o<<"three"; });
			THEN( "triggering the signal will call all the slots in connection order" ) {
				std::ostringstream ss;
				signal(ss);
				REQUIRE( ss.str() == "onetwothree" );
			}
			AND_WHEN( "we connect the second connection and trigger the signal" ) {
				c2.disconnect();
				std::ostringstream ss;
				THEN( "the remaining two slots will still be called in connecion order" ) {
					signal(ss);
					REQUIRE( ss.str() == "onethree" );
				}
				AND_WHEN( "connecting a new slot to the signal" ) {
					auto c4 = signal.connect([](std::ostream& o){ o<<"four"; });
					THEN( "signaling the slot will call all three connected slots in connection order" ) {
						signal(ss);
						REQUIRE( ss.str() == "onethreefour" );
					}
				}
			}
		}
	}
}


SCENARIO( "Signals will disconnect all slots when destroyed" ) {
	GIVEN( "A signal" ) {
		auto signal_ptr = test::make_unique<nod::signal<void()>>();
		WHEN( "Some slots gets connected to the signal" ) {
			auto connection1 = signal_ptr->connect( []() {} );
			auto connection2 = signal_ptr->connect( []() {} );
			auto connection3 = signal_ptr->connect( []() {} );
			THEN( "the connections is considered connected" ) {
				REQUIRE( connection1.connected() == true );
				REQUIRE( connection2.connected() == true );
				REQUIRE( connection3.connected() == true );
			}
			AND_WHEN( "the signal gets destructed" ) {
				signal_ptr.reset();
				THEN( "the connections is no longer considered connected" ) {
					REQUIRE( connection1.connected() == false );
					REQUIRE( connection2.connected() == false );
					REQUIRE( connection3.connected() == false );
				}
			}
		}
	}
}

SCENARIO( "Scoped connection objects are default constructible" ) {
	REQUIRE( std::is_default_constructible<nod::scoped_connection>::value == true );
	GIVEN( "a default scoped constructed connection" ) {
		nod::scoped_connection scoped;
		THEN( "the scoped connection will not be considered connected" ) {
			REQUIRE( scoped.connected() == false );
		}
	}
}

SCENARIO( "Scoped connection objects are move constructible" ) {
	REQUIRE( std::is_move_constructible<nod::scoped_connection>::value == true );
	GIVEN( "a signal with a slot managed by a scoped connection" ) {
		nod::signal<void()> signal;
		nod::scoped_connection scoped = signal.connect([](){});
		WHEN( "we try to move construct a scoped connection from the existing scoped connection" ) {
			nod::scoped_connection connection{ std::move(scoped) };
			THEN( "the new scoped connection is connected, but we still only have one slot connected" ) {
				REQUIRE( connection.connected() == true );
				REQUIRE( signal.slot_count() == 1 );
			}
		}
	}
}

SCENARIO( "Scoped connection objects are not copy constructible" ) {
	REQUIRE( std::is_copy_constructible<nod::scoped_connection>::value == false );
}

SCENARIO( "Scoped connection objects are not copy assignable" ) {
	REQUIRE( std::is_copy_assignable<nod::scoped_connection>::value == false );
}

SCENARIO( "Scoped connection objects are move assignable" ) {
	REQUIRE( std::is_move_assignable<nod::scoped_connection>::value == true );
	GIVEN( "a signal with a connected slot, managed by a scoped connection" ) {
		nod::signal<void()> signal;
		std::ostringstream ss;
		nod::scoped_connection scoped1 = signal.connect([&](){ ss << "signaled!"; });
		WHEN( "a new scoped connection can be move assigned from the original scoped connection" ) {
			nod::scoped_connection scoped2 = std::move(scoped1);
			THEN( "the new connection is connected" ) {
				REQUIRE( scoped2.connected() == true );
			}
			AND_WHEN( "firing the signal triggers the slot only once" ) {
				signal();
				REQUIRE( ss.str() == "signaled!" );
			}
		}
	}
	GIVEN( "two signals connected to a slot, managed by two scoped connections" ) {
		nod::signal<void()> signal;
		std::ostringstream ss;
		nod::scoped_connection connection1 = signal.connect([&](){ ss << "1"; });
		nod::scoped_connection connection2 = signal.connect([&](){ ss << "2"; });
		REQUIRE( signal.slot_count() == 2 );
		WHEN( "we move assign the first of the connections with the second one" ) {
			connection1 = std::move(connection2);
			THEN( "the signal only has one connected slot") {
				REQUIRE( signal.slot_count() == 1 );
			}
			AND_WHEN( "we trigger the signal" ) {
				signal();
				THEN( "only the second slot is executed" ) {
					REQUIRE( ss.str() == "2" );
				}
			}
		}
	}
}

SCENARIO( "Scoped connection will disconnect when destroyed" ) {
	GIVEN( "A signal" ) {
		std::ostringstream ss;
		nod::signal<void()> signal;
		WHEN( "A slot gets connected and managed by a scoped connection" ) {
			auto connection = test::make_unique<nod::scoped_connection>( signal.connect([&](){ ss << "signaled!"; }) );
			THEN( "the connection is considered connected" ) {
				REQUIRE( connection->connected() == true );
			}
			AND_WHEN( "the scoped_connection is destroyed ") {
				connection.reset();
				THEN( "it is disconnected so no signaling is done" ) {
					signal();
					REQUIRE( ss.str().empty() );
				}
			}
		}
	}
}

SCENARIO( "Scoped connections can be reset" ) {
	GIVEN( "A signal") {
		std::ostringstream ss;
		nod::signal<void()> signal;
		WHEN( "A slot get's connected and managed by a scoped connection" ) {
			nod::scoped_connection scoped = signal.connect( [&](){ ss << "Signaled!"; } );
			THEN( "the connection is considered connected" ) {
				REQUIRE( scoped.connected() == true );
			}
			AND_WHEN( "the connection is reset to empty") {
				scoped.reset();
				THEN( "the connection is not considered connected" ) {
					REQUIRE( scoped.connected() == false );
				}
				AND_THEN( "the connection is disconnected, so the slot will not be called when triggering the signal") {
					signal();
					REQUIRE( ss.str().empty() );
				}
			}
			AND_WHEN( "The scoped connection is reset to another connection" ) {
				scoped.reset( signal.connect( [&](){ ss << "Honk!"; } ) );
				THEN( "The scoped connection is considered connected" ) {
					REQUIRE( scoped.connected() == true );
				}
				AND_THEN( "The first connection is disconnected, so the slot will call the new slot when triggered") {
					signal();
					REQUIRE( ss.str() == "Honk!" );
				}
			}
		}
	}
}

SCENARIO( "Scoped connections can be released" ) {
	GIVEN( "A signal") {
		std::ostringstream ss;
		nod::signal<void()> signal;
		WHEN( "A slot get's connected and managed by a scoped connection" ){
			nod::scoped_connection scoped = signal.connect( [&](){ ss << "Signaled!"; } );
			THEN( "The scoped connection is considered connected" ) {
				REQUIRE( scoped.connected() == true );
			}
			AND_WHEN( "the connection is released" ) {
				nod::connection c = scoped.release();
				THEN( "the scoped connection is not considered connected" ) {
					REQUIRE( scoped.connected() == false );
				}
				AND_THEN( "the released connection is considered connected" ) {
					REQUIRE( c.connected() == true );
				}
				AND_THEN( "the slot will be called when the signal is triggered" ) {
					signal();
					REQUIRE( ss.str() == "Signaled!" );
				}
			}
		}
	}
}
