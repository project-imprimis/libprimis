
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
#include "testanimmodel.h"
#include "testskel.h"
#include "testslot.h"
#include "testmd5.h"
#include "testragdoll.h"

int main()
{
    testutils();

    testidents();
    testcs();
    test_shadowatlas();
    test_props();
    test_octa();
    test_gltf();
    test_md5();
    test_ragdoll();
    test_animmodel();
    test_skel();
    test_slot();
    test_geom();
    test_geomexts();
    test_matrix();
    return EXIT_SUCCESS;
}
