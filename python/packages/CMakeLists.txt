# Python packages
set(PYPKGS
    isce3
    )

foreach(PYPKG ${PYPKGS})
    # Copy package directory to the binary directory for testing.
    file(COPY ${PYPKG} DESTINATION ${ISCE_BUILDPACKAGESDIR})

    # Install package directory.
    install(DIRECTORY ${PYPKG} DESTINATION ${ISCE_PACKAGESDIR})
endforeach()

