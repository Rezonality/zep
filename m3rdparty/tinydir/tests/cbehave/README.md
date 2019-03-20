cbehave - A Behavior Driven Development Framework for C
=======
[![Build Status](https://travis-ci.org/cxong/cbehave.svg?branch=master)](https://travis-ci.org/cxong/cbehave)

A demonstration using real C code:

    #include "cbehave.h"

    // Step 1: define your functions
    int add(int a, int b);

    // Step 2: describe behaviour and the function calls
    FEATURE(addition, "Addition")
        SCENARIO("Add two numbers")
            GIVEN("we have two numbers 50 and 70")
                int a = 50;
                int b = 70;
            WHEN("we add them together")
                int r = add(a, b);
            THEN("the result should be 120")
                SHOULD_INT_EQUAL(r, 120);
        SCENARIO_END
    FEATURE_END

    // Step 3: write empty implementations of functions
    int add(int a, int b)
    {
        // Step 5: write code to make the behaviour pass
        return a + b;
    }

    // Step 4: run tests and watch them fail (and succeed later)
    CBEHAVE_RUN("Calculator Features are as below:", TEST_FEATURE(addition))

Introduction
-------------
CBehave - A Behavior Driven Development Framework for C.

Main Features
-------------

 - use the "feature + scenario" structure (inspired by Cucumber)
 - use classical "given-when-then" template to describe behavior scenarios
 - support mock

Example Output
-------------

   *******************************************************************
       CBEHAVE -- A Behavior Driven Development Framework for C
                By Tony Bai
   *******************************************************************
    Strstr Features are as belows:
    Feature: strstr
     Scenario: The strstr finds the first occurrence of the substring in the source string
         Given A source string: Lionel Messi is a great football player
         When we use strstr to find the first occurrence of [football]
         Then We should get the string: [football player]
     Scenario: If strstr could not find the first occurrence of the substring, it will return NULL
         Given A source string: FC Barcelona is a great football club.
         When we use strstr to find the first occurrence of [AC Milan]
         Then We should get no string but a NULL
     Summary:
     features: [1/1]
     scenarios: [2/2]

Build
------

To run the examples:

 - Clone the project
 - cmake cbehave/examples

To use cbehave in your CMake project:

- include the cbehave directory
- link against `cbehave`
