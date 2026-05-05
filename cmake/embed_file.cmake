if(NOT DEFINED INPUT_FILE)
    message(FATAL_ERROR "INPUT_FILE variable is not defined.")
endif()

if(NOT DEFINED OUTPUT_FILE)
    message(FATAL_ERROR "OUTPUT_FILE variable is not defined.")
endif()

if(NOT DEFINED VARIABLE_NAME)
    message(FATAL_ERROR "VARIABLE_NAME variable is not defined.")
endif()

file(READ "${INPUT_FILE}" HEX_DATA HEX)

string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1, " ARRAY_BODY "${HEX_DATA}")

set(HEADER_CONTENT "static const unsigned char ${VARIABLE_NAME}[] = {\n  ${ARRAY_BODY}\n};\n")

file(WRITE "${OUTPUT_FILE}" "${HEADER_CONTENT}")

message(STATUS "Embedded file ${INPUT_FILE} into ${OUTPUT_FILE} as variable ${VARIABLE_NAME}")
