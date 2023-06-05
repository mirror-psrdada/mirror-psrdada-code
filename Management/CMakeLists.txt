
if(CUDAToolkit_FOUND)
  set(linkflags "-L${CUDAToolkit_LIBRARY_DIR} -lcudart")
endif(CUDAToolkit_FOUND)

if((RDMACM_FOUND) AND (IBVERBS_FOUND))
  set(linkflags "${linkflags} ${IBVERBS_LIBRARIES} ${RDMACM_LIBRARIES}")
endif((RDMACM_FOUND) AND (IBVERBS_FOUND))

if(HWLOC_FOUND)
  set(linkflags "${linkflags} -L${HWLOC_LIBDIR} -l${HWLOC_LIBRARIES}")
endif(HWLOC_FOUND)

configure_file(cmake_psrdada_ldflags.in psrdada_ldflags @ONLY)
configure_file(cmake_psrdada_cflags.in psrdada_cflags @ONLY)

install(
  PROGRAMS
    ${PROJECT_BINARY_DIR}/Management/psrdada_cflags
    ${PROJECT_BINARY_DIR}/Management/psrdada_ldflags
  DESTINATION
    bin
)

configure_file(cmake_psrdada.pc.in psrdada.pc @ONLY)

install(
  FILES
    ${PROJECT_BINARY_DIR}/Management/psrdada.pc
  DESTINATION
    lib/pkgconfig
)