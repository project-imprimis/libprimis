This is the test suite for the libprimis library. It uses gcc's gcov utility to
check how many lines were executed by the test suite and prints them.

## Usage

To run the test suite, use ./process.sh to run the testing process. The total
code coverage will subsequently be printed into the terminal, indicating the
percent of the libprimis codebase which is covered by the tests.

## Scope

The libprimis test suite is designed for Linux and uses gcc and bash/bc/grep core
utilities to accomplish its tasks. While all of its dependencies should be easy
to acquire (gcov, bash, grep, bc), they are Linux command line utilities not
provided by Windows; as a result Windows is not supported.

The goal of the unit tests and the coverage checker is to provide a system for
the continuous integration pipeline to check how well code is being tested. While
end users may find it interesting to check the code coverage on their own, or for
the purposes of verifying that contributions increase code coverage, it is
generally recommended that using this functionality be left to the CI pipeline.

## Implementation

The testing suite uses the gcc flags and gcov's utilities to read how many lines
of code are executed by a given executable. In tandem with a separate testing
application which uses the libprimis shared library, it is therefore possible
to test engine functionality and establish a coverage metric with which to gauge
the amount to which the codebase is tested. The Makefile uses a COVERAGE_BUILD
variable, set from the command line, which allows the library to be coverage
checked (disabled for normal builds).

The script which calculates the coverage percentage parses the raw gcov output
using grep and regular expressions, and prints out the percentage of code lines
hit by the testing suite, which is a standalone program using the libprimis
library. By being standalone, it is simple to determine coverage of the codebase
without having to worry about internal testing routines complicating the library
codebase.
