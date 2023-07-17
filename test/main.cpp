
#include "libprimis.h"
#include "testutils.h"
#include "testidents.h"
#include "testcs.h"
#include "testshadowatlas.h"

int main(int argc, char **argv)
{
    testutils();

    testidents();
    testcs();
    testpacknode();
    return EXIT_SUCCESS;
}
