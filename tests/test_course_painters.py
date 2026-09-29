"""Painter point-tree helpers (tools/course_painters.py) on synthetic data; no owned assets required."""
import struct
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from course_painters import point_tree, tree_leaf


def section(nodes, root=0, scale=1.0, origin=(0.0, 0.0), outside=0xffffffff, payloads=2):
    """Painter section: (type, offset) table header, then the tree header and 8-byte nodes."""
    header = 12 + 8 * payloads
    body = bytearray(struct.pack('<3I', header, payloads, 0xc) + bytes(8 * payloads))
    # tree header: +0 scale/origin, +12 node count, +20 root, +24 outside leaf words, +40 nodes
    body += struct.pack('<3fI', scale, *origin, len(nodes)) + bytes(4) + struct.pack('<H2x', root) + struct.pack('<2I', 0, outside) + bytes(8)
    body += b''.join(struct.pack('<4H', *n) for n in nodes)
    return bytes(body)


class CoursePainterTests(unittest.TestCase):
    def test_quadrant_order_and_outside(self):
        # Root (index 0) splits into four leaves: quadrant = (x half << 1) | y half.
        leaves = [[0, 0, q, 0] for q in (0, 1, 0xffff, 1)]
        leaves[2][3] = 0xffff  # quadrant 2 has no payload (0xffffffff)
        nodes = [[(1 << 1) | 1, 2 << 1, 3 << 1, 4 << 1]] + leaves
        tree = point_tree(section(nodes, payloads=2), 2)
        self.assertEqual(tree_leaf(tree, 10, 10), (0, 16384.0))
        self.assertEqual(tree_leaf(tree, 10, 20000)[0], 1)
        self.assertEqual(tree_leaf(tree, 20000, 10)[0], None)
        self.assertEqual(tree_leaf(tree, 20000, 20000)[0], 1)
        self.assertEqual(tree_leaf(tree, -5, 10), (None, None))  # outside word 0xffffffff
        self.assertEqual(tree_leaf(tree, -0.5, 10)[0], 0)  # cvt.w.s truncation keeps (-1, 0) in cell 0

    def test_scale_and_origin(self):
        tree = point_tree(section([[0, 0, 1, 0]], scale=0.5, origin=(-100.0, 50.0)), 2)
        self.assertEqual(tree_leaf(tree, -100, 50), (1, 65536.0))
        self.assertEqual(tree_leaf(tree, -100 + 65536, 50)[0], None)

    def test_invalid_leaf(self):
        with self.assertRaises(ValueError):
            point_tree(section([[0, 0, 5, 0]]), 2)


if __name__ == '__main__':
    unittest.main()
