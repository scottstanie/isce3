#/usr/bin/env python3

import h5py
import numpy as np
import numpy.testing as npt
import isce3.ext.isce3 as isce
from isce3.core import load_orbit_from_h5_group
from iscetest import data as test_data_dir
from pathlib import Path
import json

from isce3.cal.point_target_info import analyze_point_target, tofloatvals

c = isce.core.speed_of_light

def load_h5(filename):
    filename = str(filename)
    f = h5py.File(filename, 'r')

    # load range-compressed signal data
    signal_data = f["data"][()]

    # load orbit
    orbit = load_orbit_from_h5_group(f["orbit"])

    # load Doppler
    doppler = isce.core.LUT2d.load_from_h5(f["doppler"], "doppler")

    # load radar grid parameters
    lines, samples = signal_data.shape
    sensing_start_time = f["time_of_first_pulse"][()]
    azimuth_spacing = f["pulse_spacing"][()]
    two_way_range_delay = f["two_way_range_delay"][()]
    range_sampling_rate = f["range_sample_rate"][()]
    center_frequency = f["center_frequency"][()]
    look_side = f["look_side"][()]

    near_range = c / 2. * two_way_range_delay
    range_spacing = c / (2. * range_sampling_rate)
    wavelength = c / center_frequency
    prf = 1. / azimuth_spacing

    radar_grid = isce.product.RadarGridParameters(
            sensing_start_time, wavelength, prf, near_range, range_spacing,
            look_side, lines, samples, orbit.reference_epoch)

    # load fixed-height DEM
    terrain_height = f["terrain_height"][()]
    dem = isce.geometry.DEMInterpolator(terrain_height)

    # apply dry troposphere model?
    apply_atm_model = f["atmosphere_model"][()]
    if apply_atm_model:
        dry_tropo_model = "tsx"
    else:
        dry_tropo_model = "nodelay"

    # load point target position
    target_azimuth = f["target_azimuth"][()]
    target_range = f["target_range"][()]

    return {"signal_data": signal_data,
            "radar_grid": radar_grid,
            "orbit": orbit,
            "doppler": doppler,
            "center_frequency": center_frequency,
            "range_sampling_rate": range_sampling_rate,
            "dem": dem,
            "dry_tropo_model": dry_tropo_model,
            "target_azimuth": target_azimuth,
            "target_range": target_range}

# range bandwidth (Hz)
B = 20e6

# desired azimuth resolution (m)
azimuth_res = 6.

# output chip size
nchip = 129


def focus_point_target(d, phase_arithmetic="double"):
    """
    Focus the simulated point target to a chip centered on the target.

    Returns the complex chip, the debug height layer, and the output radar
    geometry.
    """
    radar_grid = d["radar_grid"]
    orbit = d["orbit"]
    doppler = d["doppler"]

    # create 9-point Knab kernel
    # use tabulated kernel for performance
    kernel = isce.core.KnabKernel(9., B / d["range_sampling_rate"])
    kernel = isce.core.TabulatedKernelF32(kernel, 2048)

    # create output radar grid centered on the target
    dt = radar_grid.az_time_interval
    dr = radar_grid.range_pixel_spacing
    t0 = d["target_azimuth"] - 0.5 * (nchip - 1) * dt
    r0 = d["target_range"] - 0.5 * (nchip - 1) * dr
    out_grid = isce.product.RadarGridParameters(
            t0, radar_grid.wavelength, radar_grid.prf, r0, dr,
            radar_grid.lookside, nchip, nchip, orbit.reference_epoch)

    # init output buffer
    out = np.empty((nchip, nchip), np.complex64)
    # and debug height layer
    height = np.empty(out.shape, np.float32)

    # collect input & output radar_grid, orbit, and Doppler
    in_geometry = isce.container.RadarGeometry(radar_grid, orbit, doppler)
    out_geometry = isce.container.RadarGeometry(out_grid, orbit, doppler)

    # focus to output grid
    err = isce.focus.backproject(out, out_geometry, d["signal_data"],
            in_geometry, d["dem"], d["center_frequency"], azimuth_res, kernel,
            d["dry_tropo_model"], height=height,
            phase_arithmetic=phase_arithmetic)

    assert not err
    return out, height, out_geometry


def test_backproject():
    # load point target simulation data
    filename = Path(test_data_dir) / "point-target-sim-rc.h5"
    d = load_h5(filename)

    radar_grid = d["radar_grid"]
    orbit = d["orbit"]
    dem = d["dem"]
    target_azimuth = d["target_azimuth"]
    dt = radar_grid.az_time_interval
    dr = radar_grid.range_pixel_spacing

    # how much to upsample the output for point target analysis
    upsample_factor = 128

    out, height, out_geometry = focus_point_target(d)

    # We used a constant DEM height, so make sure the debug height layer
    # contains that value everywhere.
    npt.assert_allclose(height, dem.ref_height)

    # remove range carrier
    kr = 4. * np.pi / radar_grid.wavelength
    r = np.array(out_geometry.slant_range)
    out *= np.exp(-1j * kr * r)

    info, _ = analyze_point_target(out, nchip//2, nchip//2, nov=upsample_factor,
            chipsize=nchip//2)
    tofloatvals(info)

    # print point target info
    print(json.dumps(info, indent=2))

    # range resolution (m)
    range_res = c / (2. * B)

    # range position error & -3 dB main lobe width (m)
    range_err = dr * info["range"]["offset"]
    range_width = dr * info["range"]["resolution"]

    # azimuth position error & -3 dB main lobe width (m)
    _, vel = orbit.interpolate(target_azimuth)
    azimuth_err = dt * info["azimuth"]["offset"] * np.linalg.norm(vel)
    azimuth_width = dt * info["azimuth"]["resolution"] * np.linalg.norm(vel)

    # require positioning error < resolution/128
    assert(range_err < range_res / 128.)
    assert(azimuth_err < azimuth_res / 128.)

    # require 3dB width in range to be <= range resolution
    assert(range_width <= range_res)

    # azimuth response is spread slightly by the antenna pattern so the
    # threshold is slightly higher - see
    # https://github.jpl.nasa.gov/bhawkins/nisar-notebooks/blob/master/Azimuth%20Resolution.ipynb
    assert(azimuth_width <= 6.62)


def test_backproject_double_float():
    # The double-float (df64) arithmetic runs the delay & carrier phase chain
    # on float32 operations only, with expected carrier phase error on the
    # order of a microradian. Require the focused image to match the double
    # precision result far below any interferometric requirement.
    filename = Path(test_data_dir) / "point-target-sim-rc.h5"
    d = load_h5(filename)

    out_f64, height_f64, _ = focus_point_target(d)
    out_df64, height_df64, _ = focus_point_target(
            d, phase_arithmetic="double_float")

    # the geometry path is identical in both modes
    npt.assert_array_equal(height_f64, height_df64)

    # complex agreement over the whole chip, relative to the focused peak
    # (-80 dB; the df64 phase error alone would allow much tighter)
    peak = np.abs(out_f64).max()
    assert np.abs(out_df64 - out_f64).max() < 1e-4 * peak

    # phase agreement at the focused peak
    ipk = np.unravel_index(np.argmax(np.abs(out_f64)), out_f64.shape)
    dphi = np.angle(out_df64[ipk] * np.conj(out_f64[ipk]))
    assert np.abs(dphi) < 1e-4
