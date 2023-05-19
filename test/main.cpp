
#include "libprimis.h"
#include "testutils.h"
#include "testidents.h"
#include "testcs.h"

int main(int argc, char **argv)
{
    testutils();

    testidents();
    testconc();
    return EXIT_SUCCESS;
}
