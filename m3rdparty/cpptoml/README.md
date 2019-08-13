# cpptoml
A header-only library for parsing [TOML][toml] configuration files.

Targets: [TOML v0.5.0][currver] as of August 2018.

This includes support for the new DateTime format, inline tables,
multi-line basic and raw strings, digit separators, hexadecimal integers,
octal integers, binary integers, and float special values.

Alternatives:
- [Boost.toml][boost.toml] is a C++ implementation of a TOML parser using
  the Boost library. As of writing, it supports v0.5.0 as well.
- [ctoml][ctoml] is a C++11 implementation of a TOML parser, but only
  supports v0.2.0.
- [libtoml][libtoml] is a C implementation of a TOML parser, which can be
  linked to from your C++ programs easily. As of April 2016, it supports
  v0.4.0.
- [tinytoml][tinytoml] is a C++11 implementation of a TOML parser, which
  also supports v0.4.0 as of November 2015.

## Build Status
[![Build Status](https://travis-ci.org/skystrife/cpptoml.svg?branch=master)](https://travis-ci.org/skystrife/cpptoml)

## Test Results

From [the toml-test suite][toml-test]:

```
126 passed, 0 failed
```

We also currently maintain (but hopefully not indefinitely!) a [fork of the
toml-test suite][toml-test-fork] that adds tests for features and
clarifications that have been added to the TOML spec more recently than
toml-test has been updated. We pass every test there.

```
148 passed, 0 failed
```

# Compilation
Requires a well conforming C++11 compiler. On OSX this means clang++ with
libc++ and libc++abi (the default clang installed with XCode's command line
tools is sufficient).

On Linux, you should be able to use g++ >= 4.8.x, or clang++ with libc++
and libc++abi (if your package manager supplies this; most don't).

Compiling the examples can be done with cmake:

```
mkdir build
cd build
cmake ../
make
```

# Example Usage
To parse a configuration file from a file, you can do the following:

```cpp
auto config = cpptoml::parse_file("config.toml");
```

`parse_file()` returns a (shared pointer to a) `cpptoml::table`, which you
can then query. It will throw an instance of `cpptoml::parse_exception` in
the event that the file failed to parse, and the exception message should
contain the line number the error occurred as well as a description of the
error.

## Obtaining Basic Values
You can find basic values like so:

```cpp
auto val = config->get_as<int64_t>("my-int");
// val is a cpptoml::option<int64_t>

if (val)
{
    // *val is the integer value for the key "my-int"
}
else
{
    // "my-int" either did not exist or was not an integer
}
```

To simplify things, you can specify default a default value using the
`value_or` function on the `option`:

```cpp
auto baz = config->get_as<double>("baz").value_or(0.5);
// baz is now the double value for key "baz", if it exists, or 0.5 otherwise
```

cpptoml has extended support for dates and times beyond the TOML v0.4.0
spec. Specifically, it supports

- Local Date (`local_date`), which simply represents a date and lacks any time
  information, e.g. `1980-08-02`;
- Local Time (`local_time`), which simply represents a time and lacks any
  date or zone information, e.g. `12:10:03.001`;
- Local Date-time (`local_datetime`), which represents a date and a time,
  but lacks zone information, e.g. `1980-08-02T12:10:03.001`;
- and Offset Date-time (`offset_datetime`), which represents a date, a
  time, and timezone information, e.g. `1980-08-02T12:10:03.001-07:00`

Here are the fields of the date/time objects in cpptoml:

- year (`local_date`, `local_datetime`, `offset_datetime`)
- month (`local_date`, `local_datetime`, `offset_datetime`)
- day (`local_date`, `local_datetime`, `offset_datetime`)
- hour (`local_time`, `local_datetime`, `offset_datetime`)
- minute (`local_time`, `local_datetime`, `offset_datetime`)
- second (`local_time`, `local_datetime`, `offset_datetime`)
- microsecond (`local_time`, `local_datetime`, `offset_datetime`)
- hour\_offset (`offset_datetime`)
- minute\_offset (`offset_datetime`)

There are convenience functions `cpptoml::offset_datetime::from_zoned()` and
`cpptoml::offset_datetime::from_utc()` to convert `struct tm`s to
`cpptoml::offset_datetime`s.

## Nested Tables
If you want to look up things in nested tables, there are two ways of doing
this. Suppose you have the following structure:

```toml
[first-table]
key1 = 0.1
key2 = 1284

[first-table.inner]
key3 = "hello world"
```

Here's an idiomatic way of obtaining all three keys' values:

```cpp
auto config = cpptoml::parse_file("config.toml");
auto key1 = config->get_qualified_as<double>("first-table.key1");
auto key2 = config->get_qualified_as<int>("first-table.key2");
auto key3 = config->get_qualified_as<std::string>("first-table.inner.key3");
```

(Note that, because the TOML spec allows for "." to occur in a table name,
you won't *always* be able to do this for any nested key, but in practice
you should be fine.)

A slightly more verbose way of getting them would be to first obtain the
individual tables, and then query those individual tables for their keys
like so:

```cpp
auto config = cpptoml::parse_file("config.toml");

auto first = config->get_table("first-table");
auto key1 = first->get_as<double>("key1");
auto key2 = first->get_as<int>("key2");

auto inner = first->get_table("inner");
auto key3 = inner->get_as<std::string>("key3");
```

The function `get_table_qualified` also exists, so obtaining the inner
table could be written as

```cpp
auto inner2 = config->get_table_qualified("first-table.inner");
```

## Arrays of Values
Suppose you had a configuration file like the following:

```toml
arr = [1, 2, 3, 4, 5]
mixed-arr = [[1, 2, 3, 4, 5], ["hello", "world"], [0.1, 1.1, 2.1]]
```

To obtain an array of values, you can do the following:

```cpp
auto config = cpptoml::parse_file("config.toml");

auto vals = config->get_array_of<int64_t>("arr");
// vals is a cpptoml::option<std::vector<int64_t>>

for (const auto& val : *vals)
{
    // val is an int64_t
}
```

`get_array_of` will return an `option<vector<T>>`, which will be empty if
the key does not exist, is not of the array type, or contains values that
are not of type `T`.

For nested arrays, it looks like the following:

```cpp
auto nested = config->get_array_of<cpptoml::array>("mixed-arr");

auto ints = (*nested)[0]->get_array_of<int64_t>();
// ints is a cpptoml::option<std::vector<int64_t>>

auto strings = (*nested)[1]->get_array_of<std::string>();
auto doubles = (*nested)[2]->get_array_of<double>();
```

There is also a `get_qualified_array_of` for simplifying arrays located
inside nested tables.

## Arrays of Tables
Suppose you had a configuration file like the following:

```toml
[[table-array]]
key1 = "hello"

[[table-array]]
key1 = "can you hear me"
```

Arrays of tables are represented as a separate type in `cpptoml`. They can
be obtained like so:

```cpp
auto config = cpptoml::parse_file("config.toml");

auto tarr = config->get_table_array("table-array");

for (const auto& table : *tarr)
{
    // *table is a cpptoml::table
    auto key1 = table->get_as<std::string>("key1");
}
```

## More Examples
You can look at the files files `parse.cpp`, `parse_stdin.cpp`, and
`build_toml.cpp` in the root directory for some more examples.

`parse_stdin.cpp` shows how to use the visitor pattern to traverse an
entire `cpptoml::table` for serialization.

`build_toml.cpp` shows how to construct a TOML representation in-memory and
then serialize it to a stream.

[currver]: https://github.com/toml-lang/toml/blob/master/versions/en/toml-v0.5.0.md
[toml]: https://github.com/toml-lang/toml
[toml-test]: https://github.com/BurntSushi/toml-test
[toml-test-fork]: https://github.com/skystrife/toml-test
[ctoml]: https://github.com/evilncrazy/ctoml
[libtoml]: https://github.com/ajwans/libtoml
[tinytoml]: https://github.com/mayah/tinytoml
[boost.toml]: https://github.com/ToruNiina/Boost.toml
