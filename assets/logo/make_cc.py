#!/usr/bin/env python3
"""Write cc.svg: the two lower-case c's the logo carries for LBA2 Classic Community.

They are this project's own, drawn by rule and not traced from anything: a fat round
stroke round an ellipse, open on the right, a ball on its upper end and a lighter tail,
leaning right as the logo's letters do. The file is in the units of lba2-logo.svg (one
unit is one pixel of the intro video, the origin the top left of the logo's ink), so the
c's sit where they belong when the two are laid over each other: under the 2, as its
subscript (chosen from mock-ups, the maintainer, 2026-11-17).

Run it from this folder to write cc.svg again.
"""
import math

W, H = 246.41, 160.84          # the logo's ink
X, BASE, HEIGHT = 213.0, 66.0, 20.0   # the first c's centre, the baseline, the c's height
GAP = 0.96                     # from one c's centre to the next, in heights

WEIGHT = 0.25                  # the stroke's width over the height
RX = 0.40                      # the ellipse's half width over the height
OPEN = 52                      # degrees above the horizontal where the stroke starts
LEAN = 0.2                     # how far the top leans right, per unit of height
BALL, BELLY, TAIL = 1.3, 0.12, 0.9   # the upper end's swelling, the left side's, the lower end's thinning
STEPS = 48


def glyph(cx):
    w0 = WEIGHT * HEIGHT / 2
    ry = HEIGHT / 2 - w0
    rx = RX * HEIGHT
    cy = BASE - HEIGHT / 2
    a0, a1 = -math.radians(OPEN), -math.radians(360 - OPEN * 0.9)  # upper right, round the left, to lower right
    pts, nrm, hw = [], [], []
    for i in range(STEPS + 1):
        t = i / STEPS
        a = a0 + (a1 - a0) * t
        x, y = rx * math.cos(a), ry * math.sin(a)
        dx, dy = rx * math.sin(a), -ry * math.cos(a)  # the way the stroke runs (the angle falls)
        l = math.hypot(dx, dy)
        k = (1 + (BALL - 1) * math.exp(-(t / 0.10) ** 2) + BELLY * math.exp(-((t - 0.5) / 0.2) ** 2)
             + (TAIL - 1) * math.exp(-((1 - t) / 0.18) ** 2))
        pts.append((x, y)), nrm.append((dy / l, -dx / l)), hw.append(w0 * k)
    out = [(p[0] + q[0] * w, p[1] + q[1] * w) for p, q, w in zip(pts, nrm, hw)]
    inn = [(p[0] - q[0] * w, p[1] - q[1] * w) for p, q, w in zip(pts, nrm, hw)]

    def cap(i, end):  # a half circle round an end of the stroke
        p, q, w = pts[i], nrm[i], hw[i]
        tx, ty = (-q[1], q[0]) if end else (q[1], -q[0])  # along the stroke, away from it
        s = 1 if end else -1
        return [(p[0] + w * (s * math.cos(b) * q[0] + math.sin(b) * tx), p[1] + w * (s * math.cos(b) * q[1] + math.sin(b) * ty))
                for b in (math.pi * j / 12 for j in range(1, 12))]

    poly = out + cap(STEPS, 1) + inn[::-1] + cap(0, 0)
    return [(cx + x - LEAN * y, cy + y) for x, y in poly]


def path(poly):
    """A closed curve through every point (Catmull-Rom as cubics): only M, C and Z."""
    n = len(poly)
    d = 'M%.2f %.2f' % poly[0]
    for i in range(n):
        p0, p1, p2, p3 = poly[i - 1], poly[i], poly[(i + 1) % n], poly[(i + 2) % n]
        d += 'C%.2f %.2f %.2f %.2f %.2f %.2f' % (p1[0] + (p2[0] - p0[0]) / 6, p1[1] + (p2[1] - p0[1]) / 6,
                                                p2[0] - (p3[0] - p1[0]) / 6, p2[1] - (p3[1] - p1[1]) / 6, p2[0], p2[1])
    return d + 'Z'


if __name__ == '__main__':
    with open('cc.svg', 'w') as f:
        f.write('''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 %.2f %.2f">
  <!-- The cc of LBA2 Classic Community, in the units and the place of lba2-logo.svg: lay it
       over that file. Written by make_cc.py; src/video_logo.cpp reads the two paths. -->
  <path id="c1" fill="#f37d61" d="%s"/>
  <path id="c2" fill="#eb5941" d="%s"/>
</svg>
''' % (W, H, path(glyph(X)), path(glyph(X + GAP * HEIGHT))))
