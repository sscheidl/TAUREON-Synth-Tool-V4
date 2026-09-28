cmake_minimum_required(VERSION 3.25)
project(TaureonMidiEngineConsumer LANGUAGES CXX)

find_package(TaureonMidiEngine 0.3.0 EXACT CONFIG REQUIRED)

add_executable(taureon_engine_consumer main.cpp)
target_link_libraries(taureon_engine_consumer PRIVATE TaureonMidiEngine::MidiEngine)

enable_testing()
add_test(NAME taureon_engine_consumer_runs COMMAND taureon_engine_consumer)
