#pragma once

#include <gdal_priv.h>
#include <gdal_version.h>

namespace isce3 { namespace io { namespace gdal { namespace detail {

/**
 * Set geotransform on a GDALDataset, using the appropriate API for the GDAL version.
 *
 * In GDAL 3.12+, calls the virtual SetGeoTransform(const GDALGeoTransform&) method
 * directly to ensure proper polymorphic dispatch. In older versions, calls the
 * virtual SetGeoTransform(double*) method.
 */
inline CPLErr setGeoTransform(GDALDataset* dataset, const double* transform)
{
#if GDAL_VERSION_MAJOR >= 4 || (GDAL_VERSION_MAJOR == 3 && GDAL_VERSION_MINOR >= 12)
    GDALGeoTransform gt(transform);
    return dataset->SetGeoTransform(gt);
#else
    return dataset->SetGeoTransform(const_cast<double*>(transform));
#endif
}

/**
 * Get geotransform from a GDALDataset, using the appropriate API for the GDAL version.
 */
inline CPLErr getGeoTransform(GDALDataset* dataset, double* transform)
{
#if GDAL_VERSION_MAJOR >= 4 || (GDAL_VERSION_MAJOR == 3 && GDAL_VERSION_MINOR >= 12)
    GDALGeoTransform gt;
    CPLErr err = dataset->GetGeoTransform(gt);
    if (err == CE_None) {
        transform[0] = gt.xorig;
        transform[1] = gt.xscale;
        transform[2] = gt.xrot;
        transform[3] = gt.yorig;
        transform[4] = gt.yrot;
        transform[5] = gt.yscale;
    }
    return err;
#else
    return dataset->GetGeoTransform(transform);
#endif
}

}}}}
