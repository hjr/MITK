function(mitkFunctionGetLibrarySearchPaths search_path intermediate_dir)

  set(_dir_candidates ${MITK_VTK_LIBRARY_DIRS} ${MITK_ITK_LIBRARY_DIRS} ${QT_LIBRARY_DIR}
                      ${QT_LIBRARY_DIR}/../bin ${MITK_BINARY_DIR}/bin ${MITK_BINARY_DIR}/bin/plugins)

  get_property(_additional_paths GLOBAL PROPERTY MITK_ADDITIONAL_LIBRARY_SEARCH_PATHS)
  if(_additional_paths)
    list(APPEND _dir_candidates ${_additional_paths})
  endif()

  if(WIN32)
    if(DCMTK_DIR)
      list(APPEND _dir_candidates "${DCMTK_DIR}/bin")
    endif()
   list(APPEND _dir_candidates "${ITK_DIR}/bin")
  else()
    if(DCMTK_DIR)
      list(APPEND _dir_candidates "${DCMTK_DIR}/lib")
    endif()
    list(APPEND _dir_candidates "${ITK_DIR}/lib")
  endif()

  # The code below is sub-optimal. It makes assumptions about
  # the structure of the build directories, pointed to by
  # the *_DIR variables. Instead, we should rely on package
  # specific "LIBRARY_DIRS" variables, if they exist.
  if(MITK_USE_Python AND CTK_PYTHONQT_INSTALL_DIR)
    list(APPEND _dir_candidates ${CTK_PYTHONQT_INSTALL_DIR}/bin)
  endif()
  if(MITK_USE_Boost AND MITK_USE_Boost_LIBRARIES AND NOT MITK_USE_SYSTEM_Boost)
    list(APPEND _dir_candidates ${Boost_LIBRARY_DIR})
  endif()
  if(ACVD_DIR)
    list(APPEND _dir_candidates ${ACVD_DIR}/bin)
  endif()
  if(ANN_DIR)
    list(APPEND _dir_candidates ${ANN_DIR})
  endif()
  if(CppUnit_DIR)
    list(APPEND _dir_candidates ${CppUnit_DIR})
  endif()
  if(GLUT_DIR)
    list(APPEND _dir_candidates ${GLUT_DIR})
  endif()
  if(GDCM_DIR)
    list(APPEND _dir_candidates ${GDCM_DIR}/bin)
  endif()
  if(GLEW_DIR)
    list(APPEND _dir_candidates ${GLEW_DIR})
  endif()
  if(tinyxml_DIR)
    list(APPEND _dir_candidates ${tinyxml_DIR})
  endif()
  if(OpenCV_DIR)
    list(APPEND _dir_candidates ${OpenCV_DIR}/bin)
  endif()
  if(Poco_DIR)
    list(APPEND _dir_candidates ${Poco_DIR}/lib)
  endif()
  if(Qwt_DIR)
    list(APPEND _dir_candidates ${Qwt_DIR})
  endif()
  if(Qxt_DIR)
    list(APPEND _dir_candidates ${Qxt_DIR})
  endif()
  if(SOFA_DIR)
    list(APPEND _dir_candidates ${SOFA_DIR}/bin)
  endif()
  if(MITK_USE_TOF_PMDO3 OR MITK_USE_TOF_PMDCAMCUBE OR MITK_USE_TOF_PMDCAMBOARD)
    list(APPEND _dir_candidates ${MITK_PMD_SDK_DIR}/plugins)
  endif()

  if(MITK_USE_BLUEBERRY)
    # quick-fix for Bug 16606 : the CTK_RUNTIME_LIB_DIRS variable was removed
    # so check if the variable is empty, otherwise compose the path to CTK starting
    if(NOT "${CTK_RUNTIME_LIBRARY_DIRS}" STREQUAL "")
      list(APPEND _dir_candidates ${CTK_RUNTIME_LIBRARY_DIRS})
    else()
      message(STATUS "CTK Runtime library not set, composing hard-coded path")
      message(STATUS "Using candidate: ${CTK_DIR}/CTK-build/bin" )
      list(APPEND _dir_candidates ${CTK_DIR}/CTK-build/bin )
    endif()
    if(DEFINED CTK_PLUGIN_RUNTIME_OUTPUT_DIRECTORY)
      if(IS_ABSOLUTE "${CTK_PLUGIN_RUNTIME_OUTPUT_DIRECTORY}")
        list(APPEND _dir_candidates "${CTK_PLUGIN_RUNTIME_OUTPUT_DIRECTORY}")
      else()
        list(APPEND _dir_candidates "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CTK_PLUGIN_RUNTIME_OUTPUT_DIRECTORY}")
      endif()
    endif()
  endif()

  if(MITK_LIBRARY_DIRS)
    list(APPEND _dir_candidates ${MITK_LIBRARY_DIRS})
  endif()

  list(REMOVE_DUPLICATES _dir_candidates)

  set(_search_dirs )
  foreach(_dir ${_dir_candidates})
    if(EXISTS "${_dir}/${intermediate_dir}")
      list(APPEND _search_dirs "${_dir}/${intermediate_dir}")
    else()
      list(APPEND _search_dirs ${_dir})
    endif()
  endforeach()

  # Special handling for "internal" search dirs. The intermediate directory
  # might not have been created yet, so we can't check for its existence.
  # Hence we just add it for Windows without checking.
  set(_internal_search_dirs ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/plugins)
  if(WIN32)
    foreach(_dir ${_internal_search_dirs})
      set(_search_dirs ${_dir}/${intermediate_dir} ${_search_dirs})
    endforeach()
  else()
    set(_search_dirs ${_internal_search_dirs} ${_search_dirs})
  endif()
  list(REMOVE_DUPLICATES _search_dirs)

  set(${search_path} ${_search_dirs} PARENT_SCOPE)
endfunction()
