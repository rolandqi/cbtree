# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.15

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list


# Suppress display of executed commands.
$(VERBOSE).SILENT:


# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /roland.qi/cbtree

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /roland.qi/cbtree/build

# Include any dependencies generated for this target.
include test/CMakeFiles/bolt_test.dir/depend.make

# Include the progress variables for this target.
include test/CMakeFiles/bolt_test.dir/progress.make

# Include the compile flags for this target's objects.
include test/CMakeFiles/bolt_test.dir/flags.make

test/CMakeFiles/bolt_test.dir/Bolt/bolt_test.cc.o: test/CMakeFiles/bolt_test.dir/flags.make
test/CMakeFiles/bolt_test.dir/Bolt/bolt_test.cc.o: ../test/Bolt/bolt_test.cc
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/roland.qi/cbtree/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object test/CMakeFiles/bolt_test.dir/Bolt/bolt_test.cc.o"
	cd /roland.qi/cbtree/build/test && /opt/rh/devtoolset-8/root/usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/bolt_test.dir/Bolt/bolt_test.cc.o -c /roland.qi/cbtree/test/Bolt/bolt_test.cc

test/CMakeFiles/bolt_test.dir/Bolt/bolt_test.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/bolt_test.dir/Bolt/bolt_test.cc.i"
	cd /roland.qi/cbtree/build/test && /opt/rh/devtoolset-8/root/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /roland.qi/cbtree/test/Bolt/bolt_test.cc > CMakeFiles/bolt_test.dir/Bolt/bolt_test.cc.i

test/CMakeFiles/bolt_test.dir/Bolt/bolt_test.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/bolt_test.dir/Bolt/bolt_test.cc.s"
	cd /roland.qi/cbtree/build/test && /opt/rh/devtoolset-8/root/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /roland.qi/cbtree/test/Bolt/bolt_test.cc -o CMakeFiles/bolt_test.dir/Bolt/bolt_test.cc.s

# Object files for target bolt_test
bolt_test_OBJECTS = \
"CMakeFiles/bolt_test.dir/Bolt/bolt_test.cc.o"

# External object files for target bolt_test
bolt_test_EXTERNAL_OBJECTS =

test/bolt_test: test/CMakeFiles/bolt_test.dir/Bolt/bolt_test.cc.o
test/bolt_test: test/CMakeFiles/bolt_test.dir/build.make
test/bolt_test: libbolt.a
test/bolt_test: test/libgtest.a
test/bolt_test: test/CMakeFiles/bolt_test.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/roland.qi/cbtree/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX executable bolt_test"
	cd /roland.qi/cbtree/build/test && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/bolt_test.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
test/CMakeFiles/bolt_test.dir/build: test/bolt_test

.PHONY : test/CMakeFiles/bolt_test.dir/build

test/CMakeFiles/bolt_test.dir/clean:
	cd /roland.qi/cbtree/build/test && $(CMAKE_COMMAND) -P CMakeFiles/bolt_test.dir/cmake_clean.cmake
.PHONY : test/CMakeFiles/bolt_test.dir/clean

test/CMakeFiles/bolt_test.dir/depend:
	cd /roland.qi/cbtree/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /roland.qi/cbtree /roland.qi/cbtree/test /roland.qi/cbtree/build /roland.qi/cbtree/build/test /roland.qi/cbtree/build/test/CMakeFiles/bolt_test.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : test/CMakeFiles/bolt_test.dir/depend

