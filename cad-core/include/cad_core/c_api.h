#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CadCoreBuffer {
    char* ptr;
    size_t len;
} CadCoreBuffer;

typedef struct CadCoreResult {
    int32_t status;
    CadCoreBuffer json;
    CadCoreBuffer error;
} CadCoreResult;

CadCoreResult cad_core_version_json(void);

CadCoreResult cad_core_recompute_json(const char* request_json, size_t request_json_len);

void cad_core_free_result(CadCoreResult* result);

#ifdef __cplusplus
}
#endif
