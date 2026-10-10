// Native ImageIO check. Run on macOS; retain the JSON report even on failure.
import Foundation
import CoreGraphics
import ImageIO

guard CommandLine.arguments.count == 2 else {
    fputs("Usage: swift apple-reader.swift <artifact-directory>\n", stderr)
    exit(2)
}
let directory = URL(fileURLWithPath: CommandLine.arguments[1], isDirectory: true)
let files = try FileManager.default.contentsOfDirectory(at: directory,
    includingPropertiesForKeys: nil).filter { $0.pathExtension == "heic" }
    .sorted { $0.lastPathComponent < $1.lastPathComponent }
var results: [[String: Any]] = []
var failures = files.count == 19 ? 0 : 1
for file in files {
    var row: [String: Any] = ["file": file.lastPathComponent]
    let expectedBits = Int(file.lastPathComponent.split(separator: "-")[0])!
    if let source = CGImageSourceCreateWithURL(file as CFURL, nil),
       let image = CGImageSourceCreateImageAtIndex(source, 0,
           [kCGImageSourceShouldCacheImmediately: true] as CFDictionary) {
        row["width"] = image.width
        row["height"] = image.height
        row["bitsPerComponent"] = image.bitsPerComponent
        row["bitsPerPixel"] = image.bitsPerPixel
        if let name = image.colorSpace?.name {
            row["colorSpaceName"] = name as String
        } else {
            row["colorSpaceName"] = "unnamed"
        }
        if let pixels = image.dataProvider?.data {
            row["pixelDataBytes"] = CFDataGetLength(pixels)
        } else {
            row["pixelDataBytes"] = 0
            row["error"] = "ImageIO did not expose decoded pixel data"
        }
        if let space = image.colorSpace,
           let icc = space.copyICCData() {
            let original = try Data(contentsOf: file.deletingPathExtension().appendingPathExtension("icc"))
            row["readerICCBytes"] = CFDataGetLength(icc)
            row["originalICCBytes"] = original.count
            // ImageIO may normalize an ICC; record this rather than asserting
            // that every reader exposes the original container serialization.
            row["readerICCMatchesOriginal"] = (icc as Data) == original
        } else {
            row["error"] = "ImageIO did not expose an ICC color space"
        }
        if image.width != 1025 || image.height != 257 {
            row["error"] = "ImageIO did not apply clean aperture"
        }
        if image.bitsPerComponent < expectedBits {
            row["error"] = "ImageIO decoded at reduced component precision"
        }
    } else {
        row["error"] = "ImageIO failed to decode the image"
    }
    if row["error"] != nil { failures += 1 }
    results.append(row)
}
let report: [String: Any] = ["operatingSystem": ProcessInfo.processInfo.operatingSystemVersionString,
    "reader": "Apple ImageIO / CoreGraphics", "failures": failures, "results": results]
let data = try JSONSerialization.data(withJSONObject: report, options: [.prettyPrinted, .sortedKeys])
let destination = directory.deletingLastPathComponent().appendingPathComponent("apple-reader.json")
try data.write(to: destination)
print(String(decoding: data, as: UTF8.self))
exit(failures == 0 ? 0 : 1)
