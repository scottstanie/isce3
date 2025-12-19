"""
Simple SLC reader replacement for tests (replaces nisar.products.readers.SLC)

This module provides a minimal SLC reader that uses only isce3 core functionality,
allowing tests to run without the nisar package.
"""

import h5py
import isce3


class SLC:
    """
    Minimal SLC reader that loads orbit and doppler from HDF5 files.

    This replaces nisar.products.readers.SLC for testing purposes.
    """

    def __init__(self, hdf5file: str):
        """
        Initialize SLC reader with HDF5 file path.

        Parameters
        ----------
        hdf5file : str
            Path to HDF5 file containing SLC data
        """
        self.filename = hdf5file

    def getRadarGrid(self, frequency='A'):
        """
        Return RadarGridParameters object.

        Parameters
        ----------
        frequency : str, optional
            Frequency band ('A' or 'B'), default 'A'

        Returns
        -------
        isce3.product.RadarGridParameters
            Radar grid parameters
        """
        return isce3.product.RadarGridParameters(self.filename, frequency)

    def getOrbit(self):
        """
        Extract orbit from HDF5 file.

        Returns
        -------
        isce3.core.Orbit
            Orbit object loaded from HDF5
        """
        with h5py.File(self.filename, 'r') as fid:
            # Try common orbit paths
            orbit_paths = [
                '/science/LSAR/SLC/metadata/orbit',
                '/science/LSAR/RSLC/metadata/orbit',
                '/metadata/orbit',
            ]

            for orbit_path in orbit_paths:
                if orbit_path in fid:
                    return isce3.core.load_orbit_from_h5_group(fid[orbit_path])

            raise ValueError(f"Could not find orbit group in {self.filename}. Tried: {orbit_paths}")

    def getDopplerCentroid(self, frequency='A'):
        """
        Extract Doppler centroid LUT from HDF5 file.

        Parameters
        ----------
        frequency : str, optional
            Frequency band ('A' or 'B'), default 'A'

        Returns
        -------
        isce3.core.LUT2d
            Doppler centroid lookup table
        """
        with h5py.File(self.filename, 'r') as fid:
            # Try common doppler base paths
            base_paths = [
                '/science/LSAR/SLC/metadata/processingInformation/parameters',
                '/science/LSAR/RSLC/metadata/processingInformation/parameters',
                '/metadata/processingInformation/parameters',
            ]

            for base_path in base_paths:
                # Doppler is under frequency-specific subgroup
                doppler_dataset_path = f'{base_path}/frequency{frequency}/dopplerCentroid'

                # But coordinates are often at the base level or frequency level
                # Try both locations
                coord_paths = [
                    (f'{base_path}/zeroDopplerTime', f'{base_path}/slantRange'),
                    (f'{base_path}/frequency{frequency}/zeroDopplerTime',
                     f'{base_path}/frequency{frequency}/slantRange'),
                ]

                if doppler_dataset_path in fid:
                    for zero_doppler_time_path, slant_range_path in coord_paths:
                        if zero_doppler_time_path in fid and slant_range_path in fid:
                            # Load datasets
                            doppler_data = fid[doppler_dataset_path][:]
                            zero_doppler_time = fid[zero_doppler_time_path][:]
                            slant_range = fid[slant_range_path][:]

                            # Get reference epoch from orbit
                            orbit_paths = [
                                base_path.replace('/processingInformation/parameters', '/orbit'),
                                '/science/LSAR/SLC/metadata/orbit',
                                '/science/LSAR/RSLC/metadata/orbit',
                                '/metadata/orbit',
                            ]

                            orbit = None
                            for orbit_path in orbit_paths:
                                if orbit_path in fid:
                                    orbit = isce3.core.load_orbit_from_h5_group(fid[orbit_path])
                                    break

                            if orbit:
                                ref_epoch = orbit.reference_epoch
                            else:
                                # Fallback: use first zero doppler time as reference
                                ref_epoch = isce3.core.DateTime(zero_doppler_time[0])

                            # Convert zero doppler time to seconds relative to reference epoch
                            import numpy as np
                            time_seconds = np.array([
                                (isce3.core.DateTime(zdt) - ref_epoch).total_seconds()
                                for zdt in zero_doppler_time
                            ])

                            # Create LUT2d
                            # Constructor: LUT2d(xvec, yvec, zdata, interp_method="bilinear")
                            # zdata should be 2D: (len(yvec), len(xvec)) based on meshgrid conventions
                            # Our doppler_data is (len(time), len(range)) = (80, 240)
                            # So we pass time as x and range as y: zdata = (len(range), len(time))
                            # Need to transpose: doppler_data.T
                            lut = isce3.core.LUT2d(
                                time_seconds.tolist(),
                                slant_range.tolist(),
                                doppler_data.T.tolist()
                            )

                            return lut

            # If no doppler found, return zero doppler
            return isce3.core.LUT2d()
