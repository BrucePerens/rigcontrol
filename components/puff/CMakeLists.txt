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
