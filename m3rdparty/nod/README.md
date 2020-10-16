# Nod
[![Build Status](https://travis-ci.org/fr00b0/nod.svg?branch=master)](https://travis-ci.org/fr00b0/nod)
[![GitHub tag](https://img.shields.io/github/tag/fr00b0/nod.svg?label=version)](https://github.com/fr00b0/nod/releases)

Dependency free, header only signals and slot library implemented with C++11.

## Usage

### Simple usage
The following example creates a signal and then connects a lambda as a slot.

```cpp
// Create a signal which accepts slots with no arguments and void return value.
nod::signal<void()> signal;
// Connect a lambda slot that writes "Hello, World!" to stdout
signal.connect([](){
		std::cout << "Hello, World!" << std::endl;
	});
// Call the slots
signal();
```

### Connecting multiple slots
If multiple slots are connected to the same signal, all of the slots will be
called when the signal is invoked. The slots will be called in the same order
as they where connected.

```cpp
void endline() {
	std::cout << std::endl;
}

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
```

#### Slot type
The signal types in the library support connection of the same types that is
supported by `std::function<T>`.

### Slot arguments
When a signal calls it's connected slots, any arguments passed to the signal
are propagated to the slots. To make this work, we do need to specify the 
signature of the signal to accept the arguments.

```cpp
void print_sum( int x, int y ) {
	std::cout << x << "+" << y << "=" << (x+y) << std::endl;
}
void print_product( int x, int y ) {
	std::cout << x << "*" << y << "=" << (x*y) << std::endl;
}


// We create a signal with two integer arguments.
nod::signal<void(int,int)> signal;
// Let's connect our slot
signal.connect( print_sum );
signal.connect( print_product );

// Call the slots
signal(10, 15);
signal(-5, 7);	

```

### Disconnecting slots
There are many circumstances where the programmer needs to diconnect a slot that
no longer want to recieve events from the signal. This can be really important
if the lifetime of the slots are shorter than the lifetime of the signal. That
could cause the signal to call slots that have been destroyed but not
disconnected, leading to undefined behaviour and probably segmentation faults.

When a slot is connected, the return value from the  `connect` method returns
an instance of the class `nod::connection`, that can be used to disconnect
that slot.

```cpp
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
```	

### Scoped connections
To assist in disconnecting slots, one can use the class `nod::scoped_connection`
to capture a slot connection. A scoped connection will automatically disconnect
the slot when the connection object goes out of scope.

```cpp
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
```

### Slot return values

#### Accumulation of return values
It is possible for slots to have a return value. The return values can be
returned from the signal using a *accumulator*, which is a function object that
acts as a proxy object that processes the slot return values. When triggering a
signal through a accumulator, the accumulator gets called for each slot return
value, does the desired accumulation and then return the result to the code
triggering the signal. The accumulator is designed to work in a similar way as 
the STL numerical algorithm `std::accumulate`.

```cpp
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
```
#### Aggregation
As we can see from the previous example, we can use the `accumulate` method if
we want to aggregate all the return values of the slots. Doing the aggregation
that way is not very optimal. It is both a inefficient algorithm for doing
aggreagtion to a container, and it obscures the call site as the caller needs to
express the aggregation using the verb *accumulate*. To remedy these
shortcomings we can turn to the method `aggregate` instead. This is a template
method, taking the type of container to aggregate to as a template parameter.

```cpp
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
```

## Thread safety
There are two types of signals in the library. The first is `nod::signal<T>`
which is safe to use in a multi threaded environment. Multiple threads can read,
write, connect slots and disconnect slots simultaneously, and the signal will 
provide the nessesary synchronization. When triggering a slignal, all the
registered slots will be called and executed by the thread that triggered the
signal.

The second type of signal is `nod::unsafe_signal<T>` which is **not** safe to
use in a multi threaded environment. No syncronization will be performed on the
internal state of the signal. Instances of the signal should theoretically be
safe to read from multiple thread simultaneously, as long as no thread is
writing to the same object at the same time. There can be a performance gain
involved in using the unsafe version of a signal, since no syncronization
primitives will be used.

`nod::connection` and `nod::scoped_connection` are thread safe for reading from
multiple threads, as long as no thread is writing to the same object. Writing in
this context means calling any non const member function, including destructing
the object. If an object is being written by one thread, then all reads and
writes to that object from the same or other threads needs to be prevented.
This basically means that a connection is only allowed to be disconnected from
one thread, and you should not check connection status or reassign the
connection while it is being disconnected.

## Building the tests
The test project uses [premake5](https://premake.github.io/download.html) to 
generate make files or similiar.

### Linux
To build and run the tests using gcc and gmake on linux, execute the following
from the test directory:
```bash
premake5 gmake
make -C build/gmake
bin/gmake/debug/nod_tests
```

### Visual Studio 2013
To build and run the tests, execute the following from the test directory:

```batchfile
REM Adjust paths to suite your environment
c:\path\to\premake\premake5.exe vs2013
"c:\Program Files (x86)\Microsoft Visual Studio 12.0\Common7\Tools\vsvars32.bat"
msbuild /m build\vs2013\nod_tests.sln
bin\vs2013\debug\nod_tests.exe
```

## The MIT License (MIT)

Copyright (c) 2015 Fredrik Berggren

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
