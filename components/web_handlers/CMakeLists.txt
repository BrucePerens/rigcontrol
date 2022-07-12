file(GLOB DOT_C *.c)
file(GLOB DOT_CXX *.cxx)
idf_component_register(
SRCS
  ${DOT_C}
  ${DOT_CXX}
INCLUDE_DIRS
  "."
  "include"
PRIV_REQUIRES
REQUIRES
  generic_main
)

# The self-installing web handlers in this component do not have any of their symbols
# referred to by any other code, so there will be nothing that pulls them out of the
# component archive at link time. The --whole-archive flag will force linking of all
# of the objects in the archive. And then it's cleared afterwards so that no other
# component gets the same treatement.
idf_build_set_property(LINK_OPTIONS "-Wl,--whole-archive ${CMAKE_CURRENT_BINARY_DIR}/lib${COMPONENT_NAME}.a -Wl,--no-whole-archive" APPEND)
