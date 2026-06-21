#!/usr/bin/env python3
from __future__ import annotations

import sys

import FreeCAD  # type: ignore
import Part  # type: ignore


def make_plane_doc():
    doc = FreeCAD.newDocument("FillingS1ContractProbe")
    plane = doc.addObject("Part::Plane", "SupportPlane")
    plane.Length = 4
    plane.Width = 4
    plane.Placement = FreeCAD.Placement(
        FreeCAD.Vector(-2, -2, 0),
        FreeCAD.Rotation(0, 0, 0, 1),
    )
    doc.recompute()
    return doc, plane


def main() -> int:
    mode = sys.argv[-1]
    doc, plane = make_plane_doc()
    try:
        edges = plane.Shape.Faces[0].Edges
        face = plane.Shape.Faces[0]
        print(f"probe:{mode}:before", flush=True)
        if mode == "orders":
            result = Part.makeFilledFace(edges, orders=[(edges[0], 0)])
        elif mode == "surface":
            result = Part.makeFilledFace(edges, surface=face)
        elif mode == "supports":
            result = Part.makeFilledFace(edges, supports=[(edges[0], face)])
        elif mode == "support_order":
            result = Part.makeFilledFace(edges, supports=[(edges[0], face)], orders=[(edges[0], 0)])
        elif mode == "wrapper_order":
            builder = Part.BRepOffsetAPI.MakeFilling()
            for index, edge in enumerate(edges):
                builder.add(edge, 1 if index == 0 else 0, True)
            builder.build()
            result = builder.shape()
        elif mode in {"wrapper_support_order", "wrapper_support_g1", "wrapper_support_g2"}:
            support_order = {"wrapper_support_order": 0, "wrapper_support_g1": 1, "wrapper_support_g2": 2}[mode]
            builder = Part.BRepOffsetAPI.MakeFilling()
            for index, edge in enumerate(edges):
                if index == 0:
                    builder.add(edge, face, support_order, True)
                else:
                    builder.add(edge, 0, True)
            builder.build()
            result = builder.shape()
        elif mode == "wrapper_surface":
            builder = Part.BRepOffsetAPI.MakeFilling()
            builder.loadInitSurface(face)
            for edge in edges:
                builder.add(edge, 0, True)
            builder.build()
            result = builder.shape()
        else:
            raise ValueError(mode)
        print(f"probe:{mode}:after {result.ShapeType} edges={len(result.Edges)}", flush=True)
        return 0
    except Exception as exc:
        print(f"probe:{mode}:error {type(exc).__name__}: {exc}", flush=True)
        return 2
    finally:
        FreeCAD.closeDocument(doc.Name)


raise SystemExit(main())
