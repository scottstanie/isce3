set(TESTFILES
# isce3/antenna/geometry_antenna.py  # REMOVED: depends on nisar
isce3/atmosphere/ionosphere.py
# isce3/atmosphere/tec_products.py  # REMOVED: depends on nisar
# isce3/cal/corner_reflector.py  # REMOVED: depends on nisar
isce3/cal/point_target_info.py
isce3/cal/radar_cross_section.py
isce3/core/gpu_check.py
isce3/core/llh.py
isce3/core/orbit.py
isce3/core/poly2d.py
isce3/core/resample_block_generators.py
isce3/focus/caltone.py
isce3/focus/valid_regions.py
isce3/focus/notch.py
# isce3/geocode/geocode_slc.py  # REMOVED: depends on nisar
isce3/geometry/bounding_polygon.py
# isce3/geometry/bounding_radar_grid.py  # REMOVED: depends on nisar
isce3/geometry/polygons.py
isce3/geometry/radar_grid_spacing.py
isce3/image/resample_slc.py
isce3/io/gdal/gdal_raster.py
isce3/io/background.py
isce3/matchtemplate/import_ampcor.py
isce3/matchtemplate/test_ampcor.py
isce3/product/cf_conventions.py
isce3/signal/cola_windows.py
isce3/signal/doppler_est_func.py
isce3/signal/fir_filter_func.py
isce3/signal/rfi_freq_null.py
isce3/signal/rfi_process_evd.py
# isce3/solid_earth_tides/solid_earth_tides.py  # REMOVED: depends on nisar
)

if(WITH_CUDA)
    list(APPEND TESTFILES
        isce3/cuda/image/resample_slc.py
    )
endif()

foreach(TESTFILE ${TESTFILES})
    get_filename_component(NAMEWE ${TESTFILE} NAME_WE)
    get_filename_component(DIR ${TESTFILE} DIRECTORY)
    string(REPLACE "/" "." TESTNAME test.python.pkg.${DIR}.${NAMEWE})

    add_test(${TESTNAME} ${PYTEST_EXECUTABLE} ${CMAKE_CURRENT_LIST_DIR}/${TESTFILE})
    set_property(TEST ${TESTNAME} PROPERTY ENVIRONMENT
        LD_LIBRARY_PATH=${PYTEST_LD_LIBRARY_PATH}
        DYLD_FALLBACK_LIBRARY_PATH=${PYTEST_DYLD_FALLBACK_LIBRARY_PATH}
        PYTHONPATH=${PYTEST_PYTHONPATH})
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${DIR})
    set_property(TEST ${TESTNAME} PROPERTY WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${DIR})
endforeach()
