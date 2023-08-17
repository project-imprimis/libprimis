
#include "libprimis.h"
#include "testutils.h"
#include "testidents.h"
#include "testcs.h"
#include "testshadowatlas.h"
#include "testprops.h"

int main(int argc, char **argv)
{
    testutils();

    testidents();
    testcs();
    testpacknode();
    test_props();
    return EXIT_SUCCESS;
}
