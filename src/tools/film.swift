// SPDX-License-Identifier: GPL-3.0-only
// Mutable Dreams for Trimui Brick, by Kid Bunka: the promo film's encoder (1.5).
// usage: swift tools/film.swift FRAMES TITLES.tsv wide|short MUSIC.wav OUT.mp4 FONTS_DIR
// FRAMES: what brick-headless's "film" command writes (a pipe): per frame 4 bytes of the held buttons, then the stage's
// 1024 x 768 pixels (BGRA). Each frame is laid into the film's picture - the stage large, beside or above it the
// Brick's buttons lighting up as they are pressed, the scene's title and line from TITLES.tsv (start, end, title,
// line; "@card" as the title: a full card over everything, its line split at "|") - and encoded as H.264 at 30
// frames a second; then the music (the same run's WAV) goes in as AAC. wide: 1920 x 1080, short: 1080 x 1920.
import AVFoundation
import CoreGraphics
import CoreText
import Foundation

let args = CommandLine.arguments
guard args.count >= 7 else { fputs("usage: film.swift FRAMES TITLES.tsv wide|short MUSIC.wav OUT.mp4 FONTS_DIR\n", stderr); exit(2) }
let framesPath = args[1], titlesPath = args[2], wide = args[3] == "wide", wavPath = args[4], outPath = args[5], fontsDir = args[6]
let SW = 1024, SH = 768, FPS: Int32 = 30
let W = wide ? 1920 : 1080, H = wide ? 1080 : 1920

for f in ["Nunito.ttf", "BarlowSemiCondensed-Bold.ttf", "BarlowSemiCondensed-SemiBold.ttf"] {
    CTFontManagerRegisterFontsForURL(URL(fileURLWithPath: fontsDir + "/" + f) as CFURL, .process, nil)
}
func font(_ name: String, _ size: CGFloat, weight: CGFloat = 0) -> CTFont {
    if name == "Nunito" {
        let d = CTFontDescriptorCreateWithAttributes([kCTFontFamilyNameAttribute: "Nunito",
                                                      kCTFontTraitsAttribute: [kCTFontWeightTrait: weight]] as CFDictionary)
        return CTFontCreateWithFontDescriptor(d, size, nil)
    }
    return CTFontCreateWithName(name as CFString, size, nil)
}

struct Title { var t0: Double; var t1: Double; var title: String; var line: String }
var titles: [Title] = []
if let s = try? String(contentsOfFile: titlesPath, encoding: .utf8) {
    for l in s.split(separator: "\n") where !l.hasPrefix("#") {
        let p = l.split(separator: "\t", omittingEmptySubsequences: false).map(String.init)
        if p.count >= 3, let a = Double(p[0]), let b = Double(p[1]) {
            titles.append(Title(t0: a, t1: b, title: p[2], line: p.count > 3 ? p[3] : ""))
        }
    }
}

let rgb = CGColorSpace(name: CGColorSpace.sRGB)!
func col(_ r: Int, _ g: Int, _ b: Int, _ a: CGFloat = 1) -> CGColor {
    CGColor(colorSpace: rgb, components: [CGFloat(r) / 255, CGFloat(g) / 255, CGFloat(b) / 255, a])!
}
let ink = col(236, 238, 230), dim = col(150, 160, 170), green = col(150, 230, 160), night = col(5, 8, 16)

// text: drawn in a top-left coordinate system (the context is flipped); returns the height used
func text(_ c: CGContext, _ s: String, _ f: CTFont, _ color: CGColor, x: CGFloat, y: CGFloat, maxW: CGFloat, center: Bool = false, alpha: CGFloat = 1) -> CGFloat {
    let attr = NSAttributedString(string: s, attributes: [NSAttributedString.Key(kCTFontAttributeName as String): f,
                                                          NSAttributedString.Key(kCTForegroundColorAttributeName as String): color.copy(alpha: color.alpha * alpha)!])
    let setter = CTFramesetterCreateWithAttributedString(attr)
    let size = CTFramesetterSuggestFrameSizeWithConstraints(setter, CFRange(location: 0, length: 0), nil, CGSize(width: maxW, height: 2000), nil)
    c.saveGState()
    c.textMatrix = .identity
    c.translateBy(x: 0, y: y + size.height)
    c.scaleBy(x: 1, y: -1)
    let box = CGRect(x: center ? x - size.width / 2 : x, y: 0, width: ceil(size.width) + 1, height: ceil(size.height) + 1)
    let frame = CTFramesetterCreateFrame(setter, CFRange(location: 0, length: 0), CGPath(rect: box, transform: nil), nil)
    CTFrameDraw(frame, c)
    c.restoreGState()
    return size.height
}
func rrect(_ c: CGContext, _ r: CGRect, _ rad: CGFloat, _ fill: CGColor) {
    c.setFillColor(fill)
    c.addPath(CGPath(roundedRect: r, cornerWidth: rad, cornerHeight: rad, transform: nil))
    c.fillPath()
}

