cmake_minimum_required(VERSION 3.16)
project(primis VERSION 0.0.1 DESCRIPTION "libprimis")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake/modules")

find_package(SDL2 REQUIRED)
find_package(GLEW REQUIRED)
find_package(OpenGL REQUIRED COMPONENTS OpenGL)

include_directories(${SDL2_INCLUDE_DIRS} ${GLEW_INCLUDE_DIRS} ${GLUT_INCLUDE_DIRS})
include_directories(src/engine)
include_directories(src/shared)
include_directories(test)

add_library(primis SHARED
        src/engine/interface/command.cpp
        src/engine/interface/console.cpp
        src/engine/interface/console.h
        src/engine/interface/control.cpp
        src/engine/interface/control.h
        src/engine/interface/cs.h
        src/engine/interface/cubestd.cpp
        src/engine/interface/input.cpp
        src/engine/interface/input.h
        src/engine/interface/menus.cpp
        src/engine/interface/menus.h
        src/engine/interface/sound.cpp
        src/engine/interface/textedit.cpp
        src/engine/interface/textedit.h
        src/engine/interface/ui.cpp
        src/engine/interface/ui.h
        src/engine/model/animmodel.cpp
        src/engine/model/animmodel.h
        src/engine/model/gltf.cpp
        src/engine/model/gltf.h
        src/engine/model/gltfloader.cpp
        src/engine/model/gltfloader.h
        src/engine/model/md5.cpp
        src/engine/model/md5.h
        src/engine/model/model.h
        src/engine/model/obj.cpp
        src/engine/model/obj.h
        src/engine/model/ragdoll.cpp
        src/engine/model/ragdoll.h
        src/engine/model/skelmodel.cpp
        src/engine/model/skelmodel.h
        src/engine/model/vertmodel.cpp
        src/engine/model/vertmodel.h
        src/engine/render/aa.cpp
        src/engine/render/aa.h
        src/engine/render/ao.cpp
        src/engine/render/ao.h
        src/engine/render/csm.cpp
        src/engine/render/csm.h
        src/engine/render/grass.cpp
        src/engine/render/grass.h
        src/engine/render/hdr.cpp
        src/engine/render/hdr.h
        src/engine/render/hud.cpp
        src/engine/render/hud.h
        src/engine/render/imagedata.cpp
        src/engine/render/imagedata.h
        src/engine/render/normal.cpp
        src/engine/render/octarender.cpp
        src/engine/render/octarender.h
        src/engine/render/postfx.cpp
        src/engine/render/postfx.h
        src/engine/render/radiancehints.cpp
        src/engine/render/radiancehints.h
        src/engine/render/renderalpha.cpp
        src/engine/render/rendergl.cpp
        src/engine/render/rendergl.h
        src/engine/render/renderlights.cpp
        src/engine/render/renderlights.h
        src/engine/render/rendermodel.cpp
        src/engine/render/rendermodel.h
        src/engine/render/renderparticles.cpp
        src/engine/render/renderparticles.h
        src/engine/render/rendersky.cpp
        src/engine/render/rendersky.h
        src/engine/render/rendertext.cpp
        src/engine/render/rendertext.h
        src/engine/render/rendertimers.cpp
        src/engine/render/rendertimers.h
        src/engine/render/renderva.cpp
        src/engine/render/renderva.h
        src/engine/render/renderwindow.cpp
        src/engine/render/renderwindow.h
        src/engine/render/shader.cpp
        src/engine/render/shader.h
        src/engine/render/stain.cpp
        src/engine/render/stain.h
        src/engine/render/texture.cpp
        src/engine/render/texture.h
        src/engine/render/vacollect.cpp
        src/engine/render/vacollect.h
        src/engine/render/water.cpp
        src/engine/render/water.h
        src/engine/world/bih.cpp
        src/engine/world/bih.h
        src/engine/world/dynlight.cpp
        src/engine/world/dynlight.h
        src/engine/world/entities.cpp
        src/engine/world/entities.h
        src/engine/world/heightmap.cpp
        src/engine/world/light.cpp
        src/engine/world/light.h
        src/engine/world/material.cpp
        src/engine/world/material.h
        src/engine/world/mpr.cpp
        src/engine/world/mpr.h
        src/engine/world/octaedit.cpp
        src/engine/world/octaedit.h
        src/engine/world/octaworld.cpp
        src/engine/world/octaworld.h
        src/engine/world/physics.cpp
        src/engine/world/physics.h
        src/engine/world/raycube.cpp
        src/engine/world/raycube.h
        src/engine/world/world.cpp
        src/engine/world/world.h
        src/engine/world/worldio.cpp
        src/libprimis-headers/command.h
        src/libprimis-headers/consts.h
        src/libprimis-headers/cube.h
        src/libprimis-headers/ents.h
        src/libprimis-headers/geom.h
        src/libprimis-headers/iengine.h
        src/libprimis-headers/octa.h
        src/libprimis-headers/sound.h
        src/libprimis-headers/tools.h
        src/shared/geom.cpp
        src/shared/geomexts.h
        src/shared/glemu.cpp
        src/shared/glemu.h
        src/shared/glexts.h
        src/shared/matrix.cpp
        src/shared/stream.cpp
        src/shared/stream.h
        src/shared/tools.cpp
        src/shared/zip.cpp)

set_target_properties(primis PROPERTIES VERSION ${PROJECT_VERSION})
set_target_properties(primis PROPERTIES PUBLIC_HEADER src/libprimis-headers/cube.h)
set_target_properties(primis PROPERTIES PUBLIC_HEADER src/libprimis-headers/iengine.h)
install(TARGETS primis
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES ${CMAKE_BINARY_DIR}/libprimis.pc
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/pkgconfig)

add_dependencies(primis OpenGL::OpenGL)
