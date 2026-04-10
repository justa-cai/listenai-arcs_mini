# check_memory_overlap.cmake
# Parse a preprocessed linker script and detect overlapping MEMORY regions.
# Usage: cmake -DLINKER_SCRIPT=<path> [-DOVERLAP_WHITELIST="A:B;C:D"] -P check_memory_overlap.cmake
#
# OVERLAP_WHITELIST: semicolon-separated list of "REGION_A:REGION_B" pairs
# that are known safe overlaps (e.g. hardware shared regions).

if(NOT DEFINED LINKER_SCRIPT)
    message(FATAL_ERROR "LINKER_SCRIPT variable is required")
endif()

file(READ "${LINKER_SCRIPT}" _ld_content)

# Extract MEMORY{...} block
string(REGEX MATCH "MEMORY[ \t\r\n]*\\{([^}]*)\\}" _mem_block "${_ld_content}")
set(_mem_body "${CMAKE_MATCH_1}")

if(NOT _mem_body)
    message(STATUS "check_memory_overlap: no MEMORY block found, skipping")
    return()
endif()

# Parse each region line: NAME(attrs) : ORIGIN = <hex>, LENGTH = <hex>
string(REGEX MATCHALL
    "([A-Za-z_][A-Za-z0-9_]*)[ \t]*\\([^)]*\\)[ \t]*:[ \t]*ORIGIN[ \t]*=[ \t]*(0x[0-9A-Fa-f]+)[^,]*,[ \t]*LENGTH[ \t]*=[ \t]*(0x[0-9A-Fa-f]+)"
    _regions "${_mem_body}")

set(_names "")
set(_origins "")
set(_lengths "")
set(_count 0)

foreach(_match ${_regions})
    string(REGEX MATCH
        "([A-Za-z_][A-Za-z0-9_]*)[ \t]*\\([^)]*\\)[ \t]*:[ \t]*ORIGIN[ \t]*=[ \t]*(0x[0-9A-Fa-f]+)[^,]*,[ \t]*LENGTH[ \t]*=[ \t]*(0x[0-9A-Fa-f]+)"
        _m "${_match}")
    set(_name "${CMAKE_MATCH_1}")
    set(_origin "${CMAKE_MATCH_2}")
    set(_length "${CMAKE_MATCH_3}")

    # Convert hex to decimal for arithmetic
    math(EXPR _org "${_origin}" OUTPUT_FORMAT DECIMAL)
    math(EXPR _len "${_length}" OUTPUT_FORMAT DECIMAL)

    if(_len EQUAL 0)
        # Zero-length region, skip overlap check
        continue()
    endif()

    math(EXPR _end "${_org} + ${_len} - 1")

    list(APPEND _names "${_name}")
    list(APPEND _origins ${_org})
    list(APPEND _lengths ${_len})
    list(APPEND _ends ${_end})
    math(EXPR _count "${_count} + 1")
endforeach()

# Build whitelist lookup set
set(_whitelist_pairs "")
if(DEFINED OVERLAP_WHITELIST)
    foreach(_pair ${OVERLAP_WHITELIST})
        list(APPEND _whitelist_pairs "${_pair}")
    endforeach()
endif()

# Check if a pair is whitelisted (order-independent)
function(_is_whitelisted name_a name_b result_var)
    set(_found FALSE)
    foreach(_pair ${_whitelist_pairs})
        if((_pair STREQUAL "${name_a}:${name_b}") OR (_pair STREQUAL "${name_b}:${name_a}"))
            set(_found TRUE)
            break()
        endif()
    endforeach()
    set(${result_var} ${_found} PARENT_SCOPE)
endfunction()

# Check all pairs for overlap
set(_has_error FALSE)
set(_error_msg "")
set(_warn_msg "")

if(_count LESS 2)
    message(STATUS "check_memory_overlap: ${_count} region(s), nothing to check")
    return()
endif()

math(EXPR _last "${_count} - 1")
foreach(_i RANGE 0 ${_last})
    math(EXPR _j_start "${_i} + 1")
    if(_j_start GREATER ${_last})
        continue()
    endif()
    foreach(_j RANGE ${_j_start} ${_last})
        list(GET _names ${_i} _name_i)
        list(GET _origins ${_i} _org_i)
        list(GET _ends ${_i} _end_i)
        list(GET _names ${_j} _name_j)
        list(GET _origins ${_j} _org_j)
        list(GET _ends ${_j} _end_j)

        # Two ranges [a_start, a_end] and [b_start, b_end] overlap
        # iff a_start <= b_end AND b_start <= a_end
        if((_org_i LESS_EQUAL _end_j) AND (_org_j LESS_EQUAL _end_i))
            # Format addresses back to hex for readability
            math(EXPR _org_i_hex "${_org_i}" OUTPUT_FORMAT HEXADECIMAL)
            math(EXPR _end_i_hex "${_end_i} + 1" OUTPUT_FORMAT HEXADECIMAL)
            math(EXPR _org_j_hex "${_org_j}" OUTPUT_FORMAT HEXADECIMAL)
            math(EXPR _end_j_hex "${_end_j} + 1" OUTPUT_FORMAT HEXADECIMAL)

            set(_detail
                "  ${_name_i} [${_org_i_hex}, ${_end_i_hex}) overlaps "
                "${_name_j} [${_org_j_hex}, ${_end_j_hex})\n")

            _is_whitelisted("${_name_i}" "${_name_j}" _wl)
            if(_wl)
                string(APPEND _warn_msg "  [whitelisted] ${_name_i} <-> ${_name_j}\n")
            else()
                string(APPEND _error_msg ${_detail})
                set(_has_error TRUE)
            endif()
        endif()
    endforeach()
endforeach()

if(_warn_msg)
    message(STATUS "check_memory_overlap: known overlaps (whitelisted):\n${_warn_msg}")
endif()

if(_has_error)
    message(FATAL_ERROR
        "MEMORY region address overlap detected!\n"
        "The following regions have overlapping address ranges:\n"
        "${_error_msg}"
        "This will cause silent data corruption at runtime.\n"
        "Check your linker fragment registrations and Kconfig memory layout.\n"
        "If this overlap is intentional (hardware shared region), add the pair to\n"
        "LISTENAI_MEMORY_OVERLAP_WHITELIST in soc/CMakeLists.txt:\n"
        "  set_property(GLOBAL APPEND PROPERTY LISTENAI_MEMORY_OVERLAP_WHITELIST \"REGION_A:REGION_B\")\n"
        "Linker script: ${LINKER_SCRIPT}")
endif()

message(STATUS "check_memory_overlap: ${_count} regions verified, no unexpected overlaps")
