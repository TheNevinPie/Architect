"""M1 COMPLETED: world->screen math checks (independent implementation).

Reimplements the experiments/render-poc/w2s_math.h pipeline from the
formulas (not by porting the C++) and asserts physical facts: centered
target, screen edges, behind-camera rejection, resize scaling.

Run: python tests/python/test_w2s.py
"""

import math
import os
import sys

FAILURES = []


def check(name, cond, detail=""):
    print(("PASS " if cond else "FAIL ") + name + (" | " + str(detail) if detail and not cond else ""))
    if not cond:
        FAILURES.append(name)


def sub(a, b): return (a[0] - b[0], a[1] - b[1], a[2] - b[2])
def dot(a, b): return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
def cross(a, b): return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
def norm(v):
    l = math.sqrt(dot(v, v))
    return (v[0] / l, v[1] / l, v[2] / l)


def project(world, eye, forward, fov_y_deg, aspect, near, far, w_px, h_px):
    """Row-vector LH pipeline: world -> view -> clip -> screen. Returns None if behind."""
    f = norm(forward)
    r = norm(cross((0, 1, 0), f))
    u = cross(f, r)
    vx, vy, vz = dot(world, r) - dot(eye, r), dot(world, u) - dot(eye, u), dot(world, f) - dot(eye, f)
    ys = 1.0 / math.tan(math.radians(fov_y_deg) / 2.0)
    xs = ys / aspect
    cx, cy = vx * xs, vy * ys
    cz = vz * far / (far - near) - near * far / (far - near)
    cw = vz
    if cw <= 0:
        return None
    return ((cx / cw * 0.5 + 0.5) * w_px, (0.5 - cy / cw * 0.5) * h_px, cz / cw)


EYE = (0.0, 0.0, 0.0)
FWD = (0.0, 0.0, 1.0)
P = dict(eye=EYE, forward=FWD, fov_y_deg=90.0, aspect=1.0, near=0.1, far=100.0, w_px=800.0, h_px=800.0)

c = project((0, 0, 5), **P)
check("target ahead projects to center", c is not None and abs(c[0] - 400) < 1e-6 and abs(c[1] - 400) < 1e-6, c)
check("depth in (0,1)", c is not None and 0.0 < c[2] < 1.0, c)

e = project((5, 0, 5), **P)
check("x=+z at 45deg hits right edge", e is not None and abs(e[0] - 800) < 1e-6, e)
t = project((0, 5, 5), **P)
check("y=+z at 45deg hits top edge", t is not None and abs(t[1] - 0) < 1e-6, t)

check("point behind camera rejected", project((0, 0, -5), **P) is None)
check("point at eye plane rejected", project((0, 0, 0), **P) is None)

# resize: same world point, wider aspect -> x moves toward center
narrow = project((5, 0, 5), **dict(P, aspect=1.0))
wide = project((5, 0, 5), **dict(P, aspect=2.0, w_px=1600.0))
check("aspect change shifts x inward", narrow is not None and wide is not None and wide[0] < 1600.0, (narrow, wide))

# camera translation: moving the eye +x shifts a fixed cube left on screen
left = project((0, 0, 5), **dict(P, eye=(2.0, 0.0, 0.0)))
check("eye strafe shifts projection", left is not None and left[0] < 400.0, left)

# cube corners: unit cube 8 corners, all in front -> all project, spanning pixels
corners = [(x, y, z) for x in (10.0, 11.0) for y in (0.0, 1.0) for z in (5.0, 6.0)]
pts = [project(p, **dict(P, eye=(10.5, 0.5, 0.0))) for p in corners]
check("all cube corners project", all(p is not None for p in pts))
xs = [p[0] for p in pts]
check("cube spans pixels horizontally", max(xs) - min(xs) > 1.0, (min(xs), max(xs)))

print("%d failure(s)" % len(FAILURES))
sys.exit(1 if FAILURES else 0)
