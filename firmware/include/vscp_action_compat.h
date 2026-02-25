#pragma once
/**
 * @file vscp_action_compat.h
 * @brief Compatibility stub injected via -include to fix VSCP.cpp compilation.
 *
 * When VSCP_CONFIG_ENABLE_DM=0, the vscp-arduino framework conditionally
 * removes vscp_action_set() from vscp_action.h.  PlatformIO still compiles
 * src/VSCP.cpp (the C++ class wrapper) even when it is not referenced by the
 * application, and VSCP.cpp unconditionally calls vscp_action_set().
 *
 * This header is injected before every translation unit via
 *   -include vscp_action_compat.h
 * in platformio.ini build_flags.  It defines vscp_action_set as a no-op
 * macro when DM is disabled so that VSCP.cpp compiles.  The VSCP class is
 * never instantiated by the application; -Wl,--gc-sections + LTO strip it
 * entirely from the linked binary.
 */

#if !defined(VSCP_CONFIG_ENABLE_DM) || (VSCP_CONFIG_ENABLE_DM == 0)
#  ifndef vscp_action_set
#    define vscp_action_set(func)  ((void)(func))
#  endif
#endif
