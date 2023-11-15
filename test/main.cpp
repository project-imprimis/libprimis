
#include "libprimis.h"
#include "testutils.h"
#include "testidents.h"
#include "testcs.h"
#include "testshadowatlas.h"
#include "testprops.h"
#include "testocta.h"

int main(int argc, char **argv)
{
    testutils();

    testidents();
    testcs();
    testpacknode();
    test_props();
    test_octa();
    return EXIT_SUCCESS;
}
