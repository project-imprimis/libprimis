
#include "libprimis.h"
#include "testutils.h"
#include "testidents.h"
#include "testcs.h"
#include "testmatrix.h"
#include "testshadowatlas.h"
#include "testprops.h"
#include "testocta.h"
#include "testgeom.h"
#include "testgeomexts.h"
#include "testgltfloader.h"
#include "testskel.h"
#include "testmd5.h"

int main()
{
    testutils();

    testidents();
    testcs();
    testpacknode();
    test_props();
    test_octa();
    test_gltf();
    test_md5();
    test_skel();
    test_geom();
    test_geomexts();
    test_matrix();
    return EXIT_SUCCESS;
}
