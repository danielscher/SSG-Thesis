set(SAFE_START_GENERATOR_SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/safe_start_generator.cpp
        ${CMAKE_CURRENT_LIST_DIR}/safe_start_generator.h
        ${CMAKE_CURRENT_LIST_DIR}/start_generation_statistics.h
        ${CMAKE_CURRENT_LIST_DIR}/start_generation_statistics.cpp
)

# Include all files from the verification_methods directory
include(${CMAKE_CURRENT_LIST_DIR}/verification_methods/PlaJAFiles.cmake)
list(APPEND SAFE_START_GENERATOR_SOURCES ${STRENGTHENING_METHODS_SOURCES})

# Include all files from the testing directory
include(${CMAKE_CURRENT_LIST_DIR}/testing/PlaJAFiles.cmake)
list(APPEND SAFE_START_GENERATOR_SOURCES ${TESTING_SOURCES})

# Include strengthening strategy files
include(${CMAKE_CURRENT_LIST_DIR}/strengthening_strategy/PlaJAFiles.cmake)
list(APPEND SAFE_START_GENERATOR_SOURCES ${STRENGTHENING_STRATEGY_SOURCES})

# Include approximation methods files.
include(${CMAKE_CURRENT_LIST_DIR}/approximation_methods/PlaJAFiles.cmake)
list(APPEND SAFE_START_GENERATOR_SOURCES ${APPROXIMATION_METHODS_SOURCES})
