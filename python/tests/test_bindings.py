#!/usr/bin/env python3
"""
Test script for GeoDraw Python bindings.

Run from the geodraw/python directory after building:
    cd python
    python tests/test_bindings.py
"""

import sys
import os

# Add the geodraw package to path (for development)
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import numpy as np

def test_basic():
    """Test basic scene creation and geometry."""
    import geodraw

    print("Creating scene...")
    scene = geodraw.Scene()

    # Add a single point
    scene.add_point((0, 0, 0), radius=0.5, color=(1, 0, 0))

    # Add a line
    scene.add_line((0, 0, 0), (5, 0, 0), thickness=2.0, color=(0, 1, 0))

    # Add a polyline from list
    scene.add_polyline([
        (0, 0, 0),
        (1, 1, 0),
        (2, 0, 0),
        (3, 1, 0),
    ], thickness=1.5, color=(1, 1, 0))

    # Add bounding box
    scene.add_bbox((-2, -2, 0), (2, 2, 3), color=(0, 0, 1), filled=False)

    # Add coordinate axes at origin
    scene.add_axes((0, 0, 0), scale=2.0)

    print("Scene created with basic geometry")
    return scene


def test_numpy():
    """Test numpy array support."""
    import geodraw

    print("Testing numpy support...")
    scene = geodraw.Scene()

    # Add points from numpy array
    points = np.array([
        [0, 0, 1],
        [1, 0, 1],
        [2, 0, 1],
        [3, 0, 1],
        [4, 0, 1],
    ], dtype=np.float64)
    scene.add_points(points, radius=0.3, color=(1, 0.5, 0))

    # Add polyline from numpy array
    path = np.array([
        [0, 0, 0],
        [1, 0.5, 0],
        [2, 0, 0],
        [3, 0.5, 0],
        [4, 0, 0],
    ], dtype=np.float64)
    scene.add_polyline(path, thickness=2.0, color=(0, 1, 1))

    # Add multiple lines from Nx2x3 array
    lines = np.array([
        [[0, -1, 0], [0, 1, 0]],
        [[1, -1, 0], [1, 1, 0]],
        [[2, -1, 0], [2, 1, 0]],
        [[3, -1, 0], [3, 1, 0]],
    ], dtype=np.float64)
    scene.add_lines(lines, thickness=1.0, color=(0.5, 0.5, 0.5))

    # Add a mesh (simple quad as two triangles)
    vertices = np.array([
        [-2, -2, 0],
        [ 2, -2, 0],
        [ 2,  2, 0],
        [-2,  2, 0],
    ], dtype=np.float64)
    indices = np.array([0, 1, 2, 0, 2, 3], dtype=np.uint32)
    scene.add_mesh(vertices, indices, color=(0.3, 0.3, 0.8), alpha=0.5)

    print("Numpy geometry added")
    return scene


def test_oriented_bbox():
    """Test oriented bounding box."""
    import geodraw

    print("Testing oriented bounding box...")
    scene = geodraw.Scene()

    # Identity rotation
    scene.add_oriented_bbox(
        center=(0, 0, 1),
        half_extents=(1, 0.5, 0.5),
        color=(1, 0, 0),
        filled=False
    )

    # 45-degree rotation around Z
    angle = np.pi / 4
    rotation = np.array([
        [np.cos(angle), -np.sin(angle), 0],
        [np.sin(angle),  np.cos(angle), 0],
        [0,              0,             1],
    ], dtype=np.float64)

    scene.add_oriented_bbox(
        center=(3, 0, 1),
        half_extents=(1, 0.5, 0.5),
        rotation=rotation,
        color=(0, 1, 0),
        filled=False
    )

    # Add ground for reference
    scene.add_ground((0, 0, 0), width=10, height=10, color=(0.3, 0.3, 0.3))

    print("Oriented bboxes added")
    return scene


def test_pose():
    """Test pose visualization."""
    import geodraw

    print("Testing pose visualization...")
    scene = geodraw.Scene()

    # Create several poses with different orientations
    for i in range(5):
        angle = i * np.pi / 4
        rotation = np.array([
            [np.cos(angle), -np.sin(angle), 0],
            [np.sin(angle),  np.cos(angle), 0],
            [0,              0,             1],
        ], dtype=np.float64)

        scene.add_pose(
            position=(i * 2, 0, 0),
            rotation=rotation,
            scale=1.0,
            thickness=2.0
        )

    print("Poses added")
    return scene


def demo_all():
    """Interactive demo showing all features."""
    import geodraw

    print("\n=== GeoDraw Python Bindings Demo ===\n")

    # Create multiple scenes for comparison
    basic = test_basic()
    numpy_scene = test_numpy()
    bbox_scene = test_oriented_bbox()
    pose_scene = test_pose()

    print("\nOpening viewer with multiple scenes...")
    print("Press 1-4 to toggle scenes, Q or ESC to close.\n")

    geodraw.show([
        ("Basic Geometry", basic),
        ("NumPy Arrays", numpy_scene),
        ("Oriented BBoxes", bbox_scene),
        ("Poses", pose_scene),
    ], title="GeoDraw Python Demo")


def quick_test():
    """Quick visual test with simple geometry."""
    import geodraw

    print("Quick test - opening viewer...")
    scene = geodraw.Scene()

    # Simple test geometry
    scene.add_point((0, 0, 0), radius=0.5, color=(1, 0, 0))
    scene.add_bbox((-1, -1, 0), (1, 1, 2), color=(0, 1, 0))
    scene.add_axes((0, 0, 0), scale=3.0)

    geodraw.show(scene, title="Quick Test")


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--quick":
        quick_test()
    else:
        demo_all()
