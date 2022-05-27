file(GLOB DOT_C *.c)
idf_component_register(
SRCS
  ${DOT_C}
INCLUDE_DIRS
  "."
  "include"
PRIV_REQUIRES
REQUIRES
)

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/mkcrofs
  COMMAND g++ -o ${CMAKE_CURRENT_BINARY_DIR}/mkcrofs ${COMPONENT_DIR}/crofs.cpp ${COMPONENT_DIR}/miniz/miniz.c -I${COMPONENT_DIR}/include -I${COMPONENT_DIR}/miniz
)
add_custom_target(mkcrofs-binary DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/mkcrofs)
add_dependencies(${COMPONENT_LIB} mkcrofs-binary)