// the Brick's buttons, lit as pressed (bits as brick_core.h: UP DOWN LEFT RIGHT A B X Y L1 R1 L2 R2 START SELECT MENU F1 F2 FN)
var lit = [CGFloat](repeating: 0, count: 18)
func buttons(_ c: CGContext, _ ox: CGFloat, _ oy: CGFloat, _ s: CGFloat) {
    let body = col(40, 46, 58), key = col(70, 78, 92)
    func k(_ i: Int) -> CGColor { lit[i] > 0.01 ? col(150, 230, 160, 0.35 + 0.65 * lit[i]) : key }
    rrect(c, CGRect(x: ox, y: oy, width: 420 * s, height: 300 * s), 40 * s, body)
    // shoulders
    let sh: [(Int, CGFloat, String)] = [(10, 20, "L2"), (8, 90, "L1"), (9, 250, "R1"), (11, 320, "R2")]
    for (i, x, n) in sh {
        rrect(c, CGRect(x: ox + x * s, y: oy - 30 * s, width: 70 * s, height: 24 * s), 10 * s, k(i))
        _ = text(c, n, font("BarlowSemiCondensed-Bold", 18 * s), lit[i] > 0.01 ? night : ink, x: ox + (x + 35) * s, y: oy - 29 * s, maxW: 100 * s, center: true)
    }
    // D-pad
    let dx = ox + 95 * s, dy = oy + 130 * s, a = 30 * s
    rrect(c, CGRect(x: dx - a / 2, y: dy - a * 1.5, width: a, height: a), 6 * s, k(0))
    rrect(c, CGRect(x: dx - a / 2, y: dy + a * 0.5, width: a, height: a), 6 * s, k(1))
    rrect(c, CGRect(x: dx - a * 1.5, y: dy - a / 2, width: a, height: a), 6 * s, k(2))
    rrect(c, CGRect(x: dx + a * 0.5, y: dy - a / 2, width: a, height: a), 6 * s, k(3))
    rrect(c, CGRect(x: dx - a / 2, y: dy - a / 2, width: a, height: a), 4 * s, key)
    // A B X Y as on the Brick: Y X above, B A below
    let fx = ox + 290 * s, fy = oy + 100 * s, r = 24 * s
    let face: [(Int, CGFloat, CGFloat, String)] = [(7, 0, 0, "Y"), (6, 60, 0, "X"), (5, 0, 60, "B"), (4, 60, 60, "A")]
    for (i, x, y, n) in face {
        c.setFillColor(k(i))
        c.fillEllipse(in: CGRect(x: fx + x * s - r, y: fy + y * s - r, width: 2 * r, height: 2 * r))
        _ = text(c, n, font("BarlowSemiCondensed-Bold", 26 * s), lit[i] > 0.01 ? night : ink, x: fx + x * s, y: fy + y * s - 16 * s, maxW: 60 * s, center: true)
    }
    // SELECT MENU START, and the lit F1 F2 (on the Brick they glow by themselves)
    let small: [(Int, CGFloat, CGFloat, String)] = [(13, 120, 228, "SELECT"), (14, 210, 228, "MENU"), (12, 300, 228, "START"),
                                                    (15, 150, 268, "F1"), (16, 270, 268, "F2")]
    for (i, x, y, n) in small {
        let f1f2 = i == 15 || i == 16
        rrect(c, CGRect(x: ox + x * s - 38 * s, y: oy + y * s - 13 * s, width: 76 * s, height: 26 * s), 13 * s,
              f1f2 ? col(90, 200, 120, 0.35 + 0.65 * lit[i]) : k(i))
        _ = text(c, n, font("BarlowSemiCondensed-Bold", 17 * s), lit[i] > 0.01 || f1f2 ? night : ink, x: ox + x * s, y: oy + y * s - 11 * s, maxW: 90 * s, center: true)
    }
}

