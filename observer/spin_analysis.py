"""
spin_analysis.py - Spin axis calculation for tennis ball IMU data.

Converts gyroscope (gx, gy, gz) readings into spherical coordinates
(theta, phi) representing the spin axis direction.
"""

import math


def calc_spin_axis(gx: float, gy: float, gz: float):
    """Convert gyroscope xyz to spherical coordinates (theta, phi).

    Parameters
    ----------
    gx, gy, gz : float
        Gyroscope readings in degrees per second.

    Returns
    -------
    tuple(float or None, float or None)
        (theta, phi) in degrees.
        theta is the polar angle from Z axis (0-180).
        phi is the azimuthal angle in the XY plane (0-360).
        Returns (None, None) if angular velocity is below noise threshold.
    """
    omega = math.sqrt(gx * gx + gy * gy + gz * gz)
    if omega < 1.0:
        return None, None
    nx, ny, nz = gx / omega, gy / omega, gz / omega
    theta = math.degrees(math.acos(max(-1.0, min(1.0, nz))))
    phi = math.degrees(math.atan2(ny, nx))
    if phi < 0:
        phi += 360.0
    return theta, phi
