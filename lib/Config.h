#pragma once

#include "Project/Config.h"

//// Profiling ////
// #define PROFILE_FOR_SPECIALIZATION
// #define PROFILE_CALLS
// #define PROFILE_BRANCHES
// #define PRINT_WEBASSEMBLY_EXECUTION


#define PROFILE_EXECUTION_TIME

#define PROFILE_VIRTUAL_MACHINE
#define PROFILE_COMPILATION_SERVICE
#define PROFILE_WASM_SERVICE
#define PROFILE_PREDICTION_SERVICE
#define PROFILE_SIGNATURE_SERVICE
#define PROFILE_COMPILER_SERVER

//// Wiring ////
#if defined(PROFILE_FOR_SPECIALIZATION)
#define PUBLISH_CALLS
#define PUBLISH_RETURNS
#endif
#if defined(PROFILE_BRANCHES)
#define PUBLISH_BRANCHES
#endif
#if defined(PRINT_WEBASSEMBLY_EXECUTION)
#define PUBLISH_STACK
#define PUBLISH_INSTRUCTION
#endif
#if defined(PROFILE_CALLS)
#define PUBLISH_CALLS
#define PUBLISH_RETURNS
#endif

//// Printing (Virtual Machine) ////
// #define PRINT_WASI

//// Printing (Orchestrator|Worker) ////
#define TRACE_ORCHESTRATOR

//// Printing (Networking) ////
// #define TRACE_NETWORKING