let muxOnly = framesPath == "-"                 // ("-": only the music in, over the picture kept by a run with KEEP_VIDEO=1)
var n: Int64 = 0
if !muxOnly {
// the writer
try? FileManager.default.removeItem(atPath: outPath + ".video.mp4")
try? FileManager.default.removeItem(atPath: outPath)
let writer = try! AVAssetWriter(outputURL: URL(fileURLWithPath: outPath + ".video.mp4"), fileType: .mp4)
let vin = AVAssetWriterInput(mediaType: .video, outputSettings: [AVVideoCodecKey: AVVideoCodecType.h264, AVVideoWidthKey: W, AVVideoHeightKey: H,
                                                                 AVVideoCompressionPropertiesKey: [AVVideoAverageBitRateKey: wide ? 16_000_000 : 12_000_000,
                                                                                                   AVVideoProfileLevelKey: AVVideoProfileLevelH264HighAutoLevel]])
vin.expectsMediaDataInRealTime = false
let adaptor = AVAssetWriterInputPixelBufferAdaptor(assetWriterInput: vin, sourcePixelBufferAttributes: [
    kCVPixelBufferPixelFormatTypeKey as String: kCVPixelFormatType_32BGRA, kCVPixelBufferWidthKey as String: W, kCVPixelBufferHeightKey as String: H])
writer.add(vin)
writer.startWriting()
writer.startSession(atSourceTime: .zero)

guard let inf = fopen(framesPath, "rb") else { fputs("cannot open \(framesPath)\n", stderr); exit(1) }
var stage = [UInt8](repeating: 0, count: SW * SH * 4)
var heldWord: UInt32 = 0
let stageRect = wide ? CGRect(x: 36, y: 30, width: 1360, height: 1020) : CGRect(x: 20, y: 420, width: 1040, height: 780)

while fread(&heldWord, 4, 1, inf) == 1 && fread(&stage, 1, stage.count, inf) == stage.count {
    let t = Double(n) / Double(FPS)
    for i in 0..<18 { lit[i] = (heldWord >> UInt32(i)) & 1 == 1 ? 1 : max(0, lit[i] - 0.12) }
    var pbOut: CVPixelBuffer?
    CVPixelBufferPoolCreatePixelBuffer(nil, adaptor.pixelBufferPool!, &pbOut)
    let pb = pbOut!
    CVPixelBufferLockBaseAddress(pb, [])
    let c = CGContext(data: CVPixelBufferGetBaseAddress(pb), width: W, height: H, bitsPerComponent: 8, bytesPerRow: CVPixelBufferGetBytesPerRow(pb),
                      space: rgb, bitmapInfo: CGImageAlphaInfo.premultipliedFirst.rawValue | CGBitmapInfo.byteOrder32Little.rawValue)!
    c.translateBy(x: 0, y: CGFloat(H))
    c.scaleBy(x: 1, y: -1)                       // (top-left, as the stage)
    // the night behind
    let grad = CGGradient(colorsSpace: rgb, colors: [col(4, 7, 14), col(14, 22, 36)] as CFArray, locations: [0, 1])!
    c.drawLinearGradient(grad, start: .zero, end: CGPoint(x: 0, y: H), options: [])
    // the stage, large, with a soft glow round it
    let data = CFDataCreate(nil, stage, stage.count)!
    let img = CGImage(width: SW, height: SH, bitsPerComponent: 8, bitsPerPixel: 32, bytesPerRow: SW * 4, space: rgb,
                      bitmapInfo: CGBitmapInfo(rawValue: CGImageAlphaInfo.noneSkipFirst.rawValue | CGBitmapInfo.byteOrder32Little.rawValue),
                      provider: CGDataProvider(data: data)!, decode: nil, shouldInterpolate: true, intent: .defaultIntent)!
    c.saveGState()
    c.setShadow(offset: .zero, blur: 40, color: col(120, 200, 150, 0.35))
    rrect(c, stageRect.insetBy(dx: -6, dy: -6), 24, col(30, 36, 46))
    c.restoreGState()
    c.saveGState()
    c.addPath(CGPath(roundedRect: stageRect, cornerWidth: 18, cornerHeight: 18, transform: nil))
    c.clip()
    c.interpolationQuality = .high
    c.translateBy(x: stageRect.minX, y: stageRect.maxY)
    c.scaleBy(x: 1, y: -1)                       // (CGImage draws bottom-up)
    c.draw(img, in: CGRect(x: 0, y: 0, width: stageRect.width, height: stageRect.height))
    c.restoreGState()
    // the scene's title and line
    let cur = titles.first { t >= $0.t0 && t < $0.t1 && $0.title != "@card" }
    let card = titles.first { t >= $0.t0 && t < $0.t1 && $0.title == "@card" }
    func fade(_ x: Title) -> CGFloat { CGFloat(min(1, min((t - x.t0) / 0.5, (x.t1 - t) / 0.5))) }
    if wide {
        let px: CGFloat = 1430, pw: CGFloat = 450
        _ = text(c, "Mutable Dreams", font("Nunito", 52, weight: 0.4), ink, x: px, y: 40, maxW: pw)
        _ = text(c, "FOR TRIMUI BRICK", font("BarlowSemiCondensed-SemiBold", 28), green, x: px + 2, y: 110, maxW: pw)
        rrect(c, CGRect(x: px, y: 160, width: pw, height: 2), 1, col(80, 120, 90))
        if let ti = cur {
            let a = fade(ti)
            let h = text(c, ti.title, font("Nunito", 44, weight: 0.35), ink, x: px, y: 200, maxW: pw, alpha: a)
            _ = text(c, ti.line, font("BarlowSemiCondensed-SemiBold", 30), dim, x: px, y: 214 + h, maxW: pw, alpha: a)
        }
        buttons(c, px + 15, 680, 1.0)
        rrect(c, CGRect(x: px, y: 1000, width: pw, height: 48), 24, col(150, 230, 160, 0.18))
        _ = text(c, "COMING SOON", font("BarlowSemiCondensed-Bold", 30), green, x: px + pw / 2, y: 1006, maxW: pw, center: true)
    } else {
        _ = text(c, "Mutable Dreams", font("Nunito", 76, weight: 0.4), ink, x: CGFloat(W) / 2, y: 60, maxW: 1040, center: true)
        _ = text(c, "FOR TRIMUI BRICK  ·  COMING SOON", font("BarlowSemiCondensed-SemiBold", 36), green, x: CGFloat(W) / 2, y: 160, maxW: 1040, center: true)
        if let ti = cur {
            let a = fade(ti)
            let h = text(c, ti.title, font("Nunito", 54, weight: 0.35), ink, x: CGFloat(W) / 2, y: 236, maxW: 1000, center: true, alpha: a)
            _ = text(c, ti.line, font("BarlowSemiCondensed-SemiBold", 34), dim, x: CGFloat(W) / 2, y: 244 + h, maxW: 1000, center: true, alpha: a)
        }
        buttons(c, 183, 1290, 1.7)
        _ = text(c, "by Kid Bunka", font("BarlowSemiCondensed-SemiBold", 34), dim, x: CGFloat(W) / 2, y: 1840, maxW: 1000, center: true)
    }
    if let cd = card {                           // a full card: the opening and the end
        let a = fade(cd)
        c.setFillColor(col(4, 7, 14, 0.92 * a))
        c.fill(CGRect(x: 0, y: 0, width: W, height: H))
        let lines = cd.line.split(separator: "|").map(String.init)
        var y: CGFloat = CGFloat(H) / 2 - (wide ? 200 : 300)
        y += text(c, "Mutable Dreams", font("Nunito", wide ? 110 : 120, weight: 0.45), ink, x: CGFloat(W) / 2, y: y, maxW: CGFloat(W) - 80, center: true, alpha: a) + 10
        y += text(c, "FOR TRIMUI BRICK", font("BarlowSemiCondensed-Bold", wide ? 46 : 52), green, x: CGFloat(W) / 2, y: y, maxW: CGFloat(W) - 80, center: true, alpha: a) + 50
        for (i, l) in lines.enumerated() {
            y += text(c, l, i == 0 ? font("Nunito", wide ? 64 : 70, weight: 0.4) : font("BarlowSemiCondensed-SemiBold", wide ? 36 : 40),
                      i == 0 ? ink : dim, x: CGFloat(W) / 2, y: y, maxW: CGFloat(W) - 120, center: true, alpha: a) + 16
        }
    }
    CVPixelBufferUnlockBaseAddress(pb, [])
    while !vin.isReadyForMoreMediaData { usleep(2000) }
    adaptor.append(pb, withPresentationTime: CMTime(value: n, timescale: FPS))
    n += 1
    if n % 300 == 0 { fputs("film: \(n / 30) s\n", stderr) }
}
fclose(inf)
vin.markAsFinished()
let semW = DispatchSemaphore(value: 0)
writer.finishWriting { semW.signal() }
semW.wait()
fputs("film: \(n) frames written\n", stderr)
if ProcessInfo.processInfo.environment["KEEP_VIDEO"] == "1" { exit(0) }
}
let sem = DispatchSemaphore(value: 0)

