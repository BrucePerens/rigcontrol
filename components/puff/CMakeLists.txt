file(GLOB DOT_C *.c)
file(GLOB DOT_CPP *.cpp)
idf_component_register(
SRCS
  ${DOT_C}
  ${DOT_CPP}
INCLUDE_DIRS
  "."
  "include"
PRIV_REQUIRES
REQUIRES
)

# add_custom_command(
#   OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/mkcrofs
#   COMMAND g++ -o ${CMAKE_CURRENT_BINARY_DIR}/mkcrofs ${COMPONENT_DIR}/crofs.cpp ${COMPONENT_DIR}/miniz/miniz.c -I${COMPONENT_DIR}/include -I${COMPONENT_DIR}/miniz
# )
# add_custom_target(mkcrofs-binary DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/mkcrofs)
# add_dependencies(${COMPONENT_LIB} mkcrofs-binary)
# 
# add_custom_command(
#   OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/crofs.image
#   COMMAND ${CMAKE_CURRENT_BINARY_DIR}/mkcrofs ${CMAKE_CURRENT_BINARY_DIR}/crofs.image ${PROJECT_DIR}/${CONFIG_CROFS_SRC} -l${CONFIG_CROFS_LEVEL}
# )
# target_add_binary_data(${COMPONENT_LIB} ${CMAKE_CURRENT_BINARY_DIR}/crofs.image BINARY)
