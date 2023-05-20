
#include "libprimis.h"
#include "testutils.h"
#include "testidents.h"
#include "testcs.h"

int main(int argc, char **argv)
{
    testutils();

    testidents();
    testcs();
    return EXIT_SUCCESS;
}