// the music in: the film's picture and the WAV, as one MP4 (AAC)
let comp = AVMutableComposition()
let va = AVURLAsset(url: URL(fileURLWithPath: outPath + ".video.mp4")), aa = AVURLAsset(url: URL(fileURLWithPath: wavPath))
let vt = va.tracks(withMediaType: .video)[0], at = aa.tracks(withMediaType: .audio)[0]
let dur = CMTimeMinimum(va.duration, aa.duration)
let cv = comp.addMutableTrack(withMediaType: .video, preferredTrackID: kCMPersistentTrackID_Invalid)!
let ca = comp.addMutableTrack(withMediaType: .audio, preferredTrackID: kCMPersistentTrackID_Invalid)!
try! cv.insertTimeRange(CMTimeRange(start: .zero, duration: dur), of: vt, at: .zero)
try! ca.insertTimeRange(CMTimeRange(start: .zero, duration: dur), of: at, at: .zero)
let ex = AVAssetExportSession(asset: comp, presetName: AVAssetExportPresetPassthrough)!
// (passthrough keeps the picture as encoded; the WAV's PCM is not allowed in MP4 - then AAC through a second writer)
ex.outputURL = URL(fileURLWithPath: outPath + ".mov")
ex.outputFileType = .mov
try? FileManager.default.removeItem(atPath: outPath + ".mov")
ex.exportAsynchronously { sem.signal() }
sem.wait()
if ex.status != .completed { fputs("mux failed: \(String(describing: ex.error))\n", stderr); exit(1) }
// .mov (H.264 + PCM) > .mp4 (H.264 + AAC 256 kbit/s), picture passed through
let mov = AVURLAsset(url: URL(fileURLWithPath: outPath + ".mov"))
let reader = try! AVAssetReader(asset: mov)
let rv = AVAssetReaderTrackOutput(track: mov.tracks(withMediaType: .video)[0], outputSettings: nil)
let ra = AVAssetReaderTrackOutput(track: mov.tracks(withMediaType: .audio)[0], outputSettings: [AVFormatIDKey: kAudioFormatLinearPCM])
reader.add(rv); reader.add(ra)
try? FileManager.default.removeItem(atPath: outPath)
let w2 = try! AVAssetWriter(outputURL: URL(fileURLWithPath: outPath), fileType: .mp4)
let wv = AVAssetWriterInput(mediaType: .video, outputSettings: nil, sourceFormatHint: vt.formatDescriptions.first as! CMFormatDescription?)
let wa = AVAssetWriterInput(mediaType: .audio, outputSettings: [AVFormatIDKey: kAudioFormatMPEG4AAC, AVSampleRateKey: 44100, AVNumberOfChannelsKey: 2,
                                                                AVEncoderBitRateKey: 256_000])
w2.add(wv); w2.add(wa)
reader.startReading(); w2.startWriting(); w2.startSession(atSourceTime: .zero)
let g = DispatchGroup()
for (inp, out) in [(wv, rv), (wa, ra)] {
    g.enter()
    inp.requestMediaDataWhenReady(on: DispatchQueue(label: "mux")) {
        while inp.isReadyForMoreMediaData {
            if let sb = out.copyNextSampleBuffer() { inp.append(sb) } else { inp.markAsFinished(); g.leave(); return }
        }
    }
}
g.wait()
w2.finishWriting { sem.signal() }
sem.wait()
try? FileManager.default.removeItem(atPath: outPath + ".mov")
try? FileManager.default.removeItem(atPath: outPath + ".video.mp4")
fputs("film: \(outPath) (\(String(format: "%.1f", CMTimeGetSeconds(dur))) s)\n", stderr)
