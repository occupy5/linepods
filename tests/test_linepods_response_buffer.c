#include "linepods_response_buffer.h"

#include <assert.h>

int main(void)
{
    assert(linepods_response_next_capacity(0, 1, 65537) == 4096);
    assert(linepods_response_next_capacity(4097, 4097, 65537) == 4097);
    assert(linepods_response_next_capacity(4097, 4098, 65537) == 8194);
    assert(linepods_response_next_capacity(32768, 40000, 65537) == 65536);
    assert(linepods_response_next_capacity(65536, 65537, 65537) == 65537);
    assert(linepods_response_next_capacity(4096, 65538, 65537) == 0);
    assert(linepods_response_next_capacity(0, 0, 65537) == 0);
    return 0;
}
