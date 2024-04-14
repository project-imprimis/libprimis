#include "libprimis.h"
#include "../src/engine/interface/cs.h"

/*
 * Executes every ident, but does not check validity of any values.
 * global identmap is used to find idents available to execute
 */

static void loadcscommands()
{
    initidents();

    initcscmds();
    initmathcmds();
    initcontrolcmds();
    initstrcmds();
    inittextcmds();
    initconsolecmds();
    initoctaeditcmds();
    initoctaworldcmds();
    initoctaworldcmds();
    initrendermodelcmds();
    initrenderglcmds();
    initrenderlightscmds();
    initrendertextcmds();
    initrenderwindowcmds();
    initshadercmds();
    inittexturecmds();
    inithudcmds();
    initheightmapcmds();
    initmenuscmds();
    initzipcmds();

    alias("testalias", " ");

    printf("Adding idents: %lu present\n", idents.size());

}

static void tryexecvars()
{
    printf("===============================================================\n");
    printf("Testing int vars:\n");
    printf("===============================================================\n");
    uint count = 0;
    for(auto & [key, val] : idents)
    {
        if(val.type == Id_Var)
        {
            execute(val.name);
            count++;
        }
    }
    printf("===============================================================\n");
    printf("%u int vars executed\n", count);
    printf("===============================================================\n");
}

static void tryexecfloatvars()
{
    printf("===============================================================\n");
    printf("Testing float vars:\n");
    printf("===============================================================\n");
    uint count = 0;
    for(auto & [key, val] : idents)
    {
        if(val.type == Id_FloatVar)
        {
            execute(val.name);
            count++;
        }
    }
    printf("===============================================================\n");
    printf("%u float vars executed\n", count);
    printf("===============================================================\n");
}

static void tryexecstringvars()
{
    printf("===============================================================\n");
    printf("Testing string vars:\n");
    printf("===============================================================\n");
    uint count = 0;
    for(auto & [key, val] : idents)
    {
        if(val.type == Id_StringVar)
        {
            execute(val.name);
            count++;
        }
    }
    printf("===============================================================\n");
    printf("%u string vars executed\n", count);
    printf("===============================================================\n");
}

static void tryexeccommands()
{
    printf("===============================================================\n");
    printf("Testing command idents:\n");
    printf("===============================================================\n");
    uint count = 0;
    for(auto & [key, val] : idents)
    {
        if(val.type == Id_Command)
        {
            printf("> %s\n", key.c_str());
            execute(val.name);
            count++;
        }
    }
    printf("===============================================================\n");
    printf("%u commands executed\n", count);
    printf("===============================================================\n");
}

static void tryexecother()
{
    printf("===============================================================\n");
    printf("Testing other idents:\n");
    printf("===============================================================\n");
    uint count = 0;
    for(auto & [key, val] : idents)
    {
        if(val.type > Id_Command)
        {
            printf("$ %s\n", key.c_str());
            execute(val.name);
            count++;
        }
    }
    printf("===============================================================\n");
    printf("%u idents executed\n", count);
    printf("===============================================================\n");
}

static void test_clear_command()
{
    printf("Testing clearing commands (aliases)\n");

    alias("testcommand", "echo test");
    execute("testcommand");
    auto itr = idents.find("testcommand");
    assert(itr != idents.end());

    assert((*itr).second.name != nullptr);
    assert((*itr).second.code != nullptr);

    clear_command();

    assert((*itr).second.name == nullptr);
    assert((*itr).second.code == nullptr);
}

void testidents()
{
    loadcscommands();
    tryexecvars();
    tryexecfloatvars();
    tryexecstringvars();
    tryexeccommands();
    tryexecother();

    test_clear_command();
}
