#include <stddef.h>
#include <stdint.h>

#include "fuzz_target.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    slapt_fuzz_packages_txt(data, size);
    return 0;
}
